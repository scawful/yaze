#!/usr/bin/env python3
"""Build-free fixtures for nightly test selection and failure propagation."""

import json
import os
from pathlib import Path
import re
import subprocess
import tempfile
import textwrap
import unittest


ROOT = Path(__file__).resolve().parents[3]
WORKFLOW = ROOT / ".github/workflows/nightly.yml"
CASES = {
    "Run experimental AI tests": ("build_nightly", "^experimental$"),
    "Run GUI E2E tests (Linux with Xvfb)": (
        "build/presets/ci-linux", "^(gui|headless_gui)$"
    ),
    "Run GUI E2E tests (macOS)": (
        "build/presets/ci-linux", "^(gui|headless_gui)$"
    ),
    "Run benchmarks": ("build_bench", "^benchmark$"),
    "Run extended integration tests": ("build_extended", "^integration$"),
}


def run_block(name):
    """Extract only a named literal run block; no YAML dependency is needed."""
    lines = WORKFLOW.read_text().splitlines()
    marker = f"      - name: {name}"
    start = lines.index(marker) + 1
    while start < len(lines) and lines[start] != "        run: |":
        if lines[start].startswith("      - name:"):
            raise AssertionError(f"Missing run block for {name}")
        start += 1
    body = []
    for line in lines[start + 1:]:
        if line and not line.startswith("          "):
            break
        body.append(line[10:])
    command = "\n".join(body)
    return command.replace("${{ env.BUILD_TYPE }}", "RelWithDebInfo").replace(
        "${{ matrix.preset }}", "ci-linux"
    )


class NightlyWorkflowTest(unittest.TestCase):
    def setUp(self):
        self.workspace = tempfile.TemporaryDirectory()
        self.addCleanup(self.workspace.cleanup)
        self.root = Path(self.workspace.name).resolve()
        tools = self.root / "tools"
        tools.mkdir()
        ctest = tools / "ctest"
        ctest.write_text(textwrap.dedent("""\
            #!/usr/bin/env python3
            import json
            import os
            from pathlib import Path
            import sys
            args = sys.argv[1:]
            Path("ctest_args.json").write_text(json.dumps(args))
            mode = os.environ.get("NIGHTLY_FIXTURE_MODE", "pass")
            if mode == "fail":
                sys.exit(7)
            if mode == "empty":
                sys.exit(8 if "--no-tests=error" in args else 0)
            Path(args[args.index("--output-junit") + 1]).write_text(
                '<testsuite tests="1" failures="0"/>')
            """))
        ctest.chmod(0o755)
        xvfb = tools / "xvfb-run"
        xvfb.write_text('#!/bin/sh\n[ "$1" = "-a" ] && shift\nexec "$@"\n')
        xvfb.chmod(0o755)
        self.env = dict(os.environ, PATH=f"{tools}:{os.environ['PATH']}")

    def execute(self, name, mode="pass"):
        return subprocess.run(
            ["bash", "-e", "-o", "pipefail", "-c", run_block(name)],
            cwd=self.root,
            env=dict(self.env, NIGHTLY_FIXTURE_MODE=mode),
            capture_output=True,
            text=True,
        )

    def test_all_suites_use_configured_trees_and_publish_real_reports(self):
        for name, (build_dir, label) in CASES.items():
            with self.subTest(name=name):
                result = self.execute(name)
                self.assertEqual(result.returncode, 0, result.stderr)
                args = json.loads((self.root / "ctest_args.json").read_text())
                self.assertEqual(args[args.index("--test-dir") + 1], build_dir)
                self.assertEqual(args[args.index("--label-regex") + 1], label)
                report = Path(args[args.index("--output-junit") + 1])
                self.assertEqual(report.parent, self.root)
                self.assertIn('tests="1"', report.read_text())
                if "experimental" in name:
                    self.assertEqual(
                        args[args.index("--label-exclude") + 1],
                        "^(gui|headless_gui)$",
                    )

    def test_failed_and_empty_suites_fail_the_job(self):
        for name in CASES:
            for mode, expected in (("fail", 7), ("empty", 8)):
                with self.subTest(name=name, mode=mode):
                    self.assertEqual(self.execute(name, mode).returncode, expected)

    def test_workflow_does_not_mask_results_or_build_unused_suites(self):
        source = WORKFLOW.read_text()
        self.assertNotIn("continue-on-error", source)
        self.assertNotIn("would happen here", source)
        self.assertNotIn("Notification would", source)
        self.assertNotIn("Setup Ollama", source)
        self.assertNotIn("--benchmark_format", source)
        self.assertIn("targets: yaze_test_gui", source)
        self.assertIn("--target yaze_test_integration", source)
        self.assertEqual(source.count("if-no-files-found: error"), 4)
        self.assertNotRegex(source, r"--parallel\s*\n")
        self.assertRegex(source, r"defaults:\n  run:\n    shell: bash")

    def test_summary_keeps_failed_cancelled_and_skipped_results_distinct(self):
        results = dict(zip(
            ("experimental-ai-tests", "gui-e2e-tests", "benchmark-tests",
             "extended-integration-tests"),
            ({"result": value} for value in
             ("failure", "success", "skipped", "cancelled")),
        ))
        summary = self.root / "summary.md"
        result = subprocess.run(
            ["bash", "-e", "-o", "pipefail", "-c", run_block("Generate summary")],
            cwd=self.root,
            env=dict(self.env, NIGHTLY_RESULTS_JSON=json.dumps(results),
                     GITHUB_STEP_SUMMARY=str(summary)),
            capture_output=True,
            text=True,
        )
        self.assertEqual(result.returncode, 0, result.stderr)
        text = summary.read_text()
        self.assertIn("**Experimental AI Tests:** Failure", text)
        self.assertIn("**GUI E2E Tests:** Passed", text)
        self.assertIn("**Performance Benchmarks:** Skipped", text)
        self.assertIn("**Extended Integration Tests:** Cancelled", text)


if __name__ == "__main__":
    unittest.main()
