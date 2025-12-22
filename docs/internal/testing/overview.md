# CI/CD and Testing Infrastructure

This document describes YAZE's continuous integration and testing systems, including how to understand and manage test suites.

## Table of Contents

1. [CI/CD Pipeline Overview](#cicd-pipeline-overview)
2. [Test Structure](#test-structure)
3. [GitHub Workflows](#github-workflows)
4. [Test Execution](#test-execution)
5. [Adding Tests](#adding-tests)
6. [Maintenance & Troubleshooting](#maintenance--troubleshooting)

## CI/CD Pipeline Overview

YAZE uses GitHub Actions with a **tiered testing strategy** for continuous integration:

### PR/Push CI (`ci.yml`) - Fast Feedback Loop
- **Trigger**: Every PR and push to master/develop
- **Duration**: ~5-10 minutes per platform
- **Tests**: Stable suite + GUI smoke tests (ONLY)
- **Result**: Must pass before merging

### Nightly CI (`nightly.yml`) - Comprehensive Coverage
- **Trigger**: Daily at 3 AM UTC or manual dispatch
- **Duration**: ~30-60 minutes total
- **Tests**: All suites including ROM-dependent, experimental, benchmarks
- **Result**: Alerts on failure but non-blocking

### Build Targets

- **Debug/AI/Dev presets**: Always include test targets
- **Release presets**: No test targets (focused on distribution)

## Test Structure

### Default (Stable) Tests - Run in PR/Push CI

These tests are always available and ALWAYS run in PR/Push CI (blocking merges):

**Characteristics:**
- No external dependencies (no ROM files required)
- Fast execution (~5 seconds total for stable)
- Safe to run in any environment
- Must pass in all PR/Push builds
- Included in all debug/dev/AI presets

**Included:**
- **Unit tests**: Core, ROM, graphics, Zelda3 functionality (21 test files)
- **Integration tests**: Editor, ASAR, dungeon system (10 test files)
- **GUI smoke tests**: Framework validation, editor basics (3 test files)

**Run with:**
```bash
ctest --test-dir build -L stable              # All stable tests
ctest --test-dir build -L "stable|gui"        # Stable + GUI
ctest --test-dir build -L headless_gui        # GUI in headless mode (CI)
```

### Optional Test Suites - Run in Nightly CI Only

These tests are disabled in PR/Push CI but run in nightly builds for comprehensive coverage.

#### ROM-Dependent Tests

**Purpose:** Full ROM editing workflows, version upgrades, data integrity

**CI Execution:** Nightly only (non-blocking)

**Requirements to Run Locally:**
- CMake flag: `-DYAZE_ENABLE_ROM_TESTS=ON`
- ROM path: `-DYAZE_TEST_ROM_PATH=/path/to/zelda3.sfc`
- Use `mac-dev`, `lin-dev`, `win-dev` presets or configure manually

**Contents:**
- ASAR ROM patching (`integration/asar_rom_test.cc`)
- Complete ROM workflows (`e2e/rom_dependent/e2e_rom_test.cc`)
- ZSCustomOverworld upgrades (`e2e/zscustomoverworld/zscustomoverworld_upgrade_test.cc`)

**Run with:**
```bash
cmake --preset mac-dev -DYAZE_TEST_ROM_PATH=~/zelda3.sfc
ctest --test-dir build -L rom_dependent
```

#### Experimental AI Tests

**Purpose:** AI runtime features, vision models, agent automation

**CI Execution:** Nightly only (non-blocking)

**Requirements to Run Locally:**
- CMake flag: `-DYAZE_ENABLE_AI_RUNTIME=ON`
- Use `mac-ai`, `lin-ai`, `win-ai` presets

**Contents:**
- AI tile placement (`integration/ai/test_ai_tile_placement.cc`)
- Vision model integration (`integration/ai/test_gemini_vision.cc`)
- GUI controller tests (`integration/ai/ai_gui_controller_test.cc`)

**Run with:**
```bash
cmake --preset mac-ai
ctest --test-dir build -L experimental
```

#### Benchmark Tests

**Purpose:** Performance profiling and optimization validation

**CI Execution:** Nightly only (non-blocking)

**Contents:**
- Graphics optimization benchmarks (`benchmarks/gfx_optimization_benchmarks.cc`)

**Run with:**
```bash
ctest --test-dir build -L benchmark
```

## GitHub Workflows

### Primary Workflows

#### 1. CI Pipeline (`ci.yml`)

**Trigger:** Push to master/develop, pull requests, manual dispatch

**Matrix:**
```
- Ubuntu 22.04 (GCC-12)
- macOS 14 (Clang)
- Windows 2022 (Visual Studio)
```

**Jobs:**
```
build
├── Checkout
├── Setup build environment
├── Build project
└── Upload artifacts (Windows)

test
├── Checkout
├── Setup build environment
├── Build project
├── Run stable tests
├── Run GUI smoke tests
├── Run ROM tests (if available)
└── Artifacts upload (on failure)
```

**Test Execution in CI:**

```yaml
# Default: stable tests always run
- name: Run stable tests
  uses: ./.github/actions/run-tests
  with:
    test-type: stable

# Always: GUI headless smoke tests
- name: Run GUI smoke tests (headless)
  uses: ./.github/actions/run-tests
  with:
    test-type: gui-headless

# Conditional: ROM tests on develop branch
- name: Run ROM-dependent tests
  if: github.ref == 'refs/heads/develop'
  uses: ./.github/actions/run-tests
  with:
    test-type: rom
```

#### 2. Code Quality (`code-quality.yml`)

**Trigger:** Push to master/develop, pull requests

**Checks:**
- `clang-format`: Code formatting validation
- `cppcheck`: Static analysis
- `clang-tidy`: Linting and best practices

#### 3. Release Pipeline (`release.yml`)

**Trigger:** Manual dispatch or tag push

**Outputs:**
- Cross-platform binaries
- Installer packages (Windows)
- Disk images (macOS)

#### 4. Matrix Test Pipeline (`matrix-test.yml`)

**Purpose:** Extended testing on multiple compiler versions

**Configuration:**
- GCC 12, 13 (Linux)
- Clang 14, 15, 16 (macOS, Linux)
- MSVC 193 (Windows)

### Composite Actions

Located in `.github/actions/`:

#### `setup-build`
Prepares build environment with:
- Dependency caching (CPM)
- Compiler cache (sccache/ccache)
- Platform-specific tools

#### `build-project`
Builds with:
- CMake preset configuration
- Optimal compiler settings
- Build artifact staging

#### `run-tests`
Executes tests with:
- CTest label filtering
- Test result uploads
- Failure artifact collection

## Test Execution

### Local Test Runs

#### Stable Tests (Recommended for Development)

```bash
# Fast iteration
ctest --test-dir build -L stable -j4

# With output on failure
ctest --test-dir build -L stable --output-on-failure

# With GUI tests
ctest --test-dir build -L "stable|gui" -j4
```

#### ROM-Dependent Tests

```bash
# Configure with ROM
cmake --preset mac-dbg \
  -DYAZE_ENABLE_ROM_TESTS=ON \
  -DYAZE_TEST_ROM_PATH=~/zelda3.sfc

# Build ROM test suite
cmake --build --preset mac-dbg --target yaze_test_rom_dependent

# Run ROM tests
ctest --test-dir build -L rom_dependent -v
```

#### All Available Tests

```bash
# Runs all enabled test suites
ctest --test-dir build --output-on-failure
```

### Test Organization by Label

Tests are organized with ctest labels for flexible filtering:

```
Labels:
  stable               → Core unit/integration tests (default)
  gui                  → GUI smoke tests
  experimental         → AI runtime features
  rom_dependent        → Zelda3 ROM workflows
  benchmark            → Performance tests
  headless_gui         → GUI tests in headless mode
```

**Usage:**

```bash
ctest --test-dir build -L stable           # Single label
ctest --test-dir build -L "stable|gui"     # Multiple labels (OR)
ctest --test-dir build -L "^stable$"       # Exact match
ctest --test-dir build -L "^(?!benchmark)" # Exclude benchmarks
```

### CTest vs Gtest Filtering

Both approaches work, but differ in flexibility:

```bash
# CTest approach (recommended - uses CMake labels)
ctest --test-dir build -L stable
ctest --test-dir build -R "Dungeon"

# Gtest approach (direct binary execution)
./build/bin/yaze_test_stable --gtest_filter="*Dungeon*"
./build/bin/yaze_test_stable --show-gui
```

## Adding Tests

### File Organization Rules

```
test/
├── unit/               → Fast, no ROM dependency
├── integration/        → Component integration
├── e2e/                → End-to-end workflows
├── benchmarks/         → Performance tests
└── integration/ai/     → AI-specific (requires AI runtime)
```

### Adding Unit Test

1. Create file: `test/unit/new_feature_test.cc`
2. Include headers and use `gtest_add_tests()`
3. File auto-discovered by CMakeLists.txt
4. Automatically labeled as `stable`

```cpp
#include <gtest/gtest.h>
#include "app/new_feature.h"

TEST(NewFeatureTest, BasicFunctionality) {
  EXPECT_TRUE(NewFeature::Work());
}
```

### Adding Integration Test

1. Create file: `test/integration/new_feature_test.cc`
2. Same pattern as unit tests
3. May access ROM files via `YAZE_TEST_ROM_PATH`
4. Automatically labeled as `stable` (unless in special subdirectory)

### Adding ROM-Dependent Test

1. Create file: `test/e2e/rom_dependent/my_rom_test.cc`
2. Wrap ROM access in `#ifdef YAZE_ENABLE_ROM_TESTS`
3. Access ROM path via environment variable or CMake define
4. Automatically labeled as `rom_dependent`

```cpp
#ifdef YAZE_ENABLE_ROM_TESTS
TEST(MyRomTest, EditAndSave) {
  const char* rom_path = YAZE_TEST_ROM_PATH;
  // ... ROM testing code
}
#endif
```

### Adding AI/Experimental Test

1. Create file: `test/integration/ai/my_ai_test.cc`
2. Wrap code in `#ifdef YAZE_ENABLE_AI_RUNTIME`
3. Only included when `-DYAZE_ENABLE_AI_RUNTIME=ON`
4. Automatically labeled as `experimental`

### Adding GUI Test

1. Create file: `test/e2e/my_gui_test.cc`
2. Use ImGui Test Engine API
3. Register test in `test/yaze_test.cc`
4. Automatically labeled as `gui;experimental`

```cpp
#include "test_utils.h"
#include "imgui_te_engine.h"

void E2ETest_MyGuiWorkflow(ImGuiTestContext* ctx) {
  yaze::test::gui::LoadRomInTest(ctx, "zelda3.sfc");
  // ... GUI test code
}

// In yaze_test.cc RunGuiMode():
ImGuiTest* my_test = IM_REGISTER_TEST(engine, "E2ETest", "MyGuiWorkflow");
my_test->TestFunc = E2ETest_MyGuiWorkflow;
```

## CMakeLists.txt Test Configuration

The test configuration in `test/CMakeLists.txt` follows this pattern:

```cmake
if(YAZE_BUILD_TESTS)
  # Define test suites with labels
  yaze_add_test_suite(yaze_test_stable "stable" OFF ${STABLE_SOURCES})

  if(YAZE_ENABLE_ROM_TESTS)
    yaze_add_test_suite(yaze_test_rom_dependent "rom_dependent" OFF ${ROM_SOURCES})
  endif()

  yaze_add_test_suite(yaze_test_gui "gui;experimental" ON ${GUI_SOURCES})

  if(YAZE_ENABLE_AI_RUNTIME)
    yaze_add_test_suite(yaze_test_experimental "experimental" OFF ${AI_SOURCES})
  endif()

  yaze_add_test_suite(yaze_test_benchmark "benchmark" OFF ${BENCH_SOURCES})
endif()
```

**Key function:** `yaze_add_test_suite(name label is_gui_test sources...)`
- Creates executable
- Links test dependencies
- Discovers tests with gtest_discover_tests()
- Assigns ctest label

## Maintenance & Troubleshooting

### Test Flakiness

If tests intermittently fail:

1. Check for race conditions in parallel execution
2. Look for timing-dependent operations
3. Verify test isolation (no shared state)
4. Check for environment-dependent behavior

**Fix strategies:**
- Use `ctest -j1` to disable parallelization
- Add explicit synchronization points
- Use test fixtures for setup/teardown

### ROM Test Failures

If ROM tests fail:

```bash
# Verify ROM path is correct
echo $YAZE_TEST_ROM_PATH
file ~/zelda3.sfc

# Check ROM-dependent tests are enabled
cmake . | grep YAZE_ENABLE_ROM_TESTS

# Rebuild ROM test suite
cmake --build . --target yaze_test_rom_dependent
ctest --test-dir build -L rom_dependent -vv
```

### GUI Test Failures

If GUI tests crash:

```bash
# Check display available
echo $DISPLAY  # Linux/macOS

# Run headlessly
ctest --test-dir build -L headless_gui -vv

# Check test registration
grep -r "IM_REGISTER_TEST" test/e2e/
```

### Test Not Discovered

If new tests aren't found:

```bash
# Rebuild CMake
rm -rf build && cmake --preset mac-dbg

# Check file is included in CMakeLists.txt
grep "my_feature_test.cc" test/CMakeLists.txt

# Verify test definitions
ctest --test-dir build -N  # List all tests
```

### Performance Degradation

If tests run slowly:

```bash
# Run with timing
ctest --test-dir build -T performance

# Identify slow tests
ctest --test-dir build -T performance | grep "Wall Time"

# Profile specific test
time ./build/bin/yaze_test_stable "*SlowTest*"
```

## References

- **Test Documentation**: `test/README.md`
- **Quick Build Reference**: `docs/public/build/quick-reference.md`
- **CI Workflows**: `.github/workflows/ci.yml`, `matrix-test.yml`
- **Test Utilities**: `test/test_utils.h`
- **ImGui Test Engine**: `ext/imgui_test_engine/imgui_te_engine.h`
- **CMake Test Configuration**: `test/CMakeLists.txt`
