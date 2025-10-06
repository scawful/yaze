# A1 - Testing Guide

This guide provides a comprehensive overview of the testing framework for the yaze project, including unit tests, integration tests, and the end-to-end GUI automation system.

## 1. Test Categories

Tests are organized into three categories using labels to allow for flexible and efficient execution.

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

## 2. Running Tests

### Using CMake Presets

The easiest way to run tests is with `ctest` presets.

```bash
# Configure a development build (enables ROM-dependent tests)
cmake --preset mac-dev -DYAZE_TEST_ROM_PATH=/path/to/your/zelda3.sfc

# Build the tests
cmake --build --preset mac-dev --target yaze_test

# Run stable tests (fast, run in CI)
ctest --preset dev

# Run all tests, including ROM-dependent and experimental
ctest --preset all
```

### Manual Execution

You can also run tests by invoking the test executable directly or using CTest with labels.

```bash
# Run all tests via the executable
./build/bin/yaze_test

# Run only stable tests using CTest labels
ctest --test-dir build --label-regex "STABLE"

# Run tests matching a name
ctest --test-dir build -R "AsarWrapperTest"

# Exclude ROM-dependent tests
ctest --test-dir build --label-exclude "ROM_DEPENDENT"
```

## 3. Writing Tests

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

## 4. E2E GUI Testing Framework

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

### Widget Registration for Automation

All modern UI components are fully integrated with the ImGui Test Engine for seamless automation.

**Toolbar Components:**
All toolbar buttons are automatically registered with descriptive paths:
- `ModeButton:Pan (1)`
- `ModeButton:Draw (2)`
- `ToolbarAction:Toggle Tile16 Selector`
- `ToolbarAction:Open Tile16 Editor`

**Editor Cards:**
All EditorCard windows are registered as discoverable windows:
- `EditorCard:Tile16 Selector`
- `EditorCard:Area Graphics`

**State Introspection:**
Card visibility states are tracked for test automation:
- `OverworldToolbar/state:tile16_selector_visible`

### Running GUI Tests with `z3ed`

The `z3ed` CLI provides a powerful interface for running GUI tests.

```bash
# Run all E2E tests
z3ed gui replay --ci-mode

# Run a specific test suite from a YAML file
z3ed gui replay test_overworld_workflow.yaml --ci-mode

# Click a specific button
z3ed gui click "ModeButton:Draw (2)"

# Wait for a window to become visible
z3ed gui wait "window_visible:Tile16 Selector"

# Assert that a card is visible
z3ed gui assert "visible:EditorCard:Tile16 Selector"
```

### Widget Discovery

You can discover all registered UI elements for scripting and testing.

```bash
# List all widgets in the Overworld window
z3ed gui discover --window="Overworld" --format=yaml > overworld_widgets.yaml

# Find all toolbar actions
z3ed gui discover --path-prefix="ToolbarAction:"

# Find all editor cards
z3ed gui discover --path-prefix="EditorCard:"
```

### Integration with AI Agent

The agent can leverage the GUI testing framework to perform actions.

1.  **Discover UI Elements**: `Agent> describe gui`
2.  **Interact with UI**: `Agent> click toolbar button "Toggle Tile16 Selector"`
3.  **Monitor State**: `Agent> check if "Tile16 Selector" is visible`

### GUI Automation Scenarios

Scenarios are defined in JSONL files, where each line is a JSON action.

**Example Scenario (`overworld_single_tile_paint.jsonl`):**
```jsonl
{"action": "start_test", "name": "overworld_single_tile_paint", "description": "Paint single tile and verify"}
{"action": "setup", "rom": "zelda3.sfc", "window": "Overworld Editor", "map": 0}
{"action": "wait_for_window", "window_name": "Overworld Editor", "timeout_ms": 5000}
{"action": "select_tile", "tile_id": 66, "tile_id_hex": "0x0042"}
{"action": "click_canvas_tile", "canvas_id": "Overworld Canvas", "x": 10, "y": 10}
{"action": "assert_tile", "x": 10, "y": 10, "expected_tile_id": 66}
{"action": "end_test", "status": "pass"}
```

**Run a scenario:**
```bash
z3ed agent test replay overworld_single_tile_paint.jsonl --rom zelda3.sfc --grpc localhost:50051
```
