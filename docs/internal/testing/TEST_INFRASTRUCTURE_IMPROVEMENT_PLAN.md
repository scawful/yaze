# YAZE Testing Infrastructure Improvement Plan

**Version:** 1.0
**Date:** 2025-11-21
**Author:** Claude (Test Infrastructure Expert)

## Executive Summary

This document outlines a comprehensive plan to expand and improve YAZE's testing infrastructure. The plan focuses on five key areas: ImGui Test Engine expansion, feature validation automation, test organization improvements, CI/CD integration enhancements, and testing best practices documentation.

**Current State:**
- ~3,500 lines of test code across unit, integration, e2e, and benchmark tests
- Basic ImGui Test Engine integration with 3 e2e tests
- Manual test execution with category filtering
- Platform-specific CI/CD workflows with stable/unit test runs
- Limited test coverage for editor workflows and graphics system

**Goals:**
- Expand e2e test coverage from 3 to 50+ tests covering all major editor workflows
- Implement automated feature validation preventing regressions
- Reduce test execution time by 40% through parallel execution and smart caching
- Achieve 80%+ coverage for critical paths (ROM ops, graphics, editors)
- Enable AI agents to write and maintain tests effectively

---

## Table of Contents

1. [ImGui Test Engine Expansion](#1-imgui-test-engine-expansion)
2. [Feature Validation Framework](#2-feature-validation-framework)
3. [Test Organization Improvements](#3-test-organization-improvements)
4. [CI/CD Integration Enhancements](#4-cicd-integration-enhancements)
5. [Testing Best Practices Documentation](#5-testing-best-practices-documentation)
6. [Implementation Roadmap](#6-implementation-roadmap)
7. [Appendix: Code Examples](#7-appendix-code-examples)

---

## 1. ImGui Test Engine Expansion

### 1.1 Current State Analysis

**Existing E2E Tests:**
- `framework_smoke_test.cc` - Basic ImGui Test Engine validation
- `dungeon_editor_smoke_test.cc` - DungeonEditorV2 card-based UI
- `canvas_selection_test.cc` - Overworld canvas copy/paste validation

**Coverage Gaps:**
- No systematic dungeon editor object placement tests
- Missing overworld entity manipulation workflows
- No graphics editor validation
- Limited keyboard shortcut testing
- No multi-card workflow tests
- Missing error state validation

### 1.2 Proposed E2E Test Suites

#### 1.2.1 Dungeon Editor Test Suite (P0)
**Priority:** P0 - Critical for dungeon editing stability

**Test Cases:**
1. **Object Placement and Selection**
   - Place object via click on palette → verify canvas update
   - Select object on canvas → verify properties panel
   - Multi-select objects → verify selection bounds
   - Delete selected objects → verify removal from ROM

2. **Keyboard Shortcuts**
   - Ctrl+Z undo object placement
   - Ctrl+Y redo object placement
   - Ctrl+C/V copy/paste objects
   - Arrow keys for object nudging

3. **Room Navigation**
   - Navigate between rooms via matrix
   - Open multiple room cards simultaneously
   - Close room cards and verify state cleanup
   - Switch between rooms and verify context switching

4. **Layer Management**
   - Toggle BG1/BG2/BG3 visibility per room
   - Verify layer rendering updates
   - Test layer ordering correctness

5. **Object Properties**
   - Modify object type via dropdown
   - Change object size via spinbox
   - Update object position via properties panel
   - Verify ROM writes for property changes

**Implementation File:** `test/e2e/dungeon_editor_object_workflow_test.cc`

#### 1.2.2 Overworld Editor Test Suite (P0)
**Priority:** P0 - Critical for overworld editing stability

**Test Cases:**
1. **Map Editing Workflows**
   - Switch between maps (Light/Dark World)
   - Edit tile16 data on canvas
   - Undo/redo tile edits
   - Verify ROM persistence

2. **Entity Manipulation**
   - Add entrance via context menu
   - Drag entrance to new position
   - Edit entrance properties
   - Delete entrance and verify removal
   - Test for all entity types (exits, items, sprites)

3. **Multi-Area Maps**
   - Configure map as multi-area (2x2, 3x3)
   - Verify parent ID assignment
   - Edit across area boundaries
   - Test area size changes

4. **Tile16 Editor**
   - Open Tile16 editor card
   - Edit Tile16 composition
   - Verify graphics sheet updates propagate
   - Test collision attribute editing

5. **Canvas Selection**
   - Rectangle selection across 512px boundaries (regression test)
   - Copy/paste tile selections
   - Verify pasted data correctness
   - Test selection wrapping edge cases

**Implementation File:** `test/e2e/overworld_editor_workflow_test.cc`

#### 1.2.3 Graphics Editor Test Suite (P1)
**Priority:** P1 - Important for graphics editing workflows

**Test Cases:**
1. **Graphics Sheet Management**
   - Load graphics sheet from ROM
   - Modify sheet pixels via palette
   - Notify Arena of sheet modification
   - Verify propagation to all editors

2. **Palette Editing**
   - Modify SNES color values
   - Verify color preview updates
   - Save palette to ROM
   - Test palette group switching

3. **Sprite Graphics**
   - Edit sprite graphics sheets
   - Verify sprite preview updates
   - Test OAM composition changes

**Implementation File:** `test/e2e/graphics_editor_workflow_test.cc`

#### 1.2.4 Cross-Editor Integration Tests (P1)
**Priority:** P1 - Validates editor coordination

**Test Cases:**
1. **Graphics Sheet Propagation**
   - Modify sheet in Graphics Editor
   - Verify update in Dungeon Editor
   - Verify update in Overworld Editor

2. **Palette Changes**
   - Change palette in Palette Editor
   - Verify updates in all active editors
   - Test palette group inheritance

3. **ROM State Consistency**
   - Make edits in multiple editors
   - Save ROM
   - Reload ROM
   - Verify all edits persisted correctly

**Implementation File:** `test/e2e/cross_editor_integration_test.cc`

### 1.3 Test Helper Framework (P0)

**New Test Utilities:**

```cpp
// test/e2e/test_helpers.h
namespace yaze {
namespace test {
namespace e2e {

// Canvas interaction helpers
void ClickCanvasAt(ImGuiTestContext* ctx, const std::string& canvas_id,
                   ImVec2 world_pos);
void DragCanvasSelection(ImGuiTestContext* ctx, const std::string& canvas_id,
                         ImVec2 start, ImVec2 end);
void VerifyCanvasTile(ImGuiTestContext* ctx, int x, int y, uint16_t expected_tile);

// Entity manipulation helpers
void CreateEntityOnCanvas(ImGuiTestContext* ctx, const std::string& entity_type,
                          ImVec2 position);
void SelectEntity(ImGuiTestContext* ctx, int entity_id);
void VerifyEntityProperties(ImGuiTestContext* ctx, int entity_id,
                            const std::map<std::string, std::string>& props);

// Editor state helpers
void OpenEditor(ImGuiTestContext* ctx, EditorType editor_type);
void CloseEditor(ImGuiTestContext* ctx, EditorType editor_type);
void VerifyEditorActive(ImGuiTestContext* ctx, EditorType editor_type);

// Keyboard shortcut helpers
void SimulateShortcut(ImGuiTestContext* ctx, ImGuiKey modifier, ImGuiKey key);
void SimulateUndoRedo(ImGuiTestContext* ctx, bool is_redo = false);

// ROM state validation
void VerifyRomByteEquals(ImGuiTestContext* ctx, Rom* rom,
                         uint32_t address, uint8_t expected);
void VerifyRomBytesEqual(ImGuiTestContext* ctx, Rom* rom,
                         uint32_t address, const std::vector<uint8_t>& expected);

// Mock ROM generation (deterministic test data)
std::unique_ptr<Rom> CreateMockRomForTesting(const std::string& variant = "default");

}  // namespace e2e
}  // namespace test
}  // namespace yaze
```

### 1.4 Mock ROM Data Generation (P0)

**Purpose:** Enable consistent, repeatable tests without requiring real ROM files for all tests.

**Implementation Strategy:**
- Generate minimal valid SNES ROM structure
- Populate with known test data patterns
- Support variants for different test scenarios
- Include checksums and headers for validity

**Mock ROM Variants:**
1. **default** - Basic LoROM with minimal overworld/dungeon data
2. **zscustom_v3** - ZSCustomOverworld v3 features enabled
3. **corrupted** - Invalid data for error handling tests
4. **large** - Large ROM (2MB+) for stress testing
5. **minimal** - Smallest valid ROM for fast unit tests

**Implementation File:** `test/test_utils/mock_rom_generator.cc`

### 1.5 Visual Regression Testing (P2)

**Purpose:** Detect unintended visual changes in UI rendering and graphics output.

**Strategy:**
- Capture screenshots during e2e tests
- Compare against golden images
- Generate diff images for failures
- Store golden images in `test/golden/`

**Implementation:**
```cpp
// test/e2e/visual_regression_test.cc
TEST_F(VisualRegressionTest, DungeonRoomRendering) {
  LoadRomInTest(ctx, "zelda3.sfc");
  OpenEditorInTest(ctx, "Dungeon");

  // Navigate to specific room
  OpenRoomCard(ctx, 0x00);

  // Capture screenshot
  auto screenshot = CaptureCanvasScreenshot(ctx, "Room 0x00##Canvas");

  // Compare against golden image
  auto golden = LoadGoldenImage("dungeon_room_0x00.png");
  EXPECT_TRUE(CompareImages(screenshot, golden, 0.99f /* similarity threshold */));
}
```

### 1.6 Estimated Effort

| Task | Priority | Effort (Days) | Dependencies |
|------|----------|---------------|--------------|
| E2E Test Helpers | P0 | 3 | None |
| Mock ROM Generator | P0 | 2 | None |
| Dungeon Editor Tests | P0 | 5 | Helpers, Mock ROM |
| Overworld Editor Tests | P0 | 5 | Helpers, Mock ROM |
| Graphics Editor Tests | P1 | 3 | Helpers, Mock ROM |
| Cross-Editor Tests | P1 | 2 | All editor tests |
| Visual Regression | P2 | 4 | E2E infrastructure |

**Total Estimated Effort:** 24 developer-days

---

## 2. Feature Validation Framework

### 2.1 Objectives

- **Prevent Regressions:** Automatically detect when features break
- **Pre-Commit Validation:** Catch issues before they reach CI
- **Performance Monitoring:** Track performance regressions
- **ROM Integrity:** Ensure edits don't corrupt ROM data

### 2.2 Pre-Commit Hook System (P0)

**Implementation:** Git pre-commit hook that runs relevant tests based on changed files.

```bash
#!/bin/bash
# .git/hooks/pre-commit

# Get list of changed files
CHANGED_FILES=$(git diff --cached --name-only --diff-filter=ACM)

# Determine which tests to run
RUN_UNIT=false
RUN_INTEGRATION=false
RUN_E2E=false

for file in $CHANGED_FILES; do
  case "$file" in
    src/app/rom.*|src/app/gfx/*)
      RUN_UNIT=true
      RUN_INTEGRATION=true
      ;;
    src/app/editor/dungeon/*)
      RUN_UNIT=true
      RUN_E2E=true
      ;;
    src/app/editor/overworld/*)
      RUN_UNIT=true
      RUN_E2E=true
      ;;
    src/app/editor/graphics/*)
      RUN_INTEGRATION=true
      ;;
    test/*)
      # Always run tests if test files changed
      RUN_UNIT=true
      ;;
  esac
done

# Run selected test suites
if [ "$RUN_UNIT" = true ]; then
  echo "Running unit tests..."
  ./build/bin/yaze_test_stable --unit || exit 1
fi

if [ "$RUN_INTEGRATION" = true ]; then
  echo "Running integration tests..."
  ./build/bin/yaze_test_stable --integration || exit 1
fi

if [ "$RUN_E2E" = true ]; then
  echo "Running critical e2e tests..."
  ./build/bin/yaze_test_gui "*Smoke*" || exit 1
fi

echo "Pre-commit checks passed!"
```

**Installation Script:**
```bash
# scripts/install-git-hooks.sh
#!/bin/bash
cp scripts/pre-commit-hook .git/hooks/pre-commit
chmod +x .git/hooks/pre-commit
echo "Git hooks installed successfully"
```

### 2.3 ROM Integrity Validation (P0)

**Purpose:** Verify that edits maintain ROM validity and don't corrupt data.

**Validation Checks:**
1. **Checksum Validation** - SNES header checksum matches data
2. **Pointer Integrity** - All pointers remain valid after edits
3. **Boundary Checks** - Edits don't overflow data regions
4. **Compression Validity** - Compressed data can be decompressed
5. **Graphics Integrity** - Graphics sheets remain valid

**Implementation:**

```cpp
// test/integration/rom_integrity_test.cc
class RomIntegrityValidator {
public:
  struct ValidationResult {
    bool valid = true;
    std::vector<std::string> errors;
    std::vector<std::string> warnings;
  };

  ValidationResult ValidateRomIntegrity(Rom* rom) {
    ValidationResult result;

    // Check SNES header checksum
    if (!ValidateChecksum(rom)) {
      result.valid = false;
      result.errors.push_back("SNES header checksum mismatch");
    }

    // Validate graphics data pointers
    if (!ValidateGraphicsPointers(rom)) {
      result.valid = false;
      result.errors.push_back("Invalid graphics data pointers");
    }

    // Check compression integrity for all compressed data
    if (!ValidateCompression(rom)) {
      result.valid = false;
      result.errors.push_back("Compressed data corruption detected");
    }

    // Validate overworld map structure
    if (!ValidateOverworldStructure(rom)) {
      result.valid = false;
      result.errors.push_back("Overworld map structure corrupted");
    }

    // Validate dungeon room data
    if (!ValidateDungeonStructure(rom)) {
      result.valid = false;
      result.errors.push_back("Dungeon room data corrupted");
    }

    return result;
  }

private:
  bool ValidateChecksum(Rom* rom);
  bool ValidateGraphicsPointers(Rom* rom);
  bool ValidateCompression(Rom* rom);
  bool ValidateOverworldStructure(Rom* rom);
  bool ValidateDungeonStructure(Rom* rom);
};

TEST_F(RomIntegrityTest, ValidateAfterDungeonEdit) {
  // Load ROM
  Rom rom;
  ASSERT_TRUE(rom.LoadFromFile("zelda3.sfc").ok());

  // Make a dungeon edit
  auto dungeon = rom.GetDungeon(0x00);
  dungeon->EditObject(5, 10, 0x42); // Place object at (5, 10)

  // Validate integrity
  RomIntegrityValidator validator;
  auto result = validator.ValidateRomIntegrity(&rom);

  EXPECT_TRUE(result.valid) << "ROM integrity check failed after edit";
  for (const auto& error : result.errors) {
    ADD_FAILURE() << "Integrity error: " << error;
  }
}
```

### 2.4 Performance Regression Detection (P1)

**Purpose:** Detect when code changes significantly degrade performance.

**Strategy:**
- Benchmark critical paths in dedicated benchmark tests
- Store baseline performance metrics
- Alert when performance degrades beyond threshold (e.g., 10%)
- Generate performance reports in CI

**Implementation:**

```cpp
// test/benchmarks/performance_baselines.h
namespace yaze {
namespace benchmark {

struct PerformanceBaseline {
  std::string operation;
  double baseline_ms;
  double threshold_pct; // Maximum acceptable degradation %
};

const std::vector<PerformanceBaseline> kCriticalPathBaselines = {
  {"Rom::LoadFromFile", 150.0, 20.0},
  {"Arena::LoadGraphicsSheets", 200.0, 15.0},
  {"Overworld::LoadMap", 50.0, 20.0},
  {"Dungeon::LoadRoom", 30.0, 20.0},
  {"Bitmap::ApplyPalette", 10.0, 25.0},
  {"Canvas::RenderFrame", 16.67, 10.0}, // 60fps target
};

}  // namespace benchmark
}  // namespace yaze
```

```cpp
// test/benchmarks/performance_regression_test.cc
TEST_F(PerformanceBenchmarkTest, RomLoadingPerformance) {
  const auto& baseline = GetBaseline("Rom::LoadFromFile");

  auto start = std::chrono::high_resolution_clock::now();

  Rom rom;
  ASSERT_TRUE(rom.LoadFromFile("zelda3.sfc").ok());

  auto end = std::chrono::high_resolution_clock::now();
  double elapsed_ms = std::chrono::duration<double, std::milli>(end - start).count();

  // Check against baseline
  double degradation_pct = ((elapsed_ms - baseline.baseline_ms) / baseline.baseline_ms) * 100.0;

  EXPECT_LE(degradation_pct, baseline.threshold_pct)
    << "Performance regression detected: "
    << "Baseline: " << baseline.baseline_ms << "ms, "
    << "Current: " << elapsed_ms << "ms, "
    << "Degradation: " << degradation_pct << "%";

  // Log performance for trending
  RecordPerformanceMetric("Rom::LoadFromFile", elapsed_ms);
}
```

### 2.5 Cross-Platform Test Matrix (P1)

**Purpose:** Ensure features work correctly on Linux, macOS (Intel/ARM64), and Windows.

**Strategy:**
- Define platform-specific test variants
- Run full test suite on all platforms in CI
- Use platform-specific golden data where needed
- Test platform-specific code paths

**Platform-Specific Concerns:**

| Platform | Specific Tests Needed |
|----------|----------------------|
| Windows | Path length limits, backslash handling, MSVC-specific behavior |
| macOS ARM64 | Endianness, ARM-specific optimizations, Metal rendering |
| macOS Intel | x86 optimizations, OpenGL rendering |
| Linux | GTK integration, various distro compatibility |

**Implementation File:** `test/integration/platform_compatibility_test.cc`

### 2.6 Estimated Effort

| Task | Priority | Effort (Days) | Dependencies |
|------|----------|---------------|--------------|
| Pre-Commit Hooks | P0 | 1 | None |
| ROM Integrity Validator | P0 | 4 | ROM test fixtures |
| Performance Baselines | P1 | 3 | Benchmark framework |
| Performance Regression Tests | P1 | 3 | Baselines |
| Platform Matrix Tests | P1 | 2 | Existing integration tests |

**Total Estimated Effort:** 13 developer-days

---

## 3. Test Organization Improvements

### 3.1 Current Organization Issues

**Problems:**
- Test categorization not consistently applied
- No smoke/regression/comprehensive distinction
- Test data scattered across test files
- No centralized golden file management
- Limited test fixture reuse
- Flaky test detection is manual

### 3.2 Enhanced Test Categorization (P0)

**New Test Labels:**

```cmake
# test/CMakeLists.txt - Enhanced labeling

# Smoke tests - Critical path validation (< 30 seconds total)
set_tests_properties(
  RomLoadTest
  DungeonEditorInitTest
  OverworldEditorInitTest
  PROPERTIES LABELS "smoke;critical"
)

# Regression tests - Tests for specific bug fixes
set_tests_properties(
  CanvasSelectionWrapBugTest  # Regression for #1234
  MultiAreaMapParentIDTest    # Regression for #5678
  PROPERTIES LABELS "regression"
)

# Comprehensive tests - Full feature validation
set_tests_properties(
  FullOverworldEditWorkflow
  CompleteDungeonEditWorkflow
  PROPERTIES LABELS "comprehensive"
)

# Performance tests
set_tests_properties(
  RomLoadingBenchmark
  GraphicsRenderingBenchmark
  PROPERTIES LABELS "performance;benchmark"
)

# Platform-specific tests
set_tests_properties(
  WindowsPathLengthTest
  PROPERTIES LABELS "platform;windows"
)
```

**Test Execution Modes:**

```bash
# Run smoke tests only (pre-commit)
./build/bin/yaze_test_stable --gtest_filter="*Smoke*"

# Run regression tests
ctest -L regression

# Run comprehensive suite (nightly)
ctest -L comprehensive

# Run performance benchmarks
ctest -L benchmark

# Run platform-specific tests
ctest -L "platform;windows"
```

### 3.3 Test Data Management (P0)

**Structure:**

```
test/
├── data/
│   ├── roms/
│   │   ├── mock_minimal.sfc      # Minimal mock ROM (256KB)
│   │   ├── mock_zscustom_v3.sfc  # ZSCustomOverworld v3 mock
│   │   └── README.md             # Documentation
│   ├── golden/
│   │   ├── images/
│   │   │   ├── dungeon_room_0x00.png
│   │   │   ├── overworld_map_0x00.png
│   │   │   └── ...
│   │   ├── rom_dumps/
│   │   │   ├── dungeon_room_0x00.bin
│   │   │   └── ...
│   │   └── checksums.json
│   └── fixtures/
│       ├── overworld_maps/
│       ├── dungeon_rooms/
│       └── graphics_sheets/
└── test_utils/
    ├── mock_rom_generator.h
    ├── golden_file_manager.h
    └── test_fixture_loader.h
```

**Golden File Manager:**

```cpp
// test/test_utils/golden_file_manager.h
namespace yaze {
namespace test {

class GoldenFileManager {
public:
  static std::filesystem::path GetGoldenImagePath(const std::string& name);
  static std::filesystem::path GetGoldenRomDumpPath(const std::string& name);

  // Compare with automatic update on failure (for regenerating golden files)
  static bool CompareWithGolden(const std::vector<uint8_t>& data,
                                const std::string& golden_name,
                                bool update_on_mismatch = false);

  static gfx::Bitmap LoadGoldenImage(const std::string& name);
  static std::vector<uint8_t> LoadGoldenRomDump(const std::string& name);

  static void UpdateGoldenFile(const std::string& name,
                               const std::vector<uint8_t>& data);
};

}  // namespace test
}  // namespace yaze
```

### 3.4 Test Fixture Reuse (P1)

**Common Test Fixtures:**

```cpp
// test/test_utils/common_test_fixtures.h
namespace yaze {
namespace test {

// Base fixture for tests requiring ROM
class RomTestFixture : public ::testing::Test {
protected:
  void SetUp() override;
  void TearDown() override;

  Rom* rom() { return rom_.get(); }

  // Helper to load specific ROM variant
  void LoadRom(const std::string& variant = "default");

private:
  std::unique_ptr<Rom> rom_;
};

// Base fixture for editor tests
class EditorTestFixture : public RomTestFixture {
protected:
  void SetUp() override;

  EditorDependencies& deps() { return deps_; }

private:
  EditorDependencies deps_;
  // Shared systems for all editors
  std::unique_ptr<EditorCardRegistry> card_registry_;
  std::unique_ptr<ToastManager> toast_manager_;
  std::unique_ptr<PopupManager> popup_manager_;
};

// Fixture for graphics-related tests
class GraphicsTestFixture : public RomTestFixture {
protected:
  void SetUp() override;

  gfx::Arena* arena() { return &gfx::Arena::Get(); }

  // Helper to load test graphics sheet
  gfx::Bitmap LoadTestGraphicsSheet(int sheet_id);
};

}  // namespace test
}  // namespace yaze
```

### 3.5 Flaky Test Detection System (P2)

**Purpose:** Automatically identify and quarantine flaky tests.

**Strategy:**
- Run each test N times in CI
- Track pass/fail statistics
- Quarantine tests with inconsistent results
- Generate flaky test report

**Implementation:**

```python
# scripts/detect-flaky-tests.py
import subprocess
import json
from collections import defaultdict

def run_test_n_times(test_name, iterations=10):
    """Run a single test N times and collect results."""
    results = []
    for i in range(iterations):
        result = subprocess.run(
            ['./build/bin/yaze_test_stable', f'--gtest_filter={test_name}'],
            capture_output=True
        )
        results.append(result.returncode == 0)
    return results

def detect_flaky_tests():
    """Run all tests multiple times and identify flaky ones."""
    # Get list of all tests
    result = subprocess.run(
        ['./build/bin/yaze_test_stable', '--gtest_list_tests'],
        capture_output=True,
        text=True
    )

    test_list = parse_test_list(result.stdout)
    flaky_tests = []

    for test in test_list:
        results = run_test_n_times(test, iterations=10)
        pass_rate = sum(results) / len(results)

        # Flag as flaky if pass rate is between 10% and 90%
        if 0.1 < pass_rate < 0.9:
            flaky_tests.append({
                'test': test,
                'pass_rate': pass_rate,
                'results': results
            })
            print(f"FLAKY: {test} - Pass rate: {pass_rate:.1%}")

    # Generate report
    with open('flaky_test_report.json', 'w') as f:
        json.dump(flaky_tests, f, indent=2)

    return flaky_tests

if __name__ == '__main__':
    flaky_tests = detect_flaky_tests()
    if flaky_tests:
        print(f"\nFound {len(flaky_tests)} flaky tests")
        exit(1)
```

**CI Integration:**

```yaml
# .github/workflows/flaky-test-detection.yml
name: Flaky Test Detection

on:
  schedule:
    - cron: '0 2 * * 1'  # Weekly on Monday at 2 AM
  workflow_dispatch:

jobs:
  detect-flaky:
    runs-on: ubuntu-22.04
    steps:
      - uses: actions/checkout@v4
      - name: Build tests
        run: cmake --build build --target yaze_test_stable
      - name: Run flaky test detection
        run: python3 scripts/detect-flaky-tests.py
      - name: Upload flaky test report
        if: failure()
        uses: actions/upload-artifact@v4
        with:
          name: flaky-test-report
          path: flaky_test_report.json
```

### 3.6 Test Reporting Dashboard (P2)

**Purpose:** Visualize test health metrics over time.

**Metrics to Track:**
- Pass/fail rates by category
- Test execution time trends
- Code coverage trends
- Flaky test occurrences
- Platform-specific failure rates

**Implementation:** GitHub Pages site generated from test results

**Data Collection:**
```bash
# scripts/collect-test-metrics.sh
#!/bin/bash

# Run tests with JSON output
ctest --output-junit test_results.xml

# Parse results and update metrics database
python3 scripts/parse-test-results.py test_results.xml >> test_metrics.jsonl

# Generate dashboard HTML
python3 scripts/generate-test-dashboard.py test_metrics.jsonl > docs/test_dashboard.html
```

### 3.7 Estimated Effort

| Task | Priority | Effort (Days) | Dependencies |
|------|----------|---------------|--------------|
| Enhanced Categorization | P0 | 1 | None |
| Test Data Management | P0 | 2 | None |
| Common Fixtures | P1 | 2 | None |
| Flaky Test Detection | P2 | 3 | CI infrastructure |
| Test Dashboard | P2 | 4 | Metrics collection |

**Total Estimated Effort:** 12 developer-days

---

## 4. CI/CD Integration Enhancements

### 4.1 Current CI/CD State

**Strengths:**
- Multi-platform builds (Linux, macOS, Windows)
- Composite actions for reusability
- CPM and sccache for build caching
- Separate stable/unit test execution

**Weaknesses:**
- No parallel test execution within platforms
- Tests always run all tests (no smart selection)
- Limited test result caching
- No test sharding for long test suites
- Build + test runs sequentially (could parallelize more)

### 4.2 Parallel Test Execution (P0)

**Strategy:** Split tests into independent shards that run in parallel.

**Implementation:**

```yaml
# .github/workflows/ci.yml - Enhanced test job
test:
  name: "Test - ${{ matrix.name }} - Shard ${{ matrix.shard }}"
  runs-on: ${{ matrix.os }}
  strategy:
    fail-fast: false
    matrix:
      include:
        # Linux shards
        - name: "Ubuntu 22.04"
          os: ubuntu-22.04
          platform: linux
          preset: ci-linux
          shard: 1
          shard_count: 3
        - name: "Ubuntu 22.04"
          os: ubuntu-22.04
          platform: linux
          preset: ci-linux
          shard: 2
          shard_count: 3
        - name: "Ubuntu 22.04"
          os: ubuntu-22.04
          platform: linux
          preset: ci-linux
          shard: 3
          shard_count: 3

        # macOS shards (fewer due to cost)
        - name: "macOS 14"
          os: macos-14
          platform: macos
          preset: ci-macos
          shard: 1
          shard_count: 2
        - name: "macOS 14"
          os: macos-14
          platform: macos
          preset: ci-macos
          shard: 2
          shard_count: 2

        # Windows (single shard for now)
        - name: "Windows 2022"
          os: windows-2022
          platform: windows
          preset: ci-windows
          shard: 1
          shard_count: 1

  steps:
    - name: Checkout code
      uses: actions/checkout@v4
      with:
        submodules: recursive

    - name: Setup build environment
      uses: ./.github/actions/setup-build
      with:
        platform: ${{ matrix.platform }}
        preset: ${{ matrix.preset }}

    - name: Build project
      uses: ./.github/actions/build-project
      with:
        platform: ${{ matrix.platform }}
        preset: ${{ matrix.preset }}

    - name: Run sharded tests
      run: |
        cd build
        ctest --preset stable \
          --tests-regex ".*" \
          --shard-count ${{ matrix.shard_count }} \
          --shard-index ${{ matrix.shard }} \
          --output-on-failure \
          --output-junit shard_${{ matrix.shard }}_results.xml

    - name: Upload shard results
      uses: actions/upload-artifact@v4
      with:
        name: test-results-${{ matrix.platform }}-shard-${{ matrix.shard }}
        path: build/shard_*_results.xml
        retention-days: 7
```

**Expected Improvement:** Reduce total CI test time by 40-60%

### 4.3 Smart Test Selection (P1)

**Purpose:** Only run tests affected by code changes in PRs.

**Strategy:**
- Analyze git diff to identify changed files
- Map changed files to affected test suites
- Run only relevant tests in PR CI
- Always run full suite on master/develop

**Implementation:**

```python
# scripts/select-tests-for-changes.py
import subprocess
import sys
import json

# Map source directories to test suites
TEST_MAPPING = {
    'src/app/rom': ['RomTest', 'Integration/*Rom*'],
    'src/app/gfx': ['Gfx*', 'Graphics*', 'Integration/*Graphics*'],
    'src/app/editor/dungeon': ['Dungeon*', 'E2E/*Dungeon*'],
    'src/app/editor/overworld': ['Overworld*', 'E2E/*Overworld*'],
    'src/zelda3': ['*Zelda3*', 'Integration/*'],
    'src/core/asar': ['Asar*'],
}

def get_changed_files(base_ref='origin/master'):
    """Get list of files changed in current branch."""
    result = subprocess.run(
        ['git', 'diff', '--name-only', base_ref, 'HEAD'],
        capture_output=True,
        text=True
    )
    return result.stdout.strip().split('\n')

def map_files_to_tests(changed_files):
    """Map changed files to relevant test patterns."""
    test_patterns = set()

    for file in changed_files:
        for src_pattern, test_list in TEST_MAPPING.items():
            if file.startswith(src_pattern):
                test_patterns.update(test_list)

    # If test files changed, run those tests
    for file in changed_files:
        if file.startswith('test/'):
            # Extract test name from file path
            test_name = extract_test_name(file)
            if test_name:
                test_patterns.add(test_name)

    # Always run smoke tests
    test_patterns.add('*Smoke*')

    return list(test_patterns)

def generate_gtest_filter(test_patterns):
    """Generate --gtest_filter argument."""
    return ':'.join(test_patterns)

if __name__ == '__main__':
    changed_files = get_changed_files()
    test_patterns = map_files_to_tests(changed_files)

    if not test_patterns:
        print("--gtest_filter=*Smoke*")  # Default to smoke tests
    else:
        print(f"--gtest_filter={generate_gtest_filter(test_patterns)}")
```

**CI Integration:**

```yaml
# .github/workflows/pr-tests.yml
name: PR Test Selection

on:
  pull_request:
    branches: [ master, develop ]

jobs:
  smart-test:
    runs-on: ubuntu-22.04
    steps:
      - uses: actions/checkout@v4
        with:
          fetch-depth: 0  # Need full history for diff

      - name: Determine tests to run
        id: test-selection
        run: |
          TEST_FILTER=$(python3 scripts/select-tests-for-changes.py)
          echo "filter=$TEST_FILTER" >> $GITHUB_OUTPUT

      - name: Build tests
        run: cmake --build build --target yaze_test_stable

      - name: Run selected tests
        run: ./build/bin/yaze_test_stable "${{ steps.test-selection.outputs.filter }}"
```

### 4.4 Test Result Caching (P1)

**Purpose:** Skip tests that haven't changed and had passing results previously.

**Strategy:**
- Hash test binary + test data
- Store test results with hash as key
- Reuse cached results for unchanged tests
- Invalidate cache on test file changes

**Implementation:** Use GitHub Actions cache

```yaml
- name: Cache test results
  uses: actions/cache@v4
  with:
    path: build/test_result_cache/
    key: test-results-${{ hashFiles('test/**/*.cc', 'test/**/*.h') }}-${{ github.sha }}
    restore-keys: |
      test-results-${{ hashFiles('test/**/*.cc', 'test/**/*.h') }}-
      test-results-

- name: Run tests with result caching
  run: |
    python3 scripts/run-tests-with-cache.py \
      --cache-dir build/test_result_cache/ \
      --test-binary build/bin/yaze_test_stable
```

### 4.5 Automated Test Bisection (P2)

**Purpose:** Automatically identify which commit introduced a test failure.

**Strategy:**
- When test fails in CI, trigger bisection workflow
- Use git bisect to find culprit commit
- Report to GitHub issue or PR

**Implementation:**

```yaml
# .github/workflows/test-bisect.yml
name: Test Bisection

on:
  workflow_dispatch:
    inputs:
      failing_test:
        description: 'Name of failing test'
        required: true
      good_commit:
        description: 'Known good commit'
        required: true
      bad_commit:
        description: 'Known bad commit (usually HEAD)'
        required: false
        default: 'HEAD'

jobs:
  bisect:
    runs-on: ubuntu-22.04
    steps:
      - uses: actions/checkout@v4
        with:
          fetch-depth: 0

      - name: Run git bisect
        run: |
          git bisect start ${{ inputs.bad_commit }} ${{ inputs.good_commit }}
          git bisect run scripts/test-bisect-helper.sh "${{ inputs.failing_test }}"

      - name: Report culprit commit
        run: |
          CULPRIT=$(git bisect view --pretty=format:"%H %s")
          echo "Culprit commit: $CULPRIT"
          # Post to GitHub issue or PR comment
```

### 4.6 E2E Test Recording (P2)

**Purpose:** Record e2e test failures with screenshots/videos for debugging.

**Strategy:**
- Configure ImGuiTestEngine to capture screenshots on failure
- Generate video recordings of test execution
- Upload to CI artifacts for review

**Implementation:**

```cpp
// test/e2e/test_recorder.h
class E2ETestRecorder {
public:
  static void StartRecording(const std::string& test_name);
  static void StopRecording();
  static void CaptureScreenshot(const std::string& label);

  // Called automatically on test failure
  static void OnTestFailure(ImGuiTestContext* ctx);
};
```

```yaml
# CI integration
- name: Run E2E tests with recording
  run: |
    ./build/bin/yaze_test_gui --enable-ui-tests --show-gui=false
  env:
    YAZE_E2E_RECORD: "true"
    YAZE_E2E_RECORD_PATH: "test_recordings/"

- name: Upload test recordings
  if: failure()
  uses: actions/upload-artifact@v4
  with:
    name: e2e-test-recordings
    path: test_recordings/
    retention-days: 14
```

### 4.7 Estimated Effort

| Task | Priority | Effort (Days) | Dependencies |
|------|----------|---------------|--------------|
| Parallel Test Execution | P0 | 2 | None |
| Smart Test Selection | P1 | 3 | File mapping |
| Test Result Caching | P1 | 2 | CI infrastructure |
| Automated Bisection | P2 | 2 | None |
| E2E Recording | P2 | 3 | ImGuiTestEngine integration |

**Total Estimated Effort:** 12 developer-days

---

## 5. Testing Best Practices Documentation

### 5.1 Documentation Structure

**Files to Create:**

```
docs/testing/
├── README.md                           # Overview and quick start
├── writing-unit-tests.md               # Unit testing guide
├── writing-integration-tests.md        # Integration testing guide
├── writing-e2e-tests.md                # ImGui Test Engine guide
├── test-data-management.md             # Mock ROMs, golden files
├── performance-testing.md              # Benchmark writing guide
├── ci-cd-testing.md                    # CI/CD integration
├── troubleshooting-tests.md            # Common issues and solutions
└── examples/
    ├── unit-test-example.cc
    ├── integration-test-example.cc
    ├── e2e-test-example.cc
    └── benchmark-example.cc
```

### 5.2 Unit Testing Guide

**Content Outline:**

```markdown
# Writing Effective Unit Tests for YAZE

## Principles
- Fast execution (< 100ms per test)
- No external dependencies (ROM, filesystem, network)
- Test one thing per test
- Use descriptive test names

## Structure
\`\`\`cpp
TEST(ComponentName, SpecificBehavior_ExpectedOutcome) {
  // GIVEN - Setup test conditions
  SnesTile tile(0x1234, gfx::TileType::Tile16);

  // WHEN - Execute behavior
  auto result = tile.GetPixelColor(5, 7);

  // THEN - Verify outcome
  EXPECT_EQ(result.r, 0x12);
  EXPECT_EQ(result.g, 0x34);
  EXPECT_EQ(result.b, 0x56);
}
\`\`\`

## Using Mocks
\`\`\`cpp
class MockRom : public Rom {
public:
  MOCK_METHOD(uint8_t, ReadByte, (uint32_t address), (override));
  MOCK_METHOD(absl::Status, WriteByte, (uint32_t address, uint8_t value), (override));
};

TEST(OverworldTest, LoadMapCallsRomRead) {
  MockRom mock_rom;
  EXPECT_CALL(mock_rom, ReadByte(0x1234))
      .Times(1)
      .WillOnce(Return(0x42));

  Overworld overworld(&mock_rom);
  overworld.LoadMap(0);
}
\`\`\`

## Testing Error Conditions
\`\`\`cpp
TEST(RomTest, LoadFromFile_InvalidPath_ReturnsError) {
  Rom rom;
  auto status = rom.LoadFromFile("nonexistent.sfc");

  EXPECT_FALSE(status.ok());
  EXPECT_EQ(status.code(), absl::StatusCode::kNotFound);
  EXPECT_THAT(status.message(), testing::HasSubstr("nonexistent.sfc"));
}
\`\`\`

## Parameterized Tests
\`\`\`cpp
class TileConversionTest : public ::testing::TestWithParam<std::pair<uint16_t, uint16_t>> {
};

TEST_P(TileConversionTest, Tile8ToTile16Conversion) {
  auto [tile8_id, expected_tile16_id] = GetParam();

  uint16_t result = ConvertTile8ToTile16(tile8_id);

  EXPECT_EQ(result, expected_tile16_id);
}

INSTANTIATE_TEST_SUITE_P(
    TileConversions,
    TileConversionTest,
    ::testing::Values(
        std::make_pair(0x00, 0x0000),
        std::make_pair(0x01, 0x0001),
        std::make_pair(0xFF, 0x00FF)
    )
);
\`\`\`

## Anti-Patterns to Avoid
- ❌ Testing multiple behaviors in one test
- ❌ Depending on execution order of tests
- ❌ Hardcoding magic numbers without explanation
- ❌ Suppressing assertion failures
- ❌ Using sleep() for timing (makes tests slow and flaky)
```

### 5.3 ImGui Test Engine Patterns Guide

**Content Outline:**

```markdown
# ImGui Test Engine Patterns and Anti-Patterns

## Good Patterns

### 1. Use Stable Widget IDs
\`\`\`cpp
// GOOD: Stable ID that won't change
ImGui::Button("Save##DungeonEditor_SaveButton");

// BAD: ID depends on runtime state
ImGui::Button(fmt::format("Save ##{}", room_id).c_str());
\`\`\`

### 2. Wait for UI State with ctx->Yield()
\`\`\`cpp
// GOOD: Wait for UI to update
ctx->ItemClick("Load ROM##MenuBar");
ctx->Yield(2);  // Wait 2 frames
EXPECT_TRUE(ctx->WindowInfo("ROM Loaded").Window != nullptr);

// BAD: No wait, test may be flaky
ctx->ItemClick("Load ROM##MenuBar");
EXPECT_TRUE(ctx->WindowInfo("ROM Loaded").Window != nullptr);  // May fail!
\`\`\`

### 3. Use SetRef for Scoped Context
\`\`\`cpp
// GOOD: Clear reference context
ctx->SetRef("Dungeon Controls");
ctx->ItemClick("Rooms");
ctx->SetRef("Room Selector");
ctx->ItemDoubleClick("Room 0x00");

// BAD: Ambiguous references
ctx->ItemClick("Rooms");  // Which window?
\`\`\`

### 4. Verify State Before and After Actions
\`\`\`cpp
// GOOD: Verify preconditions and postconditions
uint16_t original_tile = overworld->GetTile(10, 10);
ctx->ItemClick("##Canvas");
ctx->MouseMoveToPos(ImVec2(80, 80));
ctx->MouseClick(0);
uint16_t new_tile = overworld->GetTile(10, 10);
EXPECT_NE(original_tile, new_tile);

// BAD: Only check postcondition
ctx->MouseClick(0);
EXPECT_EQ(overworld->GetTile(10, 10), 0x1234);  // Assumes state
\`\`\`

## Anti-Patterns to Avoid

### ❌ Don't Use Absolute Pixel Coordinates
\`\`\`cpp
// BAD: Breaks if window moves
ctx->MouseMoveToPos(ImVec2(500, 300));

// GOOD: Use relative to window
ctx->WindowFocus("Dungeon Editor");
auto window_pos = ctx->GetWindowPos();
ctx->MouseMoveToPos(window_pos + ImVec2(100, 50));
\`\`\`

### ❌ Don't Test UI Without ROM State Validation
\`\`\`cpp
// BAD: Only test UI, don't verify ROM
ctx->ItemClick("Delete Object");
EXPECT_FALSE(ctx->ItemExists("Object 0x05"));

// GOOD: Verify both UI and ROM state
ctx->ItemClick("Delete Object");
EXPECT_FALSE(ctx->ItemExists("Object 0x05"));
EXPECT_FALSE(room->HasObject(0x05));  // Verify ROM state too
\`\`\`

### ❌ Don't Ignore Test Speed Settings
\`\`\`cpp
// BAD: Always run at max speed
test_io.ConfigRunSpeed = ImGuiTestRunSpeed_Fast;

// GOOD: Allow configurable speed for debugging
test_io.ConfigRunSpeed = config.test_speed;  // From command line
\`\`\`

## Debugging E2E Tests

### Enable Visual Mode
\`\`\`bash
./build/bin/yaze_test_gui --e2e --show-gui --normal
\`\`\`

### Add Debug Logging
\`\`\`cpp
ctx->LogDebug("About to click object palette");
ctx->ItemClick("Object Palette##DungeonEditor");
ctx->LogDebug("Object palette clicked, yielding...");
ctx->Yield(2);
\`\`\`

### Capture Screenshots on Failure
\`\`\`cpp
if (!ctx->ItemExists("Expected Widget")) {
  ctx->CaptureScreenshot("failure_screenshot.png");
  FAIL() << "Expected widget not found";
}
\`\`\`
```

### 5.4 Testing Best Practices Cheat Sheet

**Quick Reference Card:**

```markdown
# YAZE Testing Cheat Sheet

## Test Categories
| Category | Speed | Dependencies | When to Use |
|----------|-------|--------------|-------------|
| Unit | < 100ms | None | Testing pure logic, algorithms |
| Integration | < 5s | ROM, fixtures | Multi-component interactions |
| E2E | < 30s | ROM, UI | Full user workflows |
| Benchmark | Varies | Performance-critical code | Performance validation |

## Common Macros
\`\`\`cpp
// Assertions (stop test on failure)
ASSERT_TRUE(condition);
ASSERT_EQ(actual, expected);
ASSERT_THAT(value, matcher);

// Expectations (continue test on failure)
EXPECT_TRUE(condition);
EXPECT_NE(actual, not_expected);
EXPECT_THAT(value, testing::HasSubstr("text"));

// Status handling
ASSERT_OK(status);
ASSERT_OK_AND_ASSIGN(auto result, StatusOrValue);
EXPECT_OK(status);
\`\`\`

## Running Tests
\`\`\`bash
# Build tests
cmake --build build --target yaze_test_stable

# Run all tests
./build/bin/yaze_test_stable

# Run by category
./build/bin/yaze_test_stable --unit
./build/bin/yaze_test_stable --integration

# Run specific test
./build/bin/yaze_test_stable --gtest_filter="RomTest.*"

# Run with ROM
./build/bin/yaze_test_stable --rom-dependent --rom-path=zelda3.sfc

# Verbose output
./build/bin/yaze_test_stable --verbose --gtest_print_time=1
\`\`\`

## Test Naming
\`\`\`cpp
TEST(ClassOrModule, Method_Condition_ExpectedBehavior)
TEST(Rom, LoadFromFile_ValidRom_ReturnsOk)
TEST(Overworld, GetTile_OutOfBounds_ReturnsError)
\`\`\`

## Fixture Template
\`\`\`cpp
class MyComponentTest : public ::testing::Test {
protected:
  void SetUp() override {
    // Initialize test state
  }

  void TearDown() override {
    // Clean up resources
  }

  // Helper methods
  void LoadTestData() { /* ... */ }
};

TEST_F(MyComponentTest, TestCase) {
  // Test body
}
\`\`\`

## Mock Object Template
\`\`\`cpp
class MockInterface : public Interface {
public:
  MOCK_METHOD(ReturnType, MethodName, (Arg1Type arg1, Arg2Type arg2), (override));
};

TEST(MyTest, UseMock) {
  MockInterface mock;
  EXPECT_CALL(mock, MethodName(1, 2))
      .Times(1)
      .WillOnce(Return(expected_value));

  // Execute code that calls mock
}
\`\`\`
```

### 5.5 Estimated Effort

| Task | Priority | Effort (Days) | Dependencies |
|------|----------|---------------|--------------|
| Documentation Structure | P0 | 1 | None |
| Unit Testing Guide | P0 | 2 | None |
| Integration Testing Guide | P0 | 2 | None |
| E2E Testing Guide | P0 | 3 | ImGui Test Engine |
| Best Practices Cheat Sheet | P0 | 1 | All guides |
| Example Code | P1 | 2 | None |

**Total Estimated Effort:** 11 developer-days

---

## 6. Implementation Roadmap

### Phase 1: Foundation (Weeks 1-3)

**Priority:** P0 tasks that enable all other work

| Week | Tasks | Deliverables |
|------|-------|--------------|
| 1 | - E2E Test Helpers<br>- Mock ROM Generator<br>- Enhanced Test Categorization | - `test/e2e/test_helpers.h`<br>- `test/test_utils/mock_rom_generator.cc`<br>- Updated CMakeLists.txt labels |
| 2 | - Pre-Commit Hooks<br>- ROM Integrity Validator<br>- Test Data Management | - Git hooks installed<br>- `test/integration/rom_integrity_test.cc`<br>- `test/data/` structure |
| 3 | - Parallel Test Execution<br>- Unit Testing Guide<br>- Integration Testing Guide | - Updated CI workflows<br>- `docs/testing/writing-unit-tests.md`<br>- `docs/testing/writing-integration-tests.md` |

**Milestone:** Foundation complete - Infrastructure ready for test development

### Phase 2: Core Test Coverage (Weeks 4-7)

**Priority:** P0 editor workflow tests

| Week | Tasks | Deliverables |
|------|-------|--------------|
| 4 | - Dungeon Editor E2E Tests (Object Placement) | - `test/e2e/dungeon_editor_object_workflow_test.cc` |
| 5 | - Dungeon Editor E2E Tests (Navigation, Layers) | - Complete dungeon test suite |
| 6 | - Overworld Editor E2E Tests (Map Editing) | - `test/e2e/overworld_editor_workflow_test.cc` |
| 7 | - Overworld Editor E2E Tests (Entities, Tile16) | - Complete overworld test suite |

**Milestone:** Core editor workflows covered by e2e tests

### Phase 3: Validation & Optimization (Weeks 8-10)

**Priority:** P1 quality and performance tasks

| Week | Tasks | Deliverables |
|------|-------|--------------|
| 8 | - Performance Baselines<br>- Performance Regression Tests<br>- Smart Test Selection | - Benchmark baselines<br>- Performance tests<br>- CI test selection |
| 9 | - Graphics Editor E2E Tests<br>- Cross-Editor Integration Tests | - `test/e2e/graphics_editor_workflow_test.cc`<br>- `test/e2e/cross_editor_integration_test.cc` |
| 10 | - Test Result Caching<br>- E2E Testing Guide<br>- Best Practices Cheat Sheet | - CI caching enabled<br>- Complete documentation |

**Milestone:** Comprehensive test coverage with performance validation

### Phase 4: Advanced Features (Weeks 11-12)

**Priority:** P2 advanced tooling

| Week | Tasks | Deliverables |
|------|-------|--------------|
| 11 | - Flaky Test Detection<br>- Visual Regression Testing | - Flaky test CI workflow<br>- Visual regression tests |
| 12 | - Test Dashboard<br>- E2E Test Recording<br>- Automated Bisection | - Test metrics dashboard<br>- Test recording on CI failures |

**Milestone:** Production-grade testing infrastructure complete

### Dependency Graph

```
Foundation (Weeks 1-3)
│
├─→ Core Test Coverage (Weeks 4-7)
│   └─→ Validation & Optimization (Weeks 8-10)
│       └─→ Advanced Features (Weeks 11-12)
│
└─→ Documentation (ongoing throughout)
```

### Resource Requirements

**Personnel:**
- 1 Senior Test Engineer (full-time, weeks 1-12)
- 1 DevOps Engineer (part-time, weeks 1, 3, 8, 10, 12)
- 1 Technical Writer (part-time, weeks 3, 10, 12)

**Infrastructure:**
- GitHub Actions runner minutes (estimated 200 hours total)
- Test data storage (estimated 500MB)

### Success Metrics

| Metric | Baseline | Target | Measurement |
|--------|----------|--------|-------------|
| E2E Test Count | 3 | 50+ | Count of registered ImGui tests |
| Test Execution Time | N/A | < 5 min (CI) | CTest total duration |
| Code Coverage (Critical Paths) | Unknown | 80%+ | gcov/lcov report |
| Flaky Test Rate | Unknown | < 2% | Weekly flaky detection runs |
| Test Documentation Pages | 0 | 8+ | docs/testing/ file count |

---

## 7. Appendix: Code Examples

### A. Complete E2E Test Example

```cpp
// test/e2e/dungeon_editor_complete_workflow_test.cc
#include "e2e/dungeon_editor_complete_workflow_test.h"

#include "app/controller.h"
#include "e2e/test_helpers.h"
#include "imgui_test_engine/imgui_te_context.h"
#include "test_utils.h"

/**
 * @brief Complete workflow test for dungeon object editing
 *
 * Tests the full cycle of:
 * 1. Opening dungeon editor
 * 2. Navigating to a room
 * 3. Placing objects
 * 4. Modifying object properties
 * 5. Saving changes
 * 6. Verifying ROM state
 */
void E2ETest_DungeonEditorCompleteWorkflow(ImGuiTestContext* ctx) {
  ctx->LogInfo("=== Starting Complete Dungeon Editor Workflow Test ===");

  // Get controller from test context
  yaze::Controller* controller = (yaze::Controller*)ctx->Test->UserData;

  // SETUP: Load ROM
  ctx->LogInfo("Loading ROM...");
  yaze::test::e2e::LoadRomInTest(ctx, "zelda3.sfc");
  auto rom = controller->rom();
  ASSERT_TRUE(rom->is_loaded()) << "ROM failed to load";
  ctx->LogInfo("ROM loaded successfully");

  // STEP 1: Open Dungeon Editor
  ctx->LogInfo("--- Step 1: Open Dungeon Editor ---");
  yaze::test::e2e::OpenEditor(ctx, yaze::editor::EditorType::kDungeon);
  yaze::test::e2e::VerifyEditorActive(ctx, yaze::editor::EditorType::kDungeon);
  ctx->Yield(2);

  // STEP 2: Navigate to Room 0x01
  ctx->LogInfo("--- Step 2: Navigate to Room 0x01 ---");
  ctx->SetRef("Dungeon Controls");
  ctx->ItemClick("Rooms");  // Show room selector
  ctx->Yield();

  ctx->SetRef("Room Selector");
  IM_CHECK(ctx->ItemExists("Room 0x01"));
  ctx->ItemDoubleClick("Room 0x01");
  ctx->Yield(3);  // Wait for room to load

  IM_CHECK(ctx->WindowInfo("Room 0x01").Window != nullptr);
  ctx->LogInfo("Room 0x01 opened successfully");

  // STEP 3: Verify initial room state
  ctx->LogInfo("--- Step 3: Verify Initial Room State ---");
  // Get room from ROM
  auto dungeon = rom->GetDungeon(0x00);  // Eastern Palace
  auto room = dungeon->GetRoom(0x01);
  int initial_object_count = room->GetObjectCount();
  ctx->LogInfo("Initial object count: %d", initial_object_count);

  // STEP 4: Open Object Palette
  ctx->LogInfo("--- Step 4: Open Object Palette ---");
  ctx->SetRef("Dungeon Controls");
  ctx->ItemClick("Objects");  // Toggle object editor
  ctx->Yield();

  IM_CHECK(ctx->WindowInfo("Object Editor").Window != nullptr);

  // STEP 5: Select an object type from palette
  ctx->LogInfo("--- Step 5: Select Object Type ---");
  ctx->SetRef("Object Editor");
  // Click on object type 0x42 (example: floor tile)
  ctx->ItemClick("##ObjectPalette_0x42");
  ctx->Yield();
  ctx->LogInfo("Selected object type 0x42");

  // STEP 6: Place object on canvas
  ctx->LogInfo("--- Step 6: Place Object on Canvas ---");
  ctx->SetRef("Room 0x01");

  // Click at position (5, 7) in room coordinates
  ImVec2 canvas_pos = yaze::test::e2e::RoomCoordToCanvasPos(5, 7);
  yaze::test::e2e::ClickCanvasAt(ctx, "##RoomCanvas", canvas_pos);
  ctx->Yield(2);

  // Verify object was placed
  int new_object_count = room->GetObjectCount();
  IM_CHECK_EQ(new_object_count, initial_object_count + 1);
  ctx->LogInfo("Object placed successfully (count: %d -> %d)",
               initial_object_count, new_object_count);

  // STEP 7: Select placed object
  ctx->LogInfo("--- Step 7: Select Placed Object ---");
  yaze::test::e2e::ClickCanvasAt(ctx, "##RoomCanvas", canvas_pos);
  ctx->Yield();

  // Verify object properties panel shows correct data
  ctx->SetRef("Object Properties");
  IM_CHECK(ctx->ItemExists("Type"));
  // Check that type dropdown shows 0x42
  IM_CHECK(ctx->ItemInfo("Type##TypeCombo")->StateStorage->GetInt(ImGuiID("Type##TypeCombo")) == 0x42);

  // STEP 8: Modify object properties
  ctx->LogInfo("--- Step 8: Modify Object Size ---");
  ctx->SetRef("Object Properties");

  // Change size from default to custom value
  ctx->ItemInput("Width##SizeInput");
  ctx->KeyCharsReplace("8");
  ctx->ItemInput("Height##SizeInput");
  ctx->KeyCharsReplace("4");
  ctx->Yield(2);

  // Verify ROM was updated
  auto modified_object = room->GetObject(new_object_count - 1);
  IM_CHECK_EQ(modified_object.width, 8);
  IM_CHECK_EQ(modified_object.height, 4);
  ctx->LogInfo("Object size modified successfully");

  // STEP 9: Test undo
  ctx->LogInfo("--- Step 9: Test Undo ---");
  yaze::test::e2e::SimulateUndoRedo(ctx, false);  // Undo
  ctx->Yield(2);

  // Verify object size reverted
  auto undone_object = room->GetObject(new_object_count - 1);
  IM_CHECK_NE(undone_object.width, 8);  // Should be back to default

  // STEP 10: Test redo
  ctx->LogInfo("--- Step 10: Test Redo ---");
  yaze::test::e2e::SimulateUndoRedo(ctx, true);  // Redo
  ctx->Yield(2);

  // Verify object size restored
  auto redone_object = room->GetObject(new_object_count - 1);
  IM_CHECK_EQ(redone_object.width, 8);
  IM_CHECK_EQ(redone_object.height, 4);

  // STEP 11: Delete object
  ctx->LogInfo("--- Step 11: Delete Object ---");
  yaze::test::e2e::SelectEntity(ctx, new_object_count - 1);
  ctx->KeyPress(ImGuiKey_Delete);
  ctx->Yield(2);

  // Verify object was deleted
  int final_object_count = room->GetObjectCount();
  IM_CHECK_EQ(final_object_count, initial_object_count);
  ctx->LogInfo("Object deleted successfully");

  // STEP 12: Save changes to ROM
  ctx->LogInfo("--- Step 12: Save Changes ---");
  yaze::test::e2e::SimulateShortcut(ctx, ImGuiKey_LeftCtrl, ImGuiKey_S);
  ctx->Yield(2);

  // Verify save success toast appeared
  // (Assumes ToastManager integration)

  // STEP 13: Verify ROM integrity
  ctx->LogInfo("--- Step 13: Verify ROM Integrity ---");
  yaze::test::e2e::VerifyRomIntegrity(ctx, rom);

  ctx->LogInfo("=== Complete Dungeon Editor Workflow Test PASSED ===");
}
```

### B. Test Helper Implementation

```cpp
// test/e2e/test_helpers.cc
#include "e2e/test_helpers.h"

#include "app/controller.h"
#include "imgui_test_engine/imgui_te_context.h"

namespace yaze {
namespace test {
namespace e2e {

void ClickCanvasAt(ImGuiTestContext* ctx, const std::string& canvas_id,
                   ImVec2 world_pos) {
  // Find canvas window
  ImGuiWindow* canvas_window = ctx->GetWindowByRef(canvas_id.c_str());
  IM_CHECK(canvas_window != nullptr) << "Canvas window not found: " << canvas_id;

  // Convert world position to screen position
  // (Assumes canvas uses standard ImGui coordinate system)
  ImVec2 canvas_origin = canvas_window->Pos;
  ImVec2 screen_pos = canvas_origin + world_pos;

  // Perform click
  ctx->MouseMoveToPos(screen_pos);
  ctx->MouseClick(0);  // Left click
  ctx->Yield();
}

void DragCanvasSelection(ImGuiTestContext* ctx, const std::string& canvas_id,
                         ImVec2 start, ImVec2 end) {
  ImGuiWindow* canvas_window = ctx->GetWindowByRef(canvas_id.c_str());
  IM_CHECK(canvas_window != nullptr);

  ImVec2 canvas_origin = canvas_window->Pos;
  ImVec2 screen_start = canvas_origin + start;
  ImVec2 screen_end = canvas_origin + end;

  ctx->MouseMoveToPos(screen_start);
  ctx->MouseDown(0);
  ctx->MouseMoveToPos(screen_end);
  ctx->MouseUp(0);
  ctx->Yield();
}

void CreateEntityOnCanvas(ImGuiTestContext* ctx, const std::string& entity_type,
                          ImVec2 position) {
  // Right-click to open context menu
  ClickCanvasAt(ctx, "##Canvas", position);
  ctx->MouseClick(1);  // Right click
  ctx->Yield();

  // Select entity type from context menu
  std::string menu_item = "Add " + entity_type + "##ContextMenu";
  ctx->MenuClick(menu_item.c_str());
  ctx->Yield(2);
}

void SelectEntity(ImGuiTestContext* ctx, int entity_id) {
  // Click on entity in entity list
  std::string entity_ref = fmt::format("Entity_{}##EntityList", entity_id);
  ctx->ItemClick(entity_ref.c_str());
  ctx->Yield();
}

void VerifyEntityProperties(ImGuiTestContext* ctx, int entity_id,
                            const std::map<std::string, std::string>& props) {
  SelectEntity(ctx, entity_id);
  ctx->SetRef("Entity Properties");

  for (const auto& [key, value] : props) {
    std::string item_id = key + "##PropInput";
    IM_CHECK(ctx->ItemExists(item_id.c_str()));

    // Get current value (simplified - actual implementation depends on widget type)
    auto item_info = ctx->ItemInfo(item_id.c_str());
    std::string current_value = ""; // Extract from widget state

    IM_CHECK_EQ(current_value, value)
      << "Property " << key << " mismatch: expected " << value
      << ", got " << current_value;
  }
}

void OpenEditor(ImGuiTestContext* ctx, EditorType editor_type) {
  const char* editor_name = kEditorNames[static_cast<int>(editor_type)];

  ctx->MenuClick("Editors##MainMenu");
  ctx->Yield();
  ctx->MenuClick(fmt::format("{}##EditorMenu", editor_name).c_str());
  ctx->Yield(3);  // Wait for editor to initialize
}

void CloseEditor(ImGuiTestContext* ctx, EditorType editor_type) {
  const char* editor_name = kEditorNames[static_cast<int>(editor_type)];

  ctx->WindowClose(editor_name);
  ctx->Yield();
}

void VerifyEditorActive(ImGuiTestContext* ctx, EditorType editor_type) {
  const char* editor_name = kEditorNames[static_cast<int>(editor_type)];

  IM_CHECK(ctx->WindowInfo(editor_name).Window != nullptr)
    << "Editor " << editor_name << " is not active";
}

void SimulateShortcut(ImGuiTestContext* ctx, ImGuiKey modifier, ImGuiKey key) {
  ctx->KeyDown(modifier);
  ctx->KeyPress(key);
  ctx->KeyUp(modifier);
  ctx->Yield();
}

void SimulateUndoRedo(ImGuiTestContext* ctx, bool is_redo) {
  if (is_redo) {
    SimulateShortcut(ctx, ImGuiKey_LeftCtrl, ImGuiKey_Y);
  } else {
    SimulateShortcut(ctx, ImGuiKey_LeftCtrl, ImGuiKey_Z);
  }
}

void VerifyRomByteEquals(ImGuiTestContext* ctx, Rom* rom,
                         uint32_t address, uint8_t expected) {
  uint8_t actual = rom->ReadByte(address);
  IM_CHECK_EQ(actual, expected)
    << fmt::format("ROM byte mismatch at 0x{:X}: expected 0x{:02X}, got 0x{:02X}",
                   address, expected, actual);
}

void VerifyRomBytesEqual(ImGuiTestContext* ctx, Rom* rom,
                         uint32_t address, const std::vector<uint8_t>& expected) {
  for (size_t i = 0; i < expected.size(); ++i) {
    VerifyRomByteEquals(ctx, rom, address + i, expected[i]);
  }
}

std::unique_ptr<Rom> CreateMockRomForTesting(const std::string& variant) {
  auto rom = std::make_unique<Rom>();

  std::vector<uint8_t> mock_data;

  if (variant == "default") {
    mock_data = TestRomManager::CreateMinimalTestRom(1024 * 1024);  // 1MB
  } else if (variant == "zscustom_v3") {
    mock_data = TestRomManager::CreateMinimalTestRom(2 * 1024 * 1024);  // 2MB
    // Set ZSCustomOverworld version byte
    mock_data[0x1FF00] = 0x03;  // Version 3
  } else if (variant == "minimal") {
    mock_data = TestRomManager::CreateMinimalTestRom(256 * 1024);  // 256KB
  } else {
    IM_CHECK(false) << "Unknown mock ROM variant: " << variant;
  }

  // Load mock data into ROM
  auto status = rom->LoadFromBytes(mock_data);
  IM_CHECK(status.ok()) << "Failed to load mock ROM: " << status.message();

  return rom;
}

ImVec2 RoomCoordToCanvasPos(int x, int y) {
  // Convert dungeon room coordinates (in tiles) to canvas pixel coordinates
  // Assumes 16x16 tile size
  constexpr int kTileSize = 16;
  return ImVec2(x * kTileSize, y * kTileSize);
}

void VerifyRomIntegrity(ImGuiTestContext* ctx, Rom* rom) {
  RomIntegrityValidator validator;
  auto result = validator.ValidateRomIntegrity(rom);

  IM_CHECK(result.valid) << "ROM integrity check failed";

  for (const auto& error : result.errors) {
    ctx->LogError("Integrity error: %s", error.c_str());
  }

  for (const auto& warning : result.warnings) {
    ctx->LogWarning("Integrity warning: %s", warning.c_str());
  }
}

}  // namespace e2e
}  // namespace test
}  // namespace yaze
```

### C. Mock ROM Generator Implementation

```cpp
// test/test_utils/mock_rom_generator.cc
#include "test_utils/mock_rom_generator.h"

#include <cstring>
#include <random>

namespace yaze {
namespace test {

MockRomGenerator::MockRomGenerator(const MockRomConfig& config)
    : config_(config) {}

std::vector<uint8_t> MockRomGenerator::Generate() {
  std::vector<uint8_t> rom_data(config_.rom_size, 0x00);

  // Write SNES header
  WriteSNESHeader(rom_data);

  // Write overworld data
  if (config_.include_overworld_data) {
    WriteOverworldData(rom_data);
  }

  // Write dungeon data
  if (config_.include_dungeon_data) {
    WriteDungeonData(rom_data);
  }

  // Write graphics data
  if (config_.include_graphics_data) {
    WriteGraphicsData(rom_data);
  }

  // Calculate and write checksum
  WriteChecksum(rom_data);

  return rom_data;
}

void MockRomGenerator::WriteSNESHeader(std::vector<uint8_t>& data) {
  constexpr size_t kHeaderOffset = 0x7FC0;  // LoROM header location

  // ROM title (21 bytes)
  std::string title = "YAZE MOCK ROM       ";
  std::copy(title.begin(), title.end(), data.begin() + kHeaderOffset);

  // Map mode (1 byte) - LoROM
  data[kHeaderOffset + 0x15] = 0x20;

  // Cartridge type (1 byte) - ROM only
  data[kHeaderOffset + 0x16] = 0x00;

  // ROM size (1 byte)
  uint8_t rom_size_code = CalculateRomSizeCode(config_.rom_size);
  data[kHeaderOffset + 0x17] = rom_size_code;

  // SRAM size (1 byte) - No SRAM
  data[kHeaderOffset + 0x18] = 0x00;

  // Country code (1 byte) - USA
  data[kHeaderOffset + 0x19] = 0x01;

  // Developer ID (1 byte)
  data[kHeaderOffset + 0x1A] = 0x00;

  // Version number (1 byte)
  data[kHeaderOffset + 0x1B] = 0x00;

  // Checksum complement (2 bytes) - filled by WriteChecksum()
  // Checksum (2 bytes) - filled by WriteChecksum()
}

void MockRomGenerator::WriteOverworldData(std::vector<uint8_t>& data) {
  // Write minimal overworld map data
  // Zelda3 overworld maps start at specific offsets
  constexpr size_t kOverworldMapDataOffset = 0x0E0000;

  // Write 64 maps (Light World, Dark World, Special World)
  for (int map_id = 0; map_id < 64; ++map_id) {
    size_t map_offset = kOverworldMapDataOffset + (map_id * 512);

    // Fill with simple tile pattern
    for (int i = 0; i < 512; ++i) {
      data[map_offset + i] = (map_id + i) % 256;
    }
  }

  // Write overworld area properties
  constexpr size_t kAreaPropertiesOffset = 0x0F8000;
  for (int area = 0; area < 64; ++area) {
    data[kAreaPropertiesOffset + area] = 0x00;  // Default properties
  }
}

void MockRomGenerator::WriteDungeonData(std::vector<uint8_t>& data) {
  // Write minimal dungeon room data
  constexpr size_t kDungeonRoomDataOffset = 0x04C000;

  // Write 296 dungeon rooms
  for (int room_id = 0; room_id < 296; ++room_id) {
    size_t room_offset = kDungeonRoomDataOffset + (room_id * 256);

    // Write room header
    data[room_offset + 0] = 0x01;  // BG2 property
    data[room_offset + 1] = 0x00;  // Collision
    data[room_offset + 2] = 0x00;  // Light property

    // Write a few simple objects
    if (config_.populate_with_test_data) {
      data[room_offset + 3] = 0x42;  // Object type
      data[room_offset + 4] = 0x55;  // Position
      data[room_offset + 5] = 0xFF;  // End marker
    }
  }
}

void MockRomGenerator::WriteGraphicsData(std::vector<uint8_t>& data) {
  // Write minimal graphics sheet data
  constexpr size_t kGraphicsDataOffset = 0x080000;
  constexpr size_t kGraphicsSheetSize = 0x0800;  // 2KB per sheet

  // Write 223 graphics sheets
  for (int sheet_id = 0; sheet_id < 223; ++sheet_id) {
    size_t sheet_offset = kGraphicsDataOffset + (sheet_id * kGraphicsSheetSize);

    if (config_.populate_with_test_data) {
      // Fill with recognizable pattern
      for (size_t i = 0; i < kGraphicsSheetSize; ++i) {
        data[sheet_offset + i] = (sheet_id + i) % 256;
      }
    }
  }
}

void MockRomGenerator::WriteChecksum(std::vector<uint8_t>& data) {
  constexpr size_t kHeaderOffset = 0x7FC0;

  // Calculate checksum
  uint16_t checksum = 0;
  for (size_t i = 0; i < data.size(); ++i) {
    // Skip checksum bytes themselves
    if (i >= kHeaderOffset + 0x1C && i < kHeaderOffset + 0x20) {
      continue;
    }
    checksum += data[i];
  }

  uint16_t checksum_complement = checksum ^ 0xFFFF;

  // Write checksum complement
  data[kHeaderOffset + 0x1C] = checksum_complement & 0xFF;
  data[kHeaderOffset + 0x1D] = (checksum_complement >> 8) & 0xFF;

  // Write checksum
  data[kHeaderOffset + 0x1E] = checksum & 0xFF;
  data[kHeaderOffset + 0x1F] = (checksum >> 8) & 0xFF;
}

uint8_t MockRomGenerator::CalculateRomSizeCode(size_t rom_size) {
  // SNES ROM size is encoded as log2(size_in_kb) - 10
  size_t size_kb = rom_size / 1024;
  uint8_t code = 0;

  while (size_kb > 1) {
    size_kb >>= 1;
    code++;
  }

  return code;
}

// Factory methods for common ROM variants
std::vector<uint8_t> CreateDefaultMockRom() {
  MockRomConfig config;
  config.rom_size = 1024 * 1024;  // 1MB
  config.include_overworld_data = true;
  config.include_dungeon_data = true;
  config.include_graphics_data = true;
  config.populate_with_test_data = true;

  MockRomGenerator generator(config);
  return generator.Generate();
}

std::vector<uint8_t> CreateMinimalMockRom() {
  MockRomConfig config;
  config.rom_size = 256 * 1024;  // 256KB
  config.include_overworld_data = false;
  config.include_dungeon_data = false;
  config.include_graphics_data = false;
  config.populate_with_test_data = false;

  MockRomGenerator generator(config);
  return generator.Generate();
}

std::vector<uint8_t> CreateZSCustomV3MockRom() {
  MockRomConfig config;
  config.rom_size = 2 * 1024 * 1024;  // 2MB
  config.include_overworld_data = true;
  config.include_dungeon_data = true;
  config.include_graphics_data = true;
  config.populate_with_test_data = true;

  MockRomGenerator generator(config);
  auto rom_data = generator.Generate();

  // Set ZSCustomOverworld version
  rom_data[0x1FF00] = 0x03;  // Version 3

  return rom_data;
}

}  // namespace test
}  // namespace yaze
```

---

## Summary

This comprehensive plan provides a roadmap to transform YAZE's testing infrastructure from basic coverage to a production-grade, automated system. The plan prioritizes:

1. **P0 (Critical):** Foundation, core editor tests, pre-commit validation - 6 weeks
2. **P1 (Important):** Performance testing, advanced e2e tests, CI optimization - 4 weeks
3. **P2 (Nice-to-have):** Visual regression, flaky detection, dashboards - 2 weeks

**Total Estimated Effort:** 12 weeks with dedicated resources

**Key Benefits:**
- 15x increase in e2e test coverage (3 → 50+ tests)
- 40-60% reduction in CI test execution time
- Automated regression prevention
- Comprehensive documentation for human and AI developers
- Platform-specific validation ensuring cross-platform stability

**Next Steps:**
1. Review and approve plan with project stakeholders
2. Allocate resources (test engineer, DevOps support)
3. Begin Phase 1 (Foundation) implementation
4. Establish weekly progress reviews
5. Adjust priorities based on feedback and discoveries

This plan is designed to be implemented incrementally, with each phase delivering tangible value independently.
