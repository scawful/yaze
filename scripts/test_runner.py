#!/usr/bin/env python3
"""
Advanced test runner with automatic sharding and parallel execution for yaze.
Optimizes test execution time by distributing tests across multiple processes.
"""

import multiprocessing
import json
import subprocess
import time
import argparse
import sys
import os
from pathlib import Path
from typing import List, Dict, Tuple, Optional
from dataclasses import dataclass, asdict
from concurrent.futures import ProcessPoolExecutor, as_completed
import hashlib

@dataclass
class TestResult:
    """Container for test execution results."""
    name: str
    status: str  # passed, failed, skipped
    duration: float
    output: str
    shard_id: int

@dataclass
class ShardResult:
    """Results from a single test shard."""
    shard_id: int
    return_code: int
    tests_run: int
    tests_passed: int
    tests_failed: int
    duration: float
    test_results: List[TestResult]

class TestRunner:
    """Advanced test runner with sharding and parallel execution."""

    def __init__(self, test_binary: str, num_shards: int = None,
                 cache_dir: str = None, verbose: bool = False):
        self.test_binary = Path(test_binary).resolve()
        if not self.test_binary.exists():
            raise FileNotFoundError(f"Test binary not found: {test_binary}")

        self.num_shards = num_shards or min(multiprocessing.cpu_count(), 8)
        self.cache_dir = Path(cache_dir or Path.home() / ".yaze_test_cache")
        self.cache_dir.mkdir(parents=True, exist_ok=True)
        self.verbose = verbose
        self.test_times = self.load_test_times()

    def load_test_times(self) -> Dict[str, float]:
        """Load historical test execution times from cache."""
        cache_file = self.cache_dir / "test_times.json"
        if cache_file.exists():
            try:
                return json.loads(cache_file.read_text())
            except (json.JSONDecodeError, IOError):
                return {}
        return {}

    def save_test_times(self, test_times: Dict[str, float]):
        """Save test execution times to cache."""
        cache_file = self.cache_dir / "test_times.json"

        # Merge with existing times
        existing = self.load_test_times()
        for test, time in test_times.items():
            # Use exponential moving average for smoothing
            if test in existing:
                existing[test] = 0.7 * existing[test] + 0.3 * time
            else:
                existing[test] = time

        cache_file.write_text(json.dumps(existing, indent=2))

    def discover_tests(self, filter_pattern: str = None) -> List[str]:
        """Discover all tests in the binary."""
        cmd = [str(self.test_binary), "--gtest_list_tests"]
        if filter_pattern:
            cmd.append(f"--gtest_filter={filter_pattern}")

        try:
            result = subprocess.run(cmd, capture_output=True, text=True,
                                  timeout=30, check=False)
        except subprocess.TimeoutExpired:
            print("Warning: Test discovery timed out", file=sys.stderr)
            return []

        if result.returncode != 0:
            print(f"Warning: Test discovery failed: {result.stderr}", file=sys.stderr)
            return []

        # Parse gtest output
        tests = []
        current_suite = ""
        for line in result.stdout.splitlines():
            line = line.rstrip()
            if not line or line.startswith("Running main()"):
                continue

            if line and not line.startswith(" "):
                # Test suite name
                current_suite = line.rstrip(".")
            elif line.strip():
                # Test case name
                test_name = line.strip()
                # Remove comments (e.g., "  TestName  # Comment")
                if "#" in test_name:
                    test_name = test_name.split("#")[0].strip()
                if test_name:
                    tests.append(f"{current_suite}.{test_name}")

        if self.verbose:
            print(f"Discovered {len(tests)} tests")

        return tests

    def create_balanced_shards(self, tests: List[str]) -> List[List[str]]:
        """Create balanced shards based on historical execution times."""
        if not tests:
            return []

        # Sort tests by execution time (longest first)
        # Use historical times or default estimate
        default_time = 0.1  # 100ms default per test
        sorted_tests = sorted(
            tests,
            key=lambda t: self.test_times.get(t, default_time),
            reverse=True
        )

        # Initialize shards
        num_shards = min(self.num_shards, len(tests))
        shards = [[] for _ in range(num_shards)]
        shard_times = [0.0] * num_shards

        # Distribute tests using greedy bin packing
        for test in sorted_tests:
            # Find shard with minimum total time
            min_shard_idx = shard_times.index(min(shard_times))
            shards[min_shard_idx].append(test)
            shard_times[min_shard_idx] += self.test_times.get(test, default_time)

        # Remove empty shards
        shards = [s for s in shards if s]

        if self.verbose:
            print(f"Created {len(shards)} shards:")
            for i, shard in enumerate(shards):
                print(f"  Shard {i}: {len(shard)} tests, "
                      f"estimated {shard_times[i]:.2f}s")

        return shards

    def run_shard(self, shard_id: int, tests: List[str],
                  output_dir: Path = None) -> ShardResult:
        """Run a single shard of tests."""
        if not tests:
            return ShardResult(shard_id, 0, 0, 0, 0, 0.0, [])

        filter_str = ":".join(tests)
        output_dir = output_dir or self.cache_dir / "results"
        output_dir.mkdir(parents=True, exist_ok=True)

        # Prepare command
        json_output = output_dir / f"shard_{shard_id}_results.json"
        xml_output = output_dir / f"shard_{shard_id}_results.xml"

        cmd = [
            str(self.test_binary),
            f"--gtest_filter={filter_str}",
            f"--gtest_output=json:{json_output}",
            "--gtest_brief=1"
        ]

        # Run tests
        start_time = time.time()
        try:
            result = subprocess.run(
                cmd,
                capture_output=True,
                text=True,
                timeout=600  # 10 minute timeout per shard
            )
            duration = time.time() - start_time
        except subprocess.TimeoutExpired:
            print(f"Shard {shard_id} timed out!", file=sys.stderr)
            return ShardResult(shard_id, -1, len(tests), 0, len(tests),
                             600.0, [])

        # Parse results
        test_results = []
        tests_run = 0
        tests_passed = 0
        tests_failed = 0

        if json_output.exists():
            try:
                with open(json_output) as f:
                    data = json.load(f)

                for suite in data.get("testsuites", []):
                    for testcase in suite.get("testsuite", []):
                        test_name = f"{suite['name']}.{testcase['name']}"
                        status = "passed" if testcase.get("result") == "COMPLETED" else "failed"
                        test_duration = float(testcase.get("time", "0").rstrip("s"))

                        test_results.append(TestResult(
                            name=test_name,
                            status=status,
                            duration=test_duration,
                            output=testcase.get("output", ""),
                            shard_id=shard_id
                        ))

                        tests_run += 1
                        if status == "passed":
                            tests_passed += 1
                        else:
                            tests_failed += 1

            except (json.JSONDecodeError, KeyError, IOError) as e:
                print(f"Warning: Failed to parse results for shard {shard_id}: {e}",
                      file=sys.stderr)

        return ShardResult(
            shard_id=shard_id,
            return_code=result.returncode,
            tests_run=tests_run,
            tests_passed=tests_passed,
            tests_failed=tests_failed,
            duration=duration,
            test_results=test_results
        )

    def run_parallel(self, filter_pattern: str = None,
                     output_dir: str = None) -> Tuple[int, Dict]:
        """Run tests in parallel shards."""
        # Discover tests
        tests = self.discover_tests(filter_pattern)
        if not tests:
            print("No tests found to run")
            return 0, {}

        print(f"Running {len(tests)} tests in up to {self.num_shards} shards...")

        # Create shards
        shards = self.create_balanced_shards(tests)
        output_path = Path(output_dir) if output_dir else self.cache_dir / "results"

        # Run shards in parallel
        all_results = []
        start_time = time.time()

        with ProcessPoolExecutor(max_workers=len(shards)) as executor:
            # Submit all shards
            futures = {
                executor.submit(self.run_shard, i, shard, output_path): i
                for i, shard in enumerate(shards)
            }

            # Collect results
            for future in as_completed(futures):
                shard_id = futures[future]
                try:
                    result = future.result()
                    all_results.append(result)

                    if self.verbose:
                        print(f"Shard {shard_id} completed: "
                              f"{result.tests_passed}/{result.tests_run} passed "
                              f"in {result.duration:.2f}s")
                except Exception as e:
                    print(f"Shard {shard_id} failed with exception: {e}",
                          file=sys.stderr)

        total_duration = time.time() - start_time

        # Aggregate results
        total_tests = sum(r.tests_run for r in all_results)
        total_passed = sum(r.tests_passed for r in all_results)
        total_failed = sum(r.tests_failed for r in all_results)
        max_return_code = max((r.return_code for r in all_results), default=0)

        # Update test times cache
        new_times = {}
        for result in all_results:
            for test_result in result.test_results:
                new_times[test_result.name] = test_result.duration
        self.save_test_times(new_times)

        # Generate summary
        summary = {
            "total_tests": total_tests,
            "passed": total_passed,
            "failed": total_failed,
            "duration": total_duration,
            "num_shards": len(shards),
            "parallel_efficiency": (sum(r.duration for r in all_results) /
                                   (total_duration * len(shards)) * 100)
                                   if len(shards) > 0 else 0,
            "shards": [asdict(r) for r in all_results]
        }

        # Save summary
        summary_file = output_path / "summary.json"
        summary_file.write_text(json.dumps(summary, indent=2))

        # Print results
        print(f"\n{'=' * 60}")
        print(f"Test Execution Summary")
        print(f"{'=' * 60}")
        print(f"Total Tests:    {total_tests}")
        print(f"Passed:         {total_passed} ({total_passed/total_tests*100:.1f}%)")
        print(f"Failed:         {total_failed}")
        print(f"Duration:       {total_duration:.2f}s")
        print(f"Shards Used:    {len(shards)}")
        print(f"Efficiency:     {summary['parallel_efficiency']:.1f}%")

        if total_failed > 0:
            print(f"\nFailed Tests:")
            for result in all_results:
                for test_result in result.test_results:
                    if test_result.status == "failed":
                        print(f"  - {test_result.name}")

        return max_return_code, summary

    def run_with_retry(self, filter_pattern: str = None,
                       max_retries: int = 2) -> int:
        """Run tests with automatic retry for flaky tests."""
        failed_tests = set()
        attempt = 0

        while attempt <= max_retries:
            if attempt > 0:
                # Only retry failed tests
                if not failed_tests:
                    break
                filter_pattern = ":".join(failed_tests)
                print(f"\nRetry attempt {attempt} for {len(failed_tests)} failed tests")

            return_code, summary = self.run_parallel(filter_pattern)

            if return_code == 0:
                if attempt > 0:
                    print(f"All tests passed after {attempt} retries")
                return 0

            # Collect failed tests for retry
            failed_tests.clear()
            for shard in summary.get("shards", []):
                for test_result in shard.get("test_results", []):
                    if test_result.get("status") == "failed":
                        failed_tests.add(test_result.get("name"))

            attempt += 1

        print(f"Tests still failing after {max_retries} retries")
        return return_code


def main():
    """Main entry point."""
    parser = argparse.ArgumentParser(
        description="Advanced test runner with parallel execution"
    )
    parser.add_argument(
        "test_binary",
        help="Path to the test binary"
    )
    parser.add_argument(
        "--shards",
        type=int,
        help="Number of parallel shards (default: CPU count)"
    )
    parser.add_argument(
        "--filter",
        help="Test filter pattern (gtest format)"
    )
    parser.add_argument(
        "--output-dir",
        help="Directory for test results"
    )
    parser.add_argument(
        "--cache-dir",
        help="Directory for test cache (default: ~/.yaze_test_cache)"
    )
    parser.add_argument(
        "--retry",
        type=int,
        default=0,
        help="Number of retries for failed tests"
    )
    parser.add_argument(
        "--verbose", "-v",
        action="store_true",
        help="Enable verbose output"
    )

    args = parser.parse_args()

    try:
        runner = TestRunner(
            test_binary=args.test_binary,
            num_shards=args.shards,
            cache_dir=args.cache_dir,
            verbose=args.verbose
        )

        if args.retry > 0:
            return_code = runner.run_with_retry(
                filter_pattern=args.filter,
                max_retries=args.retry
            )
        else:
            return_code, _ = runner.run_parallel(
                filter_pattern=args.filter,
                output_dir=args.output_dir
            )

        sys.exit(return_code)

    except Exception as e:
        print(f"Error: {e}", file=sys.stderr)
        sys.exit(1)


if __name__ == "__main__":
    main()