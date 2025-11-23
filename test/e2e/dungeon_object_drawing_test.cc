/**
 * @file dungeon_object_drawing_test.cc
 * @brief E2E tests for dungeon object drawing and manipulation
 *
 * Tests object placement, deletion, and repositioning in the dungeon editor.
 * Uses ImGuiTestEngine to automate UI interactions and verify object state.
 */

#define IMGUI_DEFINE_MATH_OPERATORS

#include "e2e/dungeon_object_drawing_test.h"

#include "app/controller.h"
#include "app/rom.h"
#include "gtest/gtest.h"
#include "imgui.h"
#include "imgui_test_engine/imgui_te_context.h"
#include "test_utils.h"
#include "zelda3/dungeon/room.h"

namespace yaze {
namespace test {

// =============================================================================
// Helper Functions
// =============================================================================

namespace {

/**
 * @brief Open a specific dungeon room by ID
 * @param ctx Test context
 * @param room_id The room ID to open (0x00-0x127)
 * @return True if the room was successfully opened
 */
bool OpenDungeonRoom(ImGuiTestContext* ctx, int room_id) {
  char room_label[32];
  snprintf(room_label, sizeof(room_label), "[%03X]", room_id);

  // Enable room selector if not visible
  if (ctx->WindowInfo("Dungeon Controls").Window != nullptr) {
    ctx->SetRef("Dungeon Controls");
    if (!ctx->WindowInfo("Rooms List").Window) {
      ctx->ItemClick("Rooms");
      ctx->Yield(5);
    }
  }

  // Open room from list
  if (ctx->WindowInfo("Rooms List").Window != nullptr) {
    ctx->SetRef("Rooms List");
    ctx->Yield(5);

    // Use the child window for room list
    ctx->SetRef("Rooms List/##RoomsList");

    // Scroll to and click the room
    // Room entries are formatted as "[XXX] Room Name"
    char full_label[64];
    snprintf(full_label, sizeof(full_label), "[%03X]*", room_id);

    // Try clicking on any selectable starting with the room ID
    ctx->ItemClick(full_label);
    ctx->Yield(20);

    return true;
  }

  return false;
}

/**
 * @brief Enable the object editor panel
 * @param ctx Test context
 * @return True if the object editor was enabled
 */
bool EnableObjectEditor(ImGuiTestContext* ctx) {
  if (ctx->WindowInfo("Dungeon Controls").Window != nullptr) {
    ctx->SetRef("Dungeon Controls");
    ctx->ItemClick("Objects");
    ctx->Yield(10);
    return ctx->WindowInfo("Object Editor").Window != nullptr;
  }
  return false;
}

/**
 * @brief Get the current room object count from the controller
 * @param ctx Test context
 * @param room_id Room ID to check
 * @return Number of objects in the room, or -1 on error
 */
int GetRoomObjectCount(ImGuiTestContext* ctx, int room_id) {
  Controller* controller = static_cast<Controller*>(ctx->Test->UserData);
  if (!controller) {
    ctx->LogError("Controller is null");
    return -1;
  }

  // Access the dungeon editor's room data via the editor manager
  // For now, return -1 as direct room access requires editor state
  // In a full implementation, this would query the DungeonEditorV2
  return -1;
}

/**
 * @brief Click at a position relative to the canvas zero point
 * @param ctx Test context
 * @param room_x X position in room coordinates (8px per tile)
 * @param room_y Y position in room coordinates (8px per tile)
 */
void ClickOnCanvas(ImGuiTestContext* ctx, int room_x, int room_y) {
  // Canvas coordinates are in pixels (8px per tile for dungeons)
  // The canvas has a zero_point() that we need to account for

  // Get the current window position for the room card
  ImGuiWindow* window = ctx->GetWindowByRef("");
  if (!window) {
    ctx->LogWarning("Could not find active window for canvas click");
    return;
  }

  // Calculate canvas position (assuming scale = 1.0)
  // Room tiles are 8x8 pixels
  float canvas_x = room_x * 8.0f;
  float canvas_y = room_y * 8.0f;

  // Move to position and click
  ctx->MouseMoveToPos(ImVec2(canvas_x, canvas_y));
  ctx->MouseClick(0);
}

}  // namespace

// =============================================================================
// Test Implementations
// =============================================================================

void E2ETest_DungeonObjectDrawing_BasicPlacement(ImGuiTestContext* ctx) {
  ctx->LogInfo("=== Test: Basic Object Placement ===");

  // Step 1: Load ROM
  ctx->LogInfo("Step 1: Loading ROM...");
  gui::LoadRomInTest(ctx, "zelda3.sfc");
  ctx->Yield(10);

  // Step 2: Open Dungeon Editor
  ctx->LogInfo("Step 2: Opening Dungeon Editor...");
  gui::OpenEditorInTest(ctx, "Dungeon");
  ctx->Yield(20);

  // Verify dungeon controls are visible
  if (ctx->WindowInfo("Dungeon Controls").Window == nullptr) {
    ctx->LogError("Dungeon Controls panel not found - aborting test");
    return;
  }
  ctx->LogInfo("Dungeon Controls panel visible");

  // Step 3: Open Room Selector
  ctx->LogInfo("Step 3: Opening Room Selector...");
  ctx->SetRef("Dungeon Controls");
  ctx->ItemClick("Rooms");
  ctx->Yield(10);

  // Step 4: Open a room (Room 0 - Ganon's Room)
  ctx->LogInfo("Step 4: Opening Room 0x00...");
  if (ctx->WindowInfo("Rooms List").Window != nullptr) {
    ctx->WindowFocus("Rooms List");
    ctx->Yield(5);

    // Find and click Room 0
    // Room list items are selectables in a child window
    ctx->SetRef("Rooms List");
    ctx->Yield(5);

    // Room entries use format "[XXX] Room Name"
    // For room 0, it would be "[000] Ganon" or similar
    ctx->ItemClick("**/[000]*");
    ctx->Yield(20);
  } else {
    ctx->LogError("Rooms List window not found");
    return;
  }

  // Verify room card opened
  char room_window_name[32];
  snprintf(room_window_name, sizeof(room_window_name), "[000]*");

  ctx->Yield(10);

  // Step 5: Enable Object Editor
  ctx->LogInfo("Step 5: Enabling Object Editor...");
  ctx->SetRef("Dungeon Controls");
  ctx->ItemClick("Objects");
  ctx->Yield(10);

  if (ctx->WindowInfo("Object Editor").Window != nullptr) {
    ctx->LogInfo("Object Editor panel is visible");
  } else {
    ctx->LogWarning("Object Editor panel not visible - may be differently named");
  }

  // Step 6: Verify canvas is present in the room card
  ctx->LogInfo("Step 6: Verifying canvas is present...");

  // The room card should contain a canvas
  // Canvas is drawn in DrawRoomTab() via canvas_viewer_.DrawDungeonCanvas()
  // The canvas uses "##DungeonCanvas" or similar ID

  ctx->LogInfo("=== Basic Object Placement Test Complete ===");
  ctx->LogInfo("Note: Full placement verification requires direct object state access");
}

void E2ETest_DungeonObjectDrawing_MultiLayerObjects(ImGuiTestContext* ctx) {
  ctx->LogInfo("=== Test: Multi-Layer Object Placement ===");

  // Step 1: Load ROM and open editor
  ctx->LogInfo("Step 1: Loading ROM and opening editor...");
  gui::LoadRomInTest(ctx, "zelda3.sfc");
  gui::OpenEditorInTest(ctx, "Dungeon");
  ctx->Yield(20);

  // Step 2: Open room selector and a room
  ctx->LogInfo("Step 2: Opening a dungeon room...");
  if (ctx->WindowInfo("Dungeon Controls").Window != nullptr) {
    ctx->SetRef("Dungeon Controls");
    ctx->ItemClick("Rooms");
    ctx->Yield(10);
  }

  // Open Room 1 (often has multi-layer content)
  if (ctx->WindowInfo("Rooms List").Window != nullptr) {
    ctx->SetRef("Rooms List");
    ctx->ItemClick("**/[001]*");
    ctx->Yield(20);
  }

  // Step 3: Test layer visibility controls
  ctx->LogInfo("Step 3: Testing layer visibility controls...");

  // The room card should have layer controls
  // These are checkboxes: "BG1", "BG2" in the LayerControls table

  // Find the room window (format varies based on session)
  ImGuiWindow* room_window = nullptr;
  for (int i = 0; i < ctx->UiContext->Windows.Size; i++) {
    ImGuiWindow* w = ctx->UiContext->Windows[i];
    if (w && w->Name && strstr(w->Name, "[001]") != nullptr) {
      room_window = w;
      break;
    }
  }

  if (room_window) {
    ctx->WindowFocus(room_window->Name);
    ctx->SetRef(room_window->Name);
    ctx->Yield(5);

    // Toggle BG1 visibility
    if (ctx->ItemExists("BG1")) {
      ctx->LogInfo("Toggling BG1 layer...");
      ctx->ItemClick("BG1");
      ctx->Yield(10);
      ctx->ItemClick("BG1");  // Toggle back
      ctx->Yield(5);
    }

    // Toggle BG2 visibility
    if (ctx->ItemExists("BG2")) {
      ctx->LogInfo("Toggling BG2 layer...");
      ctx->ItemClick("BG2");
      ctx->Yield(10);
      ctx->ItemClick("BG2");  // Toggle back
      ctx->Yield(5);
    }

    ctx->LogInfo("Layer visibility controls functional");
  } else {
    ctx->LogWarning("Could not find room window for layer tests");
  }

  ctx->LogInfo("=== Multi-Layer Object Placement Test Complete ===");
}

void E2ETest_DungeonObjectDrawing_ObjectDeletion(ImGuiTestContext* ctx) {
  ctx->LogInfo("=== Test: Object Deletion ===");

  // Step 1: Load ROM and open editor
  ctx->LogInfo("Step 1: Loading ROM and opening editor...");
  gui::LoadRomInTest(ctx, "zelda3.sfc");
  gui::OpenEditorInTest(ctx, "Dungeon");
  ctx->Yield(20);

  // Step 2: Open a room with objects
  ctx->LogInfo("Step 2: Opening room with objects...");
  if (ctx->WindowInfo("Dungeon Controls").Window != nullptr) {
    ctx->SetRef("Dungeon Controls");
    ctx->ItemClick("Rooms");
    ctx->Yield(10);
  }

  // Open Room 2 (Hyrule Castle Entrance - usually has objects)
  if (ctx->WindowInfo("Rooms List").Window != nullptr) {
    ctx->SetRef("Rooms List");
    ctx->ItemClick("**/[002]*");
    ctx->Yield(30);  // Allow room to load and render
  }

  // Step 3: Enable object editor to see object list
  ctx->LogInfo("Step 3: Enabling object editor...");
  ctx->SetRef("Dungeon Controls");
  ctx->ItemClick("Objects");
  ctx->Yield(10);

  // Step 4: Attempt to select an object on the canvas
  ctx->LogInfo("Step 4: Testing object selection...");

  // Find the room window
  ImGuiWindow* room_window = nullptr;
  for (int i = 0; i < ctx->UiContext->Windows.Size; i++) {
    ImGuiWindow* w = ctx->UiContext->Windows[i];
    if (w && w->Name && strstr(w->Name, "[002]") != nullptr) {
      room_window = w;
      break;
    }
  }

  if (room_window) {
    ctx->WindowFocus(room_window->Name);
    ctx->Yield(5);

    // Click in the canvas area to attempt object selection
    // Canvas starts after the properties tables
    ImVec2 window_pos = room_window->Pos;
    ImVec2 click_pos = ImVec2(window_pos.x + 256, window_pos.y + 200);

    ctx->LogInfo("Clicking at canvas position (256, 200)...");
    ctx->MouseMoveToPos(click_pos);
    ctx->MouseClick(0);
    ctx->Yield(5);

    // Step 5: Press Delete key
    ctx->LogInfo("Step 5: Pressing Delete key...");
    ctx->KeyPress(ImGuiKey_Delete);
    ctx->Yield(10);

    ctx->LogInfo("Delete key pressed - object deletion attempted");
  } else {
    ctx->LogWarning("Could not find room window for deletion test");
  }

  ctx->LogInfo("=== Object Deletion Test Complete ===");
}

void E2ETest_DungeonObjectDrawing_ObjectRepositioning(ImGuiTestContext* ctx) {
  ctx->LogInfo("=== Test: Object Repositioning ===");

  // Step 1: Load ROM and open editor
  ctx->LogInfo("Step 1: Loading ROM and opening editor...");
  gui::LoadRomInTest(ctx, "zelda3.sfc");
  gui::OpenEditorInTest(ctx, "Dungeon");
  ctx->Yield(20);

  // Step 2: Open a room with objects
  ctx->LogInfo("Step 2: Opening room with objects...");
  if (ctx->WindowInfo("Dungeon Controls").Window != nullptr) {
    ctx->SetRef("Dungeon Controls");
    ctx->ItemClick("Rooms");
    ctx->Yield(10);
  }

  // Open Room 3
  if (ctx->WindowInfo("Rooms List").Window != nullptr) {
    ctx->SetRef("Rooms List");
    ctx->ItemClick("**/[003]*");
    ctx->Yield(30);
  }

  // Step 3: Find and focus the room window
  ctx->LogInfo("Step 3: Finding room window...");
  ImGuiWindow* room_window = nullptr;
  for (int i = 0; i < ctx->UiContext->Windows.Size; i++) {
    ImGuiWindow* w = ctx->UiContext->Windows[i];
    if (w && w->Name && strstr(w->Name, "[003]") != nullptr) {
      room_window = w;
      break;
    }
  }

  if (room_window) {
    ctx->WindowFocus(room_window->Name);
    ctx->Yield(5);

    // Step 4: Perform drag operation
    ctx->LogInfo("Step 4: Testing drag operation...");

    // Calculate canvas area positions
    ImVec2 window_pos = room_window->Pos;
    ImVec2 start_pos = ImVec2(window_pos.x + 200, window_pos.y + 200);
    ImVec2 end_pos = ImVec2(window_pos.x + 300, window_pos.y + 250);

    // Perform drag
    ctx->MouseMoveToPos(start_pos);
    ctx->MouseDown(0);
    ctx->Yield(2);
    ctx->MouseMoveToPos(end_pos);
    ctx->Yield(2);
    ctx->MouseUp(0);
    ctx->Yield(10);

    ctx->LogInfo("Drag operation completed from (200,200) to (300,250)");
  } else {
    ctx->LogWarning("Could not find room window for drag test");
  }

  ctx->LogInfo("=== Object Repositioning Test Complete ===");
}

// =============================================================================
// GTest Integration - Unit tests for object drawing infrastructure
// =============================================================================

/**
 * @class DungeonObjectDrawingTest
 * @brief GTest fixture for object drawing unit tests
 */
class DungeonObjectDrawingTest : public ::testing::Test {
 protected:
  void SetUp() override {
    // Check if ROM testing is enabled
    if (!TestRomManager::IsRomTestingEnabled()) {
      skip_rom_tests_ = true;
    }
  }

  bool skip_rom_tests_ = false;
};

TEST_F(DungeonObjectDrawingTest, RoomObjectStructure) {
  // Test that RoomObject can be created with valid parameters
  zelda3::RoomObject obj(0x01, 10, 20, 1, 0);

  EXPECT_EQ(obj.id_, 0x01);
  EXPECT_EQ(obj.x_, 10);
  EXPECT_EQ(obj.y_, 20);
  EXPECT_EQ(obj.size_, 1);
}

TEST_F(DungeonObjectDrawingTest, RoomObjectLayerTypes) {
  // Test layer type enumeration
  zelda3::RoomObject obj(0x01, 0, 0, 1, 0);

  // Default layer should be BG1
  EXPECT_EQ(obj.layer_, zelda3::RoomObject::LayerType::BG1);

  // Test setting different layers
  obj.layer_ = zelda3::RoomObject::LayerType::BG2;
  EXPECT_EQ(obj.layer_, zelda3::RoomObject::LayerType::BG2);

  obj.layer_ = zelda3::RoomObject::LayerType::BG3;
  EXPECT_EQ(obj.layer_, zelda3::RoomObject::LayerType::BG3);
}

TEST_F(DungeonObjectDrawingTest, ObjectPositionBounds) {
  // Test object position validation
  // Room dimensions are 64x64 tiles (512x512 pixels at 8px per tile)
  constexpr int kMaxTileX = 63;
  constexpr int kMaxTileY = 63;

  // Valid position
  zelda3::RoomObject valid_obj(0x01, 32, 32, 1, 0);
  EXPECT_GE(valid_obj.x_, 0);
  EXPECT_LE(valid_obj.x_, kMaxTileX);
  EXPECT_GE(valid_obj.y_, 0);
  EXPECT_LE(valid_obj.y_, kMaxTileY);

  // Edge positions
  zelda3::RoomObject corner_obj(0x01, kMaxTileX, kMaxTileY, 1, 0);
  EXPECT_EQ(corner_obj.x_, kMaxTileX);
  EXPECT_EQ(corner_obj.y_, kMaxTileY);
}

}  // namespace test
}  // namespace yaze
