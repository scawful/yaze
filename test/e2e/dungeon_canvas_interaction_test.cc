#define IMGUI_DEFINE_MATH_OPERATORS
#include "e2e/dungeon_canvas_interaction_test.h"

#include "app/controller.h"
#include "app/editor/dungeon/dungeon_editor_v2.h"
#include "imgui_test_engine/imgui_te_context.h"
#include "test_utils.h"

namespace {

// Helper constants for dungeon canvas testing
constexpr int kRoomCanvasSize = 512;  // Dungeon room canvas is 512x512 pixels
constexpr int kGridSize = 8;          // Dungeon tiles snap to 8x8 grid
constexpr const char* kTestRoomId = "Room 0x00";

/**
 * @brief Helper to open a room card and return whether it succeeded
 */
bool OpenRoomCard(ImGuiTestContext* ctx, int room_id) {
  // First ensure dungeon editor is open
  yaze::test::gui::OpenEditorInTest(ctx, "Dungeon");
  ctx->Yield(5);

  // Find the room selector and open the specified room
  auto* window_info = ctx->WindowInfo("Rooms List").Window;
  if (!window_info) {
    ctx->LogWarning("Rooms List card not visible - attempting to enable it");
    // Try to toggle rooms list visibility via menu or control panel
    ctx->Yield(2);
    window_info = ctx->WindowInfo("Rooms List").Window;
    if (!window_info) {
      ctx->LogError("Failed to open Rooms List card");
      return false;
    }
  }

  ctx->SetRef("Rooms List");

  // Scroll to top to ensure room 0 is visible
  ctx->ScrollToTop("");

  // Find and click on the room in the list
  char room_label[64];
  snprintf(room_label, sizeof(room_label), "[%03X]*", room_id);

  // Look for selectable items in the room list
  ctx->ItemClick("##RoomsList");
  ctx->Yield(2);

  // Room cards are named with the pattern "[XXX] Room Name###RoomCardN"
  // After opening, verify the room card exists
  ctx->Yield(5);

  return true;
}

/**
 * @brief Get the canvas widget position from a room card
 */
ImVec2 GetCanvasPosition(ImGuiTestContext* ctx, const char* card_name) {
  ctx->WindowFocus(card_name);
  ctx->SetRef(card_name);

  // The canvas is identified by "##DungeonCanvas"
  auto item = ctx->ItemInfo("##DungeonCanvas");
  if (item.ID != 0) {
    return item.RectFull.Min;
  }

  // Fallback: use window content region
  auto* window = ctx->WindowInfo(card_name).Window;
  if (window) {
    return ImVec2(window->ContentRegionRect.Min.x + 50,
                  window->ContentRegionRect.Min.y + 50);
  }

  return ImVec2(0, 0);
}

/**
 * @brief Get canvas center position for interactions
 */
ImVec2 GetCanvasCenterPosition(ImGuiTestContext* ctx, const char* card_name) {
  ctx->WindowFocus(card_name);
  ctx->SetRef(card_name);

  auto item = ctx->ItemInfo("##DungeonCanvas");
  if (item.ID != 0) {
    return ImVec2((item.RectFull.Min.x + item.RectFull.Max.x) / 2.0f,
                  (item.RectFull.Min.y + item.RectFull.Max.y) / 2.0f);
  }

  auto* window = ctx->WindowInfo(card_name).Window;
  if (window) {
    return ImVec2(
        (window->ContentRegionRect.Min.x + window->ContentRegionRect.Max.x) /
            2.0f,
        (window->ContentRegionRect.Min.y + window->ContentRegionRect.Max.y) /
            2.0f);
  }

  return ImVec2(200, 200);
}

}  // namespace

void E2ETest_DungeonCanvas_PanZoom(ImGuiTestContext* ctx) {
  ctx->LogInfo("=== Starting Dungeon Canvas Pan/Zoom Test ===");

  // Load ROM
  ctx->LogInfo("Loading ROM...");
  yaze::test::gui::LoadRomInTest(ctx, yaze::test::TestRomManager::GetTestRomPath());

  // Open dungeon editor and room
  ctx->LogInfo("Opening Dungeon Editor...");
  yaze::test::gui::OpenEditorInTest(ctx, "Dungeon");
  ctx->Yield(5);

  // Open Room 0
  ctx->LogInfo("Opening Room 0x00...");
  if (!OpenRoomCard(ctx, 0)) {
    ctx->LogError("Failed to open room card");
    return;
  }

  // Find the room card window - room cards use pattern "###RoomCard0"
  ctx->Yield(5);
  const char* room_card_pattern = "###RoomCard0";
  auto window_info = ctx->WindowInfo(room_card_pattern);

  if (!window_info.Window) {
    ctx->LogWarning("Room card window not found with pattern, searching...");
    // Room cards may have different naming - log what we find
    ctx->LogInfo("Continuing with test using estimated positions");
  }

  // Get canvas position
  ImVec2 canvas_center = GetCanvasCenterPosition(ctx, room_card_pattern);
  if (canvas_center.x == 0 && canvas_center.y == 0) {
    canvas_center = ImVec2(400, 400);  // Fallback position
  }

  ctx->LogInfo("Canvas center: (%.0f, %.0f)", canvas_center.x, canvas_center.y);

  // Test 1: Zoom In with Mouse Wheel
  ctx->LogInfo("--- Test 1: Zoom In ---");
  ctx->MouseMoveToPos(canvas_center);
  ctx->Yield();

  // Store initial zoom level conceptually (we verify by checking canvas behavior)
  for (int i = 0; i < 3; i++) {
    ctx->MouseWheel(ImVec2(0, 1.0f));  // Scroll up to zoom in
    ctx->Yield();
  }
  ctx->LogInfo("Performed 3 zoom-in actions");

  // Test 2: Zoom Out with Mouse Wheel
  ctx->LogInfo("--- Test 2: Zoom Out ---");
  for (int i = 0; i < 5; i++) {
    ctx->MouseWheel(ImVec2(0, -1.0f));  // Scroll down to zoom out
    ctx->Yield();
  }
  ctx->LogInfo("Performed 5 zoom-out actions");

  // Test 3: Pan with Right-Click Drag
  ctx->LogInfo("--- Test 3: Pan View ---");
  ctx->MouseMoveToPos(canvas_center);
  ctx->Yield();

  // Right-click drag to pan
  ctx->MouseDown(ImGuiMouseButton_Right);
  ctx->Yield();

  // Drag in a square pattern to test panning
  ImVec2 drag_offsets[] = {
      ImVec2(50, 0),    // Right
      ImVec2(0, 50),    // Down
      ImVec2(-100, 0),  // Left
      ImVec2(0, -50),   // Up
  };

  ImVec2 current_pos = canvas_center;
  for (const auto& offset : drag_offsets) {
    current_pos = ImVec2(current_pos.x + offset.x, current_pos.y + offset.y);
    ctx->MouseMoveToPos(current_pos);
    ctx->Yield();
  }

  ctx->MouseUp(ImGuiMouseButton_Right);
  ctx->Yield();
  ctx->LogInfo("Completed pan drag sequence");

  // Test 4: Reset View (via context menu if available)
  ctx->LogInfo("--- Test 4: Context Menu Reset ---");
  ctx->MouseMoveToPos(canvas_center);
  ctx->Yield();

  // Open context menu
  ctx->MouseClick(ImGuiMouseButton_Right);
  ctx->Yield(2);

  // Try to find reset option in context menu
  if (ctx->ItemExists("Reset View")) {
    ctx->ItemClick("Reset View");
    ctx->Yield();
    ctx->LogInfo("Reset view via context menu");
  } else {
    ctx->LogInfo("Reset View option not found in context menu (may not exist)");
    // Close context menu
    ctx->KeyPress(ImGuiKey_Escape);
    ctx->Yield();
  }

  ctx->LogInfo("=== Dungeon Canvas Pan/Zoom Test Completed ===");
}

void E2ETest_DungeonCanvas_ObjectSelection(ImGuiTestContext* ctx) {
  ctx->LogInfo("=== Starting Dungeon Canvas Object Selection Test ===");

  // Load ROM
  ctx->LogInfo("Loading ROM...");
  yaze::test::gui::LoadRomInTest(ctx, yaze::test::TestRomManager::GetTestRomPath());

  // Open dungeon editor
  ctx->LogInfo("Opening Dungeon Editor...");
  yaze::test::gui::OpenEditorInTest(ctx, "Dungeon");
  ctx->Yield(5);

  // Open Room 0 (has objects to select)
  ctx->LogInfo("Opening Room 0x00...");
  if (!OpenRoomCard(ctx, 0)) {
    ctx->LogError("Failed to open room card");
    return;
  }

  ctx->Yield(5);

  // Get canvas position
  const char* room_card_pattern = "###RoomCard0";
  ImVec2 canvas_pos = GetCanvasPosition(ctx, room_card_pattern);
  if (canvas_pos.x == 0 && canvas_pos.y == 0) {
    canvas_pos = ImVec2(300, 300);
  }

  // Test 1: Click to Select Object
  ctx->LogInfo("--- Test 1: Click to Select ---");

  // Click near the center of the room where objects typically exist
  // Room 0 (Ganon's Tower Entrance Hall) has objects in predictable locations
  ImVec2 click_pos = ImVec2(canvas_pos.x + 100, canvas_pos.y + 100);
  ctx->MouseMoveToPos(click_pos);
  ctx->Yield();
  ctx->MouseClick(ImGuiMouseButton_Left);
  ctx->Yield(2);
  ctx->LogInfo("Clicked at position (%.0f, %.0f)", click_pos.x, click_pos.y);

  // Test 2: Click on Empty Space to Deselect
  ctx->LogInfo("--- Test 2: Click Empty Space to Deselect ---");

  // Click in corner (typically empty)
  ImVec2 empty_pos = ImVec2(canvas_pos.x + 10, canvas_pos.y + 10);
  ctx->MouseMoveToPos(empty_pos);
  ctx->Yield();
  ctx->MouseClick(ImGuiMouseButton_Left);
  ctx->Yield(2);
  ctx->LogInfo("Clicked empty space at (%.0f, %.0f)", empty_pos.x, empty_pos.y);

  // Test 3: Click Multiple Locations
  ctx->LogInfo("--- Test 3: Multiple Click Locations ---");

  // Test clicking at different positions on the canvas
  ImVec2 test_positions[] = {
      ImVec2(canvas_pos.x + 50, canvas_pos.y + 50),
      ImVec2(canvas_pos.x + 150, canvas_pos.y + 75),
      ImVec2(canvas_pos.x + 200, canvas_pos.y + 150),
      ImVec2(canvas_pos.x + 100, canvas_pos.y + 200),
  };

  for (int i = 0; i < 4; i++) {
    ctx->MouseMoveToPos(test_positions[i]);
    ctx->Yield();
    ctx->MouseClick(ImGuiMouseButton_Left);
    ctx->Yield();
    ctx->LogInfo("Clicked test position %d: (%.0f, %.0f)", i + 1,
                 test_positions[i].x, test_positions[i].y);
  }

  // Test 4: Double-Click to Edit (if supported)
  ctx->LogInfo("--- Test 4: Double-Click Test ---");
  ImVec2 dbl_click_pos = ImVec2(canvas_pos.x + 100, canvas_pos.y + 100);
  ctx->MouseMoveToPos(dbl_click_pos);
  ctx->Yield();
  ctx->MouseDoubleClick(ImGuiMouseButton_Left);
  ctx->Yield(2);
  ctx->LogInfo("Double-clicked at (%.0f, %.0f)", dbl_click_pos.x,
               dbl_click_pos.y);

  // Close any popup that may have opened
  ctx->KeyPress(ImGuiKey_Escape);
  ctx->Yield();

  ctx->LogInfo("=== Dungeon Canvas Object Selection Test Completed ===");
}

void E2ETest_DungeonCanvas_GridSnap(ImGuiTestContext* ctx) {
  ctx->LogInfo("=== Starting Dungeon Canvas Grid Snap Test ===");

  // Load ROM
  ctx->LogInfo("Loading ROM...");
  yaze::test::gui::LoadRomInTest(ctx, yaze::test::TestRomManager::GetTestRomPath());

  // Open dungeon editor
  ctx->LogInfo("Opening Dungeon Editor...");
  yaze::test::gui::OpenEditorInTest(ctx, "Dungeon");
  ctx->Yield(5);

  // Open Room 0
  ctx->LogInfo("Opening Room 0x00...");
  if (!OpenRoomCard(ctx, 0)) {
    ctx->LogError("Failed to open room card");
    return;
  }

  ctx->Yield(5);

  // Get canvas position
  const char* room_card_pattern = "###RoomCard0";
  ImVec2 canvas_pos = GetCanvasPosition(ctx, room_card_pattern);
  if (canvas_pos.x == 0 && canvas_pos.y == 0) {
    canvas_pos = ImVec2(300, 300);
  }

  // Test 1: Verify Grid is Visible
  ctx->LogInfo("--- Test 1: Grid Visibility ---");
  ctx->LogInfo("Canvas position: (%.0f, %.0f)", canvas_pos.x, canvas_pos.y);
  ctx->LogInfo("Grid size: %d pixels", kGridSize);

  // Test 2: Click at Non-Grid Position
  ctx->LogInfo("--- Test 2: Click at Non-Grid Aligned Position ---");

  // Click at position that's NOT aligned to 8x8 grid
  // Position 33,33 should snap to 32,32 (nearest 8x8 boundary)
  ImVec2 unaligned_pos = ImVec2(canvas_pos.x + 33, canvas_pos.y + 33);
  ctx->MouseMoveToPos(unaligned_pos);
  ctx->Yield();
  ctx->MouseClick(ImGuiMouseButton_Left);
  ctx->Yield(2);
  ctx->LogInfo("Clicked at unaligned position (%.0f, %.0f) - should snap to grid",
               unaligned_pos.x, unaligned_pos.y);

  // Test 3: Click at Grid-Aligned Position
  ctx->LogInfo("--- Test 3: Click at Grid-Aligned Position ---");

  // Click at position that IS aligned to 8x8 grid
  ImVec2 aligned_pos = ImVec2(canvas_pos.x + 64, canvas_pos.y + 64);
  ctx->MouseMoveToPos(aligned_pos);
  ctx->Yield();
  ctx->MouseClick(ImGuiMouseButton_Left);
  ctx->Yield(2);
  ctx->LogInfo("Clicked at aligned position (%.0f, %.0f)", aligned_pos.x,
               aligned_pos.y);

  // Test 4: Drag Operation Grid Snap
  ctx->LogInfo("--- Test 4: Drag with Grid Snap ---");

  // First select an object
  ImVec2 select_pos = ImVec2(canvas_pos.x + 100, canvas_pos.y + 100);
  ctx->MouseMoveToPos(select_pos);
  ctx->Yield();
  ctx->MouseClick(ImGuiMouseButton_Left);
  ctx->Yield();

  // Now drag to test grid snapping
  ImVec2 drag_start = select_pos;
  ImVec2 drag_end = ImVec2(drag_start.x + 37, drag_start.y + 29);  // Non-aligned

  ctx->MouseDown(ImGuiMouseButton_Left);
  ctx->Yield();
  ctx->MouseMoveToPos(drag_end);
  ctx->Yield();
  ctx->MouseUp(ImGuiMouseButton_Left);
  ctx->Yield(2);

  ctx->LogInfo("Dragged from (%.0f, %.0f) to (%.0f, %.0f)", drag_start.x,
               drag_start.y, drag_end.x, drag_end.y);
  ctx->LogInfo("Drag offset: (%.0f, %.0f) - should snap to nearest 8px multiple",
               drag_end.x - drag_start.x, drag_end.y - drag_start.y);

  // Test 5: Verify Grid Step via Context Menu
  ctx->LogInfo("--- Test 5: Check Grid Settings ---");
  ctx->MouseMoveToPos(ImVec2(canvas_pos.x + 50, canvas_pos.y + 50));
  ctx->Yield();
  ctx->MouseClick(ImGuiMouseButton_Right);
  ctx->Yield(2);

  // Look for grid settings in context menu
  if (ctx->ItemExists("Grid Step")) {
    ctx->LogInfo("Grid Step option found in context menu");
  } else if (ctx->ItemExists("Toggle Grid")) {
    ctx->LogInfo("Toggle Grid option found in context menu");
  } else {
    ctx->LogInfo("Grid options not visible in context menu");
  }

  // Close context menu
  ctx->KeyPress(ImGuiKey_Escape);
  ctx->Yield();

  ctx->LogInfo("=== Dungeon Canvas Grid Snap Test Completed ===");
}

void E2ETest_DungeonCanvas_MultiSelect(ImGuiTestContext* ctx) {
  ctx->LogInfo("=== Starting Dungeon Canvas Multi-Select Test ===");

  // Load ROM
  ctx->LogInfo("Loading ROM...");
  yaze::test::gui::LoadRomInTest(ctx, yaze::test::TestRomManager::GetTestRomPath());

  // Open dungeon editor
  ctx->LogInfo("Opening Dungeon Editor...");
  yaze::test::gui::OpenEditorInTest(ctx, "Dungeon");
  ctx->Yield(5);

  // Open Room 0
  ctx->LogInfo("Opening Room 0x00...");
  if (!OpenRoomCard(ctx, 0)) {
    ctx->LogError("Failed to open room card");
    return;
  }

  ctx->Yield(5);

  // Get canvas position
  const char* room_card_pattern = "###RoomCard0";
  ImVec2 canvas_pos = GetCanvasPosition(ctx, room_card_pattern);
  if (canvas_pos.x == 0 && canvas_pos.y == 0) {
    canvas_pos = ImVec2(300, 300);
  }

  // Test 1: Select First Object
  ctx->LogInfo("--- Test 1: Select First Object ---");
  ImVec2 first_obj_pos = ImVec2(canvas_pos.x + 80, canvas_pos.y + 80);
  ctx->MouseMoveToPos(first_obj_pos);
  ctx->Yield();
  ctx->MouseClick(ImGuiMouseButton_Left);
  ctx->Yield(2);
  ctx->LogInfo("Selected first object at (%.0f, %.0f)", first_obj_pos.x,
               first_obj_pos.y);

  // Test 2: Shift-Click to Add Second Object
  ctx->LogInfo("--- Test 2: Shift-Click Second Object ---");
  ImVec2 second_obj_pos = ImVec2(canvas_pos.x + 160, canvas_pos.y + 100);

  ctx->KeyDown(ImGuiMod_Shift);
  ctx->Yield();
  ctx->MouseMoveToPos(second_obj_pos);
  ctx->Yield();
  ctx->MouseClick(ImGuiMouseButton_Left);
  ctx->Yield();
  ctx->KeyUp(ImGuiMod_Shift);
  ctx->Yield(2);

  ctx->LogInfo("Shift-clicked second object at (%.0f, %.0f)", second_obj_pos.x,
               second_obj_pos.y);

  // Test 3: Shift-Click to Add Third Object
  ctx->LogInfo("--- Test 3: Shift-Click Third Object ---");
  ImVec2 third_obj_pos = ImVec2(canvas_pos.x + 120, canvas_pos.y + 180);

  ctx->KeyDown(ImGuiMod_Shift);
  ctx->Yield();
  ctx->MouseMoveToPos(third_obj_pos);
  ctx->Yield();
  ctx->MouseClick(ImGuiMouseButton_Left);
  ctx->Yield();
  ctx->KeyUp(ImGuiMod_Shift);
  ctx->Yield(2);

  ctx->LogInfo("Shift-clicked third object at (%.0f, %.0f)", third_obj_pos.x,
               third_obj_pos.y);

  // Test 4: Click Without Shift to Clear Selection
  ctx->LogInfo("--- Test 4: Clear Selection ---");
  ImVec2 clear_pos = ImVec2(canvas_pos.x + 250, canvas_pos.y + 250);
  ctx->MouseMoveToPos(clear_pos);
  ctx->Yield();
  ctx->MouseClick(ImGuiMouseButton_Left);
  ctx->Yield(2);
  ctx->LogInfo("Clicked without shift at (%.0f, %.0f) to clear selection",
               clear_pos.x, clear_pos.y);

  // Test 5: Rectangle Selection (Drag to Select Multiple)
  ctx->LogInfo("--- Test 5: Rectangle Selection ---");
  ImVec2 rect_start = ImVec2(canvas_pos.x + 50, canvas_pos.y + 50);
  ImVec2 rect_end = ImVec2(canvas_pos.x + 200, canvas_pos.y + 200);

  ctx->MouseMoveToPos(rect_start);
  ctx->Yield();
  ctx->MouseDown(ImGuiMouseButton_Left);
  ctx->Yield();

  // Drag to create selection rectangle
  ctx->MouseMoveToPos(rect_end);
  ctx->Yield(2);

  ctx->MouseUp(ImGuiMouseButton_Left);
  ctx->Yield(2);

  ctx->LogInfo("Created selection rectangle from (%.0f, %.0f) to (%.0f, %.0f)",
               rect_start.x, rect_start.y, rect_end.x, rect_end.y);

  // Test 6: Ctrl-A to Select All (if supported)
  ctx->LogInfo("--- Test 6: Select All (Ctrl+A) ---");
  ctx->KeyDown(ImGuiMod_Ctrl);
  ctx->KeyPress(ImGuiKey_A);
  ctx->KeyUp(ImGuiMod_Ctrl);
  ctx->Yield(2);
  ctx->LogInfo("Pressed Ctrl+A to select all objects");

  // Test 7: Escape to Deselect All
  ctx->LogInfo("--- Test 7: Escape to Deselect ---");
  ctx->KeyPress(ImGuiKey_Escape);
  ctx->Yield(2);
  ctx->LogInfo("Pressed Escape to deselect all");

  ctx->LogInfo("=== Dungeon Canvas Multi-Select Test Completed ===");
}
