#!/usr/bin/env python3
"""Quiet-build regressions using fake commands and a disposable build-mode file."""

import contextlib
import importlib.util
import io
import json
import os
from pathlib import Path
import re
import shutil
import signal
import subprocess
import sys
import tempfile
import time
from types import SimpleNamespace
import unittest
from unittest import mock

MODULE_PATH = Path(__file__).resolve().parents[1] / "agent_build.py"
SPEC = importlib.util.spec_from_file_location("agent_build", MODULE_PATH)
build = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(build)

FAKE_CMAKE = r'''
import json, os, signal, subprocess, sys, time
from pathlib import Path
args = sys.argv[1:]
with open(os.environ['FAKE_LOG'], 'a') as stream:
    stream.write(json.dumps({'args': args, 'base': os.environ.get('CCACHE_BASEDIR'), 'marker': os.environ.get('BUILD_MARKER'), 'jobs': os.environ.get('CMAKE_BUILD_PARALLEL_LEVEL')}) + '\n')
if '--preset' in args:
    directory = Path(args[args.index('-B') + 1])
    directory.mkdir(parents=True, exist_ok=True)
    (directory / 'CMakeCache.txt').write_text(
        'CMAKE_HOME_DIRECTORY:INTERNAL=' + os.getcwd() + '\n'
        'CMAKE_GENERATOR:INTERNAL=Ninja Multi-Config\n'
        'CMAKE_CONFIGURATION_TYPES:STRING=Debug;Release\n')
if os.environ.get('FAKE_LONG') and '--build' in args:
    stop = False
    def cancel(signum, frame):
        global stop
        stop = True
    signal.signal(signal.SIGTERM, cancel)
    signal.signal(signal.SIGINT, cancel)
    marker = Path(os.environ['FAKE_LONG'])
    child = subprocess.Popen([sys.executable, '-c',
        'import signal,sys,time; from pathlib import Path; '
        'signal.signal(signal.SIGINT, lambda *_: sys.exit(0)); '
        'signal.signal(signal.SIGTERM, lambda *_: sys.exit(0)); '
        'Path(sys.argv[1]).write_text("ready"); time.sleep(60)', str(marker) + '.child'])
    while not Path(str(marker) + '.child').exists():
        time.sleep(.01)
    marker.write_text(json.dumps([os.getpid(), child.pid]))
    while not stop:
        time.sleep(.02)
    child.wait(timeout=5)
    Path(str(marker) + '.reaped').write_text('done')
sys.exit(int(os.environ.get('FAKE_EXIT', '0')))
'''


class AgentBuildTest(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        self.addCleanup(self.tmp.cleanup)
        self.directory = Path(self.tmp.name).resolve()
        self.root = self.directory / "repo"
        self.root.mkdir()
        self.mode_file = self.directory / "build_mode"
        self.log = self.directory / "calls.jsonl"
        self.bin = self.directory / "bin"
        self.bin.mkdir()
        self.cache_dir = self.root / "build/presets/mac-ai"
        data = {
            "version": 6,
            "configurePresets": [
                {"name": "base", "hidden": True, "generator": "Ninja Multi-Config",
                 "binaryDir": "${sourceDir}/build/presets/${presetName}"},
                {"name": "mac-ai", "inherits": "base"}],
            "buildPresets": [{"name": "mac-ai", "configurePreset": "mac-ai", "configuration": "Debug"}],
        }
        (self.root / "CMakePresets.json").write_text(json.dumps(data))
        self.command("cmake", FAKE_CMAKE)
        self.command("taskpolicy", "import os,sys\nskip = 3 if sys.argv[1] == '-c' else 2\n"
                                   "os.execvp(sys.argv[skip], sys.argv[skip:])\n")
        self.command("nice", "import os,sys\nos.execvp(sys.argv[3], sys.argv[3:])\n")
        self.env = {"PATH": str(self.bin) + os.pathsep + os.defpath,
                    "FAKE_LOG": str(self.log), "PYTHONDONTWRITEBYTECODE": "1",
                    "OOS_BUILD_MODE_FILE": str(self.mode_file)}
        self.pressure = (500 * 2**30, 0.0, 32768.0)
        patcher = mock.patch.object(build, "host_pressure", lambda _path: self.pressure)
        patcher.start()
        self.addCleanup(patcher.stop)

    def command(self, name, code):
        path = self.bin / name
        path.write_text(f"#!{sys.executable}\n" + code)
        path.chmod(0o755)

    def cache(self, root=None, config="Debug;Release"):
        self.cache_dir.mkdir(parents=True, exist_ok=True)
        (self.cache_dir / "CMakeCache.txt").write_text(
            f"CMAKE_HOME_DIRECTORY:INTERNAL={root or self.root}\n"
            "CMAKE_GENERATOR:INTERNAL=Ninja Multi-Config\n"
            f"CMAKE_CONFIGURATION_TYPES:STRING={config}\n")

    def invoke(self, *args, extra_env=None, allow_new_build=True):
        output = io.StringIO()
        with mock.patch.dict(os.environ, {**self.env, **(extra_env or {})}, clear=True), \
                contextlib.redirect_stdout(output), contextlib.redirect_stderr(output):
            options = ["--allow-new-build"] if allow_new_build else []
            result = build.main([*options, *args], root=self.root, platform="Darwin")
        return result, output.getvalue()

    def calls(self):
        return [json.loads(line) for line in self.log.read_text().splitlines()] if self.log.exists() else []

    def test_new_build_configures_preset_directory_then_selected_targets(self):
        result, output = self.invoke("yaze", "z3ed")
        self.assertEqual(result, 0, output)
        calls = self.calls()
        self.assertEqual(calls[0]["args"], ["--preset", "mac-ai", "-DCMAKE_DISABLE_PRECOMPILE_HEADERS=ON",
                                            "-B", str(self.cache_dir)])
        self.assertEqual(calls[1]["args"], ["--build", str(self.cache_dir), "--config", "Debug", "--target", "yaze", "z3ed", "--parallel", "2"])
        self.assertEqual(calls[1]["base"], str(self.root.parent))
        self.assertEqual([call["jobs"] for call in calls], ["2", "2"])
        self.assertIn("taskpolicy -b nice -n 10", output)
        self.assertIn("Build mode: laptop", output)

    def test_builds_never_take_the_native_heavy_lock(self):
        self.assertNotIn("oos-heavy.lock", MODULE_PATH.read_text())

    def test_new_build_can_opt_back_into_precompiled_headers(self):
        result, output = self.invoke(extra_env={"YAZE_AGENT_PCH": "1"})
        self.assertEqual(result, 0, output)
        self.assertEqual(self.calls()[0]["args"], ["--preset", "mac-ai", "-DCMAKE_DISABLE_PRECOMPILE_HEADERS=OFF",
                                                   "-B", str(self.cache_dir)])

    def test_fresh_keeps_the_existing_tree_precompiled_header_choice(self):
        self.cache()
        result, output = self.invoke("--fresh", extra_env={"YAZE_AGENT_PCH": "0"})
        self.assertEqual(result, 0, output)
        self.assertIn("-DCMAKE_DISABLE_PRECOMPILE_HEADERS=OFF", self.calls()[0]["args"])
        for recorded in ("ON", "YES", "off", "2"):
            self.cache()
            with (self.cache_dir / "CMakeCache.txt").open("a") as stream:
                stream.write(f"CMAKE_DISABLE_PRECOMPILE_HEADERS:BOOL={recorded}\n")
            result, output = self.invoke("--fresh", extra_env={"YAZE_AGENT_PCH": "1"})
            self.assertEqual(result, 0, output)
            self.assertIn(f"-DCMAKE_DISABLE_PRECOMPILE_HEADERS={recorded}", self.calls()[-2]["args"])

    @unittest.skipUnless(shutil.which("cmake") and shutil.which("ninja"), "needs cmake and ninja")
    def test_real_cmake_cache_keeps_the_choice_over_a_preset_value(self):
        (self.root / "CMakeLists.txt").write_text("cmake_minimum_required(VERSION 3.20)\nproject(probe NONE)\n")
        data = json.loads((self.root / "CMakePresets.json").read_text())
        data["configurePresets"][0]["generator"] = "Ninja"
        data["configurePresets"][0]["cacheVariables"] = {"CMAKE_DISABLE_PRECOMPILE_HEADERS": "ON"}
        del data["buildPresets"][0]["configuration"]
        (self.root / "CMakePresets.json").write_text(json.dumps(data))
        env = {"PATH": os.environ.get("PATH", os.defpath)}
        cache = self.cache_dir / "CMakeCache.txt"

        def configure(fresh, extra_env=None):
            args = SimpleNamespace(preset="mac-ai", jobs=1, targets=["all"], fresh=fresh,
                                   allow_new_build=True)
            command = build.plan(self.root, args, {**env, **(extra_env or {})}, "Linux")[0][0]
            subprocess.run(command, cwd=self.root, env=env, check=True, capture_output=True)
            match = re.search(r"^CMAKE_DISABLE_PRECOMPILE_HEADERS:[^=]*=(.*)$", cache.read_text(), re.M)
            return match[1] if match else None

        self.assertEqual(configure(False, {"YAZE_AGENT_PCH": "1"}), "OFF")
        self.assertEqual(configure(True), "OFF")
        cache.write_text(re.sub(r"^CMAKE_DISABLE_PRECOMPILE_HEADERS:.*$",
                                "CMAKE_DISABLE_PRECOMPILE_HEADERS:BOOL=YES", cache.read_text(), flags=re.M))
        self.assertEqual(configure(True, {"YAZE_AGENT_PCH": "1"}), "YES")

    def test_warm_cache_builds_only_once_without_configure(self):
        self.cache()
        result, output = self.invoke("yaze_test_unit", "--jobs", "4")
        self.assertEqual(result, 0, output)
        self.assertEqual(len(self.calls()), 1)
        self.assertEqual(self.calls()[0]["args"][-1], "4")

    def test_cached_build_preserves_inherited_preset_environment(self):
        self.cache()
        path = self.root / "CMakePresets.json"
        data = json.loads(path.read_text())
        data["configurePresets"][0]["environment"] = {"PATH": "$penv{PATH}", "BUILD_MARKER": "$env{INNER}", "INNER": "configured"}
        data["buildPresets"][0]["environment"] = {"INNER": "build-override"}
        path.write_text(json.dumps(data))
        result, output = self.invoke()
        self.assertEqual(result, 0, output)
        self.assertEqual(self.calls()[0]["marker"], "build-override")

    def test_build_directory_override_is_used_by_configure_and_build(self):
        other = self.directory / "custom"
        result, output = self.invoke(extra_env={"YAZE_BUILD_DIR": str(other)})
        self.assertEqual(result, 0, output)
        self.assertEqual(self.calls()[0]["args"][-1], str(other))
        self.assertEqual(self.calls()[1]["args"][1], str(other))

    def test_fresh_is_explicit_and_still_refuses_foreign_cache(self):
        self.cache()
        result, output = self.invoke("--fresh")
        self.assertEqual(result, 0, output)
        self.assertEqual(self.calls()[0]["args"][-1], "--fresh")
        self.cache(root=self.directory / "foreign")
        before = len(self.calls())
        result, output = self.invoke("--fresh")
        self.assertEqual(result, 2, output)
        self.assertEqual(len(self.calls()), before)

    def test_foreign_source_and_wrong_config_fail_before_build(self):
        for root, config in ((self.directory / "foreign", "Debug"), (self.root, "Release")):
            with self.subTest(root=root, config=config):
                self.cache(root=root, config=config)
                result, output = self.invoke()
                self.assertEqual(result, 2, output)
                self.assertFalse(self.calls())

    def test_nonempty_directory_without_cache_is_not_reconfigured(self):
        self.cache_dir.mkdir(parents=True)
        (self.cache_dir / "unknown").write_text("keep")
        result, output = self.invoke()
        self.assertEqual(result, 2, output)
        self.assertFalse(self.calls())

    def test_jobs_precedence_and_maximum(self):
        self.cache()
        result, output = self.invoke("--dry-run", extra_env={"YAZE_BUILD_JOBS": "3", "CMAKE_BUILD_PARALLEL_LEVEL": "4"})
        self.assertEqual(result, 0, output)
        self.assertIn("--parallel 3", output)
        result, output = self.invoke("--dry-run", extra_env={"CMAKE_BUILD_PARALLEL_LEVEL": "4"})
        self.assertEqual(result, 0, output)
        self.assertIn("--parallel 2", output)
        result, output = self.invoke("--dry-run", "--jobs", "4", extra_env={"YAZE_BUILD_JOBS": "3"})
        self.assertEqual(result, 0, output)
        self.assertIn("--parallel 4", output)
        for jobs in ("0", "5", "8"):
            result, output = self.invoke(extra_env={"YAZE_BUILD_JOBS": jobs})
            self.assertEqual(result, 2, output)
        self.assertFalse(self.calls())

    def test_desk_mode_raises_default_and_maximum_and_allows_p_cores(self):
        self.cache()
        self.mode_file.write_text("desk\n")
        result, output = self.invoke("--dry-run", extra_env={"CMAKE_BUILD_PARALLEL_LEVEL": "4"})
        self.assertEqual(result, 0, output)
        self.assertIn("Build mode: desk", output)
        self.assertIn(f"--parallel {build.HOST_JOBS}", output)
        self.assertNotIn("taskpolicy", output)
        self.assertNotIn("nice -n", output)
        result, output = self.invoke("--dry-run", extra_env={"YAZE_BUILD_JOBS": "1"})
        self.assertEqual(result, 0, output)
        self.assertIn("--parallel 1", output)
        result, output = self.invoke("--jobs", str(build.HOST_JOBS + 1))
        self.assertEqual(result, 2, output)
        self.assertFalse(self.calls())

    def test_cold_build_requires_explicit_choice_before_cmake(self):
        result, output = self.invoke(allow_new_build=False)
        self.assertEqual(result, 2, output)
        self.assertIn("--allow-new-build", output)
        self.assertFalse(self.calls())

    def test_mac_swap_pressure_refuses_build_and_points_to_windows_box(self):
        self.cache()
        self.pressure = (500 * 2**30, 30000.0, 32768.0)
        result, output = self.invoke()
        self.assertEqual(result, 2, output)
        self.assertIn("swap 29.3 of 32.0 GiB used", output)
        self.assertIn("scripts/remote/mm-test.sh", output)
        self.assertFalse(self.calls())

    def test_low_disk_refuses_only_a_new_build_tree(self):
        self.pressure = (20 * 2**30, 0.0, 32768.0)
        result, output = self.invoke()
        self.assertEqual(result, 2, output)
        self.assertIn("20 GiB free disk for a new build tree", output)
        self.assertFalse(self.calls())
        self.cache()
        result, output = self.invoke()
        self.assertEqual(result, 0, output)
        self.assertEqual(len(self.calls()), 1)

    def test_local_override_and_preview_ignore_mac_pressure(self):
        self.cache()
        self.pressure = (1 * 2**30, 32000.0, 32768.0)
        result, output = self.invoke("--preflight")
        self.assertEqual(result, 0, output)
        self.assertFalse(self.calls())
        result, output = self.invoke(extra_env={"YAZE_AGENT_BUILD_LOCAL": "1"})
        self.assertEqual(result, 0, output)
        self.assertEqual(len(self.calls()), 1)

    def test_preflight_reports_cold_cache_without_building(self):
        result, output = self.invoke("--preflight", allow_new_build=False)
        self.assertEqual(result, 0, output)
        self.assertIn("Cache: COLD", output)
        self.assertFalse(self.calls())

    def test_explicit_source_checkout_is_used_without_modifying_helper_source(self):
        self.cache()
        result, output = self.invoke("--source-checkout", str(self.root),
                                     "--preflight", allow_new_build=False)
        self.assertEqual(result, 0, output)
        self.assertIn(f"Source: {self.root}", output)
        self.assertIn("Cache: REUSE", output)
        self.assertFalse(self.calls())

    def test_warm_cache_needs_no_cold_build_permission(self):
        self.cache()
        result, output = self.invoke(allow_new_build=False)
        self.assertEqual(result, 0, output)
        self.assertIn("Cache: REUSE", output)
        self.assertEqual(len(self.calls()), 1)

    def test_missing_nested_implot_stops_before_configure(self):
        (self.root / ".gitmodules").write_text('[submodule "ext/imgui"]\n')
        for relative in ("ext/imgui/imgui.h", "ext/imgui/imgui.cpp",
                         "ext/imgui_test_engine/imgui_test_engine/imgui_te_engine.h"):
            path = self.root / relative
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_text("fixture\n")
        result, output = self.invoke()
        self.assertEqual(result, 2, output)
        self.assertIn("thirdparty/implot/implot.h", output)
        self.assertFalse(self.calls())

    def test_complete_bundled_files_pass_archive_preflight(self):
        (self.root / ".gitmodules").write_text('[submodule "ext/imgui"]\n')
        for relative in ("ext/imgui/imgui.h", "ext/imgui/imgui.cpp",
                         "ext/imgui_test_engine/imgui_test_engine/imgui_te_engine.h",
                         "ext/imgui_test_engine/imgui_test_suite/thirdparty/implot/implot.h",
                         "ext/imgui_test_engine/imgui_test_suite/thirdparty/implot/implot.cpp",
                         "ext/imgui_test_engine/imgui_test_suite/thirdparty/implot/implot_items.cpp"):
            path = self.root / relative
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_text("fixture\n")
        result, output = self.invoke("--preflight")
        self.assertEqual(result, 0, output)
        self.assertFalse(self.calls())

    def test_reuse_checkout_uses_same_commit_cache_and_rejects_dirty_or_other_head(self):
        git = shutil.which("git")
        if not git:
            self.skipTest("git unavailable")
        hooks = self.directory / "fixture-hooks"
        hooks.mkdir()
        def run(*args, cwd=self.root):
            return subprocess.run(
                [git, "-c", f"core.hooksPath={hooks}", "-c", "commit.gpgsign=false", *args],
                cwd=cwd, check=True, capture_output=True, text=True,
                env=build.git_environment())
        run("init")
        run("add", "CMakePresets.json")
        run("-c", "user.name=Fixture", "-c", "user.email=fixture@example.invalid",
            "commit", "-m", "fixture")
        other = self.directory / "warm-owner"
        run("clone", "--shared", str(self.root), str(other))
        cache = other / "build/presets/mac-ai"
        cache.mkdir(parents=True)
        (cache / "CMakeCache.txt").write_text(
            f"CMAKE_HOME_DIRECTORY:INTERNAL={other}\n"
            "CMAKE_GENERATOR:INTERNAL=Ninja Multi-Config\n"
            "CMAKE_CONFIGURATION_TYPES:STRING=Debug;Release\n")
        result, output = self.invoke("--reuse-from", str(other), allow_new_build=False)
        self.assertEqual(result, 0, output)
        self.assertEqual(self.calls()[0]["args"][1], str(cache))
        data = json.loads((other / "CMakePresets.json").read_text())
        data["configurePresets"][1]["cacheVariables"] = {"YAZE_ENABLE_AI": "ON"}
        (other / "CMakePresets.json").write_text(json.dumps(data))
        run("add", "CMakePresets.json", cwd=other)
        run("-c", "user.name=Fixture", "-c", "user.email=fixture@example.invalid",
            "commit", "-m", "feature", cwd=other)
        run("pull", "--ff-only", str(other), "HEAD")
        with (cache / "CMakeCache.txt").open("a") as stream:
            stream.write("YAZE_ENABLE_AI:BOOL=OFF\n")
        result, output = self.invoke("--reuse-from", str(other), allow_new_build=False)
        self.assertEqual(result, 2, output)
        self.assertIn("Reuse cache option YAZE_ENABLE_AI differs", output)
        with (other / "CMakePresets.json").open("a") as stream:
            stream.write("\n")
        with self.assertRaisesRegex(build.BuildError, "clean tracked"):
            build.reuse_checkout(self.root, other)
        run("add", "CMakePresets.json", cwd=other)
        run("-c", "user.name=Fixture", "-c", "user.email=fixture@example.invalid",
            "commit", "-m", "changed", cwd=other)
        with self.assertRaisesRegex(build.BuildError, "commit differs"):
            build.reuse_checkout(self.root, other)
        # A hook exporting one repository for both commands must not make two
        # actually different commits appear identical.
        with mock.patch.dict(os.environ, {"GIT_DIR": str(self.root / ".git"),
                                          "GIT_WORK_TREE": str(self.root)}):
            with self.assertRaisesRegex(build.BuildError, "commit differs"):
                build.reuse_checkout(self.root, other)

    def test_unknown_build_mode_falls_back_to_laptop(self):
        self.cache()
        self.mode_file.write_text("turbo\n")
        result, output = self.invoke("--dry-run")
        self.assertEqual(result, 0, output)
        self.assertIn("Build mode: laptop", output)
        self.assertIn("taskpolicy -b nice -n 10", output)
        self.assertIn("--parallel 2", output)

    def test_failure_returns_build_status(self):
        self.cache()
        result, output = self.invoke(extra_env={"FAKE_EXIT": "7"})
        self.assertEqual(result, 7, output)

    def test_second_writer_refuses_before_running_cmake(self):
        self.cache()
        with build.BuildDirectoryLock(self.cache_dir):
            result, output = self.invoke()
        self.assertEqual(result, 2, output)
        self.assertIn("already in use", output)
        self.assertFalse(self.calls())
        result, output = self.invoke()
        self.assertEqual(result, 0, output)

    def test_lock_is_released_on_exception_and_dry_run_does_not_acquire(self):
        with self.assertRaisesRegex(RuntimeError, "fixture"):
            with build.BuildDirectoryLock(self.cache_dir):
                raise RuntimeError("fixture")
        self.assertFalse(self.cache_dir.exists())
        with build.BuildDirectoryLock(self.cache_dir):
            result, output = self.invoke("--preflight")
        self.assertEqual(result, 0, output)
        self.assertFalse(self.calls())

    @unittest.skipUnless(shutil.which("cmake"), "needs cmake")
    def test_raw_cmake_rejects_concurrent_configure_and_releases_lock(self):
        cmake = shutil.which("cmake")
        module = MODULE_PATH.parents[2] / "cmake/build_ownership.cmake"
        source = self.root / "CMakeLists.txt"
        ready = self.directory / "ready"
        source.write_text(
            'cmake_minimum_required(VERSION 3.16)\n'
            f'include("{module.as_posix()}")\nproject(lock_probe NONE)\n'
            'if(DEFINED ENV{PROBE_HOLD})\n'
            f'  file(WRITE "{ready.as_posix()}" "ready")\n'
            '  execute_process(COMMAND "${CMAKE_COMMAND}" -E sleep 2)\nendif()\n')
        command = [cmake, "-S", str(self.root), "-B", str(self.cache_dir)]
        first = subprocess.Popen(command, env={**os.environ, "PROBE_HOLD": "1"},
                                 stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True)
        try:
            deadline = time.monotonic() + 8
            while not ready.exists() and first.poll() is None and time.monotonic() < deadline:
                time.sleep(.02)
            self.assertTrue(ready.exists(), "first configure did not acquire lock")
            second = subprocess.run(command, capture_output=True, text=True, timeout=10)
            self.assertNotEqual(second.returncode, 0)
            self.assertIn("Another configure is using", second.stderr)
            output, _ = first.communicate(timeout=10)
            self.assertEqual(first.returncode, 0, output)
            third = subprocess.run(command, capture_output=True, text=True, timeout=10)
            self.assertEqual(third.returncode, 0, third.stderr)
        finally:
            if first.poll() is None:
                first.terminate()
                first.communicate(timeout=10)

    @unittest.skipUnless(shutil.which("cmake"), "needs cmake")
    def test_cpm_defaults_to_one_source_cache_and_preserves_explicit_off(self):
        cmake = shutil.which("cmake")
        home = self.directory / "home"
        module = MODULE_PATH.parents[2] / "cmake/CPM.cmake"
        shared = home / ".cpm-cache"
        bootstrap = shared / "cpm/CPM_0.38.7.cmake"
        bootstrap.parent.mkdir(parents=True)
        bootstrap.write_text("# Offline fixture: no network or package work.\n")
        env = {k: v for k, v in os.environ.items()
               if k not in ("CPM_SOURCE_CACHE", "GITHUB_ACTIONS")}
        env["HOME"] = str(home)
        for name in ("a", "b", "explicit-off"):
            root = self.directory / name
            root.mkdir()
            directory = root / "build"
            root.joinpath("CMakeLists.txt").write_text(
                'cmake_minimum_required(VERSION 3.16)\nproject(cache_probe NONE)\n'
                f'include("{module.as_posix()}")\n'
                'file(WRITE "${CMAKE_BINARY_DIR}/selected.txt" "${CPM_SOURCE_CACHE}")\n')
            command = [cmake, "-S", str(root), "-B", str(directory)]
            if name == "explicit-off":
                local = directory / "cmake/CPM_0.38.7.cmake"
                local.parent.mkdir(parents=True)
                local.write_text("# Offline fixture.\n")
                command.append("-DCPM_SOURCE_CACHE=OFF")
            result = subprocess.run(command, env=env, capture_output=True, text=True, timeout=10)
            self.assertEqual(result.returncode, 0, result.stderr)
            expected = "OFF" if name == "explicit-off" else str(shared)
            self.assertEqual((directory / "selected.txt").read_text(), expected)

    @unittest.skipUnless(shutil.which("cmake") and shutil.which("ninja")
                         and shutil.which("ccache"), "needs cmake, ninja and ccache")
    def test_real_two_checkout_objects_use_no_pch_and_share_portable_object(self):
        cmake, ccache = shutil.which("cmake"), shutil.which("ccache")
        template = MODULE_PATH.parents[2] / "test/yaze_test_paths.h.in"
        env = {k: v for k, v in os.environ.items() if not k.startswith("CCACHE_")}
        env.update(CCACHE_DIR=str(self.directory / "ccache"),
                   CCACHE_BASEDIR=str(self.directory), CCACHE_NOHASHDIR="1",
                   CCACHE_SLOPPINESS="include_file_mtime,include_file_ctime",
                   OOS_BUILD_MODE_FILE=str(self.mode_file))
        for name in ("a", "b"):
            root = self.directory / name
            root.mkdir()
            root.joinpath("test").mkdir()
            shutil.copyfile(template, root / "test/yaze_test_paths.h.in")
            root.joinpath("main.cc").write_text(
                'const char* fixture_path();\nint main() { return fixture_path()[0] == 0; }\n')
            root.joinpath("fixture.cc").write_text(
                '#include "yaze_test_paths.h"\nconst char* fixture_path() { return YAZE_TEST_FIXTURE_DIR; }\n')
            root.joinpath("CMakeLists.txt").write_text(
                'cmake_minimum_required(VERSION 3.20)\nproject(object_probe CXX)\n'
                'configure_file(test/yaze_test_paths.h.in generated/yaze_test_paths.h @ONLY)\n'
                'add_executable(proof main.cc fixture.cc)\n'
                'target_include_directories(proof PRIVATE "${CMAKE_BINARY_DIR}/generated")\n'
                'target_precompile_headers(proof PRIVATE <vector>)\n')
            data = {"version": 6, "configurePresets": [{"name": "probe", "generator": "Ninja",
                    "binaryDir": "${sourceDir}/build", "cacheVariables": {
                        "CMAKE_CXX_COMPILER_LAUNCHER": ccache,
                        "CMAKE_EXPORT_COMPILE_COMMANDS": "ON"}}],
                    "buildPresets": [{"name": "probe", "configurePreset": "probe"}]}
            root.joinpath("CMakePresets.json").write_text(json.dumps(data))
            stats = self.directory / (name + ".stats")
            with mock.patch.dict(os.environ, {**env, "CCACHE_STATSLOG": str(stats)}, clear=True):
                result = build.main(["--preset", "probe", "--jobs", "1", "--allow-new-build", "proof"],
                                    root=root, platform="Linux")
            self.assertEqual(result, 0)
            commands = json.loads((root / "build/compile_commands.json").read_text())
            self.assertEqual(len(commands), 2)
            self.assertNotIn("cmake_pch", json.dumps(commands))
            events = stats.read_text().splitlines()
            self.assertEqual(events.count("cache_miss"), 2 if name == "a" else 1, events)
            if name == "b":
                self.assertEqual(events.count("direct_cache_hit"), 1, events)
            # The path-sensitive source deliberately misses: its compiled
            # fixture path must identify the new checkout, not the first one.
            self.assertIn(str(root), (root / "build/generated/yaze_test_paths.h").read_text())

    def test_group_exit_readback_distinguishes_live_and_terminated_members(self):
        cancellation = build.Cancellation()
        cancellation.child = SimpleNamespace(pid=12345)
        for output, expected in (("12345 S\n99 R\n", True),
                                 ("12345 Z\n99 R\n", False),
                                 ("99 R\n", False)):
            with mock.patch.object(build.subprocess, "run", return_value=SimpleNamespace(stdout=output)):
                self.assertEqual(cancellation.group_alive(), expected)
        with mock.patch.object(build.os, "killpg", side_effect=PermissionError), \
                mock.patch.object(cancellation, "group_alive", return_value=False):
            cancellation.send_group(signal.SIGTERM)
        with mock.patch.object(build.os, "killpg", side_effect=PermissionError), \
                mock.patch.object(cancellation, "group_alive", return_value=True):
            with self.assertRaises(PermissionError):
                cancellation.send_group(signal.SIGTERM)

    def test_missing_qos_tool_and_action_target_fail_before_build(self):
        (self.bin / "taskpolicy").unlink()
        with mock.patch.object(build.shutil, "which", return_value=None):
            result, output = self.invoke()
        self.assertEqual(result, 2, output)
        for target in ("all", "install", "test", "test-fast"):
            result, output = self.invoke(target)
            self.assertEqual(result, 2, output)
        self.assertFalse(self.calls())

    def test_sigint_and_sigterm_wait_for_owned_process_group(self):
        self.cache()
        driver = (
            "import importlib.util,sys; from pathlib import Path; "
            f"s=importlib.util.spec_from_file_location('helper',{str(MODULE_PATH)!r}); "
            "m=importlib.util.module_from_spec(s); s.loader.exec_module(m); "
            f"sys.exit(m.main([],root=Path({str(self.root)!r}),platform='Darwin'))")
        for signum in (signal.SIGINT, signal.SIGTERM):
            with self.subTest(signum=signum):
                marker = self.directory / f"process-{signum}"
                proc = subprocess.Popen([sys.executable, "-c", driver],
                                        env={**self.env, "FAKE_LONG": str(marker)},
                                        stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True)
                try:
                    deadline = time.monotonic() + 8
                    while not marker.exists() and proc.poll() is None and time.monotonic() < deadline:
                        time.sleep(.02)
                    self.assertTrue(marker.exists(), "fake build did not start")
                    pids = json.loads(marker.read_text())
                    proc.send_signal(signum)
                    output, _ = proc.communicate(timeout=8)
                    self.assertEqual(proc.returncode, 2, output)
                    self.assertTrue(Path(str(marker) + ".reaped").exists())
                    for pid in pids:
                        with self.assertRaises(ProcessLookupError):
                            os.kill(pid, 0)
                finally:
                    if proc.poll() is None:
                        proc.terminate()
                        proc.wait(timeout=15)


if __name__ == "__main__":
    unittest.main()
