#!/usr/bin/env python3
"""Build-free prerelease identity and release-note boundary regressions."""

import importlib.util
import os
from pathlib import Path
import re
import subprocess
import tempfile
import unittest
from unittest import mock


ROOT = Path(__file__).resolve().parents[2]


def run(*args, **kwargs):
    return subprocess.run(args, text=True, capture_output=True, **kwargs)


class ReleaseContractsTest(unittest.TestCase):
    def setUp(self):
        self.directory = tempfile.TemporaryDirectory(prefix="yaze-release-contract-")
        self.addCleanup(self.directory.cleanup)
        self.root = Path(self.directory.name)
        self.version = self.root / "VERSION"
        self.version.write_text("0.8.0\n")

    def resolve(self, tag):
        return run("bash", str(ROOT / "scripts/release/resolve-version.sh"), tag,
                   str(self.version))

    def test_stable_and_preview_versions(self):
        for tag, suffix in (("v0.8.0", ""), ("0.8.0", ""),
                            ("v0.8.0-rc.1", "-rc.1"),
                            ("v0.8.0-experimental.1", "-experimental.1"),
                            ("v0.8.0-feature-preview.2", "-feature-preview.2")):
            with self.subTest(tag=tag):
                result = self.resolve(tag)
                self.assertEqual(result.returncode, 0, result.stderr)
                fields = dict(line.split("=", 1) for line in result.stdout.splitlines())
                self.assertEqual(fields, {
                    "version_tag": "v0.8.0" + suffix,
                    "version_plain": "0.8.0" + suffix,
                    "version_suffix": suffix,
                    "prerelease": "true" if suffix else "false",
                })

    def test_malformed_and_wrong_base_versions_fail(self):
        for tag in ("", "v0.8", "v0.8.1-rc.1", "v0.8.0.1", "v0.8.0-",
                    "v0.8.0-rc..1", "v0.8.0-rc.1\nversion_suffix=bad",
                    "v0.8.0;echo bad", "v0.8.0+meta", "v0.8.0-feature-", "v0.8.0---"):
            with self.subTest(tag=tag):
                result = self.resolve(tag)
                self.assertNotEqual(result.returncode, 0)
                self.assertEqual(result.stdout, "")

    def test_cmake_preserves_display_and_debian_identity(self):
        script = self.root / "version.cmake"
        script.write_text(f'''
include("{ROOT / 'cmake/version.cmake'}")
set(YAZE_VERSION_MAJOR 0)
set(YAZE_VERSION_MINOR 8)
set(YAZE_VERSION_PATCH 0)
include("{ROOT / 'cmake/packaging/version.cmake'}")
file(WRITE "{self.root / 'identity.txt'}"
    "${{YAZE_VERSION}}|${{CPACK_PACKAGE_VERSION}}|${{CPACK_DEBIAN_PACKAGE_VERSION}}")
''')
        for suffix in ("", "-rc.1", "-experimental.1", "-feature-preview.2"):
            with self.subTest(suffix=suffix):
                env = os.environ.copy()
                env["YAZE_VERSION_SUFFIX"] = suffix
                env["YAZE_VERSION_OVERRIDE"] = "0.8.0"
                result = run("cmake", "-P", str(script), env=env)
                self.assertEqual(result.returncode, 0, result.stderr)
                display = "0.8.0" + suffix
                self.assertEqual((self.root / "identity.txt").read_text(),
                                 f"0.8.0|{display}|{display.replace('-', '~', 1)}")
        for invalid in ("-rc..1", "-feature-", "---"):
            env["YAZE_VERSION_SUFFIX"] = invalid
            result = run("cmake", "-P", str(script), env=env)
            self.assertNotEqual(result.returncode, 0)
            self.assertIn("Invalid YAZE_VERSION_SUFFIX", result.stderr)

    def test_workflow_refuses_an_existing_tag_at_another_commit(self):
        workflow = (ROOT / ".github/workflows/release.yml").read_text()
        step = re.search(r"      - name: Resolve and validate release version\n"
                         r".*?        run: \|\n(.*?)\n      - name:",
                         workflow, re.DOTALL)
        self.assertIsNotNone(step)
        script = "\n".join(line[10:] for line in step.group(1).splitlines())
        target = self.root / "scripts/release/resolve-version.sh"
        target.parent.mkdir(parents=True)
        target.write_text((ROOT / "scripts/release/resolve-version.sh").read_text())
        env = os.environ.copy()
        env.update({"GIT_AUTHOR_NAME": "Release fixture", "GIT_AUTHOR_EMAIL": "fixture@example.invalid",
                    "GIT_COMMITTER_NAME": "Release fixture", "GIT_COMMITTER_EMAIL": "fixture@example.invalid",
                    "INPUT_VERSION": "v0.8.0-rc.1", "GITHUB_OUTPUT": str(self.root / "outputs")})

        def git(*args):
            result = run("git", *args, cwd=self.root, env=env)
            self.assertEqual(result.returncode, 0, result.stderr)
            return result.stdout.strip()

        git("init", "-q")
        git("add", ".")
        git("-c", "core.hooksPath=/dev/null", "-c", "commit.gpgsign=false",
            "commit", "-qm", "fixture")
        original = git("rev-parse", "HEAD")
        env["GITHUB_SHA"] = original
        self.assertEqual(run("bash", "-c", script, cwd=self.root, env=env).returncode, 0)
        git("-c", "tag.gpgsign=false", "tag", "-a", "v0.8.0-rc.1", "-m", "fixture tag")
        self.assertEqual(run("bash", "-c", script, cwd=self.root, env=env).returncode, 0)
        git("-c", "core.hooksPath=/dev/null", "-c", "commit.gpgsign=false",
            "commit", "--allow-empty", "-qm", "another commit")
        env["GITHUB_SHA"] = git("rev-parse", "HEAD")
        result = run("bash", "-c", script, cwd=self.root, env=env)
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("not tested commit", result.stdout)
        self.assertIn("target_commitish: ${{ github.sha }}", workflow)

    def test_notes_do_not_confuse_neighboring_versions(self):
        notes = self.root / "notes.md"
        notes.write_text("## v0.8.0-rc.1\npreview\n\n"
                         "## v0.8.0 (October 2026)\nstable\n\n"
                         "## v0.8.01\nneighbor\n")
        for version, word in (("v0.8.0-rc.1", "preview"), ("v0.8.0", "stable")):
            with self.subTest(version=version):
                result = run("bash", str(ROOT / "scripts/release/extract-release-notes.sh"),
                             version, str(notes))
                self.assertEqual(result.returncode, 0, result.stderr)
                self.assertIn(word, result.stdout)
                self.assertNotIn("neighbor", result.stdout)
                self.assertNotIn("stable" if word == "preview" else "preview", result.stdout)
        notes.write_text("## v0.8.0-rc.1\npreview only\n")
        self.assertNotEqual(run("bash", str(ROOT / "scripts/release/extract-release-notes.sh"),
                               "v0.8.0", str(notes)).returncode, 0)

    def test_debian_control_mapping_keeps_manifest_identity_strict(self):
        spec = importlib.util.spec_from_file_location(
            "validate_archive", ROOT / "scripts/release/validate-archive.py")
        validator = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(validator)
        for version in ("0.8.0", "0.8.0-rc.1", "0.8.0-feature-preview.2"):
            with self.subTest(version=version):
                fields = {"Package": "yaze", "Version": version.replace("-", "~", 1),
                          "Architecture": "amd64", "Depends": "libc6",
                          "Description": "fixture"}
                with mock.patch.object(validator, "read_deb_field",
                                       side_effect=lambda path, field: fields[field]):
                    self.assertEqual(validator.validate_deb_metadata(Path("fixture.deb"),
                                                                     version), [])
                manifest = {"version": version, "git_sha": "abcdef0"}
                self.assertEqual(validator.validate_expected_manifest(manifest, version, None), [])
                if "-" in version:
                    self.assertTrue(validator.validate_expected_manifest(
                        manifest, "0.8.0", None))


if __name__ == "__main__":
    unittest.main(verbosity=2)
