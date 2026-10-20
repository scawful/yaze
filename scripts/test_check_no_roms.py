#!/usr/bin/env python3
"""Exercise the ROM guard with text placeholders in disposable Git repositories."""

import os
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest


GUARD = Path(__file__).resolve().with_name("check-no-roms.sh")
BASH = os.environ.get("YAZE_ROM_GUARD_SHELL") or shutil.which("bash")
ZERO = "0" * 40


@unittest.skipUnless(BASH and shutil.which("git"), "bash and git are required")
class RomGuardTest(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory(prefix="yaze-rom-guard-test-")
        self.addCleanup(self.temporary.cleanup)
        self.repo = Path(self.temporary.name)
        self.git("init", "-q")
        self.git("config", "user.name", "ROM Guard Test")
        self.git("config", "user.email", "rom-guard@example.invalid")
        self.git("config", "commit.gpgsign", "false")
        self.git("config", "core.hooksPath", str(self.repo / "no-hooks"))
        self.add_file("clean.txt")
        self.commit("clean fixture")
        self.clean_tip = self.git("rev-parse", "HEAD").strip()

    def git(self, *args):
        return subprocess.check_output(
            ["git", *args], cwd=self.repo, stderr=subprocess.PIPE, text=True
        )

    def add_file(self, name):
        path = self.repo / name
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text("Text fixture only; this is not a game ROM.\n")
        self.git("add", "--", name)

    def commit(self, message):
        self.git("commit", "-qm", message)

    def scan(self, pre_push=False):
        args = [BASH, str(GUARD)]
        input_text = ""
        if pre_push:
            self.git("update-ref", "refs/remotes/origin/master", self.clean_tip)
            tip = self.git("rev-parse", "HEAD").strip()
            args.append("--pre-push")
            input_text = f"refs/heads/test {tip} refs/heads/test {ZERO}\n"
        else:
            args.append("HEAD")
        return subprocess.run(
            args, cwd=self.repo, input=input_text, capture_output=True, text=True
        )

    def assert_blocked(self, pre_push=False):
        result = self.scan(pre_push)
        self.assertEqual(result.returncode, 1, result.stdout + result.stderr)
        self.assertIn("ROM guard: blocked.", result.stderr)

    def test_clean_history_passes(self):
        for pre_push in (False, True):
            with self.subTest(pre_push=pre_push):
                result = self.scan(pre_push)
                self.assertEqual(result.returncode, 0, result.stderr)

    def test_added_then_deleted_rom_is_blocked(self):
        self.add_file("dummy.sfc")
        self.commit("add text placeholder")
        self.git("rm", "dummy.sfc")
        self.commit("delete text placeholder")
        self.assert_blocked()
        self.assert_blocked(pre_push=True)

    def test_renamed_then_deleted_rom_is_blocked(self):
        self.add_file("data.bin")
        self.commit("add text placeholder")
        self.git("mv", "data.bin", "dummy.sfc")
        self.commit("rename text placeholder")
        self.git("rm", "dummy.sfc")
        self.commit("delete text placeholder")
        self.assert_blocked()
        self.assert_blocked(pre_push=True)

    def test_all_extensions_are_case_insensitive(self):
        for extension in ("sFc", "SmC", "fIg", "SwC"):
            self.add_file(f"dummy.{extension}")
        self.commit("add text placeholders")
        result = self.scan()
        self.assertEqual(result.returncode, 1, result.stderr)
        for extension in ("sFc", "SmC", "fIg", "SwC"):
            self.assertIn(f"dummy.{extension}", result.stderr)

    def test_merge_added_then_deleted_rom_is_blocked(self):
        self.git("checkout", "-qb", "side")
        self.add_file("side.txt")
        self.commit("side change")
        self.git("checkout", "-qb", "main", self.clean_tip)
        self.add_file("main.txt")
        self.commit("main change")
        self.git("merge", "--no-ff", "--no-commit", "side")
        self.add_file("merge-only.sfc")
        self.commit("merge with text placeholder")
        self.git("rm", "merge-only.sfc")
        self.commit("delete text placeholder")
        self.assert_blocked()
        self.assert_blocked(pre_push=True)

    def test_quoted_paths_are_blocked(self):
        for name in ("odd\tdir/dummy.sfc", "odd\ndir/dummy.SMC"):
            with self.subTest(name=name):
                self.add_file(name)
                self.commit("add text placeholder with quoted path")
                self.assert_blocked()
                self.assert_blocked(pre_push=True)

    def test_quoted_path_in_history_is_blocked(self):
        name = "odd\tdir/dummy.sfc"
        self.add_file(name)
        self.commit("add text placeholder with quoted path")
        self.git("rm", "--", name)
        self.commit("delete text placeholder")
        self.assert_blocked()
        self.assert_blocked(pre_push=True)

    def test_branch_deletion_is_allowed(self):
        result = subprocess.run(
            [BASH, str(GUARD), "--pre-push"],
            cwd=self.repo,
            input=f"(delete) {ZERO} refs/heads/test {self.clean_tip}\n",
            capture_output=True,
            text=True,
        )
        self.assertEqual(result.returncode, 0, result.stderr)

    def test_noninteractive_pre_push_checks_head_with_empty_stdin(self):
        self.add_file("dummy.sfc")
        self.commit("add text placeholder")
        env = os.environ.copy()
        env["YAZE_PREPUSH_BUILD_DIR"] = str(self.repo / "missing-build")
        result = subprocess.run(
            [
                BASH,
                str(GUARD.with_name("pre-push.sh")),
                "--skip-build",
                "--skip-tests",
                "--skip-format",
            ],
            cwd=self.repo,
            input="",
            capture_output=True,
            text=True,
            env=env,
        )
        self.assertEqual(result.returncode, 5, result.stdout + result.stderr)
        self.assertIn("ROM guard: blocked.", result.stderr)
        self.assertNotIn("Build directory not configured", result.stdout)


if __name__ == "__main__":
    unittest.main()
