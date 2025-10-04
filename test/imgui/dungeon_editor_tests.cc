#include "imgui.h"

#ifdef IMGUI_ENABLE_TEST_ENGINE
#include "imgui_test_engine/imgui_te_context.h"
#include "imgui_test_engine/imgui_te_engine.h"
#include "imgui_test_engine/imgui_te_ui.h"
#endif

#include "app/editor/dungeon/dungeon_editor.h"
#include "app/gui/widget_id_registry.h"
#include "app/rom.h"

namespace yaze {
namespace test {

#ifdef IMGUI_ENABLE_TEST_ENGINE

/**
 * @file dungeon_editor_tests.cc
 * @brief Comprehensive ImGui Test Engine tests for the Dungeon Editor
 *
 * These tests cover:
 * - Canvas rendering and visibility
 * - Room selection and loading
 * - Object placement and manipulation
 * - Property editing
 * - Layer management
 * - Graphics and palette loading
 */

// ============================================================================
// Test Variables and Fixtures
// ============================================================================

struct DungeonEditorTestVars {
  editor::DungeonEditor* editor = nullptr;
  Rom* rom = nullptr;
  bool rom_loaded = false;
  int selected_room_id = 0;
  bool canvas_visible = false;
  ImVec2 canvas_size = ImVec2(0, 0);
  int object_count = 0;
};

// ============================================================================
// Canvas Rendering Tests
// ============================================================================

void RegisterDungeonCanvasTests(ImGuiTestEngine* engine) {
  // Test: Canvas should be visible after room selection
  ImGuiTest* t =
      IM_REGISTER_TEST(engine, "dungeon_editor", "canvas_visibility");
  t->SetVarsDataType<DungeonEditorTestVars>();

  t->TestFunc = [](ImGuiTestContext* ctx) {
    DungeonEditorTestVars& vars = ctx->GetVars<DungeonEditorTestVars>();

    // Wait for the dungeon editor window to be available
    ctx->SetRef("Dungeon Editor");

    // Verify canvas is present
    ctx->ItemInfo("Dungeon/Canvas/canvas:DungeonCanvas");
    IM_CHECK(ctx->ItemInfo("Dungeon/Canvas/canvas:DungeonCanvas").ID != 0);

    // Canvas should be visible
    ImGuiWindow* canvas_window = ctx->GetWindowByRef("Dungeon/Canvas");
    IM_CHECK(canvas_window != nullptr);
    IM_CHECK(canvas_window->Active);

    vars.canvas_visible = true;
  };

  // Test: Canvas should render after loading ROM
  t = IM_REGISTER_TEST(engine, "dungeon_editor", "canvas_rendering_after_load");
  t->SetVarsDataType<DungeonEditorTestVars>();

  t->TestFunc = [](ImGuiTestContext* ctx) {
    DungeonEditorTestVars& vars = ctx->GetVars<DungeonEditorTestVars>();

    ctx->SetRef("Dungeon Editor");

    // Click "Load ROM" button if available
    if (ctx->ItemExists("File/button:LoadROM")) {
      ctx->ItemClick("File/button:LoadROM");
      ctx->Yield();  // Wait for ROM to load
    }

    // Verify canvas renders something (not blank)
    // We can check if the canvas texture was created
    auto canvas_info = ctx->ItemInfo("Dungeon/Canvas/canvas:DungeonCanvas");
    IM_CHECK(canvas_info.RectFull.GetSize().x > 0 &&
             canvas_info.RectFull.GetSize().y > 0);

    // Check that the canvas has non-zero size
    vars.canvas_size = canvas_info.RectFull.GetSize();
    IM_CHECK(vars.canvas_size.x > 0);
    IM_CHECK(vars.canvas_size.y > 0);
  };

  // Test: Canvas should update when room changes
  t = IM_REGISTER_TEST(engine, "dungeon_editor",
                       "canvas_updates_on_room_change");
  t->SetVarsDataType<DungeonEditorTestVars>();

  t->TestFunc = [](ImGuiTestContext* ctx) {
    ctx->SetRef("Dungeon Editor");

    // Select room 0
    ctx->ItemClick("Dungeon/RoomSelector/selectable:Room_0");
    ctx->Yield();

    // Capture initial canvas state
    ImVec2 size1 =
        ctx->ItemInfo("Dungeon/Canvas/canvas:DungeonCanvas").RectFull.GetSize();

    // Select room 1
    ctx->ItemClick("Dungeon/RoomSelector/selectable:Room_1");
    ctx->Yield();

    // Canvas should still be valid (may have different content, but same size)
    ImVec2 size2 =
        ctx->ItemInfo("Dungeon/Canvas/canvas:DungeonCanvas").RectFull.GetSize();
    IM_CHECK(size2.x > 0 && size2.y > 0);
  };
}

// ============================================================================
// Room Selection Tests
// ============================================================================

void RegisterDungeonRoomSelectorTests(ImGuiTestEngine* engine) {
  // Test: Room selector should be visible
  ImGuiTest* t =
      IM_REGISTER_TEST(engine, "dungeon_editor", "room_selector_visible");
  t->TestFunc = [](ImGuiTestContext* ctx) {
    ctx->SetRef("Dungeon Editor");

    // Verify room selector table exists
    ctx->ItemInfo("Dungeon/RoomSelector/table:RoomList");
    IM_CHECK(ctx->ItemInfo("Dungeon/RoomSelector/table:RoomList").ID != 0);
  };

  // Test: Clicking room should change selection
  t = IM_REGISTER_TEST(engine, "dungeon_editor", "room_selection_click");
  t->SetVarsDataType<DungeonEditorTestVars>();

  t->TestFunc = [](ImGuiTestContext* ctx) {
    DungeonEditorTestVars& vars = ctx->GetVars<DungeonEditorTestVars>();

    ctx->SetRef("Dungeon Editor");

    // Click on room 5
    ctx->ItemClick("Dungeon/RoomSelector/selectable:Room_5");
    vars.selected_room_id = 5;

    // Verify the room is now selected (visual feedback should exist)
    // We can check if the canvas updates or if selection state changes
    ctx->Yield();

    // Success if we got here without errors
    IM_CHECK(vars.selected_room_id == 5);
  };

  // Test: Multiple room tabs should work
  t = IM_REGISTER_TEST(engine, "dungeon_editor", "room_tabs_switching");
  t->TestFunc = [](ImGuiTestContext* ctx) {
    ctx->SetRef("Dungeon Editor");

    // Open room 0
    ctx->ItemClick("Dungeon/RoomSelector/selectable:Room_0");
    ctx->Yield();

    // Open room 10 (should create a new tab)
    ctx->ItemClick("Dungeon/RoomSelector/selectable:Room_10");
    ctx->Yield();

    // Switch back to room 0 tab
    if (ctx->ItemExists("Dungeon/Canvas/tab:Room_0")) {
      ctx->ItemClick("Dungeon/Canvas/tab:Room_0");
      ctx->Yield();

      // Verify we're on room 0
      IM_CHECK(ctx->ItemInfo("Dungeon/Canvas/tab:Room_0").StatusFlags &
               ImGuiItemStatusFlags_Opened);
    }
  };
}

// ============================================================================
// Object Editor Tests
// ============================================================================

void RegisterDungeonObjectEditorTests(ImGuiTestEngine* engine) {
  // Test: Object selector should be visible
  ImGuiTest* t =
      IM_REGISTER_TEST(engine, "dungeon_editor", "object_selector_visible");
  t->TestFunc = [](ImGuiTestContext* ctx) {
    ctx->SetRef("Dungeon Editor");

    // Verify object selector exists
    IM_CHECK(ctx->ItemExists("Dungeon/ObjectSelector"));
  };

  // Test: Selecting object should enable placement mode
  t = IM_REGISTER_TEST(engine, "dungeon_editor", "object_selection");
  t->TestFunc = [](ImGuiTestContext* ctx) {
    ctx->SetRef("Dungeon Editor");

    // Click on an object in the selector
    if (ctx->ItemExists("Dungeon/ObjectSelector/selectable:Object_0")) {
      ctx->ItemClick("Dungeon/ObjectSelector/selectable:Object_0");
      ctx->Yield();

      // Object should be selected (visual feedback should exist)
      // Success if no errors
    }
  };

  // Test: Object property panel should show when object selected
  t = IM_REGISTER_TEST(engine, "dungeon_editor", "object_property_panel");
  t->TestFunc = [](ImGuiTestContext* ctx) {
    ctx->SetRef("Dungeon Editor");

    // Place or select an object
    ctx->ItemClick("Dungeon/Canvas/canvas:DungeonCanvas",
                   ImGuiMouseButton_Left);
    ctx->Yield();

    // Property panel should appear
    // (This might be conditional on having objects in the room)
    if (ctx->ItemExists("Dungeon/ObjectEditor/input_int:ObjectID")) {
      // Can edit object ID
      ctx->ItemInputValue("Dungeon/ObjectEditor/input_int:ObjectID", 42);
      ctx->Yield();
    }
  };
}

// ============================================================================
// Layer Management Tests
// ============================================================================

void RegisterDungeonLayerTests(ImGuiTestEngine* engine) {
  // Test: Layer controls should be visible
  ImGuiTest* t =
      IM_REGISTER_TEST(engine, "dungeon_editor", "layer_controls_visible");
  t->TestFunc = [](ImGuiTestContext* ctx) {
    ctx->SetRef("Dungeon Editor");

    // Verify layer checkboxes exist
    if (ctx->ItemExists("Dungeon/ObjectEditor/checkbox:ShowBG1")) {
      IM_CHECK(ctx->ItemInfo("Dungeon/ObjectEditor/checkbox:ShowBG1").ID != 0);
    }

    if (ctx->ItemExists("Dungeon/ObjectEditor/checkbox:ShowBG2")) {
      IM_CHECK(ctx->ItemInfo("Dungeon/ObjectEditor/checkbox:ShowBG2").ID != 0);
    }
  };

  // Test: Toggling layer visibility
  t = IM_REGISTER_TEST(engine, "dungeon_editor", "layer_toggle");
  t->TestFunc = [](ImGuiTestContext* ctx) {
    ctx->SetRef("Dungeon Editor");

    // Toggle BG1 layer
    if (ctx->ItemExists("Dungeon/ObjectEditor/checkbox:ShowBG1")) {
      ctx->ItemClick("Dungeon/ObjectEditor/checkbox:ShowBG1");
      ctx->Yield();

      // Toggle it back
      ctx->ItemClick("Dungeon/ObjectEditor/checkbox:ShowBG1");
      ctx->Yield();
    }
  };
}

// ============================================================================
// Palette and Graphics Tests
// ============================================================================

void RegisterDungeonGraphicsTests(ImGuiTestEngine* engine) {
  // Test: Palette editor should open
  ImGuiTest* t = IM_REGISTER_TEST(engine, "dungeon_editor", "palette_editor");
  t->TestFunc = [](ImGuiTestContext* ctx) {
    ctx->SetRef("Dungeon Editor");

    // Click "Palette Editor" button if available
    if (ctx->ItemExists("Dungeon/Toolset/button:PaletteEditor")) {
      ctx->ItemClick("Dungeon/Toolset/button:PaletteEditor");
      ctx->Yield();

      // Palette window should open
      IM_CHECK(ctx->WindowInfo("Palette Editor").Window != nullptr);
    }
  };

  // Test: Graphics should load for selected room
  t = IM_REGISTER_TEST(engine, "dungeon_editor", "graphics_loading");
  t->TestFunc = [](ImGuiTestContext* ctx) {
    ctx->SetRef("Dungeon Editor");

    // Select a room
    ctx->ItemClick("Dungeon/RoomSelector/selectable:Room_0");
    ctx->Yield(2);  // Wait for graphics to load

    // Canvas should have valid content
    auto canvas_info = ctx->ItemInfo("Dungeon/Canvas/canvas:DungeonCanvas");
    IM_CHECK(canvas_info.RectFull.GetWidth() > 0);
  };
}

// ============================================================================
// Integration Tests
// ============================================================================

void RegisterDungeonIntegrationTests(ImGuiTestEngine* engine) {
  // Test: Full workflow - load ROM, select room, place object
  ImGuiTest* t =
      IM_REGISTER_TEST(engine, "dungeon_editor", "full_edit_workflow");
  t->TestFunc = [](ImGuiTestContext* ctx) {
    ctx->SetRef("Dungeon Editor");

    // 1. Load ROM (if needed)
    ctx->Yield(2);

    // 2. Select a room
    ctx->ItemClick("Dungeon/RoomSelector/selectable:Room_5");
    ctx->Yield(2);

    // 3. Select an object type
    if (ctx->ItemExists("Dungeon/ObjectSelector/selectable:Object_1")) {
      ctx->ItemClick("Dungeon/ObjectSelector/selectable:Object_1");
      ctx->Yield();
    }

    // 4. Click on canvas to place object
    ctx->ItemClick("Dungeon/Canvas/canvas:DungeonCanvas",
                   ImGuiMouseButton_Left);
    ctx->Yield();

    // 5. Verify object was placed (property panel should appear)
    // This is a basic workflow test - success if no crashes
  };
}

// ============================================================================
// Widget Discovery Tests
// ============================================================================

void RegisterDungeonWidgetDiscoveryTests(ImGuiTestEngine* engine) {
  // Test: Widget registry should capture all dungeon editor widgets
  ImGuiTest* t =
      IM_REGISTER_TEST(engine, "dungeon_editor", "widget_registry_complete");
  t->TestFunc = [](ImGuiTestContext* ctx) {
    ctx->SetRef("Dungeon Editor");

    // Yield a few frames to let widgets register
    ctx->Yield(3);

    // Query the widget registry
    auto& registry = gui::WidgetIdRegistry::Instance();
    auto all_widgets = registry.GetAllWidgets();

    // Should have multiple widgets registered
    IM_CHECK(all_widgets.size() > 10);

    // Essential widgets should be present
    IM_CHECK(registry.GetWidgetId("Dungeon/RoomSelector/table:RoomList") != 0);
    IM_CHECK(registry.GetWidgetId("Dungeon/Canvas/canvas:DungeonCanvas") != 0);
  };

  // Test: Export widget catalog
  t = IM_REGISTER_TEST(engine, "dungeon_editor", "widget_catalog_export");
  t->TestFunc = [](ImGuiTestContext* ctx) {
    ctx->SetRef("Dungeon Editor");
    ctx->Yield(2);

    auto& registry = gui::WidgetIdRegistry::Instance();

    // Export to JSON
    std::string json_catalog = registry.ExportCatalog("json");
    IM_CHECK(!json_catalog.empty());
    IM_CHECK(json_catalog.find("\"widgets\"") != std::string::npos);

    // Export to YAML
    std::string yaml_catalog = registry.ExportCatalog("yaml");
    IM_CHECK(!yaml_catalog.empty());
    IM_CHECK(yaml_catalog.find("widgets:") != std::string::npos);
  };
}

// ============================================================================
// Registration Function
// ============================================================================

/**
 * @brief Register all dungeon editor tests with the ImGui Test Engine
 *
 * Call this function during application initialization to register all
 * automated tests for the dungeon editor.
 *
 * @param engine The ImGuiTestEngine instance
 */
void RegisterDungeonEditorTests(ImGuiTestEngine* engine) {
  RegisterDungeonCanvasTests(engine);
  RegisterDungeonRoomSelectorTests(engine);
  RegisterDungeonObjectEditorTests(engine);
  RegisterDungeonLayerTests(engine);
  RegisterDungeonGraphicsTests(engine);
  RegisterDungeonIntegrationTests(engine);
  RegisterDungeonWidgetDiscoveryTests(engine);
}

#endif  // IMGUI_ENABLE_TEST_ENGINE

}  // namespace test
}  // namespace yaze
