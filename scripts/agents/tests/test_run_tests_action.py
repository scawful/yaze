#!/usr/bin/env python3
"""Build-free checks of the composite action's actual shell run blocks."""

import json
import os
from pathlib import Path
import re
import subprocess
import tempfile
import textwrap
import unittest


ROOT = Path(__file__).resolve().parents[3]
ACTION = ROOT / ".github/actions/run-tests/action.yml"


def run_block(name, test_type="all"):
    lines = ACTION.read_text().splitlines()
    start = lines.index(f"    - name: {name}") + 1
    while start < len(lines) and lines[start] != "      run: |":
        if lines[start].startswith("    - name:"):
            raise AssertionError(f"Missing run block for {name}")
        start += 1
    body = []
    for line in lines[start + 1:]:
        if line and not line.startswith("        "):
            break
        body.append(line[8:])
    command = "\n".join(body)
    for key, value in {
        "test-type": test_type, "preset": "fixture", "platform": "fixture",
        "build-type": "RelWithDebInfo",
    }.items():
        command = command.replace("${{ inputs." + key + " }}", value)
    return command


class RunTestsActionTest(unittest.TestCase):
    def setUp(self):
        self.workspace = tempfile.TemporaryDirectory()
        self.addCleanup(self.workspace.cleanup)
        self.root = Path(self.workspace.name).resolve()
        self.build = self.root / "build/presets/fixture"
        binary_dir = self.build / "bin"
        binary_dir.mkdir(parents=True)
        for suite in ("unit", "integration"):
            (binary_dir / f"yaze_test_{suite}").touch()
        tools = self.root / "tools"
        tools.mkdir()
        ctest = tools / "ctest"
        ctest.write_text(textwrap.dedent("""\
            #!/usr/bin/env python3
            import json
            import os
            from pathlib import Path
            import re
            import sys
            args = sys.argv[1:]
            with Path("ctest_calls.jsonl").open("a") as calls:
                calls.write(json.dumps(args) + "\\n")
            label = args[args.index("--label-regex") + 1]
            mode = os.environ.get("ACTION_FIXTURE_MODE", "pass")
            tests = [
                {"name": "unit_shard", "properties": [
                    {"name": "LABELS", "value": ["stable", "unit"]}]},
                {"name": "integration_shard", "properties": [
                    {"name": "LABELS", "value": ["stable", "integration"]}]},
                {"name": "z3ed_self_test", "properties": [
                    {"name": "LABELS", "value": ["stable", "cli", "z3ed"]}]},
            ]
            if mode == "missing-label":
                tests[1]["properties"][0]["value"] = ["stable"]
            tests = [t for t in tests if any(
                re.search(label, x) for x in t["properties"][0]["value"])]
            if mode == "empty":
                tests = []
            if "--show-only=json-v1" in args:
                print(json.dumps({"tests": tests}))
                sys.exit(0)
            report = Path(args[args.index("--output-junit") + 1])
            failed = mode == "fail"
            report.write_text(
                f'<testsuite tests="{len(tests)}" failures="{int(failed)}" time="0.1"/>')
            if not tests and "--no-tests=error" in args:
                sys.exit(8)
            sys.exit(7 if failed else 0)
            """))
        ctest.chmod(0o755)
        self.env = dict(
            os.environ, PATH=f"{tools}:{os.environ['PATH']}",
            GITHUB_STEP_SUMMARY=str(self.root / "summary.md"),
        )

    def execute(self, name, test_type="all", mode="pass"):
        return subprocess.run(
            ["bash", "-e", "-o", "pipefail", "-c", run_block(name, test_type)],
            cwd=self.root,
            env=dict(self.env, ACTION_FIXTURE_MODE=mode),
            capture_output=True, text=True,
        )

    def test_each_mode_runs_one_inventory_and_one_selected_suite(self):
        for mode, label, count in (
            ("all", "stable", 3), ("stable", "stable", 3),
            ("unit", "unit", 1), ("integration", "integration", 1),
        ):
            with self.subTest(mode=mode):
                (self.root / "ctest_calls.jsonl").unlink(missing_ok=True)
                for step in ("Verify test inventory", "Run selected tests",
                             "Summarize test results"):
                    result = self.execute(step, mode)
                    self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
                calls = [json.loads(x) for x in
                         (self.root / "ctest_calls.jsonl").read_text().splitlines()]
                self.assertEqual(len(calls), 2)
                self.assertIn("--show-only=json-v1", calls[0])
                self.assertNotIn("--show-only=json-v1", calls[1])
                self.assertEqual(calls[1][calls[1].index("--label-regex") + 1],
                                 f"^{label}$")
                report = self.build / f"{label}_test_results.xml"
                self.assertIn(f'tests="{count}"', report.read_text())

    def test_inventory_still_refuses_missing_binary_or_required_label(self):
        result = self.execute("Verify test inventory", mode="missing-label")
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("missing labeled, built required suites", result.stderr)
        (self.build / "bin/yaze_test_integration").unlink()
        result = self.execute("Verify test inventory")
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("yaze_test_integration", result.stderr)

    def test_failed_or_empty_suites_and_missing_or_empty_reports_fail(self):
        for mode, expected in (("fail", 7), ("empty", 8)):
            with self.subTest(mode=mode):
                self.assertEqual(self.execute("Run selected tests", mode=mode).returncode,
                                 expected)
        self.assertNotEqual(self.execute("Summarize test results").returncode, 0)
        (self.build / "stable_test_results.xml").unlink()
        self.assertNotEqual(self.execute("Summarize test results").returncode, 0)

    def test_source_labels_cover_unit_integration_and_cli_in_stable(self):
        cmake = (ROOT / "test/CMakeLists.txt").read_text()
        for suite in ("unit", "integration"):
            self.assertIn(
                f'yaze_add_test_suite(yaze_test_{suite} "stable;{suite}"', cmake)
        self.assertRegex(cmake, r'set_tests_properties\(z3ed_self_test PROPERTIES\s+'
                         r'LABELS "stable;cli;z3ed"')
        action = ACTION.read_text()
        self.assertEqual(action.count("    - name: Run "), 1)
        upload = action.split("    - name: Upload test results", 1)[1]
        self.assertIn("if: always()", upload)
        self.assertIn("if-no-files-found: error", upload)
        self.assertNotIn("continue-on-error", action)


if __name__ == "__main__":
    unittest.main()
