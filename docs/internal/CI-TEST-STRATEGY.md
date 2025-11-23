# CI Test Strategy

## Overview

The yaze project uses a **tiered testing strategy** to balance CI speed with comprehensive coverage. This document explains the strategy, configuration, and how to add tests.

**Key Distinction:**
- **Default Tests** (PR/Push CI): Stable, fast, no external dependencies - ALWAYS run, MUST pass
- **Optional Tests** (Nightly CI): ROM-dependent, experimental, benchmarks - Run nightly, non-blocking

Tier breakdown:
- **Tier 1 (PR/Push CI)**: Fast feedback loop with stable tests only (~5-10 minutes total)
- **Tier 2 (Nightly CI)**: Full test suite including heavy/flaky/ROM tests (~30-60 minutes total)
- **Tier 3 (Configuration Matrix)**: Weekly cross-platform configuration validation

## Test Tiers

### Tier 1: PR/Push Tests (ci.yml)
**When:** Every PR and push to master/develop
**Duration:** 5-10 minutes per platform
**Coverage:**
- Stable tests (unit + integration that don't require ROM)
- Smoke tests for GUI framework validation (Linux only)
- Basic build validation across all platforms

**Test Labels:**
- `stable`: Core functionality tests with stable contracts
- Includes both unit and integration tests that are fast and reliable

### Tier 2: Nightly Tests (nightly.yml)
**When:** Nightly at 3 AM UTC (or manual trigger)
**Duration:** 30-60 minutes total
**Coverage:**
- ROM-dependent tests (with test ROM if available)
- Experimental AI tests (with Ollama integration)
- GUI E2E tests (full workflows with ImGuiTestEngine)
- Performance benchmarks
- Extended integration tests with all features enabled

**Test Labels:**
- `rom_dependent`: Tests requiring actual Zelda3 ROM
- `experimental`: AI and unstable feature tests
- `gui`: Full GUI automation tests
- `benchmark`: Performance regression tests

### Tier 3: Configuration Matrix (matrix-test.yml)
**When:** Nightly at 2 AM UTC (or manual trigger)
**Duration:** 20-30 minutes
**Coverage:**
- Different feature combinations (minimal, gRPC-only, full AI, etc.)
- Platform-specific configurations
- Build configuration validation

## CTest Label System

Tests are organized with labels for selective execution:

```cmake
# In test/CMakeLists.txt
yaze_add_test_suite(yaze_test_stable "stable" OFF ${STABLE_TEST_SOURCES})
yaze_add_test_suite(yaze_test_rom_dependent "rom_dependent" OFF ${ROM_DEPENDENT_SOURCES})
yaze_add_test_suite(yaze_test_gui "gui;experimental" ON ${GUI_TEST_SOURCES})
yaze_add_test_suite(yaze_test_experimental "experimental" OFF ${EXPERIMENTAL_SOURCES})
yaze_add_test_suite(yaze_test_benchmark "benchmark" OFF ${BENCHMARK_SOURCES})
```

## Running Tests Locally

### Run specific test categories:
```bash
# Stable tests only (what PR CI runs)
ctest -L stable --output-on-failure

# ROM-dependent tests
ctest -L rom_dependent --output-on-failure

# Experimental tests
ctest -L experimental --output-on-failure

# GUI tests headlessly
./build/bin/yaze_test_gui -nogui

# Benchmarks
./build/bin/yaze_test_benchmark
```

### Using test executables directly:
```bash
# Run stable test suite
./build/bin/yaze_test_stable

# Run with specific filter
./build/bin/yaze_test_stable --gtest_filter="*Overworld*"

# Run GUI smoke tests only
./build/bin/yaze_test_gui -nogui --gtest_filter="*Smoke*"
```

## Test Presets

CMakePresets.json defines test presets for different scenarios:

- `stable`: Run stable tests only (no ROM dependency)
- `unit`: Run unit tests only
- `integration`: Run integration tests only
- `stable-ai`: Stable tests with AI stack enabled
- `unit-ai`: Unit tests with AI stack enabled

Example usage:
```bash
# Configure with preset
cmake --preset ci-linux

# Run tests with preset
ctest --preset stable
```

## Adding New Tests

### For PR/Push CI (Tier 1 - Default):
Add to `STABLE_TEST_SOURCES` in `test/CMakeLists.txt`:
- **Requirements**: Must not require ROM files, must complete in < 30 seconds, stable behavior (no flakiness)
- **Examples**: Unit tests, basic integration tests, framework smoke tests
- **Location**: `test/unit/`, `test/integration/` (excluding subdirs below)
- **Labels assigned**: `stable`

### For Nightly CI (Tier 2 - Optional):
Add to appropriate test suite in `test/CMakeLists.txt`:

- `ROM_DEPENDENT_TEST_SOURCES` - Tests requiring ROM
  - Location: `test/e2e/rom_dependent/` or `test/integration/` (ROM-gated with `#ifdef`)
  - Labels: `rom_dependent`

- `GUI_TEST_SOURCES` / `EXPERIMENTAL_TEST_SOURCES` - Experimental features
  - Location: `test/integration/ai/` for AI tests
  - Labels: `experimental`

- `BENCHMARK_TEST_SOURCES` - Performance tests
  - Location: `test/benchmarks/`
  - Labels: `benchmark`

## CI Optimization Tips

### For Faster PR CI:
1. Keep tests in STABLE_TEST_SOURCES minimal
2. Use `continue-on-error: true` for non-critical tests
3. Leverage caching (CPM, sccache, build artifacts)
4. Run platform tests in parallel

### For Comprehensive Coverage:
1. Use nightly.yml for heavy tests
2. Schedule at low-traffic times
3. Upload artifacts for debugging failures
4. Use longer timeouts for integration tests

## Monitoring and Alerts

### PR/Push Failures:
- Block merging if stable tests fail
- Immediate feedback in PR comments
- Required status checks on protected branches

### Nightly Failures:
- Summary report in GitHub Actions
- Optional Slack/email notifications for failures
- Artifacts retained for 30 days for debugging
- Non-blocking for development

## Future Improvements

1. **Test Result Trends**: Track test success rates over time
2. **Flaky Test Detection**: Automatically identify and quarantine flaky tests
3. **Performance Tracking**: Graph benchmark results over commits
4. **ROM Test Infrastructure**: Secure storage/retrieval of test ROM
5. **Parallel Test Execution**: Split test suites across multiple runners