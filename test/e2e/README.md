# End-to-End (E2E) Tests

This directory contains E2E tests using ImGui Test Engine to validate complete user workflows.

## Active Tests

### ✅ Working Tests

1. **framework_smoke_test.cc** - Basic framework validation
2. **canvas_selection_test.cc** - Canvas selection and copy/paste workflow  
3. **dungeon_editor_smoke_test.cc** - Dungeon editor UI navigation and interaction
4. **editor_smoke_tests.cc** - Graphics/Sprite/Message/Music/Emulator smoke tests
5. **overworld/overworld_e2e_test.cc** - Overworld editor workflows
6. **rom_dependent/e2e_rom_test.cc** - ROM-dependent functionality tests
7. **zscustomoverworld/zscustomoverworld_upgrade_test.cc** - ZSCustomOverworld upgrade tests

### 📝 Dungeon Editor Smoke Test

**File**: `dungeon_editor_smoke_test.cc`  
**Status**: ✅ Working and registered

Tests complete dungeon editor workflow:
- ROM loading
- Editor window opening
- Room selection (0x00, 0x01, 0x02)
- Canvas interaction
- Tab navigation (Object Selector, Room Graphics, Object Editor, Entrances)
- Mode button verification (Select, Insert, Edit)
- Detailed logging at each step

## Running Tests

### All E2E Tests (GUI Mode)
```bash
./build/bin/yaze_test --show-gui
```

### Specific Test Category
```bash
./build/bin/yaze_test --show-gui --gtest_filter="E2ETest*"
```

### Dungeon Editor Test Only
```bash
./build/bin/yaze_test --show-gui --gtest_filter="*DungeonEditorSmokeTest"
```

## Test Development

### Creating New Tests

Follow the pattern in `dungeon_editor_smoke_test.cc`:

```cpp
#include "e2e/my_new_test.h"
#include "test_utils.h"
#include "app/core/controller.h"

void E2ETest_MyNewTest(ImGuiTestContext* ctx) {
    // Load ROM
    yaze::test::gui::LoadRomInTest(ctx, yaze::test::TestRomManager::GetTestRomPath());
    
    // Open editor
    yaze::test::gui::OpenEditorInTest(ctx, "My Editor");
    
    // Test interactions with logging
    ctx->LogInfo("Starting test...");
    ctx->WindowFocus("My Editor");
    ctx->ItemClick("MyButton");
    ctx->LogInfo("Test completed");
}
```

### Register in yaze_test.cc

```cpp
#include "e2e/my_new_test.h"

// In RunGuiMode():
ImGuiTest* my_test = IM_REGISTER_TEST(engine, "E2ETest", "MyNewTest");
my_test->TestFunc = E2ETest_MyNewTest;
my_test->UserData = &controller;
```

### ImGui Test Engine API

Key methods available:
- `ctx->WindowFocus("WindowName")` - Focus a window
- `ctx->SetRef("WindowName")` - Set reference window for relative queries
- `ctx->ItemClick("ButtonName")` - Click an item
- `ctx->ItemExists("ItemName")` - Check if item exists
- `ctx->LogInfo("message", ...)` - Log information
- `ctx->LogWarning("message", ...)` - Log warning
- `ctx->LogError("message", ...)` - Log error
- `ctx->Yield()` - Yield to allow UI to update

Full API: `ext/imgui_test_engine/imgui_te_engine.h`

## Test Logging

Tests log detailed information during execution. View logs:
- In GUI mode: Check ImGui Test Engine window
- In CI mode: Check console output
- Look for lines starting with date/time stamps

Example log output:
```
2025-10-04 14:03:38 INFO: === Starting Dungeon Editor E2E Test ===
2025-10-04 14:03:38 INFO: Loading ROM...
2025-10-04 14:03:38 INFO: ROM loaded successfully
2025-10-04 14:03:38 INFO: Opening Dungeon Editor...
```

## Test Infrastructure

### File Organization
```
test/e2e/
├── README.md (this file)
├── framework_smoke_test.{cc,h}
├── canvas_selection_test.{cc,h}
├── dungeon_editor_smoke_test.{cc,h}  ← Latest dungeon test
├── editor_smoke_tests.{cc,h}         ← Graphics/Sprite/Message/Music/Emulator smoke tests
├── overworld/
│   └── overworld_e2e_test.cc
├── rom_dependent/
│   └── e2e_rom_test.cc
└── zscustomoverworld/
    └── zscustomoverworld_upgrade_test.cc
```

### Helper Functions

Available in `test_utils.h`:
- `yaze::test::gui::LoadRomInTest(ctx, yaze::test::TestRomManager::GetTestRomPath())` - Load ROM for testing
- `yaze::test::gui::OpenEditorInTest(ctx, "Editor Name")` - Open an editor window

## Future Test Ideas

Potential tests to add:
- [ ] Object placement workflow
- [ ] Object property editing
- [ ] Layer visibility toggling
- [ ] Save workflow validation
- [ ] Sprite editor workflow depth (properties, custom sprites)
- [ ] Palette editor workflows
- [ ] Music editor workflow depth (tracker, piano roll)

## Troubleshooting

### Test Crashes in GUI Mode
- Ensure ROM exists at `roms/alttp_vanilla.sfc` (or set `YAZE_TEST_ROM_VANILLA`)
- Check logs for specific error messages
- Try running without `--show-gui` first

### Tests Not Found
- Verify test is registered in `yaze_test.cc`
- Check that files are added to CMakeLists.txt
- Rebuild: `make -C build yaze_test`

### ImGui Items Not Found
- Use `ctx->ItemExists("ItemName")` to check availability
- Ensure window is focused with `ctx->WindowFocus()`
- Check actual widget IDs in source code (look for `##` suffixes)

## References

- **ImGui Test Engine**: `ext/imgui_test_engine/`
- **Test Registration**: `test/yaze_test.cc`
- **Test Utilities**: `test/test_utils.h`
- **Working Examples**: See existing tests in this directory

## Status

**Current State**: E2E testing infrastructure is working with 6+ active tests.
**Test Coverage**: Basic workflows covered; opportunity for expansion.
**Stability**: Tests run reliably in both GUI and CI modes.
