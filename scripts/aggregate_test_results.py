#!/usr/bin/env python3
"""
Aggregate test results from multiple sources and generate comprehensive reports.
Used by CI/CD pipeline to combine results from parallel test execution.
"""

import json
import xml.etree.ElementTree as ET
import argparse
import sys
from pathlib import Path
from typing import Dict, List, Any
from dataclasses import dataclass, asdict
from datetime import datetime
import re

@dataclass
class TestCase:
    """Individual test case result."""
    name: str
    suite: str
    status: str  # passed, failed, skipped, error
    duration: float
    message: str = ""
    output: str = ""

@dataclass
class TestSuite:
    """Test suite results."""
    name: str
    tests: int = 0
    passed: int = 0
    failed: int = 0
    skipped: int = 0
    errors: int = 0
    duration: float = 0.0
    test_cases: List[TestCase] = None

    def __post_init__(self):
        if self.test_cases is None:
            self.test_cases = []

@dataclass
class StageResults:
    """Results for a testing stage."""
    name: str
    status: str
    total: int
    passed: int
    failed: int
    skipped: int
    duration: float
    pass_rate: float
    emoji: str = ""

@dataclass
class AggregatedResults:
    """Complete aggregated test results."""
    overall_passed: bool
    total_tests: int
    total_passed: int
    total_failed: int
    total_skipped: int
    total_duration: float
    tests_per_second: float
    parallel_efficiency: float
    stage1: StageResults
    stage2: StageResults
    stage3: StageResults
    test_suites: List[TestSuite]
    failed_tests: List[TestCase]
    slowest_tests: List[TestCase]
    timestamp: str

class TestResultAggregator:
    """Aggregates test results from multiple sources."""

    def __init__(self, input_dir: Path):
        self.input_dir = input_dir
        self.test_suites = {}
        self.all_tests = []
        self.stage_results = {}

    def parse_junit_xml(self, xml_file: Path) -> TestSuite:
        """Parse JUnit XML test results."""
        try:
            tree = ET.parse(xml_file)
            root = tree.getroot()

            # Handle both testsuites and testsuite root elements
            if root.tag == "testsuites":
                suites = root.findall("testsuite")
            else:
                suites = [root]

            suite_results = TestSuite(name=xml_file.stem)

            for suite_elem in suites:
                suite_name = suite_elem.get("name", "unknown")

                for testcase_elem in suite_elem.findall("testcase"):
                    test_name = testcase_elem.get("name")
                    classname = testcase_elem.get("classname", suite_name)
                    time = float(testcase_elem.get("time", 0))

                    # Determine status
                    status = "passed"
                    message = ""
                    output = ""

                    failure = testcase_elem.find("failure")
                    error = testcase_elem.find("error")
                    skipped = testcase_elem.find("skipped")

                    if failure is not None:
                        status = "failed"
                        message = failure.get("message", "")
                        output = failure.text or ""
                    elif error is not None:
                        status = "error"
                        message = error.get("message", "")
                        output = error.text or ""
                    elif skipped is not None:
                        status = "skipped"
                        message = skipped.get("message", "")

                    test_case = TestCase(
                        name=test_name,
                        suite=classname,
                        status=status,
                        duration=time,
                        message=message,
                        output=output
                    )

                    suite_results.test_cases.append(test_case)
                    suite_results.tests += 1
                    suite_results.duration += time

                    if status == "passed":
                        suite_results.passed += 1
                    elif status == "failed":
                        suite_results.failed += 1
                    elif status == "skipped":
                        suite_results.skipped += 1
                    elif status == "error":
                        suite_results.errors += 1

            return suite_results

        except (ET.ParseError, IOError) as e:
            print(f"Warning: Failed to parse {xml_file}: {e}", file=sys.stderr)
            return TestSuite(name=xml_file.stem)

    def parse_json_results(self, json_file: Path) -> TestSuite:
        """Parse JSON test results (gtest format)."""
        try:
            with open(json_file) as f:
                data = json.load(f)

            suite_results = TestSuite(name=json_file.stem)

            # Handle both single suite and multiple suites
            if "testsuites" in data:
                suites = data["testsuites"]
            elif "testsuite" in data:
                suites = [data]
            else:
                suites = []

            for suite in suites:
                suite_name = suite.get("name", "unknown")

                for test in suite.get("testsuite", []):
                    test_name = test.get("name")
                    status = "passed" if test.get("result") == "COMPLETED" else "failed"
                    duration = float(test.get("time", "0").replace("s", ""))

                    test_case = TestCase(
                        name=test_name,
                        suite=suite_name,
                        status=status,
                        duration=duration,
                        output=test.get("output", "")
                    )

                    suite_results.test_cases.append(test_case)
                    suite_results.tests += 1
                    suite_results.duration += duration

                    if status == "passed":
                        suite_results.passed += 1
                    else:
                        suite_results.failed += 1

            return suite_results

        except (json.JSONDecodeError, IOError, KeyError) as e:
            print(f"Warning: Failed to parse {json_file}: {e}", file=sys.stderr)
            return TestSuite(name=json_file.stem)

    def collect_results(self):
        """Collect all test results from input directory."""
        # Find all result files
        xml_files = list(self.input_dir.rglob("*.xml"))
        json_files = list(self.input_dir.rglob("*.json"))

        print(f"Found {len(xml_files)} XML and {len(json_files)} JSON result files")

        # Parse XML results
        for xml_file in xml_files:
            # Skip non-test XML files
            if "coverage" in xml_file.name.lower():
                continue

            suite = self.parse_junit_xml(xml_file)
            if suite.tests > 0:
                self.test_suites[suite.name] = suite
                self.all_tests.extend(suite.test_cases)

        # Parse JSON results
        for json_file in json_files:
            # Skip non-test JSON files
            if any(skip in json_file.name.lower()
                   for skip in ["summary", "metrics", "times", "coverage"]):
                continue

            suite = self.parse_json_results(json_file)
            if suite.tests > 0:
                # Merge with existing suite if name matches
                if suite.name in self.test_suites:
                    existing = self.test_suites[suite.name]
                    existing.test_cases.extend(suite.test_cases)
                    existing.tests += suite.tests
                    existing.passed += suite.passed
                    existing.failed += suite.failed
                    existing.skipped += suite.skipped
                    existing.errors += suite.errors
                    existing.duration += suite.duration
                else:
                    self.test_suites[suite.name] = suite
                    self.all_tests.extend(suite.test_cases)

    def categorize_by_stage(self):
        """Categorize results by CI stage."""
        # Initialize stage results
        stages = {
            "stage1": StageResults("Smoke Tests", "unknown", 0, 0, 0, 0, 0.0, 0.0),
            "stage2": StageResults("Unit Tests", "unknown", 0, 0, 0, 0, 0.0, 0.0),
            "stage3": StageResults("Integration Tests", "unknown", 0, 0, 0, 0, 0.0, 0.0),
        }

        # Categorize tests
        for test in self.all_tests:
            # Determine stage based on test name or suite
            stage = None
            if "smoke" in test.name.lower() or "critical" in test.name.lower():
                stage = "stage1"
            elif "unit" in test.suite.lower() or "unit" in test.name.lower():
                stage = "stage2"
            elif ("integration" in test.suite.lower() or
                  "integration" in test.name.lower() or
                  "e2e" in test.name.lower() or
                  "gui" in test.name.lower()):
                stage = "stage3"
            else:
                # Default to unit tests
                stage = "stage2"

            if stage:
                stage_result = stages[stage]
                stage_result.total += 1
                stage_result.duration += test.duration

                if test.status == "passed":
                    stage_result.passed += 1
                elif test.status in ["failed", "error"]:
                    stage_result.failed += 1
                elif test.status == "skipped":
                    stage_result.skipped += 1

        # Calculate pass rates and status
        for stage_key, stage in stages.items():
            if stage.total > 0:
                stage.pass_rate = (stage.passed / stage.total) * 100
                stage.status = "✅" if stage.failed == 0 else "❌"
                stage.emoji = "✅" if stage.failed == 0 else "❌"
            else:
                stage.status = "⏭️"
                stage.emoji = "⏭️"

        self.stage_results = stages

    def generate_summary(self) -> AggregatedResults:
        """Generate aggregated summary of all results."""
        total_tests = len(self.all_tests)
        total_passed = sum(1 for t in self.all_tests if t.status == "passed")
        total_failed = sum(1 for t in self.all_tests
                          if t.status in ["failed", "error"])
        total_skipped = sum(1 for t in self.all_tests if t.status == "skipped")
        total_duration = sum(t.duration for t in self.all_tests)

        # Find failed tests
        failed_tests = [t for t in self.all_tests
                       if t.status in ["failed", "error"]]

        # Find slowest tests
        slowest_tests = sorted(self.all_tests,
                               key=lambda t: t.duration,
                               reverse=True)[:10]

        # Calculate metrics
        tests_per_second = total_tests / total_duration if total_duration > 0 else 0

        # Estimate parallel efficiency (simplified)
        num_shards = len(self.test_suites)
        if num_shards > 1:
            ideal_time = total_duration / num_shards
            actual_time = max(suite.duration for suite in self.test_suites.values())
            parallel_efficiency = (ideal_time / actual_time * 100) if actual_time > 0 else 0
        else:
            parallel_efficiency = 100

        return AggregatedResults(
            overall_passed=(total_failed == 0),
            total_tests=total_tests,
            total_passed=total_passed,
            total_failed=total_failed,
            total_skipped=total_skipped,
            total_duration=round(total_duration, 2),
            tests_per_second=round(tests_per_second, 2),
            parallel_efficiency=round(parallel_efficiency, 1),
            stage1=self.stage_results.get("stage1"),
            stage2=self.stage_results.get("stage2"),
            stage3=self.stage_results.get("stage3"),
            test_suites=list(self.test_suites.values()),
            failed_tests=failed_tests,
            slowest_tests=slowest_tests,
            timestamp=datetime.now().isoformat()
        )

    def generate_html_report(self, results: AggregatedResults, output_path: Path):
        """Generate HTML report from aggregated results."""
        html = f"""<!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>Yaze Test Results - {datetime.now().strftime('%Y-%m-%d %H:%M')}</title>
    <style>
        body {{
            font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', Roboto, sans-serif;
            margin: 0;
            padding: 20px;
            background: linear-gradient(135deg, #667eea 0%, #764ba2 100%);
            min-height: 100vh;
        }}
        .container {{
            max-width: 1200px;
            margin: 0 auto;
            background: white;
            border-radius: 10px;
            padding: 30px;
            box-shadow: 0 20px 60px rgba(0,0,0,0.3);
        }}
        h1 {{
            color: #333;
            border-bottom: 3px solid #667eea;
            padding-bottom: 10px;
        }}
        .summary {{
            display: grid;
            grid-template-columns: repeat(auto-fit, minmax(200px, 1fr));
            gap: 20px;
            margin: 30px 0;
        }}
        .metric {{
            background: #f8f9fa;
            padding: 20px;
            border-radius: 8px;
            text-align: center;
            border-left: 4px solid #667eea;
        }}
        .metric-value {{
            font-size: 32px;
            font-weight: bold;
            color: #667eea;
        }}
        .metric-label {{
            color: #666;
            font-size: 14px;
            margin-top: 5px;
        }}
        .status-pass {{
            color: #28a745;
        }}
        .status-fail {{
            color: #dc3545;
        }}
        table {{
            width: 100%;
            border-collapse: collapse;
            margin: 20px 0;
        }}
        th {{
            background: #667eea;
            color: white;
            padding: 12px;
            text-align: left;
        }}
        td {{
            padding: 10px;
            border-bottom: 1px solid #ddd;
        }}
        tr:hover {{
            background: #f8f9fa;
        }}
        .stage-badge {{
            display: inline-block;
            padding: 4px 8px;
            border-radius: 4px;
            font-size: 12px;
            font-weight: bold;
        }}
        .stage-pass {{
            background: #d4edda;
            color: #155724;
        }}
        .stage-fail {{
            background: #f8d7da;
            color: #721c24;
        }}
        .progress-bar {{
            width: 100%;
            height: 30px;
            background: #f0f0f0;
            border-radius: 15px;
            overflow: hidden;
            margin: 10px 0;
        }}
        .progress-fill {{
            height: 100%;
            background: linear-gradient(90deg, #28a745 0%, #20c997 100%);
            display: flex;
            align-items: center;
            padding-left: 10px;
            color: white;
            font-weight: bold;
        }}
    </style>
</head>
<body>
    <div class="container">
        <h1>🎯 Yaze Test Results Report</h1>

        <div class="summary">
            <div class="metric">
                <div class="metric-value {'status-pass' if results.overall_passed else 'status-fail'}">
                    {'PASSED' if results.overall_passed else 'FAILED'}
                </div>
                <div class="metric-label">Overall Status</div>
            </div>
            <div class="metric">
                <div class="metric-value">{results.total_tests}</div>
                <div class="metric-label">Total Tests</div>
            </div>
            <div class="metric">
                <div class="metric-value">{results.total_passed}</div>
                <div class="metric-label">Passed</div>
            </div>
            <div class="metric">
                <div class="metric-value">{results.total_failed}</div>
                <div class="metric-label">Failed</div>
            </div>
            <div class="metric">
                <div class="metric-value">{results.total_duration}s</div>
                <div class="metric-label">Duration</div>
            </div>
            <div class="metric">
                <div class="metric-value">{results.parallel_efficiency}%</div>
                <div class="metric-label">Efficiency</div>
            </div>
        </div>

        <h2>📊 Pass Rate</h2>
        <div class="progress-bar">
            <div class="progress-fill" style="width: {results.total_passed / results.total_tests * 100:.1f}%">
                {results.total_passed / results.total_tests * 100:.1f}%
            </div>
        </div>

        <h2>🚀 Stage Results</h2>
        <table>
            <tr>
                <th>Stage</th>
                <th>Status</th>
                <th>Tests</th>
                <th>Passed</th>
                <th>Failed</th>
                <th>Pass Rate</th>
                <th>Duration</th>
            </tr>
            <tr>
                <td>Stage 1: Smoke</td>
                <td><span class="stage-badge {'stage-pass' if results.stage1.failed == 0 else 'stage-fail'}">
                    {results.stage1.emoji}
                </span></td>
                <td>{results.stage1.total}</td>
                <td>{results.stage1.passed}</td>
                <td>{results.stage1.failed}</td>
                <td>{results.stage1.pass_rate:.1f}%</td>
                <td>{results.stage1.duration:.2f}s</td>
            </tr>
            <tr>
                <td>Stage 2: Unit</td>
                <td><span class="stage-badge {'stage-pass' if results.stage2.failed == 0 else 'stage-fail'}">
                    {results.stage2.emoji}
                </span></td>
                <td>{results.stage2.total}</td>
                <td>{results.stage2.passed}</td>
                <td>{results.stage2.failed}</td>
                <td>{results.stage2.pass_rate:.1f}%</td>
                <td>{results.stage2.duration:.2f}s</td>
            </tr>
            <tr>
                <td>Stage 3: Integration</td>
                <td><span class="stage-badge {'stage-pass' if results.stage3.failed == 0 else 'stage-fail'}">
                    {results.stage3.emoji}
                </span></td>
                <td>{results.stage3.total}</td>
                <td>{results.stage3.passed}</td>
                <td>{results.stage3.failed}</td>
                <td>{results.stage3.pass_rate:.1f}%</td>
                <td>{results.stage3.duration:.2f}s</td>
            </tr>
        </table>

        {'<h2>❌ Failed Tests</h2><table><tr><th>Test</th><th>Suite</th><th>Message</th></tr>' if results.failed_tests else ''}
        {''.join(f'<tr><td>{t.name}</td><td>{t.suite}</td><td>{t.message[:100]}</td></tr>' for t in results.failed_tests[:20])}
        {'</table>' if results.failed_tests else ''}

        <h2>🐌 Slowest Tests</h2>
        <table>
            <tr>
                <th>Test</th>
                <th>Suite</th>
                <th>Duration</th>
            </tr>
            {''.join(f'<tr><td>{t.name}</td><td>{t.suite}</td><td>{t.duration:.3f}s</td></tr>' for t in results.slowest_tests)}
        </table>

        <p style="text-align: center; color: #666; margin-top: 40px;">
            Generated: {datetime.now().strftime('%Y-%m-%d %H:%M:%S')} |
            <a href="https://github.com/yaze/yaze">Yaze Project</a>
        </p>
    </div>
</body>
</html>"""

        output_path.write_text(html)


def main():
    """Main entry point."""
    parser = argparse.ArgumentParser(
        description="Aggregate test results from multiple sources"
    )
    parser.add_argument(
        "--input-dir",
        type=Path,
        required=True,
        help="Directory containing test result files"
    )
    parser.add_argument(
        "--output",
        type=Path,
        default=Path("results_summary.json"),
        help="Output JSON file for aggregated results"
    )
    parser.add_argument(
        "--generate-html",
        type=Path,
        help="Generate HTML report at specified path"
    )

    args = parser.parse_args()

    if not args.input_dir.exists():
        print(f"Error: Input directory not found: {args.input_dir}", file=sys.stderr)
        sys.exit(1)

    # Create aggregator
    aggregator = TestResultAggregator(args.input_dir)

    # Collect and process results
    print("Collecting test results...")
    aggregator.collect_results()

    print(f"Found {len(aggregator.all_tests)} total tests across "
          f"{len(aggregator.test_suites)} suites")

    # Categorize by stage
    aggregator.categorize_by_stage()

    # Generate summary
    summary = aggregator.generate_summary()

    # Save JSON summary
    with open(args.output, 'w') as f:
        # Convert dataclasses to dict
        summary_dict = asdict(summary)
        json.dump(summary_dict, f, indent=2, default=str)

    print(f"Summary saved to {args.output}")

    # Generate HTML report if requested
    if args.generate_html:
        aggregator.generate_html_report(summary, args.generate_html)
        print(f"HTML report saved to {args.generate_html}")

    # Print summary
    print(f"\n{'=' * 60}")
    print(f"Test Results Summary")
    print(f"{'=' * 60}")
    print(f"Overall Status: {'✅ PASSED' if summary.overall_passed else '❌ FAILED'}")
    print(f"Total Tests:    {summary.total_tests}")
    print(f"Passed:         {summary.total_passed} ({summary.total_passed/summary.total_tests*100:.1f}%)")
    print(f"Failed:         {summary.total_failed}")
    print(f"Duration:       {summary.total_duration}s")

    # Exit with appropriate code
    sys.exit(0 if summary.overall_passed else 1)


if __name__ == "__main__":
    main()