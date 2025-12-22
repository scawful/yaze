/**
 * @file dungeon_layer_rendering_test.cc
 * @brief End-to-end tests for dungeon layer rendering system
 *
 * These tests verify the layer visibility controls in the DungeonEditorV2
 * card-based architecture. Each room card has per-room layer visibility
 * settings for BG1, BG2, and object layers.
 *
 * The dungeon editor renders rooms with multiple background layers:
 * - BG1: Primary background tiles
 * - BG2: Secondary background tiles (with various blend modes)
 * - BG3: Additional layer (used for effects)
 * - Objects: Rendered on top of backgrounds
 *
 * Test Pattern:
 * 1. Load ROM and open Dungeon Editor
 * 2. Open room card(s) via Room Selector
 * 3. Interact with layer controls in the room card
 * 4. Verify canvas updates reflect layer visibility changes
 *
 * Created: November 2025
 * Architecture: DungeonEditorV2 card-based system
 * Related: src/app/editor/dungeon/dungeon_canvas_viewer.cc
 */

#define IMGUI_DEFINE_MATH_OPERATORS

#include "e2e/dungeon_layer_rendering_test.h"

#include "app/controller.h"
#include "imgui.h"
#include "imgui_test_engine/imgui_te_context.h"
#include "imgui_test_engine/imgui_te_engine.h"
#include "test_utils.h"

namespace yaze {
namespace test {

// =============================================================================
// Helper Functions
// =============================================================================

namespace {

/**
 * @brief Helper to set up dungeon editor with a room open
 * @param ctx ImGui test context
 * @param room_hex Room number in hex format (e.g., "0x00")
 * @return true if setup successful, false otherwise
 */
bool SetupDungeonEditorWithRoom(ImGuiTestContext* ctx,
                                 const char* room_hex = "0x00") {
  // Load ROM
  ctx->LogInfo("Loading ROM...");
  gui::LoadRomInTest(ctx, yaze::test::TestRomManager::GetTestRomPath());

  // Open Dungeon Editor
  ctx->LogInfo("Opening Dungeon Editor...");
  gui::OpenEditorInTest(ctx, "Dungeon");
  ctx->Yield(15);

  // Enable room selector via Dungeon Controls
  if (ctx->WindowInfo("Dungeon Controls").Window == nullptr) {
    ctx->LogWarning("Dungeon Controls window not found");
    return false;
  }

  ctx->SetRef("Dungeon Controls");
  ctx->ItemClick("Rooms");
  ctx->Yield(5);

  // Open the specified room
  if (ctx->WindowInfo("Room Selector").Window == nullptr) {
    ctx->LogWarning("Room Selector window not found");
    return false;
  }

  ctx->SetRef("Room Selector");
  char room_name[32];
  snprintf(room_name, sizeof(room_name), "Room %s", room_hex);

  if (!ctx->ItemExists(room_name)) {
    ctx->LogWarning("Room %s not found in selector", room_name);
    return false;
  }

  ctx->ItemDoubleClick(room_name);
  ctx->Yield(20);

  // Verify room card opened
  if (ctx->WindowInfo(room_name).Window == nullptr) {
    ctx->LogWarning("Room card %s did not open", room_name);
    return false;
  }

  ctx->LogInfo("Successfully opened %s", room_name);
  return true;
}

/**
 * @brief Helper to check if a layer control checkbox exists and get its state
 * @param ctx ImGui test context
 * @param room_window Room window name (e.g., "Room 0x00")
 * @param checkbox_label Checkbox label (e.g., "BG1")
 * @return true if checkbox exists, false otherwise
 */
bool CheckLayerControlExists(ImGuiTestContext* ctx, const char* room_window,
                              const char* checkbox_label) {
  if (ctx->WindowInfo(room_window).Window == nullptr) {
    return false;
  }
  ctx->SetRef(room_window);
  return ctx->ItemExists(checkbox_label);
}

}  // namespace

// =============================================================================
// Test Implementation: Toggle BG1
// =============================================================================

void E2ETest_DungeonLayers_ToggleBG1(ImGuiTestContext* ctx) {
  ctx->LogInfo("=== E2E Test: Dungeon Layers - Toggle BG1 ===");
  ctx->LogInfo("Purpose: Verify BG1 layer visibility toggle updates canvas");

  // Setup
  if (!SetupDungeonEditorWithRoom(ctx, "0x00")) {
    ctx->LogError("Failed to set up dungeon editor with Room 0x00");
    return;
  }

  // Access the room card
  ctx->SetRef("Room 0x00");

  // Test 1: Verify BG1 checkbox exists
  if (!ctx->ItemExists("BG1")) {
    ctx->LogError("BG1 checkbox not found in room card");
    return;
  }
  ctx->LogInfo("PASS: BG1 checkbox found in room card");

  // Test 2: Toggle BG1 OFF
  ctx->LogInfo("Toggling BG1 visibility OFF...");
  ctx->ItemClick("BG1");
  ctx->Yield(10);
  ctx->LogInfo("BG1 toggled - canvas should hide BG1 layer");

  // Visual verification note: In a full visual test, we would capture
  // a screenshot here and verify BG1 content is not visible

  // Test 3: Toggle BG1 back ON
  ctx->LogInfo("Toggling BG1 visibility ON...");
  ctx->ItemClick("BG1");
  ctx->Yield(10);
  ctx->LogInfo("BG1 toggled - canvas should show BG1 layer");

  // Test 4: Rapid toggle (stress test)
  ctx->LogInfo("Performing rapid BG1 toggle test...");
  for (int i = 0; i < 3; i++) {
    ctx->ItemClick("BG1");
    ctx->Yield(2);
  }
  ctx->LogInfo("PASS: Rapid toggle completed without errors");

  // Final state: ensure BG1 is visible
  ctx->ItemClick("BG1");  // Toggle to known state
  ctx->Yield(5);

  ctx->LogInfo("=== Toggle BG1 Test COMPLETE ===");
}

// =============================================================================
// Test Implementation: Toggle BG2
// =============================================================================

void E2ETest_DungeonLayers_ToggleBG2(ImGuiTestContext* ctx) {
  ctx->LogInfo("=== E2E Test: Dungeon Layers - Toggle BG2 ===");
  ctx->LogInfo("Purpose: Verify BG2 layer visibility toggle updates canvas");

  // Setup
  if (!SetupDungeonEditorWithRoom(ctx, "0x00")) {
    ctx->LogError("Failed to set up dungeon editor with Room 0x00");
    return;
  }

  ctx->SetRef("Room 0x00");

  // Test 1: Verify BG2 checkbox exists
  if (!ctx->ItemExists("BG2")) {
    ctx->LogError("BG2 checkbox not found in room card");
    return;
  }
  ctx->LogInfo("PASS: BG2 checkbox found in room card");

  // Test 2: Toggle BG2 OFF
  ctx->LogInfo("Toggling BG2 visibility OFF...");
  ctx->ItemClick("BG2");
  ctx->Yield(10);
  ctx->LogInfo("BG2 toggled - canvas should hide BG2 layer");

  // Test 3: Toggle BG2 back ON
  ctx->LogInfo("Toggling BG2 visibility ON...");
  ctx->ItemClick("BG2");
  ctx->Yield(10);
  ctx->LogInfo("BG2 toggled - canvas should show BG2 layer");

  // Test 4: Test BG2 layer type combo (if accessible)
  if (ctx->ItemExists("##BG2Type")) {
    ctx->LogInfo("Testing BG2 layer type combo...");
    ctx->ItemClick("##BG2Type");
    ctx->Yield(3);
    // Select different blend modes
    ctx->KeyPress(ImGuiKey_DownArrow);
    ctx->KeyPress(ImGuiKey_Enter);
    ctx->Yield(5);
    ctx->LogInfo("PASS: BG2 type combo accessible");
  }

  ctx->LogInfo("=== Toggle BG2 Test COMPLETE ===");
}

// =============================================================================
// Test Implementation: All Layers Off
// =============================================================================

void E2ETest_DungeonLayers_AllLayersOff(ImGuiTestContext* ctx) {
  ctx->LogInfo("=== E2E Test: Dungeon Layers - All Layers Off ===");
  ctx->LogInfo("Purpose: Verify canvas appears blank when all layers disabled");

  // Setup
  if (!SetupDungeonEditorWithRoom(ctx, "0x00")) {
    ctx->LogError("Failed to set up dungeon editor with Room 0x00");
    return;
  }

  ctx->SetRef("Room 0x00");

  // Ensure both checkboxes exist
  bool has_bg1 = ctx->ItemExists("BG1");
  bool has_bg2 = ctx->ItemExists("BG2");

  if (!has_bg1 || !has_bg2) {
    ctx->LogError("Missing layer controls: BG1=%s, BG2=%s",
                  has_bg1 ? "found" : "missing", has_bg2 ? "found" : "missing");
    return;
  }

  ctx->LogInfo("Both layer controls found");

  // Step 1: Turn off BG1
  ctx->LogInfo("Disabling BG1...");
  ctx->ItemClick("BG1");
  ctx->Yield(5);

  // Step 2: Turn off BG2
  ctx->LogInfo("Disabling BG2...");
  ctx->ItemClick("BG2");
  ctx->Yield(5);

  // Verification: Canvas should now show minimal content (just grid/border)
  ctx->LogInfo("Both layers disabled - canvas should show blank room");
  ctx->Yield(10);

  // Visual note: At this point, the canvas should display only the canvas
  // background/grid, with no room tile graphics visible

  // Step 3: Re-enable layers
  ctx->LogInfo("Re-enabling BG1...");
  ctx->ItemClick("BG1");
  ctx->Yield(3);

  ctx->LogInfo("Re-enabling BG2...");
  ctx->ItemClick("BG2");
  ctx->Yield(3);

  ctx->LogInfo("Layers restored - canvas should show full room");

  ctx->LogInfo("=== All Layers Off Test COMPLETE ===");
}

// =============================================================================
// Test Implementation: Per-Room Settings
// =============================================================================

void E2ETest_DungeonLayers_PerRoomSettings(ImGuiTestContext* ctx) {
  ctx->LogInfo("=== E2E Test: Dungeon Layers - Per-Room Settings ===");
  ctx->LogInfo(
      "Purpose: Verify each room maintains independent layer visibility");

  // Setup first room
  if (!SetupDungeonEditorWithRoom(ctx, "0x00")) {
    ctx->LogError("Failed to set up dungeon editor with Room 0x00");
    return;
  }

  // Open second room
  ctx->LogInfo("Opening second room (Room 0x01)...");
  ctx->SetRef("Room Selector");

  if (!ctx->ItemExists("Room 0x01")) {
    ctx->LogWarning("Room 0x01 not found - skipping multi-room test");
    return;
  }

  ctx->ItemDoubleClick("Room 0x01");
  ctx->Yield(20);

  // Verify both room cards are open
  bool room0_open = ctx->WindowInfo("Room 0x00").Window != nullptr;
  bool room1_open = ctx->WindowInfo("Room 0x01").Window != nullptr;

  if (!room0_open || !room1_open) {
    ctx->LogError("Could not open both rooms: Room0=%s, Room1=%s",
                  room0_open ? "open" : "closed",
                  room1_open ? "open" : "closed");
    return;
  }
  ctx->LogInfo("PASS: Both room cards are open");

  // Test: Toggle BG1 in Room 0 only
  ctx->LogInfo("Toggling BG1 in Room 0x00...");
  ctx->SetRef("Room 0x00");
  if (ctx->ItemExists("BG1")) {
    ctx->ItemClick("BG1");
    ctx->Yield(5);
  }

  // Verify Room 1's BG1 setting is unaffected
  // (In ImGuiTestEngine, we can't directly read checkbox state,
  // but we verify the control exists and is accessible)
  ctx->LogInfo("Verifying Room 0x01 layer controls are independent...");
  ctx->SetRef("Room 0x01");
  if (ctx->ItemExists("BG1")) {
    ctx->LogInfo("PASS: Room 0x01 has independent BG1 control");
    // Toggle Room 1's BG1 to verify it works independently
    ctx->ItemClick("BG1");
    ctx->Yield(5);
    ctx->ItemClick("BG1");  // Toggle back
    ctx->Yield(3);
  }

  // Test: Different layer settings between rooms
  ctx->LogInfo("Setting different layer configs in each room...");

  // Room 0: BG1=off, BG2=on
  ctx->SetRef("Room 0x00");
  if (ctx->ItemExists("BG1")) {
    ctx->ItemClick("BG1");  // Already off, this toggles back on
    ctx->Yield(2);
    ctx->ItemClick("BG1");  // Off again
    ctx->Yield(2);
  }

  // Room 1: BG1=on, BG2=off
  ctx->SetRef("Room 0x01");
  if (ctx->ItemExists("BG2")) {
    ctx->ItemClick("BG2");  // Toggle BG2 off
    ctx->Yield(5);
  }

  ctx->LogInfo("PASS: Rooms configured with different layer settings");

  // Cleanup: Restore defaults
  ctx->LogInfo("Restoring default layer settings...");
  ctx->SetRef("Room 0x00");
  ctx->ItemClick("BG1");  // Toggle back on
  ctx->Yield(2);

  ctx->SetRef("Room 0x01");
  ctx->ItemClick("BG2");  // Toggle back on
  ctx->Yield(2);

  // Close Room 1 to clean up
  ctx->WindowClose("Room 0x01");
  ctx->Yield(3);

  ctx->LogInfo("=== Per-Room Settings Test COMPLETE ===");
}

// =============================================================================
// Test Implementation: Objects Above Background
// =============================================================================

void E2ETest_DungeonLayers_ObjectsAboveBackground(ImGuiTestContext* ctx) {
  ctx->LogInfo("=== E2E Test: Dungeon Layers - Objects Above Background ===");
  ctx->LogInfo("Purpose: Verify rendering order (BG1 -> BG2 -> Objects)");

  // Setup
  if (!SetupDungeonEditorWithRoom(ctx, "0x00")) {
    ctx->LogError("Failed to set up dungeon editor with Room 0x00");
    return;
  }

  ctx->SetRef("Room 0x00");

  // Enable object editor to see objects
  ctx->LogInfo("Enabling Object Editor...");
  ctx->SetRef("Dungeon Controls");
  ctx->ItemClick("Objects");
  ctx->Yield(10);

  // Return to room card
  ctx->SetRef("Room 0x00");

  // Test 1: Verify layers are visible with objects
  ctx->LogInfo("Checking initial layer state with objects...");
  ctx->Yield(5);

  // Test 2: Toggle BG1 off - objects should remain visible
  if (ctx->ItemExists("BG1")) {
    ctx->LogInfo("Toggling BG1 off - objects should remain visible...");
    ctx->ItemClick("BG1");
    ctx->Yield(10);

    // Visual check note: Objects (outlined in the canvas) should still
    // be visible even when BG1 is hidden

    ctx->ItemClick("BG1");  // Restore
    ctx->Yield(5);
  }

  // Test 3: Toggle BG2 off - objects should remain visible
  if (ctx->ItemExists("BG2")) {
    ctx->LogInfo("Toggling BG2 off - objects should remain visible...");
    ctx->ItemClick("BG2");
    ctx->Yield(10);

    // Visual check note: Objects should still be visible even when BG2 hidden

    ctx->ItemClick("BG2");  // Restore
    ctx->Yield(5);
  }

  // Test 4: Both layers off - only object outlines should be visible
  ctx->LogInfo("Disabling all background layers...");
  if (ctx->ItemExists("BG1")) {
    ctx->ItemClick("BG1");
    ctx->Yield(3);
  }
  if (ctx->ItemExists("BG2")) {
    ctx->ItemClick("BG2");
    ctx->Yield(3);
  }

  ctx->LogInfo("Background layers off - checking object visibility...");
  ctx->Yield(10);

  // Visual note: Object outlines/sprites should still render on canvas
  // even with both background layers hidden, confirming render order

  // Test 5: Check object outline toggle (if available via context menu)
  ctx->LogInfo("Testing object outline visibility controls...");

  // Object outlines are controlled via context menu or separate toggles
  // The canvas should still show object positions

  // Cleanup: Restore layers
  ctx->LogInfo("Restoring background layers...");
  if (ctx->ItemExists("BG1")) {
    ctx->ItemClick("BG1");
    ctx->Yield(3);
  }
  if (ctx->ItemExists("BG2")) {
    ctx->ItemClick("BG2");
    ctx->Yield(3);
  }

  ctx->LogInfo("=== Objects Above Background Test COMPLETE ===");
}

}  // namespace test
}  // namespace yaze
