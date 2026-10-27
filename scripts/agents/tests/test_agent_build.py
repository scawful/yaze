#!/usr/bin/env python3
"""Quiet-build regressions using fake commands and a disposable build-mode file."""

import contextlib
import importlib.util
import io
import json
import os
from pathlib import Path
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

    def invoke(self, *args, extra_env=None):
        output = io.StringIO()
        with mock.patch.dict(os.environ, {**self.env, **(extra_env or {})}, clear=True), \
                contextlib.redirect_stdout(output), contextlib.redirect_stderr(output):
            result = build.main(list(args), root=self.root, platform="Darwin")
        return result, output.getvalue()

    def calls(self):
        return [json.loads(line) for line in self.log.read_text().splitlines()] if self.log.exists() else []

    def test_new_build_configures_preset_directory_then_selected_targets(self):
        result, output = self.invoke("yaze", "z3ed")
        self.assertEqual(result, 0, output)
        calls = self.calls()
        self.assertEqual(calls[0]["args"], ["--preset", "mac-ai", "-B", str(self.cache_dir)])
        self.assertEqual(calls[1]["args"], ["--build", str(self.cache_dir), "--config", "Debug", "--target", "yaze", "z3ed", "--parallel", "2"])
        self.assertEqual(calls[1]["base"], str(self.root.parent))
        self.assertEqual([call["jobs"] for call in calls], ["2", "2"])
        self.assertIn("taskpolicy -b nice -n 10", output)
        self.assertIn("Build mode: laptop", output)

    def test_builds_never_take_the_native_heavy_lock(self):
        self.assertNotIn("oos-heavy.lock", MODULE_PATH.read_text())

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
        self.assertIn("--parallel 8", output)
        self.assertIn("taskpolicy -c utility nice -n 10", output)
        result, output = self.invoke("--dry-run", extra_env={"YAZE_BUILD_JOBS": "6"})
        self.assertEqual(result, 0, output)
        self.assertIn("--parallel 6", output)
        result, output = self.invoke("--jobs", "9")
        self.assertEqual(result, 2, output)
        self.assertFalse(self.calls())

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
