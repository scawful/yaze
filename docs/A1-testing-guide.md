# Testing Guide

Comprehensive testing framework with efficient CI/CD integration and ROM-dependent test separation.

## Test Categories

### Stable Tests (STABLE)
**Always run in CI/CD - Required for releases**

- **AsarWrapperTest**: Core Asar functionality tests
- **SnesTileTest**: SNES tile format handling  
- **CompressionTest**: Data compression/decompression
- **SnesPaletteTest**: SNES palette operations
- **HexTest**: Hexadecimal utilities
- **AsarIntegrationTest**: Asar integration without ROM dependencies

**Characteristics:**
- Fast execution (< 30 seconds total)
- No external dependencies (ROMs, complex setup)
- High reliability and deterministic results

### ROM-Dependent Tests (ROM_DEPENDENT)
**Only run in development with available ROM files**

- **AsarRomIntegrationTest**: Real ROM patching and symbol extraction
- **ROM-based integration tests**: Tests requiring actual game ROM files

**Characteristics:**
- Require specific ROM files to be present
- Test real-world functionality
- Automatically skipped in CI if ROM files unavailable

### Experimental Tests (EXPERIMENTAL)
**Run separately, allowed to fail**

- **CpuTest**: 65816 CPU emulation tests
- **Spc700Test**: SPC700 audio processor tests
- **ApuTest**: Audio Processing Unit tests
- **PpuTest**: Picture Processing Unit tests

**Characteristics:**
- May be unstable due to emulation complexity
- Test advanced/experimental features
- Allowed to fail without blocking releases

## Command Line Usage

```bash
# Run only stable tests (release-ready)
ctest --test-dir build --label-regex "STABLE"

# Run experimental tests (allowed to fail)
ctest --test-dir build --label-regex "EXPERIMENTAL"

# Run Asar-specific tests
ctest --test-dir build -R "*Asar*"

# Run tests excluding ROM-dependent ones
ctest --test-dir build --label-exclude "ROM_DEPENDENT"

# Run with specific preset
ctest --preset stable
ctest --preset experimental
```

## CMake Presets

```bash
# Development workflow
cmake --preset dev
cmake --build --preset dev
ctest --preset dev

# CI workflow  
cmake --preset ci
cmake --build --preset ci
ctest --preset ci

# Release workflow
cmake --preset release
cmake --build --preset release
ctest --preset stable
```

## Writing Tests

### Stable Tests
```cpp
TEST(SnesTileTest, UnpackBppTile) {
    std::vector<uint8_t> tile_data = {0xAA, 0x55, 0xAA, 0x55};
    std::vector<uint8_t> result = UnpackBppTile(tile_data, 2);
    EXPECT_EQ(result.size(), 64);
    // Test specific pixel values...
}
```

### ROM-Dependent Tests
```cpp
YAZE_ROM_TEST(AsarIntegration, RealRomPatching) {
    auto rom_data = TestRomManager::LoadTestRom();
    if (!rom_data.has_value()) {
        GTEST_SKIP() << "ROM file not available";
    }
    
    AsarWrapper wrapper;
    wrapper.Initialize();
    
    auto result = wrapper.ApplyPatch("test.asm", *rom_data);
    EXPECT_TRUE(result.ok());
}
```

### Experimental Tests
```cpp
TEST(CpuTest, InstructionExecution) {
    // Complex emulation tests
    // May be timing-sensitive or platform-dependent
}
```

## CI/CD Integration

### GitHub Actions
```yaml
# Main CI pipeline
- name: Run Stable Tests
  run: ctest --label-regex "STABLE"

# Experimental tests (allowed to fail)
- name: Run Experimental Tests
  run: ctest --label-regex "EXPERIMENTAL"
  continue-on-error: true
```

### Test Execution Strategy
1. **Stable tests run first** - Quick feedback for developers
2. **Experimental tests run in parallel** - Don't block on unstable tests
3. **ROM tests skipped** - No dependency on external files
4. **Selective test execution** - Only run relevant tests for changes

## Test Development Guidelines

### Writing Stable Tests
- **Fast execution**: Aim for < 1 second per test
- **No external dependencies**: Self-contained test data
- **Deterministic**: Same results every run
- **Core functionality**: Test essential features only

### Writing ROM-Dependent Tests
- **Use TestRomManager**: Proper ROM file handling
- **Graceful skipping**: Skip if ROM not available
- **Real-world scenarios**: Test with actual game data
- **Label appropriately**: Always include ROM_DEPENDENT label

### Writing Experimental Tests
- **Complex scenarios**: Multi-component integration
- **Advanced features**: Emulation, complex algorithms
- **Performance tests**: May vary by system
- **GUI components**: May require display context

## E2E GUI Testing Framework

An agent-friendly, end-to-end testing framework built on `ImGuiTestEngine` to automate UI interaction testing for the YAZE editor.

### Architecture

**Test Execution Flow**:
1. `z3ed test-gui` command invokes the modified `yaze_test` executable
2. `yaze_test` initializes an application window and `ImGuiTestEngine`
3. Tests are registered and executed against the live GUI
4. Results are reported back with detailed logs and assertions

**Key Components**:
- `test/yaze_test.cc` - Main test executable with GUI initialization
- `test/e2e/framework_smoke_test.cc` - Basic infrastructure verification
- `test/e2e/canvas_selection_test.cc` - Canvas interaction tests
- `test/test_utils.h` - High-level action wrappers (LoadRomInTest, OpenEditorInTest, etc.)

### Writing E2E Tests

```cpp
// Example: Canvas selection test
#include "test/test_utils.h"

void RegisterCanvasSelectionTest() {
  ImGuiTestEngine* engine = ImGuiTestEngine_GetCurrentContext();
  
  ImGuiTest* t = IM_REGISTER_TEST(engine, "e2e", "canvas_selection");
  t->GuiFunc = [](ImGuiTestContext* ctx) {
    // Load ROM and open editor
    LoadRomInTest(ctx, "zelda3.sfc");
    OpenEditorInTest(ctx, "Overworld Editor");
    
    // Perform actions
    ctx->MouseMove("##OverworldCanvas");
    ctx->MouseClick(ImGuiMouseButton_Left);
    ctx->KeyPress(ImGuiMod_Ctrl | ImGuiKey_C);  // Copy
    
    // Assertions
    IM_CHECK(VerifyTileData(ctx, expected_data));
  };
}
```

### Running GUI Tests

```bash
# Run all E2E tests
z3ed test-gui --rom zelda3.sfc --test all

# Run specific test suite
z3ed test-gui --rom zelda3.sfc --test canvas_selection

# Run in headless mode (CI)
z3ed test-gui --rom zelda3.sfc --test all --headless
```
