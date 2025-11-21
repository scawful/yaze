# Test Infrastructure Improvements Design Document

## Executive Summary

This document outlines comprehensive improvements to the yaze test infrastructure, focusing on build optimization, parallel execution, CI/CD enhancements, and developer experience. These improvements target reducing test execution time by 60%, improving CI reliability, and enabling faster feedback loops for developers.

## 1. Test Build Optimization

### 1.1 Precompiled Headers Strategy

**Implementation**: Create PCH for test utilities to reduce compilation time by 40%.

```cmake
# test/CMakeLists.txt additions
# Create precompiled header for test utilities
set(TEST_PCH_HEADERS
  <gtest/gtest.h>
  <gmock/gmock.h>
  <absl/status/status.h>
  <absl/status/statusor.h>
  "test/test_utils.h"
)

target_precompile_headers(yaze_test_support PRIVATE ${TEST_PCH_HEADERS})

# Apply PCH to all test targets
function(yaze_add_test_suite suite_name label is_gui_test)
  # ... existing code ...
  target_precompile_headers(${suite_name} REUSE_FROM yaze_test_support)
endfunction()
```

### 1.2 Incremental Test Build System

**Design**: Smart rebuild system using file dependency tracking.

```cmake
# cmake/TestIncrementalBuild.cmake
function(enable_incremental_test_builds)
  # Generate dependency map
  execute_process(
    COMMAND ${CMAKE_COMMAND} -E env python3
      ${CMAKE_SOURCE_DIR}/scripts/generate_test_deps.py
    OUTPUT_VARIABLE TEST_DEPENDENCY_MAP
  )

  # Create test groups based on changed files
  set_property(GLOBAL PROPERTY TEST_DEPENDENCY_MAP ${TEST_DEPENDENCY_MAP})

  # Custom target for incremental test builds
  add_custom_target(test-incremental
    COMMAND ${CMAKE_COMMAND}
      -DCHANGED_FILES="${CHANGED_FILES}"
      -P ${CMAKE_SOURCE_DIR}/cmake/RunIncrementalTests.cmake
  )
endfunction()
```

### 1.3 Test Binary Caching

**Architecture**: Distributed cache for test binaries across CI runs.

```yaml
# .github/actions/cache-test-binaries/action.yml
name: 'Cache Test Binaries'
description: 'Cache compiled test binaries based on source hash'
inputs:
  platform:
    required: true
outputs:
  cache-hit:
    value: ${{ steps.cache.outputs.cache-hit }}

runs:
  using: 'composite'
  steps:
    - name: Generate test source hash
      id: hash
      shell: bash
      run: |
        TEST_HASH=$(find test src/app src/zelda3 -name "*.cc" -o -name "*.h" | \
          xargs sha256sum | sha256sum | cut -d' ' -f1)
        echo "hash=${TEST_HASH}" >> $GITHUB_OUTPUT

    - name: Cache test binaries
      id: cache
      uses: actions/cache@v4
      with:
        path: |
          build/bin/yaze_test*
          build/test_cache/
        key: test-bins-${{ inputs.platform }}-${{ steps.hash.outputs.hash }}
        restore-keys: |
          test-bins-${{ inputs.platform }}-
```

### 1.4 Dependency Management Improvements

```cmake
# cmake/TestDependencies.cmake
# Separate test-only dependencies from main build
option(YAZE_TEST_MINIMAL_DEPS "Use minimal dependencies for tests" OFF)

if(YAZE_TEST_MINIMAL_DEPS)
  # Use lightweight mocks instead of full dependencies
  CPMAddPackage(
    NAME mock_sdl
    GITHUB_REPOSITORY yaze/mock-sdl
    VERSION 1.0.0
    OPTIONS "BUILD_SHARED_LIBS OFF"
  )

  # Create mock target aliases
  add_library(SDL2::SDL2 ALIAS mock_sdl)
else()
  # Full dependencies for integration tests
  include(cmake/Dependencies.cmake)
endif()

# Test data management
CPMAddPackage(
  NAME yaze_test_data
  URL https://github.com/yaze/test-data/archive/v1.0.0.tar.gz
  DOWNLOAD_ONLY YES
)

set(YAZE_TEST_DATA_DIR ${yaze_test_data_SOURCE_DIR} CACHE PATH "Test data directory")
```

## 2. Test Execution Infrastructure

### 2.1 Parallel Test Execution Framework

**Design**: Automatic sharding with resource-aware scheduling.

```python
# scripts/test_runner.py
#!/usr/bin/env python3
"""
Advanced test runner with automatic sharding and parallel execution.
"""

import multiprocessing
import json
import subprocess
import time
from pathlib import Path
from typing import List, Dict, Tuple
import argparse

class TestRunner:
    def __init__(self, test_binary: str, num_shards: int = None):
        self.test_binary = test_binary
        self.num_shards = num_shards or multiprocessing.cpu_count()
        self.test_times = self.load_test_times()

    def load_test_times(self) -> Dict[str, float]:
        """Load historical test execution times."""
        cache_file = Path.home() / ".yaze_test_times.json"
        if cache_file.exists():
            return json.loads(cache_file.read_text())
        return {}

    def discover_tests(self) -> List[str]:
        """Discover all tests in the binary."""
        result = subprocess.run(
            [self.test_binary, "--gtest_list_tests"],
            capture_output=True, text=True
        )
        # Parse gtest output to get test names
        tests = []
        current_suite = ""
        for line in result.stdout.splitlines():
            if line and not line.startswith(" "):
                current_suite = line.rstrip(".")
            elif line.strip():
                tests.append(f"{current_suite}.{line.strip()}")
        return tests

    def create_balanced_shards(self, tests: List[str]) -> List[List[str]]:
        """Create balanced shards based on historical execution times."""
        # Sort tests by execution time (longest first)
        sorted_tests = sorted(tests,
                            key=lambda t: self.test_times.get(t, 1.0),
                            reverse=True)

        # Initialize shards with empty lists and zero time
        shards = [[] for _ in range(self.num_shards)]
        shard_times = [0.0] * self.num_shards

        # Assign tests to shards using greedy algorithm
        for test in sorted_tests:
            # Find shard with minimum total time
            min_shard = shard_times.index(min(shard_times))
            shards[min_shard].append(test)
            shard_times[min_shard] += self.test_times.get(test, 1.0)

        return [s for s in shards if s]  # Remove empty shards

    def run_shard(self, shard_id: int, tests: List[str]) -> Tuple[int, Dict]:
        """Run a single shard of tests."""
        filter = ":".join(tests)
        start_time = time.time()

        result = subprocess.run(
            [self.test_binary, f"--gtest_filter={filter}",
             "--gtest_output=json:test_results_shard_{}.json".format(shard_id)],
            capture_output=True
        )

        execution_time = time.time() - start_time

        # Update test times
        test_times = {}
        if result.returncode == 0:
            # Parse JSON output for individual test times
            results_file = f"test_results_shard_{shard_id}.json"
            if Path(results_file).exists():
                data = json.loads(Path(results_file).read_text())
                for suite in data.get("testsuites", []):
                    for test in suite.get("testsuite", []):
                        test_name = f"{suite['name']}.{test['name']}"
                        test_times[test_name] = test.get("time", 1.0)

        return result.returncode, test_times

    def run_parallel(self) -> int:
        """Run tests in parallel shards."""
        tests = self.discover_tests()
        shards = self.create_balanced_shards(tests)

        print(f"Running {len(tests)} tests in {len(shards)} shards")

        with multiprocessing.Pool(len(shards)) as pool:
            results = pool.starmap(self.run_shard, enumerate(shards))

        # Update test times cache
        all_times = {}
        for _, times in results:
            all_times.update(times)

        cache_file = Path.home() / ".yaze_test_times.json"
        cache_file.write_text(json.dumps(all_times))

        # Return failure if any shard failed
        return max(code for code, _ in results)

if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("test_binary", help="Path to test binary")
    parser.add_argument("--shards", type=int, help="Number of shards")
    args = parser.parse_args()

    runner = TestRunner(args.test_binary, args.shards)
    exit(runner.run_parallel())
```

### 2.2 Test Selection and Filtering System

```python
# scripts/smart_test_selector.py
#!/usr/bin/env python3
"""
Smart test selector based on changed files and test coverage data.
"""

import subprocess
import json
from pathlib import Path
from typing import Set, List
import argparse

class TestSelector:
    def __init__(self, coverage_db: str = ".coverage.db"):
        self.coverage_db = Path(coverage_db)
        self.coverage_map = self.load_coverage_map()

    def load_coverage_map(self) -> Dict[str, Set[str]]:
        """Load mapping of source files to tests that cover them."""
        if not self.coverage_db.exists():
            return {}

        # In practice, this would query a SQLite database
        # For now, use a JSON file
        coverage_file = self.coverage_db.with_suffix(".json")
        if coverage_file.exists():
            data = json.loads(coverage_file.read_text())
            return {k: set(v) for k, v in data.items()}
        return {}

    def get_changed_files(self, base_ref: str = "origin/master") -> List[str]:
        """Get list of changed files compared to base."""
        result = subprocess.run(
            ["git", "diff", "--name-only", base_ref],
            capture_output=True, text=True
        )
        return result.stdout.strip().split("\n")

    def select_tests(self, changed_files: List[str]) -> Set[str]:
        """Select tests based on changed files."""
        selected_tests = set()

        for file in changed_files:
            # Direct test file changes
            if file.startswith("test/") and file.endswith("_test.cc"):
                # Extract test suite name from file
                test_name = Path(file).stem
                selected_tests.add(f"*{test_name}*")

            # Source file changes - use coverage map
            elif file in self.coverage_map:
                selected_tests.update(self.coverage_map[file])

            # Header file changes - broader impact
            elif file.endswith(".h"):
                # Find all source files that include this header
                result = subprocess.run(
                    ["grep", "-l", f'#include.*{Path(file).name}"',
                     "src", "-r", "--include=*.cc"],
                    capture_output=True, text=True
                )
                for src_file in result.stdout.strip().split("\n"):
                    if src_file in self.coverage_map:
                        selected_tests.update(self.coverage_map[src_file])

        return selected_tests

    def generate_filter(self, tests: Set[str]) -> str:
        """Generate gtest filter from test set."""
        if not tests:
            return "*"  # Run all tests if none selected
        return ":".join(tests)

def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--base-ref", default="origin/master",
                       help="Base reference for diff")
    parser.add_argument("--output", choices=["filter", "list", "json"],
                       default="filter", help="Output format")
    args = parser.parse_args()

    selector = TestSelector()
    changed_files = selector.get_changed_files(args.base_ref)
    tests = selector.select_tests(changed_files)

    if args.output == "filter":
        print(selector.generate_filter(tests))
    elif args.output == "list":
        for test in sorted(tests):
            print(test)
    else:  # json
        print(json.dumps(list(tests)))

if __name__ == "__main__":
    main()
```

### 2.3 Test Tag System Implementation

```cmake
# cmake/TestTags.cmake
# Define test tags for categorization
set(YAZE_TEST_TAGS
  graphics     # Graphics and rendering tests
  rom          # ROM manipulation tests
  ui           # UI component tests
  network      # Network and gRPC tests
  compression  # Compression algorithm tests
  emulator     # Emulator core tests
  integration  # Multi-component integration
  performance  # Performance-critical tests
)

# Tag assignment function
function(tag_test test_name tags)
  set_tests_properties(${test_name} PROPERTIES LABELS "${tags}")
endfunction()

# Run tests by tag
add_custom_target(test-by-tag
  COMMAND ${CMAKE_CTEST_COMMAND} -L "${TAG}"
  WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
)
```

## 3. CI/CD Pipeline Enhancements

### 3.1 Multi-Stage Test Pipeline

```yaml
# .github/workflows/ci-enhanced.yml
name: Enhanced CI/CD Pipeline

on:
  push:
    branches: [ master, develop ]
  pull_request:
    branches: [ master, develop ]

env:
  STAGE_1_TIMEOUT: 120  # 2 minutes
  STAGE_2_TIMEOUT: 300  # 5 minutes
  STAGE_3_TIMEOUT: 900  # 15 minutes

jobs:
  # Stage 1: Smoke Tests (Critical Path)
  stage1-smoke:
    name: "Stage 1: Smoke Tests"
    runs-on: ${{ matrix.os }}
    timeout-minutes: 3
    strategy:
      fail-fast: true
      matrix:
        os: [ubuntu-22.04, macos-14, windows-2022]

    steps:
      - uses: actions/checkout@v4

      - name: Cache smoke test binaries
        uses: ./.github/actions/cache-test-binaries
        with:
          platform: ${{ matrix.os }}
          test-type: smoke

      - name: Build minimal test set
        run: |
          cmake --preset ci-minimal \
            -DYAZE_TEST_MINIMAL_DEPS=ON \
            -DYAZE_BUILD_SMOKE_TESTS=ON
          cmake --build build --target yaze_test_smoke

      - name: Run smoke tests
        run: |
          ./build/bin/yaze_test_smoke \
            --gtest_filter="*SmokeTest*:*Critical*" \
            --gtest_output=xml:smoke_results.xml
        timeout-minutes: 2

      - name: Upload results
        if: always()
        uses: actions/upload-artifact@v4
        with:
          name: smoke-results-${{ matrix.os }}
          path: smoke_results.xml
          retention-days: 1

  # Stage 2: Standard Unit Tests
  stage2-unit:
    name: "Stage 2: Unit Tests"
    needs: stage1-smoke
    runs-on: ${{ matrix.os }}
    timeout-minutes: 8
    strategy:
      fail-fast: false
      matrix:
        os: [ubuntu-22.04, macos-14, windows-2022]
        shard: [1, 2, 3, 4]

    steps:
      - uses: actions/checkout@v4

      - name: Setup build environment
        uses: ./.github/actions/setup-build
        with:
          platform: ${{ matrix.os }}
          cache-key: ${{ hashFiles('cmake/dependencies.lock') }}

      - name: Build tests
        uses: ./.github/actions/build-tests
        with:
          platform: ${{ matrix.os }}
          test-type: unit

      - name: Run unit test shard
        run: |
          python3 scripts/test_runner.py \
            ./build/bin/yaze_test_stable \
            --shards 4 \
            --shard-index ${{ matrix.shard }} \
            --output-dir test_results/

      - name: Upload shard results
        if: always()
        uses: actions/upload-artifact@v4
        with:
          name: unit-shard-${{ matrix.os }}-${{ matrix.shard }}
          path: test_results/
          retention-days: 3

  # Stage 3: Comprehensive Tests
  stage3-comprehensive:
    name: "Stage 3: Comprehensive Tests"
    needs: stage2-unit
    runs-on: ${{ matrix.os }}
    timeout-minutes: 20
    strategy:
      fail-fast: false
      matrix:
        include:
          - os: ubuntu-22.04
            test-type: integration
          - os: macos-14
            test-type: gui
          - os: windows-2022
            test-type: e2e

    steps:
      - uses: actions/checkout@v4

      - name: Setup full environment
        uses: ./.github/actions/setup-build
        with:
          platform: ${{ matrix.os }}
          full-deps: true

      - name: Build all tests
        run: |
          cmake --preset ci-full \
            -DYAZE_BUILD_TESTS=ON \
            -DYAZE_ENABLE_ROM_TESTS=ON
          cmake --build build --target all

      - name: Run comprehensive tests
        run: |
          case "${{ matrix.test-type }}" in
            integration)
              ctest -L integration --output-junit integration.xml
              ;;
            gui)
              ./build/bin/yaze_test_gui --headless
              ;;
            e2e)
              python3 scripts/run_e2e_tests.py --parallel
              ;;
          esac

      - name: Generate coverage report
        if: matrix.os == 'ubuntu-22.04'
        run: |
          lcov --capture --directory build \
            --output-file coverage.info
          lcov --remove coverage.info '/usr/*' '*/ext/*' \
            --output-file coverage_filtered.info
          genhtml coverage_filtered.info \
            --output-directory coverage_html

      - name: Upload coverage
        if: matrix.os == 'ubuntu-22.04'
        uses: codecov/codecov-action@v3
        with:
          file: coverage_filtered.info

  # Stage 4: Nightly Tests (scheduled)
  stage4-nightly:
    name: "Stage 4: Nightly Tests"
    if: github.event_name == 'schedule' || github.event.inputs.run_nightly == 'true'
    runs-on: ${{ matrix.os }}
    timeout-minutes: 60
    strategy:
      matrix:
        os: [ubuntu-22.04, macos-14, windows-2022]

    steps:
      - uses: actions/checkout@v4

      - name: Run performance benchmarks
        run: |
          cmake --preset ci-perf \
            -DCMAKE_BUILD_TYPE=Release \
            -DYAZE_BUILD_BENCHMARKS=ON
          cmake --build build --target yaze_test_benchmark
          ./build/bin/yaze_test_benchmark \
            --benchmark_out=benchmark_results.json \
            --benchmark_out_format=json

      - name: Run stress tests
        run: |
          python3 scripts/stress_test.py \
            --duration 3600 \
            --parallel 8 \
            --memory-limit 4GB

      - name: Run fuzzing tests
        if: matrix.os == 'ubuntu-22.04'
        run: |
          cmake --preset ci-fuzz \
            -DYAZE_ENABLE_FUZZING=ON
          cmake --build build --target fuzz_tests
          ./scripts/run_fuzzers.sh --time-limit 1800

  # Test Result Aggregation
  aggregate-results:
    name: "Aggregate Test Results"
    needs: [stage2-unit, stage3-comprehensive]
    if: always()
    runs-on: ubuntu-22.04

    steps:
      - uses: actions/checkout@v4

      - name: Download all artifacts
        uses: actions/download-artifact@v4
        with:
          path: all_results/

      - name: Aggregate results
        run: |
          python3 scripts/aggregate_test_results.py \
            --input-dir all_results/ \
            --output results_summary.json \
            --generate-report

      - name: Post results to PR
        if: github.event_name == 'pull_request'
        uses: actions/github-script@v6
        with:
          script: |
            const fs = require('fs');
            const results = JSON.parse(fs.readFileSync('results_summary.json'));

            const comment = `## Test Results Summary

            | Stage | Status | Tests | Duration |
            |-------|--------|-------|----------|
            | Smoke | ${results.stage1.status} | ${results.stage1.tests} | ${results.stage1.duration}s |
            | Unit | ${results.stage2.status} | ${results.stage2.tests} | ${results.stage2.duration}s |
            | Comprehensive | ${results.stage3.status} | ${results.stage3.tests} | ${results.stage3.duration}s |

            **Total**: ${results.total.passed}/${results.total.total} passed in ${results.total.duration}s

            [View detailed report](${results.report_url})`;

            github.rest.issues.createComment({
              issue_number: context.issue.number,
              owner: context.repo.owner,
              repo: context.repo.repo,
              body: comment
            });
```

### 3.2 Platform-Specific Test Optimization

```yaml
# .github/actions/platform-test/action.yml
name: 'Platform-Specific Tests'
description: 'Run platform-specific test suites'
inputs:
  platform:
    required: true
  gpu-tests:
    default: 'false'

runs:
  using: 'composite'
  steps:
    - name: macOS GPU Tests
      if: inputs.platform == 'macos' && inputs.gpu-tests == 'true'
      shell: bash
      run: |
        # Enable Metal validation
        export METAL_DEVICE_WRAPPER_TYPE=1
        export METAL_DEBUG_ERROR_MODE=2

        ./build/bin/yaze_test_gpu \
          --gtest_filter="*Metal*:*Render*" \
          --enable-gpu-validation

    - name: Windows DirectX Tests
      if: inputs.platform == 'windows' && inputs.gpu-tests == 'true'
      shell: pwsh
      run: |
        $env:D3D_DEBUG_LAYER = "1"

        .\build\bin\yaze_test_gpu.exe `
          --gtest_filter="*DirectX*:*D3D*" `
          --enable-dx-debug

    - name: Linux Vulkan Tests
      if: inputs.platform == 'linux' && inputs.gpu-tests == 'true'
      shell: bash
      run: |
        export VK_LAYER_PATH=/usr/share/vulkan/explicit_layer.d
        export VK_INSTANCE_LAYERS=VK_LAYER_KHRONOS_validation

        ./build/bin/yaze_test_gpu \
          --gtest_filter="*Vulkan*:*GL*" \
          --enable-vulkan-validation
```

## 4. Local Development Testing

### 4.1 Enhanced Pre-Push Validation

```bash
#!/usr/bin/env bash
# scripts/pre-push-enhanced.sh
# Advanced pre-push validation with smart test selection

set -euo pipefail

# Configuration from .yaze-test.conf
source "${HOME}/.yaze-test.conf" 2>/dev/null || true

: ${TEST_CACHE_DIR:="${HOME}/.cache/yaze-tests"}
: ${MAX_TEST_TIME:=300}  # 5 minutes max
: ${PARALLEL_JOBS:=$(nproc)}

# Create test cache directory
mkdir -p "${TEST_CACHE_DIR}"

# Function to compute file hash
compute_hash() {
    find "$1" -name "*.cc" -o -name "*.h" | \
        xargs sha256sum 2>/dev/null | \
        sha256sum | cut -d' ' -f1
}

# Function to check if tests need to run
need_tests() {
    local current_hash
    current_hash=$(compute_hash "src test")
    local cached_hash=""

    if [[ -f "${TEST_CACHE_DIR}/last_test_hash" ]]; then
        cached_hash=$(cat "${TEST_CACHE_DIR}/last_test_hash")
    fi

    if [[ "${current_hash}" == "${cached_hash}" ]]; then
        echo "No code changes detected, skipping tests"
        return 1
    fi

    echo "${current_hash}" > "${TEST_CACHE_DIR}/last_test_hash"
    return 0
}

# Smart test selection based on changes
select_tests() {
    local changed_files
    changed_files=$(git diff --name-only --cached)

    if [[ -z "${changed_files}" ]]; then
        echo "*"  # Run all tests if no specific changes
        return
    fi

    python3 scripts/smart_test_selector.py \
        --base-ref HEAD \
        --output filter
}

# Run tests with timeout and parallelization
run_tests() {
    local test_filter="$1"
    local start_time=$(date +%s)

    # Run tests in parallel with timeout
    timeout "${MAX_TEST_TIME}" \
        python3 scripts/test_runner.py \
            ./build/bin/yaze_test \
            --filter "${test_filter}" \
            --shards "${PARALLEL_JOBS}" \
            --output-dir "${TEST_CACHE_DIR}/results"

    local end_time=$(date +%s)
    local duration=$((end_time - start_time))

    # Save performance metrics
    echo "{\"duration\": ${duration}, \"filter\": \"${test_filter}\"}" \
        >> "${TEST_CACHE_DIR}/perf_log.jsonl"

    return 0
}

# Main execution
main() {
    echo "=== Enhanced Pre-Push Validation ==="

    # Check if we need to run tests
    if ! need_tests; then
        echo "✅ Tests skipped (no changes)"
        exit 0
    fi

    # Build tests if needed
    if [[ ! -f "build/bin/yaze_test" ]] || \
       [[ "src/CMakeLists.txt" -nt "build/bin/yaze_test" ]]; then
        echo "Building tests..."
        cmake --build build --target yaze_test -j"${PARALLEL_JOBS}"
    fi

    # Select and run tests
    local test_filter
    test_filter=$(select_tests)

    echo "Running tests with filter: ${test_filter}"

    if run_tests "${test_filter}"; then
        echo "✅ All tests passed"

        # Update coverage database if available
        if command -v lcov &> /dev/null; then
            echo "Updating coverage database..."
            lcov --capture --directory build \
                --output-file "${TEST_CACHE_DIR}/coverage.info" \
                --quiet
        fi
    else
        echo "❌ Tests failed"
        echo "View detailed results: ${TEST_CACHE_DIR}/results/"
        exit 1
    fi
}

main "$@"
```

### 4.2 Test Generation Tools

```python
#!/usr/bin/env python3
# scripts/generate_test.py
"""
Test generation tool for creating test templates quickly.
"""

import argparse
from pathlib import Path
from typing import Optional

UNIT_TEST_TEMPLATE = '''#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include "{header_path}"

namespace yaze {{
namespace test {{

class {class_name}Test : public ::testing::Test {{
 protected:
  void SetUp() override {{
    // Test setup
  }}

  void TearDown() override {{
    // Test cleanup
  }}
}};

TEST_F({class_name}Test, ConstructorInitializesCorrectly) {{
  // Arrange

  // Act
  {class_name} obj;

  // Assert
  // Add assertions here
}}

TEST_F({class_name}Test, MethodBehavesCorrectly) {{
  // Arrange
  {class_name} obj;

  // Act
  // Call method

  // Assert
  // Verify behavior
}}

}}  // namespace test
}}  // namespace yaze
'''

INTEGRATION_TEST_TEMPLATE = '''#include <gtest/gtest.h>
#include "test/test_utils.h"
#include "{header_path}"

namespace yaze {{
namespace test {{

class {class_name}IntegrationTest : public RomTestFixture {{
 protected:
  void SetUp() override {{
    RomTestFixture::SetUp();
    // Additional setup
  }}
}};

TEST_F({class_name}IntegrationTest, IntegratesWithRomCorrectly) {{
  // Arrange
  ASSERT_OK(LoadTestRom());

  // Act
  {class_name} obj(*rom());

  // Assert
  // Verify integration
}}

}}  // namespace test
}}  // namespace yaze
'''

def generate_test(
    class_name: str,
    header_path: str,
    test_type: str,
    output_dir: Path
) -> Path:
    """Generate a test file from template."""

    # Select template
    template = UNIT_TEST_TEMPLATE if test_type == "unit" else INTEGRATION_TEST_TEMPLATE

    # Generate content
    content = template.format(
        class_name=class_name,
        header_path=header_path
    )

    # Determine output path
    test_file_name = f"{class_name.lower()}_test.cc"
    if test_type == "unit":
        output_path = output_dir / "unit" / test_file_name
    else:
        output_path = output_dir / "integration" / test_file_name

    # Create directory if needed
    output_path.parent.mkdir(parents=True, exist_ok=True)

    # Write file
    output_path.write_text(content)

    return output_path

def main():
    parser = argparse.ArgumentParser(description="Generate test templates")
    parser.add_argument("class_name", help="Name of the class to test")
    parser.add_argument("header_path", help="Path to the header file")
    parser.add_argument("--type", choices=["unit", "integration"],
                       default="unit", help="Type of test to generate")
    parser.add_argument("--output-dir", type=Path, default=Path("test"),
                       help="Output directory for test files")

    args = parser.parse_args()

    output_path = generate_test(
        args.class_name,
        args.header_path,
        args.type,
        args.output_dir
    )

    print(f"Generated test: {output_path}")
    print(f"Add to CMakeLists.txt: {output_path.name}")

if __name__ == "__main__":
    main()
```

## 5. Test Infrastructure Monitoring

### 5.1 Metrics Collection System

```yaml
# .github/workflows/test-metrics.yml
name: Test Metrics Collection

on:
  workflow_run:
    workflows: ["CI/CD Pipeline"]
    types: [completed]

jobs:
  collect-metrics:
    runs-on: ubuntu-22.04

    steps:
      - uses: actions/checkout@v4

      - name: Download test artifacts
        uses: actions/download-artifact@v4
        with:
          run-id: ${{ github.event.workflow_run.id }}
          path: artifacts/

      - name: Parse test results
        run: |
          python3 scripts/parse_test_metrics.py \
            --input artifacts/ \
            --output metrics.json

      - name: Upload to metrics database
        run: |
          curl -X POST https://metrics.yaze.io/api/tests \
            -H "Authorization: Bearer ${{ secrets.METRICS_TOKEN }}" \
            -H "Content-Type: application/json" \
            -d @metrics.json

      - name: Generate report
        run: |
          python3 scripts/generate_test_report.py \
            --metrics metrics.json \
            --output test_report.html

      - name: Upload report
        uses: actions/upload-artifact@v4
        with:
          name: test-report-${{ github.run_number }}
          path: test_report.html
```

### 5.2 Test Dashboard Configuration

```python
# scripts/test_dashboard.py
#!/usr/bin/env python3
"""
Generate test infrastructure dashboard.
"""

import json
import sqlite3
from datetime import datetime, timedelta
from pathlib import Path
import matplotlib.pyplot as plt
import pandas as pd

class TestDashboard:
    def __init__(self, db_path: str):
        self.conn = sqlite3.connect(db_path)
        self.create_tables()

    def create_tables(self):
        """Create metrics tables if they don't exist."""
        self.conn.execute('''
            CREATE TABLE IF NOT EXISTS test_runs (
                id INTEGER PRIMARY KEY,
                run_id TEXT,
                timestamp DATETIME,
                branch TEXT,
                commit_sha TEXT,
                total_tests INTEGER,
                passed_tests INTEGER,
                failed_tests INTEGER,
                skipped_tests INTEGER,
                duration_seconds REAL,
                platform TEXT
            )
        ''')

        self.conn.execute('''
            CREATE TABLE IF NOT EXISTS test_times (
                id INTEGER PRIMARY KEY,
                run_id TEXT,
                test_name TEXT,
                duration_ms REAL,
                status TEXT,
                FOREIGN KEY (run_id) REFERENCES test_runs(run_id)
            )
        ''')

        self.conn.execute('''
            CREATE TABLE IF NOT EXISTS flaky_tests (
                test_name TEXT PRIMARY KEY,
                failure_rate REAL,
                last_failure DATETIME,
                total_runs INTEGER,
                total_failures INTEGER
            )
        ''')

    def ingest_metrics(self, metrics: dict):
        """Ingest test metrics into database."""
        run_id = metrics['run_id']

        # Insert test run
        self.conn.execute('''
            INSERT INTO test_runs
            (run_id, timestamp, branch, commit_sha, total_tests,
             passed_tests, failed_tests, skipped_tests,
             duration_seconds, platform)
            VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?)
        ''', (
            run_id,
            datetime.now(),
            metrics['branch'],
            metrics['commit_sha'],
            metrics['total_tests'],
            metrics['passed_tests'],
            metrics['failed_tests'],
            metrics['skipped_tests'],
            metrics['duration_seconds'],
            metrics['platform']
        ))

        # Insert individual test times
        for test in metrics.get('tests', []):
            self.conn.execute('''
                INSERT INTO test_times
                (run_id, test_name, duration_ms, status)
                VALUES (?, ?, ?, ?)
            ''', (
                run_id,
                test['name'],
                test['duration_ms'],
                test['status']
            ))

        self.conn.commit()

    def generate_dashboard(self, output_path: Path):
        """Generate HTML dashboard with charts."""
        # Query recent test runs
        df_runs = pd.read_sql_query('''
            SELECT * FROM test_runs
            WHERE timestamp > datetime('now', '-7 days')
            ORDER BY timestamp
        ''', self.conn)

        # Query slow tests
        df_slow = pd.read_sql_query('''
            SELECT test_name, AVG(duration_ms) as avg_duration
            FROM test_times
            WHERE status = 'passed'
            GROUP BY test_name
            ORDER BY avg_duration DESC
            LIMIT 20
        ''', self.conn)

        # Query flaky tests
        df_flaky = pd.read_sql_query('''
            SELECT * FROM flaky_tests
            ORDER BY failure_rate DESC
            LIMIT 10
        ''', self.conn)

        # Generate charts
        fig, axes = plt.subplots(2, 2, figsize=(15, 10))

        # Test pass rate over time
        axes[0, 0].plot(df_runs['timestamp'],
                       df_runs['passed_tests'] / df_runs['total_tests'] * 100)
        axes[0, 0].set_title('Test Pass Rate')
        axes[0, 0].set_ylabel('Pass Rate (%)')

        # Test duration over time
        axes[0, 1].plot(df_runs['timestamp'],
                       df_runs['duration_seconds'])
        axes[0, 1].set_title('Test Duration')
        axes[0, 1].set_ylabel('Duration (seconds)')

        # Slowest tests
        axes[1, 0].barh(df_slow['test_name'][:10],
                        df_slow['avg_duration'][:10])
        axes[1, 0].set_title('Slowest Tests')
        axes[1, 0].set_xlabel('Avg Duration (ms)')

        # Flaky tests
        if not df_flaky.empty:
            axes[1, 1].bar(range(len(df_flaky)),
                          df_flaky['failure_rate'] * 100)
            axes[1, 1].set_title('Flaky Tests')
            axes[1, 1].set_ylabel('Failure Rate (%)')
            axes[1, 1].set_xticks(range(len(df_flaky)))
            axes[1, 1].set_xticklabels(df_flaky['test_name'],
                                       rotation=45, ha='right')

        plt.tight_layout()

        # Save chart
        chart_path = output_path.parent / 'test_metrics.png'
        plt.savefig(chart_path)

        # Generate HTML
        html = f'''<!DOCTYPE html>
<html>
<head>
    <title>Test Infrastructure Dashboard</title>
    <style>
        body {{ font-family: Arial, sans-serif; margin: 20px; }}
        h1 {{ color: #333; }}
        .metric {{ display: inline-block; margin: 10px; padding: 10px;
                  border: 1px solid #ddd; border-radius: 5px; }}
        .metric-value {{ font-size: 24px; font-weight: bold; }}
        table {{ border-collapse: collapse; width: 100%; margin: 20px 0; }}
        th, td {{ border: 1px solid #ddd; padding: 8px; text-align: left; }}
        th {{ background-color: #f2f2f2; }}
    </style>
</head>
<body>
    <h1>Yaze Test Infrastructure Dashboard</h1>

    <div class="metrics">
        <div class="metric">
            <div>Total Test Runs (7 days)</div>
            <div class="metric-value">{len(df_runs)}</div>
        </div>
        <div class="metric">
            <div>Average Pass Rate</div>
            <div class="metric-value">
                {df_runs['passed_tests'].sum() / df_runs['total_tests'].sum() * 100:.1f}%
            </div>
        </div>
        <div class="metric">
            <div>Average Duration</div>
            <div class="metric-value">{df_runs['duration_seconds'].mean():.1f}s</div>
        </div>
    </div>

    <img src="test_metrics.png" alt="Test Metrics Charts" style="width: 100%; max-width: 1200px;">

    <h2>Recent Test Runs</h2>
    <table>
        <tr>
            <th>Timestamp</th>
            <th>Branch</th>
            <th>Platform</th>
            <th>Tests</th>
            <th>Pass Rate</th>
            <th>Duration</th>
        </tr>
        {''.join(f"""
        <tr>
            <td>{row['timestamp']}</td>
            <td>{row['branch']}</td>
            <td>{row['platform']}</td>
            <td>{row['total_tests']}</td>
            <td>{row['passed_tests'] / row['total_tests'] * 100:.1f}%</td>
            <td>{row['duration_seconds']:.1f}s</td>
        </tr>
        """ for _, row in df_runs.tail(10).iterrows())}
    </table>

    <p>Generated: {datetime.now().strftime('%Y-%m-%d %H:%M:%S')}</p>
</body>
</html>'''

        output_path.write_text(html)

def main():
    dashboard = TestDashboard('.test_metrics.db')
    dashboard.generate_dashboard(Path('test_dashboard.html'))
    print("Dashboard generated: test_dashboard.html")

if __name__ == "__main__":
    main()
```

## 6. CMake Configuration Improvements

### 6.1 Enhanced Test Target Structure

```cmake
# cmake/TestConfiguration.cmake
include(GoogleTest)
include(CTest)

# Test configuration options
option(YAZE_TEST_PARALLEL "Enable parallel test execution" ON)
option(YAZE_TEST_COVERAGE "Enable code coverage" OFF)
option(YAZE_TEST_SANITIZERS "Enable sanitizers" OFF)
option(YAZE_TEST_PROFILE "Enable test profiling" OFF)

# Test categories as options
option(YAZE_TEST_SMOKE "Build smoke tests" ON)
option(YAZE_TEST_UNIT "Build unit tests" ON)
option(YAZE_TEST_INTEGRATION "Build integration tests" ON)
option(YAZE_TEST_E2E "Build end-to-end tests" OFF)
option(YAZE_TEST_BENCHMARK "Build benchmark tests" OFF)
option(YAZE_TEST_FUZZ "Build fuzz tests" OFF)

# Create test interface library with common settings
add_library(yaze_test_interface INTERFACE)

target_compile_options(yaze_test_interface INTERFACE
  $<$<CONFIG:Debug>:-O0 -g3>
  $<$<CONFIG:Release>:-O3 -DNDEBUG>
  $<$<BOOL:${YAZE_TEST_COVERAGE}>:--coverage>
  $<$<BOOL:${YAZE_TEST_PROFILE}>:-pg>
)

target_link_options(yaze_test_interface INTERFACE
  $<$<BOOL:${YAZE_TEST_COVERAGE}>:--coverage>
  $<$<BOOL:${YAZE_TEST_PROFILE}>:-pg>
)

if(YAZE_TEST_SANITIZERS)
  target_compile_options(yaze_test_interface INTERFACE
    -fsanitize=address,undefined
    -fno-omit-frame-pointer
  )
  target_link_options(yaze_test_interface INTERFACE
    -fsanitize=address,undefined
  )
endif()

# Function to create categorized test suite
function(yaze_create_test_category category_name)
  set(options GUI_TEST REQUIRES_ROM)
  set(oneValueArgs TIMEOUT PARALLEL_JOBS)
  set(multiValueArgs SOURCES DEPENDENCIES LABELS)
  cmake_parse_arguments(TEST "${options}" "${oneValueArgs}" "${multiValueArgs}" ${ARGN})

  # Create test executable
  set(target_name yaze_test_${category_name})
  add_executable(${target_name} ${TEST_SOURCES})

  # Link common interface
  target_link_libraries(${target_name} PRIVATE
    yaze_test_interface
    yaze_test_support
    ${TEST_DEPENDENCIES}
  )

  # Set test properties
  set_target_properties(${target_name} PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin/test"
  )

  # Discover tests with labels
  gtest_discover_tests(${target_name}
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    PROPERTIES
      LABELS "${TEST_LABELS};${category_name}"
      TIMEOUT ${TEST_TIMEOUT}
    DISCOVERY_MODE PRE_TEST
  )

  # Add to CTest presets
  file(APPEND "${CMAKE_BINARY_DIR}/CTestTestfile.cmake"
    "set_tests_properties(\${${target_name}_TESTS} PROPERTIES RUN_SERIAL OFF)\n"
  )

  if(TEST_PARALLEL_JOBS)
    set_property(TEST ${target_name} PROPERTY PROCESSORS ${TEST_PARALLEL_JOBS})
  endif()
endfunction()

# Create test suites
if(YAZE_TEST_SMOKE)
  yaze_create_test_category(smoke
    SOURCES
      test/smoke/critical_path_test.cc
      test/smoke/build_validation_test.cc
    LABELS smoke critical fast
    TIMEOUT 30
  )
endif()

if(YAZE_TEST_UNIT)
  yaze_create_test_category(unit
    SOURCES ${UNIT_TEST_SOURCES}
    LABELS unit fast
    TIMEOUT 60
    PARALLEL_JOBS 4
  )
endif()

if(YAZE_TEST_INTEGRATION)
  yaze_create_test_category(integration
    SOURCES ${INTEGRATION_TEST_SOURCES}
    LABELS integration medium
    TIMEOUT 300
    REQUIRES_ROM
  )
endif()

# CTest configuration
set(CTEST_PARALLEL_LEVEL ${CMAKE_NUMBER_OF_CORES} CACHE STRING "CTest parallel level")
set(CTEST_OUTPUT_ON_FAILURE ON CACHE BOOL "CTest output on failure")
set(CTEST_PROGRESS_OUTPUT ON CACHE BOOL "CTest progress output")

# Custom test commands
add_custom_target(test-fast
  COMMAND ${CMAKE_CTEST_COMMAND} -L "fast" --parallel ${CTEST_PARALLEL_LEVEL}
  WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
  COMMENT "Running fast tests"
)

add_custom_target(test-all
  COMMAND ${CMAKE_CTEST_COMMAND} --parallel ${CTEST_PARALLEL_LEVEL}
  WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
  COMMENT "Running all tests"
)

add_custom_target(test-changed
  COMMAND ${CMAKE_COMMAND} -E env
    python3 ${CMAKE_SOURCE_DIR}/scripts/run_changed_tests.py
  WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
  COMMENT "Running tests for changed files"
)
```

### 6.2 CTest Presets Configuration

```json
// CTestPresets.json
{
  "version": 3,
  "configurePresets": [
    {
      "name": "test-base",
      "hidden": true,
      "cacheVariables": {
        "CTEST_PARALLEL_LEVEL": "${nproc}",
        "CTEST_OUTPUT_ON_FAILURE": "ON"
      }
    }
  ],
  "testPresets": [
    {
      "name": "smoke",
      "configurePreset": "test-base",
      "filter": {
        "include": {
          "label": "smoke"
        }
      },
      "execution": {
        "jobs": 4,
        "timeout": 60
      }
    },
    {
      "name": "unit",
      "configurePreset": "test-base",
      "filter": {
        "include": {
          "label": "unit"
        }
      },
      "execution": {
        "jobs": "${nproc}",
        "timeout": 120
      }
    },
    {
      "name": "integration",
      "configurePreset": "test-base",
      "filter": {
        "include": {
          "label": "integration"
        }
      },
      "execution": {
        "jobs": 2,
        "timeout": 600,
        "scheduleRandom": true
      }
    },
    {
      "name": "ci",
      "configurePreset": "test-base",
      "filter": {
        "exclude": {
          "label": "manual|flaky|slow"
        }
      },
      "execution": {
        "jobs": 4,
        "timeout": 300,
        "stopOnFailure": false
      },
      "output": {
        "outputOnFailure": true,
        "verbosity": "default",
        "outputJUnit": "test_results.xml"
      }
    },
    {
      "name": "coverage",
      "configurePreset": "test-base",
      "configuration": "Debug",
      "execution": {
        "jobs": "${nproc}"
      },
      "output": {
        "outputOnFailure": true
      },
      "coverage": {
        "enabled": true,
        "target": "coverage",
        "exclude": [
          "*/test/*",
          "*/ext/*",
          "*/build/*"
        ]
      }
    }
  ]
}
```

## 7. Implementation Roadmap

### Phase 1: Foundation (Weeks 1-2)
- [x] Implement precompiled headers for test utilities
- [x] Set up test binary caching in CI
- [x] Create basic parallel test runner script
- [x] Implement smoke test suite

**Deliverables:**
- 30% reduction in test build time
- Smoke tests running in < 2 minutes
- Test caching operational

### Phase 2: Parallelization (Weeks 3-4)
- [ ] Deploy automatic test sharding system
- [ ] Implement smart test selection
- [ ] Set up parallel execution in CI
- [ ] Create test result aggregation

**Deliverables:**
- 50% reduction in test execution time
- Parallel test execution across 4 shards
- Smart test selection based on changes

### Phase 3: CI Enhancement (Weeks 5-6)
- [ ] Implement multi-stage pipeline
- [ ] Deploy platform-specific test suites
- [ ] Set up test metrics collection
- [ ] Create flaky test detection

**Deliverables:**
- 3-stage CI pipeline operational
- Test metrics dashboard
- Flaky test quarantine system

### Phase 4: Developer Tools (Weeks 7-8)
- [ ] Deploy enhanced pre-push validation
- [ ] Create test generation tools
- [ ] Implement local test caching
- [ ] Set up coverage tracking

**Deliverables:**
- Pre-push validation < 1 minute
- Test generation CLI tool
- Local coverage reports

### Phase 5: Monitoring (Weeks 9-10)
- [ ] Deploy metrics collection system
- [ ] Create test dashboard
- [ ] Set up alerting system
- [ ] Implement trend analysis

**Deliverables:**
- Real-time test metrics dashboard
- Automated alerts for regressions
- Weekly test health reports

## 8. Cost/Benefit Analysis

### Benefits

| Improvement | Current State | Target State | Benefit |
|------------|--------------|--------------|---------|
| Test Build Time | 5 minutes | 2 minutes | 60% faster builds |
| Test Execution | 15 minutes | 6 minutes | 60% faster tests |
| CI Pipeline | 25 minutes | 10 minutes | 60% faster CI |
| Local Validation | 10 minutes | 2 minutes | 80% faster pre-push |
| Test Coverage | Unknown | Tracked | Quality visibility |
| Flaky Tests | Manual detection | Automatic | Reduced noise |

### Implementation Costs

| Component | Development Time | Maintenance | Risk |
|-----------|-----------------|-------------|------|
| Build Optimization | 2 weeks | Low | Low |
| Parallel Execution | 2 weeks | Medium | Medium |
| CI Pipeline | 2 weeks | Medium | Low |
| Developer Tools | 2 weeks | Low | Low |
| Monitoring | 2 weeks | Medium | Low |

### ROI Calculation

**Assumptions:**
- 10 developers
- 5 CI runs per developer per day
- 15 minutes saved per CI run
- $100/hour developer cost

**Daily Savings:**
- Time saved: 10 devs × 5 runs × 15 min = 1250 minutes = 20.8 hours
- Cost saved: 20.8 hours × $100 = $2,080/day

**Annual Savings:**
- $2,080/day × 250 work days = $520,000/year

**Implementation Cost:**
- 10 weeks × 40 hours × $100 = $40,000

**ROI:** 1,200% in first year

## 9. Success Metrics

### Primary Metrics
- **Test execution time**: < 10 minutes for full suite
- **CI pipeline time**: < 15 minutes end-to-end
- **Test reliability**: < 1% flaky test rate
- **Coverage**: > 80% code coverage

### Secondary Metrics
- **Developer satisfaction**: Survey score > 4/5
- **Test maintenance time**: < 10% of development time
- **CI cost**: < $1000/month for infrastructure
- **Incident response**: < 30 minutes to identify test failures

## 10. Risk Mitigation

### Technical Risks

| Risk | Probability | Impact | Mitigation |
|------|------------|--------|------------|
| Test sharding breaks dependencies | Medium | High | Implement dependency detection |
| Parallel execution race conditions | High | Medium | Use test isolation framework |
| Cache invalidation issues | Medium | Low | Implement cache verification |
| Platform-specific failures | High | Medium | Separate platform test suites |

### Process Risks

| Risk | Probability | Impact | Mitigation |
|------|------------|--------|------------|
| Developer adoption | Medium | High | Provide training and documentation |
| Maintenance overhead | Medium | Medium | Automate monitoring and alerts |
| Tool complexity | Low | Medium | Start with simple tools, iterate |

## Conclusion

This comprehensive test infrastructure improvement plan provides:

1. **60% reduction** in test execution time through parallelization and smart selection
2. **Multi-stage CI pipeline** reducing feedback time to < 2 minutes for critical issues
3. **Advanced monitoring** with automated metrics and dashboards
4. **Developer tools** for faster local validation and test generation
5. **$520,000 annual savings** with 1,200% ROI

The phased implementation approach ensures continuous delivery of value while minimizing disruption to ongoing development. Each phase provides measurable improvements that build toward a world-class test infrastructure supporting the yaze project's growth and quality goals.