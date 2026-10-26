#!/usr/bin/env python3
"""Check Oracle CI executable selection without building or opening a ROM."""

import json
import os
from pathlib import Path
import re
import subprocess
import tempfile
import unittest


ROOT = Path(__file__).resolve().parents[2]


def oracle_job():
    workflow = (ROOT / ".github/workflows/ci.yml").read_text()
    match = re.search(
        r"^  oracle-smoke-check:\n(?P<job>.*?)(?=^  [\w-]+:|\Z)",
        workflow,
        re.MULTILINE | re.DOTALL,
    )
    if match is None:
        raise AssertionError("Oracle smoke job is missing")
    return match.group("job")


def selected_executable(job):
    match = re.search(r"^      Z3ED_BIN: (\S+)\s*$", job, re.MULTILINE)
    if match is None:
        raise AssertionError("Oracle smoke executable must be explicit at job scope")
    return match.group(1)


def preset_value(presets, name, key):
    preset = presets[name]
    if key in preset:
        return preset[key]
    parents = preset.get("inherits", [])
    if isinstance(parents, str):
        parents = [parents]
    for parent in parents:
        value = preset_value(presets, parent, key)
        if value is not None:
            return value
    return None


class OracleSmokeWorkflowContractTest(unittest.TestCase):
    def test_executable_matches_configured_preset_output(self):
        job = oracle_job()
        build = re.search(
            r"cmake --build --preset (\S+) --config (\S+) --target z3ed", job
        )
        self.assertIsNotNone(build, "Oracle smoke must build an explicit z3ed target")
        preset, config = build.groups()
        self.assertIn(f"cmake --preset {preset}", job)
        presets = {
            item["name"]: item
            for item in json.loads((ROOT / "CMakePresets.json").read_text())[
                "configurePresets"
            ]
        }
        binary_dir = preset_value(presets, preset, "binaryDir")
        self.assertIsNotNone(binary_dir)
        binary_dir = binary_dir.replace("${sourceDir}", ".").replace(
            "${presetName}", preset
        )
        self.assertEqual(
            Path(selected_executable(job)), Path(binary_dir) / "bin" / config / "z3ed"
        )

    def test_both_checks_use_the_verified_job_executable(self):
        job = oracle_job()
        verification = re.search(
            r"- name: Verify Oracle smoke executable\n"
            r"        if: steps\.rom-check\.outputs\.available == 'true'\n"
            r'        run: test -x "\$Z3ED_BIN"',
            job,
        )
        self.assertIsNotNone(verification)
        checks = list(re.finditer(r"\./scripts/z3ed \\\n", job))
        self.assertEqual(len(checks), 2)
        for check in checks:
            self.assertLess(verification.start(), check.start())
        self.assertNotIn("Z3ED_BIN=", job)
        self.assertIn("--strict-readiness", job)

    def test_wrapper_executes_the_selected_binary(self):
        relative_executable = selected_executable(oracle_job())
        with tempfile.TemporaryDirectory(prefix="yaze-oracle-ci-contract-") as temporary:
            executable = Path(temporary) / relative_executable
            executable.parent.mkdir(parents=True)
            executable.write_text('#!/bin/sh\nprintf "selected:%s\\n" "$*"\n')
            executable.chmod(0o755)
            env = dict(os.environ, Z3ED_BIN=relative_executable)
            probe = subprocess.run(
                ["bash", "-c", 'test -x "$Z3ED_BIN"'],
                cwd=temporary,
                env=env,
                capture_output=True,
                text=True,
                check=False,
            )
            self.assertEqual(probe.returncode, 0, probe.stderr)
            result = subprocess.run(
                ["bash", str(ROOT / "scripts/z3ed"), "oracle-smoke-check", "--format=json"],
                cwd=temporary,
                env=env,
                capture_output=True,
                text=True,
                check=False,
            )
            self.assertEqual(result.returncode, 0, result.stderr)
            self.assertEqual(
                result.stdout, "selected:oracle-smoke-check --format=json\n"
            )

    def test_missing_selected_binary_fails_the_executable_probe(self):
        with tempfile.TemporaryDirectory(prefix="yaze-oracle-ci-contract-") as temporary:
            result = subprocess.run(
                ["bash", "-c", 'test -x "$Z3ED_BIN"'],
                cwd=temporary,
                env=dict(os.environ, Z3ED_BIN=selected_executable(oracle_job())),
                capture_output=True,
                text=True,
                check=False,
            )
            self.assertNotEqual(result.returncode, 0)


if __name__ == "__main__":
    unittest.main()
