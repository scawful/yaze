#!/usr/bin/env python3
"""Quiet-build regressions using fake commands and disposable lock directories."""

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
import threading
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
        self.lock = self.directory / "heavy.lock"
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
        self.command("taskpolicy", "import os,sys\nos.execvp(sys.argv[2], sys.argv[2:])\n")
        self.command("nice", "import os,sys\nos.execvp(sys.argv[3], sys.argv[3:])\n")
        self.env = {"PATH": str(self.bin) + os.pathsep + os.defpath,
                    "FAKE_LOG": str(self.log), "PYTHONDONTWRITEBYTECODE": "1"}

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
            result = build.main(list(args), root=self.root, lock_path=self.lock, platform="Darwin")
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
        self.assertFalse(self.lock.exists())

    def test_new_build_can_opt_back_into_precompiled_headers(self):
        result, output = self.invoke(extra_env={"YAZE_AGENT_PCH": "1"})
        self.assertEqual(result, 0, output)
        self.assertEqual(self.calls()[0]["args"], ["--preset", "mac-ai", "-B", str(self.cache_dir)])

    def test_fresh_keeps_the_existing_tree_precompiled_header_choice(self):
        self.cache()
        result, output = self.invoke("--fresh")
        self.assertEqual(result, 0, output)
        self.assertNotIn("-DCMAKE_DISABLE_PRECOMPILE_HEADERS=ON", self.calls()[0]["args"])
        with (self.cache_dir / "CMakeCache.txt").open("a") as stream:
            stream.write("CMAKE_DISABLE_PRECOMPILE_HEADERS:BOOL=ON\n")
        result, output = self.invoke("--fresh")
        self.assertEqual(result, 0, output)
        self.assertIn("-DCMAKE_DISABLE_PRECOMPILE_HEADERS=ON", self.calls()[-2]["args"])

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

    def test_foreign_source_and_wrong_config_fail_before_lock(self):
        for root, config in ((self.directory / "foreign", "Debug"), (self.root, "Release")):
            with self.subTest(root=root, config=config):
                self.cache(root=root, config=config)
                result, output = self.invoke()
                self.assertEqual(result, 2, output)
                self.assertFalse(self.lock.exists())
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
        for jobs in ("0", "6", "7"):
            result, output = self.invoke(extra_env={"YAZE_BUILD_JOBS": jobs})
            self.assertEqual(result, 2, output)
        self.assertFalse(self.calls())

    def test_status_and_dry_run_do_not_modify_legacy_lock(self):
        self.cache()
        self.lock.mkdir()
        owner = self.lock / "owner"
        owner.write_text("other-agent\n")
        before = owner.stat()
        for args in (("--status",), ("--dry-run",)):
            result, output = self.invoke(*args)
            self.assertEqual(result, 0, output)
            self.assertIn("owner=other-agent", output)
        self.assertEqual(owner.stat().st_mtime_ns, before.st_mtime_ns)
        self.assertEqual(owner.read_text(), "other-agent\n")
        self.assertFalse(self.calls())

    def test_legacy_lock_timeout_never_removes_owner(self):
        self.cache()
        self.lock.mkdir()
        (self.lock / "owner").write_text("foreign\n")
        result, output = self.invoke("--wait-seconds", "0")
        self.assertEqual(result, 2, output)
        self.assertEqual((self.lock / "owner").read_text(), "foreign\n")
        self.assertFalse(self.calls())

    def test_wait_acquires_only_after_test_owner_releases(self):
        self.cache()
        self.lock.mkdir()
        (self.lock / "owner").write_text("test-owner\n")
        def release():
            time.sleep(.1)
            (self.lock / "owner").unlink()
            self.lock.rmdir()
        thread = threading.Thread(target=release)
        thread.start()
        try:
            result, output = self.invoke("--wait-seconds", "4")
        finally:
            thread.join()
        self.assertEqual(result, 0, output)
        self.assertFalse(self.lock.exists())

    def test_cache_identity_is_rechecked_after_wait(self):
        self.cache()
        self.lock.mkdir()
        (self.lock / "owner").write_text("test-owner\n")
        def replace_cache_then_release():
            time.sleep(.1)
            self.cache(root=self.directory / "foreign")
            (self.lock / "owner").unlink()
            self.lock.rmdir()
        thread = threading.Thread(target=replace_cache_then_release)
        thread.start()
        try:
            result, output = self.invoke("--wait-seconds", "4")
        finally:
            thread.join()
        self.assertEqual(result, 2, output)
        self.assertFalse(self.calls())
        self.assertFalse(self.lock.exists())

    def test_malformed_metadata_and_symlink_lock_refuse(self):
        self.cache()
        self.lock.mkdir()
        (self.lock / "owner").write_text("foreign\n")
        metadata = self.lock / "metadata.json"
        for value in ("{", "null", "{}"):
            metadata.write_text(value)
            result, output = self.invoke("--wait-seconds", "0")
            self.assertEqual(result, 2, output)
            self.assertEqual(metadata.read_text(), value)
        metadata.unlink()
        (self.lock / "owner").unlink()
        self.lock.rmdir()
        self.lock.symlink_to(self.root, target_is_directory=True)
        result, output = self.invoke("--wait-seconds", "0")
        self.assertEqual(result, 2, output)
        self.assertTrue(self.lock.is_symlink())
        self.assertFalse(self.calls())

    def test_changed_ownership_is_not_released(self):
        lane = build.HeavyLane(self.lock, "test", self.root)
        lane.acquire(0, build.Cancellation())
        (self.lock / "owner").write_text("new-owner\n")
        with self.assertRaises(build.BuildError):
            lane.release()
        self.assertTrue(self.lock.exists())

    def test_metadata_publication_is_atomic_for_another_waiter(self):
        lane = build.HeavyLane(self.lock, "test", self.root)
        original_dump = json.dump
        observed = []
        def paused_dump(value, stream):
            stream.write('{')
            stream.flush()
            observed.append(build.lock_status(self.lock))
            stream.seek(0)
            stream.truncate()
            original_dump(value, stream)
        with mock.patch.object(build.json, "dump", side_effect=paused_dump):
            lane.acquire(0, build.Cancellation())
        self.assertEqual(observed[0]["owner"], "test")
        self.assertIsNone(observed[0]["metadata"])
        self.assertEqual(build.lock_status(self.lock)["metadata"], lane.metadata)
        lane.release()
        self.assertFalse(self.lock.exists())

    def test_failure_releases_own_lock(self):
        self.cache()
        result, output = self.invoke(extra_env={"FAKE_EXIT": "7"})
        self.assertEqual(result, 7, output)
        self.assertFalse(self.lock.exists())

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

    def test_missing_qos_tool_and_action_target_fail_before_lock(self):
        (self.bin / "taskpolicy").unlink()
        with mock.patch.object(build.shutil, "which", return_value=None):
            result, output = self.invoke()
        self.assertEqual(result, 2, output)
        for target in ("all", "install", "test", "test-fast"):
            result, output = self.invoke(target)
            self.assertEqual(result, 2, output)
        self.assertFalse(self.lock.exists())
        self.assertFalse(self.calls())

    def test_sigint_and_sigterm_wait_for_owned_process_group(self):
        self.cache()
        driver = (
            "import importlib.util,sys; from pathlib import Path; "
            f"s=importlib.util.spec_from_file_location('helper',{str(MODULE_PATH)!r}); "
            "m=importlib.util.module_from_spec(s); s.loader.exec_module(m); "
            f"sys.exit(m.main([],root=Path({str(self.root)!r}),lock_path=Path({str(self.lock)!r}),platform='Darwin'))")
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
                    self.assertTrue(self.lock.exists())
                    proc.send_signal(signum)
                    output, _ = proc.communicate(timeout=8)
                    self.assertEqual(proc.returncode, 2, output)
                    self.assertTrue(Path(str(marker) + ".reaped").exists())
                    self.assertFalse(self.lock.exists(), output)
                    for pid in pids:
                        with self.assertRaises(ProcessLookupError):
                            os.kill(pid, 0)
                finally:
                    if proc.poll() is None:
                        proc.terminate()
                        proc.wait(timeout=15)

    def test_sigterm_while_waiting_preserves_existing_lock(self):
        self.cache()
        self.lock.mkdir()
        (self.lock / "owner").write_text("test-other-owner\n")
        driver = (
            "import importlib.util,sys; from pathlib import Path; "
            f"s=importlib.util.spec_from_file_location('helper',{str(MODULE_PATH)!r}); "
            "m=importlib.util.module_from_spec(s); s.loader.exec_module(m); "
            f"sys.exit(m.main(['--wait-seconds','5'],root=Path({str(self.root)!r}),lock_path=Path({str(self.lock)!r}),platform='Darwin'))")
        proc = subprocess.Popen([sys.executable, "-c", driver], env=self.env,
                                stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True)
        try:
            while proc.poll() is None:
                if "owner=test-other-owner" in proc.stdout.readline():
                    break
            proc.terminate()
            output, _ = proc.communicate(timeout=5)
            self.assertEqual(proc.returncode, 2, output)
            self.assertEqual((self.lock / "owner").read_text(), "test-other-owner\n")
            self.assertFalse(self.calls())
        finally:
            if proc.poll() is None:
                proc.terminate()
                proc.wait(timeout=8)


if __name__ == "__main__":
    unittest.main()
