"""Guards that scripts/ tooling keeps portable, repository-relative defaults.

Host-specific absolute paths (a developer's home directory) make scripts
unrunnable for everyone else and silently rot. Retained utilities must take
their paths from CLI flags or environment variables instead.

Run: python3 -m unittest discover -s scripts/agents/tests
"""

from __future__ import annotations

import importlib.util
import os
import re
import sys
import unittest
from pathlib import Path

SCRIPTS_DIR = Path(__file__).resolve().parents[2]
REPO_ROOT = SCRIPTS_DIR.parent

# Archived/handoff docs may keep historical paths; executable tooling may not.
_SOURCE_SUFFIXES = {".py", ".sh", ".ps1", ".cmake"}
_HOME_PATH = re.compile(r"(?:/Users/[a-z][\w.-]*|/home/[a-z][\w.-]*)/", re.IGNORECASE)
_HOME_PATH_ALLOWED = ("/home/runner/", "/home/linuxbrew/")


def _load(relative_path: str):
    """Import a script the way it resolves when run directly.

    Running `python3 scripts/foo.py` puts the script's own directory on
    sys.path, which is how sibling packages such as `profiles` are found.
    """
    script = SCRIPTS_DIR / relative_path
    name = script.stem
    sys.path.insert(0, str(script.parent))
    try:
        spec = importlib.util.spec_from_file_location(name, script)
        module = importlib.util.module_from_spec(spec)
        sys.modules[name] = module
        spec.loader.exec_module(module)
        return module
    finally:
        sys.path.remove(str(script.parent))


class TestNoHostAbsolutePaths(unittest.TestCase):
    def test_scripts_have_no_developer_home_paths(self):
        offenders = []
        for path in sorted(SCRIPTS_DIR.rglob("*")):
            if not path.is_file() or path.suffix not in _SOURCE_SUFFIXES:
                continue
            text = path.read_text(encoding="utf-8", errors="ignore")
            for line_no, line in enumerate(text.splitlines(), start=1):
                match = _HOME_PATH.search(line)
                if not match or match.group(0) in _HOME_PATH_ALLOWED:
                    continue
                offenders.append(
                    f"{path.relative_to(REPO_ROOT)}:{line_no}: {line.strip()}"
                )
        self.assertEqual(
            [],
            offenders,
            "Hardcoded developer home paths found; use a CLI flag or env var "
            "with a repository-relative default:\n" + "\n".join(offenders),
        )


class TestLocationMapperDefaults(unittest.TestCase):
    def test_default_rom_is_repository_relative(self):
        module = _load("location_mapper.py")
        self.assertEqual(
            module.DEFAULT_ROM,
            str(REPO_ROOT / "roms" / "alttp_vanilla.sfc"),
        )

    def test_alttp_rom_env_overrides_default(self):
        os.environ["ALTTP_ROM"] = "/tmp/custom.sfc"
        try:
            module = _load("location_mapper.py")
            self.assertEqual(module.DEFAULT_ROM, "/tmp/custom.sfc")
        finally:
            del os.environ["ALTTP_ROM"]


class TestAsmTunerZ3asmResolution(unittest.TestCase):
    def setUp(self):
        self.module = _load("ai/asm_tuner.py")

    def test_explicit_argument_wins(self):
        os.environ["Z3ASM_BIN"] = "/tmp/from-env"
        try:
            self.assertEqual(
                self.module.resolve_z3asm("/tmp/explicit"), "/tmp/explicit"
            )
        finally:
            del os.environ["Z3ASM_BIN"]

    def test_env_used_when_no_argument(self):
        os.environ["Z3ASM_BIN"] = "/tmp/from-env"
        try:
            self.assertEqual(self.module.resolve_z3asm(), "/tmp/from-env")
        finally:
            del os.environ["Z3ASM_BIN"]

    def test_missing_binary_returns_none(self):
        os.environ.pop("Z3ASM_BIN", None)
        os.environ["Z3DK_ROOT"] = "/nonexistent-z3dk"
        original_path = os.environ.get("PATH", "")
        os.environ["PATH"] = ""
        try:
            module = _load("ai/asm_tuner.py")
            self.assertIsNone(module.resolve_z3asm())
        finally:
            os.environ["PATH"] = original_path
            del os.environ["Z3DK_ROOT"]


if __name__ == "__main__":
    unittest.main()
