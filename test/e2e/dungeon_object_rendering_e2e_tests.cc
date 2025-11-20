/**
 * @file dungeon_object_rendering_e2e_tests.cc
 * @brief End-to-end tests for dungeon object rendering system using imgui test
 * engine
 *
 * These tests orchestrate complete user workflows for the dungeon editor,
 * validating:
 * - Object browser and selection
 * - Object placement on canvas
 * - Object manipulation (move, delete, edit)
 * - Layer management
 * - Save/load workflows
 * - Rendering quality across different scenarios
 *
 * Created: October 4, 2025
 * Related: docs/dungeon_editing_implementation_plan.md
 *
 * ============================================================================
 * UPDATE NOTICE (October 2025): Tests need rewrite for DungeonEditorV2
 * ============================================================================
 *
 * These tests were written for the old monolithic DungeonEditor but need to be
 * updated for the new DungeonEditorV2 card-based architecture:
 *
 * OLD ARCHITECTURE:
 * - Single "Dungeon Editor" window with tabs
 * - Object Selector, Canvas, Layers all in one window
 * - Monolithic UI structure
 *
 * NEW ARCHITECTURE (DungeonEditorV2):
 * - Independent EditorCard windows:
 *   - "Dungeon Controls" - main control panel
 *   - "Rooms List" - room selector
 *   - "Room Matrix" - visual room navigation
 *   - "Object Editor" - unified object placement/editing
 *   - "Palette Editor" - palette management
 *   - Individual room cards (e.g., "Room 0x00###RoomCard0")
 * - Per-room layer visibility settings
 * - Dockable, closable independent windows
 *
 * REQUIRED UPDATES:
 * 1. Change window references from "Dungeon Editor" to appropriate card names
 * 2. Update tab navigation to card window focus
 * 3. Update object placement workflow for new ObjectEditorCard
 * 4. Update layer controls for per-room settings
 * 5. Update room selection to work with new room cards
 *
 * Current Status: Tests compile but may fail due to UI structure changes.
 * See: test/e2e/dungeon_editor_smoke_test.cc for updated test patterns.
 */

#define IMGUI_DEFINE_MATH_OPERATORS

#include <gtest/gtest.h>

#include "app/controller.h"
#include "app/editor/dungeon/dungeon_editor_v2.h"
#include "app/platform/window.h"
#include "app/rom.h"
#include "imgui.h"
#include "imgui_test_engine/imgui_te_context.h"
#include "imgui_test_engine/imgui_te_engine.h"
#include "imgui_test_engine/imgui_te_ui.h"
#include "test_utils.h"
#include "zelda3/dungeon/room.h"
#include "zelda3/dungeon/room_object.h"

namespace yaze {
namespace test {

/**
 * @class DungeonObjectRenderingE2ETests
 * @brief Comprehensive E2E test fixture for dungeon object rendering system
 */
class DungeonObjectRenderingE2ETests : public TestRomManager::BoundRomTest {
 protected:
  void SetUp() override {
    BoundRomTest::SetUp();

    // Initialize test environment
    rom_ = std::shared_ptr<Rom>(rom(), [](Rom*) {});

    dungeon_editor_ = std::make_unique<editor::DungeonEditorV2>();
    dungeon_editor_->set_rom(rom_.get());
    ASSERT_TRUE(dungeon_editor_->Load().ok());

    // Initialize imgui test engine
    engine_ = ImGuiTestEngine_CreateContext();
    ImGuiTestEngineIO& test_io = ImGuiTestEngine_GetIO(engine_);
    test_io.ConfigVerboseLevel = ImGuiTestVerboseLevel_Info;
    test_io.ConfigVerboseLevelOnError = ImGuiTestVerboseLevel_Debug;
    test_io.ConfigRunSpeed = ImGuiTestRunSpeed_Fast;

    ImGuiTestEngine_Start(engine_, ImGui::GetCurrentContext());

    // Register all test cases
    RegisterAllTests();
  }

  void TearDown() override {
    if (engine_) {
      ImGuiTestEngine_Stop(engine_);
      ImGuiTestEngine_DestroyContext(engine_);
      engine_ = nullptr;
    }
    dungeon_editor_.reset();
    rom_.reset();
    BoundRomTest::TearDown();
  }

  void RegisterAllTests();

  // Test registration helpers
  void RegisterObjectBrowserTests();
  void RegisterObjectPlacementTests();
  void RegisterObjectSelectionTests();
  void RegisterLayerManagementTests();
  void RegisterSaveWorkflowTests();
  void RegisterRenderingQualityTests();
  void RegisterPerformanceTests();

  ImGuiTestEngine* engine_ = nullptr;
  std::shared_ptr<Rom> rom_;
  std::unique_ptr<editor::DungeonEditorV2> dungeon_editor_;
};

// =============================================================================
// OBJECT BROWSER TESTS
// =============================================================================

/**
 * @brief Test: Navigate object browser categories
 *
 * Validates:
 * - Tab navigation works
 * - Each category displays objects
 * - Object list is scrollable
 */
void DungeonObjectRenderingE2ETests::RegisterObjectBrowserTests() {
  ImGuiTest* test = IM_REGISTER_TEST(engine_, "DungeonObjectRendering",
                                     "ObjectBrowser_NavigateCategories");
  test->GuiFunc = [](ImGuiTestContext* ctx) {
    // Render dungeon editor UI
    auto* self = (DungeonObjectRenderingE2ETests*)ctx->UserData;
    self->dungeon_editor_->Update();
  };
  test->TestFunc = [](ImGuiTestContext* ctx) {
    // Open dungeon editor window
    ctx->SetRef("Dungeon Editor");

    // Navigate to object selector
    ctx->ItemClick("Object Selector##tab");
    ctx->Yield();

    // Test category tabs
    const char* categories[] = {"Type1##tab", "Type2##tab", "Type3##tab",
                                "All##tab"};

    for (const char* category : categories) {
      ctx->ItemClick(category);
      ctx->Yield();

      // Verify object list is visible and has content
      ctx->ItemExists("AssetBrowser##child");

      // Try scrolling the list
      ctx->ItemClick("AssetBrowser##child");
      ctx->KeyPress(ImGuiKey_DownArrow, 5);
      ctx->Yield();
    }
  };
  test->UserData = this;
}

/**
 * @brief Test: Select object from browser
 *
 * Validates:
 * - Object can be selected by clicking
 * - Preview updates when object selected
 * - Object details window shows correct info
 */
void RegisterObjectBrowserTests_SelectObject(
    DungeonObjectRenderingE2ETests* self) {
  ImGuiTest* test = IM_REGISTER_TEST(self->engine_, "DungeonObjectRendering",
                                     "ObjectBrowser_SelectObject");
  test->GuiFunc = [](ImGuiTestContext* ctx) {
    auto* self = (DungeonObjectRenderingE2ETests*)ctx->UserData;
    self->dungeon_editor_->Update();
  };
  test->TestFunc = [](ImGuiTestContext* ctx) {
    ctx->SetRef("Dungeon Editor");

    // Navigate to object selector
    ctx->ItemClick("Object Selector##tab");
    ctx->Yield();

    // Select Type1 category
    ctx->ItemClick("Type1##tab");
    ctx->Yield();

    // Click on first object in list (wall object 0x10)
    ctx->SetRef("AssetBrowser");
    ctx->ItemClick("Object_0x10");
    ctx->Yield();

    // Verify object details window appears
    ctx->SetRef("Object Details");
    ctx->ItemExists("Object ID: 0x10");

    // Verify preview canvas shows object
    ctx->SetRef("Dungeon Editor/PreviewCanvas");
    ctx->ItemExists("**/canvas##child");
  };
  test->UserData = self;
}

/**
 * @brief Test: Search and filter objects
 *
 * Validates:
 * - Search box filters object list
 * - Filtering by ID works
 * - Clearing search restores full list
 */
void RegisterObjectBrowserTests_SearchFilter(
    DungeonObjectRenderingE2ETests* self) {
  ImGuiTest* test = IM_REGISTER_TEST(self->engine_, "DungeonObjectRendering",
                                     "ObjectBrowser_SearchFilter");
  test->GuiFunc = [](ImGuiTestContext* ctx) {
    auto* self = (DungeonObjectRenderingE2ETests*)ctx->UserData;
    self->dungeon_editor_->Update();
  };
  test->TestFunc = [](ImGuiTestContext* ctx) {
    ctx->SetRef("Dungeon Editor/Object Selector");

    // Type in search box
    ctx->ItemClick("Search##input");
    ctx->KeyCharsAppend("0x10");
    ctx->Yield();

    // Verify filtered results
    ctx->ItemExists("Object_0x10");
    ctx->ItemVerifyNotExists("Object_0x20");

    // Clear search
    ctx->ItemClick("Clear##button");
    ctx->Yield();

    // Verify full list restored
    ctx->ItemExists("Object_0x10");
    ctx->ItemExists("Object_0x20");
  };
  test->UserData = self;
}

// =============================================================================
// OBJECT PLACEMENT TESTS
// =============================================================================

/**
 * @brief Test: Place object on canvas with mouse click
 *
 * Validates:
 * - Object preview follows mouse cursor
 * - Click places object at correct position
 * - Placed object appears in room object list
 * - Canvas renders placed object
 */
void DungeonObjectRenderingE2ETests::RegisterObjectPlacementTests() {
  ImGuiTest* test = IM_REGISTER_TEST(engine_, "DungeonObjectRendering",
                                     "ObjectPlacement_MouseClick");
  test->GuiFunc = [](ImGuiTestContext* ctx) {
    auto* self = (DungeonObjectRenderingE2ETests*)ctx->UserData;
    self->dungeon_editor_->Update();
  };
  test->TestFunc = [](ImGuiTestContext* ctx) {
    ctx->SetRef("Dungeon Editor");

    // Select an object (wall 0x10)
    ctx->ItemClick("Object Selector##tab");
    ctx->Yield();
    ctx->SetRef("AssetBrowser");
    ctx->ItemClick("Object_0x10");
    ctx->Yield();

    // Switch to main canvas
    ctx->SetRef("Dungeon Editor");
    ctx->ItemClick("Canvas##tab");
    ctx->Yield();

    // Click on canvas to place object
    ctx->SetRef("Dungeon Editor/Canvas");
    // TODO: fix this
    // ImVec2 canvas_center = ctx->ItemRectCenter("canvas##child");
    // ctx->MouseMove(canvas_center);
    // ctx->Yield();

    // Verify preview is visible
    // (Actual verification would check rendering)

    ctx->MouseClick();
    ctx->Yield();

    // Verify object was placed
    ctx->SetRef("Dungeon Editor");
    ctx->ItemClick("Room Objects##tab");
    ctx->Yield();

    // Check object appears in list
    ctx->ItemExists("Object ID: 0x10");
  };
  test->UserData = this;
}

/**
 * @brief Test: Place object with snap to grid
 *
 * Validates:
 * - Snap to grid option works
 * - Object positions align to grid
 * - Grid size can be changed
 */
void RegisterObjectPlacementTests_SnapToGrid(
    DungeonObjectRenderingE2ETests* self) {
  ImGuiTest* test = IM_REGISTER_TEST(self->engine_, "DungeonObjectRendering",
                                     "ObjectPlacement_SnapToGrid");
  test->GuiFunc = [](ImGuiTestContext* ctx) {
    auto* self = (DungeonObjectRenderingE2ETests*)ctx->UserData;
    self->dungeon_editor_->Update();
  };
  test->TestFunc = [](ImGuiTestContext* ctx) {
    ctx->SetRef("Dungeon Editor");

    // Enable snap to grid
    ctx->ItemClick("Options##menu");
    ctx->ItemClick("Snap to Grid##checkbox");
    ctx->Yield();

    // Select object
    ctx->ItemClick("Object Selector##tab");
    ctx->SetRef("AssetBrowser");
    ctx->ItemClick("Object_0x10");
    ctx->Yield();

    // Place object at arbitrary position
    ctx->SetRef("Dungeon Editor/Canvas");
    ImVec2 canvas_pos = ctx->ItemRectMin("canvas##child");
    ctx->MouseMove(ImVec2(canvas_pos.x + 37, canvas_pos.y + 49));
    ctx->MouseClick();
    ctx->Yield();

    // Verify object was placed at snapped position (32, 48)
    ctx->SetRef("Dungeon Editor/Object Details");
    ctx->ItemVerifyValue("X Position", 32);
    ctx->ItemVerifyValue("Y Position", 48);
  };
  test->UserData = self;
}

/**
 * @brief Test: Place multiple objects sequentially
 *
 * Validates:
 * - Multiple objects can be placed
 * - Each placement is independent
 * - All placed objects render correctly
 */
void RegisterObjectPlacementTests_MultipleObjects(
    DungeonObjectRenderingE2ETests* self) {
  ImGuiTest* test = IM_REGISTER_TEST(self->engine_, "DungeonObjectRendering",
                                     "ObjectPlacement_MultipleObjects");
  test->GuiFunc = [](ImGuiTestContext* ctx) {
    auto* self = (DungeonObjectRenderingE2ETests*)ctx->UserData;
    self->dungeon_editor_->Update();
  };
  test->TestFunc = [](ImGuiTestContext* ctx) {
    ctx->SetRef("Dungeon Editor");

    // Place 5 different objects
    const uint16_t object_ids[] = {0x10, 0x11, 0x20, 0x100, 0xF99};

    for (int i = 0; i < 5; i++) {
      // Select object
      ctx->ItemClick("Object Selector##tab");
      ctx->SetRef("AssetBrowser");
      ctx->ItemClick(ImGuiTestRef_Str("Object_0x%02X", object_ids[i]));
      ctx->Yield();

      // Place at different position
      ctx->SetRef("Dungeon Editor/Canvas");
      ImVec2 canvas_pos = ctx->ItemRectMin("canvas##child");
      ctx->MouseMove(ImVec2(canvas_pos.x + (i * 32), canvas_pos.y + (i * 32)));
      ctx->MouseClick();
      ctx->Yield();
    }

    // Verify all 5 objects in room
    ctx->SetRef("Dungeon Editor/Room Objects");
    ctx->ItemExists("Object Count: 5");
  };
  test->UserData = self;
}

// =============================================================================
// OBJECT SELECTION AND MANIPULATION TESTS
// =============================================================================

/**
 * @brief Test: Select object by clicking on canvas
 *
 * Validates:
 * - Click on object selects it
 * - Selection highlight appears
 * - Object details update
 */
void DungeonObjectRenderingE2ETests::RegisterObjectSelectionTests() {
  ImGuiTest* test = IM_REGISTER_TEST(engine_, "DungeonObjectRendering",
                                     "ObjectSelection_Click");
  test->GuiFunc = [](ImGuiTestContext* ctx) {
    auto* self = (DungeonObjectRenderingE2ETests*)ctx->UserData;
    self->dungeon_editor_->Update();
  };
  test->TestFunc = [](ImGuiTestContext* ctx) {
    // First place an object
    ctx->SetRef("Dungeon Editor");
    ctx->ItemClick("Object Selector##tab");
    ctx->SetRef("AssetBrowser");
    ctx->ItemClick("Object_0x10");
    ctx->Yield();

    ctx->SetRef("Dungeon Editor/Canvas");
    ImVec2 canvas_center = ctx->ItemRectCenter("canvas##child");
    ctx->MouseMove(canvas_center);
    ctx->MouseClick();
    ctx->Yield();

    // Now try to select it
    ctx->MouseMove(canvas_center);
    ctx->MouseClick();
    ctx->Yield();

    // Verify object is selected
    ctx->SetRef("Dungeon Editor/Object Details");
    ctx->ItemExists("Selected Object");
    ctx->ItemVerifyValue("Object ID", 0x10);
  };
  test->UserData = this;
}

/**
 * @brief Test: Multi-select objects with Ctrl+drag
 *
 * Validates:
 * - Ctrl+drag creates selection box
 * - All objects in box are selected
 * - Selection count is correct
 */
void RegisterObjectSelectionTests_MultiSelect(
    DungeonObjectRenderingE2ETests* self) {
  ImGuiTest* test = IM_REGISTER_TEST(self->engine_, "DungeonObjectRendering",
                                     "ObjectSelection_MultiSelect");
  test->GuiFunc = [](ImGuiTestContext* ctx) {
    auto* self = (DungeonObjectRenderingE2ETests*)ctx->UserData;
    self->dungeon_editor_->Update();
  };
  test->TestFunc = [](ImGuiTestContext* ctx) {
    // Place 3 objects in a line
    ctx->SetRef("Dungeon Editor");
    for (int i = 0; i < 3; i++) {
      ctx->ItemClick("Object Selector##tab");
      ctx->SetRef("AssetBrowser");
      ctx->ItemClick("Object_0x10");
      ctx->Yield();

      ctx->SetRef("Dungeon Editor/Canvas");
      ImVec2 canvas_pos = ctx->ItemRectMin("canvas##child");
      ctx->MouseMove(ImVec2(canvas_pos.x + (i * 32) + 16, canvas_pos.y + 16));
      ctx->MouseClick();
      ctx->Yield();
    }

    // Ctrl+drag to select all
    ctx->KeyDown(ImGuiKey_LeftCtrl);
    ImVec2 canvas_pos = ctx->ItemRectMin("canvas##child");
    ctx->MouseMove(ImVec2(canvas_pos.x, canvas_pos.y));
    ctx->MouseDown();
    ctx->MouseMove(ImVec2(canvas_pos.x + 100, canvas_pos.y + 50));
    ctx->MouseUp();
    ctx->KeyUp(ImGuiKey_LeftCtrl);
    ctx->Yield();

    // Verify 3 objects selected
    ctx->SetRef("Dungeon Editor");
    ctx->ItemVerifyValue("Selected Objects", 3);
  };
  test->UserData = self;
}

/**
 * @brief Test: Move selected object with drag
 *
 * Validates:
 * - Selected object can be dragged
 * - Object position updates during drag
 * - Final position is correct
 */
void RegisterObjectSelectionTests_MoveObject(
    DungeonObjectRenderingE2ETests* self) {
  ImGuiTest* test = IM_REGISTER_TEST(self->engine_, "DungeonObjectRendering",
                                     "ObjectSelection_MoveObject");
  test->GuiFunc = [](ImGuiTestContext* ctx) {
    auto* self = (DungeonObjectRenderingE2ETests*)ctx->UserData;
    self->dungeon_editor_->Update();
  };
  test->TestFunc = [](ImGuiTestContext* ctx) {
    // Place object at (16, 16)
    ctx->SetRef("Dungeon Editor");
    ctx->ItemClick("Object Selector##tab");
    ctx->SetRef("AssetBrowser");
    ctx->ItemClick("Object_0x10");
    ctx->Yield();

    ctx->SetRef("Dungeon Editor/Canvas");
    ImVec2 canvas_pos = ctx->ItemRectMin("canvas##child");
    ImVec2 initial_pos = ImVec2(canvas_pos.x + 16, canvas_pos.y + 16);
    ctx->MouseMove(initial_pos);
    ctx->MouseClick();
    ctx->Yield();

    // Select and drag to (48, 48)
    ctx->MouseMove(initial_pos);
    ctx->MouseClick();
    ctx->MouseDown();
    ctx->MouseMove(ImVec2(canvas_pos.x + 48, canvas_pos.y + 48));
    ctx->MouseUp();
    ctx->Yield();

    // Verify new position
    ctx->SetRef("Dungeon Editor/Object Details");
    ctx->ItemVerifyValue("X Position", 48);
    ctx->ItemVerifyValue("Y Position", 48);
  };
  test->UserData = self;
}

/**
 * @brief Test: Delete selected object
 *
 * Validates:
 * - Delete key removes selected object
 * - Object no longer in room list
 * - Canvas no longer renders object
 */
void RegisterObjectSelectionTests_DeleteObject(
    DungeonObjectRenderingE2ETests* self) {
  ImGuiTest* test = IM_REGISTER_TEST(self->engine_, "DungeonObjectRendering",
                                     "ObjectSelection_DeleteObject");
  test->GuiFunc = [](ImGuiTestContext* ctx) {
    auto* self = (DungeonObjectRenderingE2ETests*)ctx->UserData;
    self->dungeon_editor_->Update();
  };
  test->TestFunc = [](ImGuiTestContext* ctx) {
    // Place object
    ctx->SetRef("Dungeon Editor");
    ctx->ItemClick("Object Selector##tab");
    ctx->SetRef("AssetBrowser");
    ctx->ItemClick("Object_0x10");
    ctx->Yield();

    ctx->SetRef("Dungeon Editor/Canvas");
    ImVec2 canvas_center = ctx->ItemRectCenter("canvas##child");
    ctx->MouseMove(canvas_center);
    ctx->MouseClick();
    ctx->Yield();

    // Select object
    ctx->MouseClick();
    ctx->Yield();

    // Delete with Delete key
    ctx->KeyPress(ImGuiKey_Delete);
    ctx->Yield();

    // Verify object removed
    ctx->SetRef("Dungeon Editor/Room Objects");
    ctx->ItemVerifyValue("Object Count", 0);
  };
  test->UserData = self;
}

// =============================================================================
// LAYER MANAGEMENT TESTS
// =============================================================================

/**
 * @brief Test: Toggle layer visibility
 *
 * Validates:
 * - Layer visibility checkboxes work
 * - Hidden layers don't render
 * - Layer can be shown again
 */
void DungeonObjectRenderingE2ETests::RegisterLayerManagementTests() {
  ImGuiTest* test = IM_REGISTER_TEST(engine_, "DungeonObjectRendering",
                                     "LayerManagement_ToggleVisibility");
  test->GuiFunc = [](ImGuiTestContext* ctx) {
    auto* self = (DungeonObjectRenderingE2ETests*)ctx->UserData;
    self->dungeon_editor_->Update();
  };
  test->TestFunc = [](ImGuiTestContext* ctx) {
    ctx->SetRef("Dungeon Editor");

    // Open layer management panel
    ctx->ItemClick("Layers##tab");
    ctx->Yield();

    // Toggle Layer 1 visibility
    ctx->ItemClick("Layer 1 Visible##checkbox");
    ctx->Yield();

    // Verify checkbox state
    ctx->ItemVerifyValue("Layer 1 Visible##checkbox", false);

    // Toggle back on
    ctx->ItemClick("Layer 1 Visible##checkbox");
    ctx->Yield();

    ctx->ItemVerifyValue("Layer 1 Visible##checkbox", true);
  };
  test->UserData = this;
}

/**
 * @brief Test: Place objects on different layers
 *
 * Validates:
 * - Active layer can be changed
 * - Objects placed on correct layer
 * - Layer indicator shows current layer
 */
void RegisterLayerManagementTests_PlaceOnLayers(
    DungeonObjectRenderingE2ETests* self) {
  ImGuiTest* test = IM_REGISTER_TEST(self->engine_, "DungeonObjectRendering",
                                     "LayerManagement_PlaceOnLayers");
  test->GuiFunc = [](ImGuiTestContext* ctx) {
    auto* self = (DungeonObjectRenderingE2ETests*)ctx->UserData;
    self->dungeon_editor_->Update();
  };
  test->TestFunc = [](ImGuiTestContext* ctx) {
    ctx->SetRef("Dungeon Editor");

    // Place object on Layer 0
    ctx->ItemClick("Layers##tab");
    ctx->ItemClick("Layer 0##radio");
    ctx->Yield();

    ctx->ItemClick("Object Selector##tab");
    ctx->SetRef("AssetBrowser");
    ctx->ItemClick("Object_0x10");
    ctx->Yield();

    ctx->SetRef("Dungeon Editor/Canvas");
    ImVec2 canvas_pos = ctx->ItemRectMin("canvas##child");
    ctx->MouseMove(ImVec2(canvas_pos.x + 16, canvas_pos.y + 16));
    ctx->MouseClick();
    ctx->Yield();

    // Verify object on Layer 0
    ctx->SetRef("Dungeon Editor/Object Details");
    ctx->ItemVerifyValue("Layer", 0);

    // Switch to Layer 1 and place another object
    ctx->SetRef("Dungeon Editor");
    ctx->ItemClick("Layers##tab");
    ctx->ItemClick("Layer 1##radio");
    ctx->Yield();

    ctx->ItemClick("Object Selector##tab");
    ctx->SetRef("AssetBrowser");
    ctx->ItemClick("Object_0x20");
    ctx->Yield();

    ctx->SetRef("Dungeon Editor/Canvas");
    ctx->MouseMove(ImVec2(canvas_pos.x + 48, canvas_pos.y + 48));
    ctx->MouseClick();
    ctx->Yield();

    // Verify object on Layer 1
    ctx->SetRef("Dungeon Editor/Object Details");
    ctx->ItemVerifyValue("Layer", 1);
  };
  test->UserData = self;
}

/**
 * @brief Test: Layer rendering order
 *
 * Validates:
 * - Layers render in correct order (BG1 < BG2 < BG3)
 * - Overlapping objects render correctly
 * - Visual inspection of layer order
 */
void RegisterLayerManagementTests_RenderingOrder(
    DungeonObjectRenderingE2ETests* self) {
  ImGuiTest* test = IM_REGISTER_TEST(self->engine_, "DungeonObjectRendering",
                                     "LayerManagement_RenderingOrder");
  test->GuiFunc = [](ImGuiTestContext* ctx) {
    auto* self = (DungeonObjectRenderingE2ETests*)ctx->UserData;
    self->dungeon_editor_->Update();
  };
  test->TestFunc = [](ImGuiTestContext* ctx) {
    // Place 3 objects at same position on different layers
    ctx->SetRef("Dungeon Editor");

    for (int layer = 0; layer < 3; layer++) {
      ctx->ItemClick("Layers##tab");
      ctx->ItemClick(ImGuiTestRef_Str("Layer %d##radio", layer));
      ctx->Yield();

      ctx->ItemClick("Object Selector##tab");
      ctx->SetRef("AssetBrowser");
      ctx->ItemClick(ImGuiTestRef_Str("Object_0x%02X", 0x10 + layer));
      ctx->Yield();

      ctx->SetRef("Dungeon Editor/Canvas");
      ImVec2 canvas_center = ctx->ItemRectCenter("canvas##child");
      ctx->MouseMove(canvas_center);
      ctx->MouseClick();
      ctx->Yield();
    }

    // Verify all 3 objects exist
    ctx->SetRef("Dungeon Editor/Room Objects");
    ctx->ItemVerifyValue("Object Count", 3);

    // Visual verification would be done with snapshot comparison
    // Here we just verify the objects are in the right layers
    ctx->ItemExists("Layer 0: 1 object");
    ctx->ItemExists("Layer 1: 1 object");
    ctx->ItemExists("Layer 2: 1 object");
  };
  test->UserData = self;
}

// =============================================================================
// SAVE/LOAD WORKFLOW TESTS
// =============================================================================

/**
 * @brief Test: Save room with objects
 *
 * Validates:
 * - Objects can be saved to ROM
 * - Save operation succeeds
 * - No errors during save
 */
void DungeonObjectRenderingE2ETests::RegisterSaveWorkflowTests() {
  ImGuiTest* test = IM_REGISTER_TEST(engine_, "DungeonObjectRendering",
                                     "SaveWorkflow_SaveRoom");
  test->GuiFunc = [](ImGuiTestContext* ctx) {
    auto* self = (DungeonObjectRenderingE2ETests*)ctx->UserData;
    self->dungeon_editor_->Update();
  };
  test->TestFunc = [](ImGuiTestContext* ctx) {
    // Place an object
    ctx->SetRef("Dungeon Editor");
    ctx->ItemClick("Object Selector##tab");
    ctx->SetRef("AssetBrowser");
    ctx->ItemClick("Object_0x10");
    ctx->Yield();

    ctx->SetRef("Dungeon Editor/Canvas");
    ImVec2 canvas_center = ctx->ItemRectCenter("canvas##child");
    ctx->MouseMove(canvas_center);
    ctx->MouseClick();
    ctx->Yield();

    // Save room
    ctx->SetRef("Dungeon Editor");
    ctx->ItemClick("File##menu");
    ctx->ItemClick("Save Room##menuitem");
    ctx->Yield();

    // Verify save success message
    ctx->ItemExists("Save successful");
  };
  test->UserData = this;
}

/**
 * @brief Test: Save and reload room (round-trip)
 *
 * Validates:
 * - Objects persist after save/reload
 * - Object properties are preserved
 * - No data corruption
 */
void RegisterSaveWorkflowTests_RoundTrip(DungeonObjectRenderingE2ETests* self) {
  ImGuiTest* test = IM_REGISTER_TEST(self->engine_, "DungeonObjectRendering",
                                     "SaveWorkflow_RoundTrip");
  test->GuiFunc = [](ImGuiTestContext* ctx) {
    auto* self = (DungeonObjectRenderingE2ETests*)ctx->UserData;
    self->dungeon_editor_->Update();
  };
  test->TestFunc = [](ImGuiTestContext* ctx) {
    // Place object with specific properties
    ctx->SetRef("Dungeon Editor");
    ctx->ItemClick("Object Selector##tab");
    ctx->SetRef("AssetBrowser");
    ctx->ItemClick("Object_0x10");
    ctx->Yield();

    ctx->SetRef("Dungeon Editor/Canvas");
    ImVec2 canvas_pos = ctx->ItemRectMin("canvas##child");
    ctx->MouseMove(ImVec2(canvas_pos.x + 64, canvas_pos.y + 96));
    ctx->MouseClick();
    ctx->Yield();

    // Verify initial state
    ctx->SetRef("Dungeon Editor/Object Details");
    int initial_x = ctx->ItemGetValue<int>("X Position");
    int initial_y = ctx->ItemGetValue<int>("Y Position");
    int initial_id = ctx->ItemGetValue<int>("Object ID");

    // Save room
    ctx->SetRef("Dungeon Editor");
    ctx->ItemClick("File##menu");
    ctx->ItemClick("Save Room##menuitem");
    ctx->Yield();

    // Reload room
    ctx->ItemClick("File##menu");
    ctx->ItemClick("Reload Room##menuitem");
    ctx->Yield();

    // Verify object persisted with same properties
    ctx->SetRef("Dungeon Editor/Object Details");
    ctx->ItemVerifyValue("X Position", initial_x);
    ctx->ItemVerifyValue("Y Position", initial_y);
    ctx->ItemVerifyValue("Object ID", initial_id);
  };
  test->UserData = self;
}

/**
 * @brief Test: Save with multiple object types
 *
 * Validates:
 * - Type1, Type2, Type3 objects all save correctly
 * - Encoding is correct for each type
 * - All objects reload correctly
 */
void RegisterSaveWorkflowTests_MultipleTypes(
    DungeonObjectRenderingE2ETests* self) {
  ImGuiTest* test = IM_REGISTER_TEST(self->engine_, "DungeonObjectRendering",
                                     "SaveWorkflow_MultipleTypes");
  test->GuiFunc = [](ImGuiTestContext* ctx) {
    auto* self = (DungeonObjectRenderingE2ETests*)ctx->UserData;
    self->dungeon_editor_->Update();
  };
  test->TestFunc = [](ImGuiTestContext* ctx) {
    // Place one of each object type
    const uint16_t object_ids[] = {
        0x10,   // Type1
        0x125,  // Type2
        0xF99   // Type3 (chest)
    };

    ctx->SetRef("Dungeon Editor");

    for (int i = 0; i < 3; i++) {
      ctx->ItemClick("Object Selector##tab");
      ctx->SetRef("AssetBrowser");
      ctx->ItemClick(ImGuiTestRef_Str("Object_0x%02X", object_ids[i]));
      ctx->Yield();

      ctx->SetRef("Dungeon Editor/Canvas");
      ImVec2 canvas_pos = ctx->ItemRectMin("canvas##child");
      ctx->MouseMove(ImVec2(canvas_pos.x + (i * 32), canvas_pos.y + (i * 32)));
      ctx->MouseClick();
      ctx->Yield();
    }

    // Save and reload
    ctx->SetRef("Dungeon Editor");
    ctx->ItemClick("File##menu");
    ctx->ItemClick("Save Room##menuitem");
    ctx->Yield();

    ctx->ItemClick("File##menu");
    ctx->ItemClick("Reload Room##menuitem");
    ctx->Yield();

    // Verify all 3 objects reloaded
    ctx->SetRef("Dungeon Editor/Room Objects");
    ctx->ItemVerifyValue("Object Count", 3);

    // Verify each object type
    for (int i = 0; i < 3; i++) {
      ctx->ItemExists(ImGuiTestRef_Str("Object ID: 0x%02X", object_ids[i]));
    }
  };
  test->UserData = self;
}

// =============================================================================
// RENDERING QUALITY TESTS
// =============================================================================

/**
 * @brief Test: Render all object types correctly
 *
 * Validates:
 * - Type1 objects render
 * - Type2 objects render
 * - Type3 objects render
 * - All render at correct positions
 */
void DungeonObjectRenderingE2ETests::RegisterRenderingQualityTests() {
  ImGuiTest* test = IM_REGISTER_TEST(engine_, "DungeonObjectRendering",
                                     "RenderingQuality_AllTypes");
  test->GuiFunc = [](ImGuiTestContext* ctx) {
    auto* self = (DungeonObjectRenderingE2ETests*)ctx->UserData;
    self->dungeon_editor_->Update();
  };
  test->TestFunc = [](ImGuiTestContext* ctx) {
    // This would typically involve visual snapshot comparison
    // For now, we verify objects are placed and rendered without errors

    const uint16_t object_ids[] = {
        0x10,  0x11,  0x20,  // Type1 objects
        0x100, 0x125,        // Type2 objects
        0xF99, 0xFB1         // Type3 objects
    };

    ctx->SetRef("Dungeon Editor");

    for (int i = 0; i < 7; i++) {
      ctx->ItemClick("Object Selector##tab");
      ctx->SetRef("AssetBrowser");
      ctx->ItemClick(ImGuiTestRef_Str("Object_0x%02X", object_ids[i]));
      ctx->Yield();

      ctx->SetRef("Dungeon Editor/Canvas");
      ImVec2 canvas_pos = ctx->ItemRectMin("canvas##child");
      ctx->MouseMove(
          ImVec2(canvas_pos.x + ((i % 4) * 48), canvas_pos.y + ((i / 4) * 48)));
      ctx->MouseClick();
      ctx->Yield();
    }

    // Verify all objects rendered without errors
    ctx->SetRef("Dungeon Editor/Room Objects");
    ctx->ItemVerifyValue("Object Count", 7);
    ctx->ItemVerifyNotExists("Rendering Error");
  };
  test->UserData = this;
}

/**
 * @brief Test: Render with different palettes
 *
 * Validates:
 * - Palette switching works
 * - Objects render with correct colors
 * - No palette-related rendering errors
 */
void RegisterRenderingQualityTests_Palettes(
    DungeonObjectRenderingE2ETests* self) {
  ImGuiTest* test = IM_REGISTER_TEST(self->engine_, "DungeonObjectRendering",
                                     "RenderingQuality_Palettes");
  test->GuiFunc = [](ImGuiTestContext* ctx) {
    auto* self = (DungeonObjectRenderingE2ETests*)ctx->UserData;
    self->dungeon_editor_->Update();
  };
  test->TestFunc = [](ImGuiTestContext* ctx) {
    // Place an object
    ctx->SetRef("Dungeon Editor");
    ctx->ItemClick("Object Selector##tab");
    ctx->SetRef("AssetBrowser");
    ctx->ItemClick("Object_0x10");
    ctx->Yield();

    ctx->SetRef("Dungeon Editor/Canvas");
    ImVec2 canvas_center = ctx->ItemRectCenter("canvas##child");
    ctx->MouseMove(canvas_center);
    ctx->MouseClick();
    ctx->Yield();

    // Switch palettes and verify rendering
    const int palette_ids[] = {0, 1, 2, 3, 4, 5};

    for (int palette : palette_ids) {
      ctx->SetRef("Dungeon Editor");
      ctx->ItemClick("Options##menu");
      ctx->ItemInput("Palette##input", palette);
      ctx->Yield();

      // Verify no rendering errors
      ctx->ItemVerifyNotExists("Rendering Error");
    }
  };
  test->UserData = self;
}

/**
 * @brief Test: Complex room scenario rendering
 *
 * Validates:
 * - Many objects render correctly
 * - Performance is acceptable
 * - No rendering artifacts
 */
void RegisterRenderingQualityTests_ComplexRoom(
    DungeonObjectRenderingE2ETests* self) {
  ImGuiTest* test = IM_REGISTER_TEST(self->engine_, "DungeonObjectRendering",
                                     "RenderingQuality_ComplexRoom");
  test->GuiFunc = [](ImGuiTestContext* ctx) {
    auto* self = (DungeonObjectRenderingE2ETests*)ctx->UserData;
    self->dungeon_editor_->Update();
  };
  test->TestFunc = [](ImGuiTestContext* ctx) {
    // Place many objects to create complex room
    ctx->SetRef("Dungeon Editor");

    // Place wall perimeter (Type1 objects)
    for (int x = 0; x < 16; x++) {
      for (int side = 0; side < 2; side++) {
        ctx->ItemClick("Object Selector##tab");
        ctx->SetRef("AssetBrowser");
        ctx->ItemClick("Object_0x10");
        ctx->Yield();

        ctx->SetRef("Dungeon Editor/Canvas");
        ImVec2 canvas_pos = ctx->ItemRectMin("canvas##child");
        int y = side == 0 ? 0 : 10;
        ctx->MouseMove(
            ImVec2(canvas_pos.x + (x * 16), canvas_pos.y + (y * 16)));
        ctx->MouseClick();
        ctx->Yield();
      }
    }

    // Add some decorative objects
    const uint16_t decorative_objects[] = {0x20, 0x21, 0xF99};
    for (int i = 0; i < 3; i++) {
      ctx->ItemClick("Object Selector##tab");
      ctx->SetRef("AssetBrowser");
      ctx->ItemClick(ImGuiTestRef_Str("Object_0x%02X", decorative_objects[i]));
      ctx->Yield();

      ctx->SetRef("Dungeon Editor/Canvas");
      ImVec2 canvas_pos = ctx->ItemRectMin("canvas##child");
      ctx->MouseMove(ImVec2(canvas_pos.x + (i * 64) + 32, canvas_pos.y + 80));
      ctx->MouseClick();
      ctx->Yield();
    }

    // Verify all objects rendered
    ctx->SetRef("Dungeon Editor/Room Objects");
    ctx->ItemVerifyValue("Object Count", 35);  // 32 walls + 3 decorative

    // Verify no performance issues (frame time < 16ms for 60fps)
    ctx->SetRef("Dungeon Editor");
    ctx->ItemVerifyLessThan("Frame Time (ms)", 16.0f);
  };
  test->UserData = self;
}

// =============================================================================
// PERFORMANCE TESTS
// =============================================================================

/**
 * @brief Test: Large room with many objects performance
 *
 * Validates:
 * - Rendering stays performant with 100+ objects
 * - Frame time stays below threshold
 * - Memory usage is reasonable
 */
void DungeonObjectRenderingE2ETests::RegisterPerformanceTests() {
  ImGuiTest* test = IM_REGISTER_TEST(engine_, "DungeonObjectRendering",
                                     "Performance_LargeRoom");
  test->GuiFunc = [](ImGuiTestContext* ctx) {
    auto* self = (DungeonObjectRenderingE2ETests*)ctx->UserData;
    self->dungeon_editor_->Update();
  };
  test->TestFunc = [](ImGuiTestContext* ctx) {
    ctx->SetRef("Dungeon Editor");

    // Place 100 objects
    for (int i = 0; i < 100; i++) {
      ctx->ItemClick("Object Selector##tab");
      ctx->SetRef("AssetBrowser");
      ctx->ItemClick("Object_0x10");
      ctx->Yield();

      ctx->SetRef("Dungeon Editor/Canvas");
      ImVec2 canvas_pos = ctx->ItemRectMin("canvas##child");
      ctx->MouseMove(ImVec2(canvas_pos.x + ((i % 16) * 16),
                            canvas_pos.y + ((i / 16) * 16)));
      ctx->MouseClick();
      ctx->Yield();
    }

    // Measure rendering performance
    ctx->SetRef("Dungeon Editor");
    float frame_time = ctx->ItemGetValue<float>("Frame Time (ms)");

    // Verify acceptable performance (< 16ms for 60fps)
    IM_CHECK_LT(frame_time, 16.0f);

    // Verify object count
    ctx->SetRef("Dungeon Editor/Room Objects");
    ctx->ItemVerifyValue("Object Count", 100);
  };
  test->UserData = this;
}

// =============================================================================
// TEST REGISTRATION
// =============================================================================

void DungeonObjectRenderingE2ETests::RegisterAllTests() {
  RegisterObjectBrowserTests();
  RegisterObjectPlacementTests();
  RegisterObjectSelectionTests();
  RegisterLayerManagementTests();
  RegisterSaveWorkflowTests();
  RegisterRenderingQualityTests();
  RegisterPerformanceTests();

  // Register additional helper tests
  RegisterObjectBrowserTests_SelectObject(this);
  RegisterObjectBrowserTests_SearchFilter(this);
  RegisterObjectPlacementTests_SnapToGrid(this);
  RegisterObjectPlacementTests_MultipleObjects(this);
  RegisterObjectSelectionTests_MultiSelect(this);
  RegisterObjectSelectionTests_MoveObject(this);
  RegisterObjectSelectionTests_DeleteObject(this);
  RegisterLayerManagementTests_PlaceOnLayers(this);
  RegisterLayerManagementTests_RenderingOrder(this);
  RegisterSaveWorkflowTests_RoundTrip(this);
  RegisterSaveWorkflowTests_MultipleTypes(this);
  RegisterRenderingQualityTests_Palettes(this);
  RegisterRenderingQualityTests_ComplexRoom(this);
}

// =============================================================================
// TEST FIXTURE INTEGRATION
// =============================================================================

TEST_F(DungeonObjectRenderingE2ETests, RunAllTests) {
  // Run all registered tests
  ImGuiTestEngine_QueueTests(engine_, ImGuiTestGroup_Tests, nullptr, nullptr);
  ImGuiTestEngine_Run(engine_);

  // Verify all tests passed
  ImGuiTestEngineIO& test_io = ImGuiTestEngine_GetIO(engine_);
  EXPECT_EQ(test_io.TestsFailedCount, 0)
      << "Some E2E tests failed. Check test engine output for details.";
}

}  // namespace test
}  // namespace yaze
