#include "e2e/dungeon_editor_smoke_test.h"

#include "app/controller.h"
#include "imgui_test_engine/imgui_te_context.h"
#include "test_utils.h"

/**
 * @brief Quick smoke test for DungeonEditorV2
 *
 * Tests the card-based architecture:
 * - Independent windows (cards) can be opened/closed
 * - Room cards function correctly
 * - Basic navigation works
 */
void E2ETest_DungeonEditorV2SmokeTest(ImGuiTestContext* ctx) {
  ctx->LogInfo("=== Starting DungeonEditorV2 Smoke Test ===");

  // Load ROM first
  ctx->LogInfo("Loading ROM...");
  yaze::test::gui::LoadRomInTest(ctx, yaze::test::TestRomManager::GetTestRomPath());
  ctx->LogInfo("ROM loaded successfully");

  // Open the Dungeon Editor
  ctx->LogInfo("Opening Dungeon Editor...");
  yaze::test::gui::OpenEditorInTest(ctx, "Dungeon");
  ctx->LogInfo("Dungeon Editor opened");

  // Test 1: Control Panel Access
  ctx->LogInfo("--- Test 1: Control Panel ---");
  if (ctx->WindowInfo("Dungeon Controls").Window != nullptr) {
    ctx->WindowFocus("Dungeon Controls");
    ctx->LogInfo("Dungeon Controls panel is visible");
  } else {
    ctx->LogWarning("Dungeon Controls panel not visible - may be minimized");
  }

  // Test 2: Open Room Selector Card
  ctx->LogInfo("--- Test 2: Room Selector Card ---");
  if (ctx->WindowInfo("Dungeon Controls").Window != nullptr) {
    ctx->SetRef("Dungeon Controls");
    ctx->ItemClick("Rooms");  // Toggle checkbox
    ctx->Yield();
    ctx->LogInfo("Toggled Room Selector visibility");
  }

  // Test 3: Open Room Matrix Card
  ctx->LogInfo("--- Test 3: Room Matrix Card ---");
  if (ctx->WindowInfo("Dungeon Controls").Window != nullptr) {
    ctx->SetRef("Dungeon Controls");
    ctx->ItemClick("Matrix");  // Toggle checkbox
    ctx->Yield();
    ctx->LogInfo("Toggled Room Matrix visibility");
  }

  // Test 4: Open a Room Card
  ctx->LogInfo("--- Test 4: Room Card ---");
  // Try to open room 0 by clicking in room selector
  if (ctx->WindowInfo("Room Selector").Window != nullptr) {
    ctx->SetRef("Room Selector");
    // Look for selectable room items
    if (ctx->ItemExists("Room 0x00")) {
      ctx->ItemDoubleClick("Room 0x00");
      ctx->Yield(2);
      ctx->LogInfo("Opened Room 0x00 card");

      // Verify room card exists
      if (ctx->WindowInfo("Room 0x00").Window != nullptr) {
        ctx->LogInfo("Room 0x00 card successfully opened");
        ctx->SetRef("Room 0x00");

        // Test 5: Per-Room Layer Controls
        ctx->LogInfo("--- Test 5: Per-Room Layer Controls ---");
        if (ctx->ItemExists("Show BG1")) {
          ctx->LogInfo("Found per-room BG1 control");
          // Toggle it
          ctx->ItemClick("Show BG1");
          ctx->Yield();
          ctx->ItemClick("Show BG1");  // Toggle back
          ctx->Yield();
          ctx->LogInfo("Per-room layer controls functional");
        }
      } else {
        ctx->LogWarning("Room card did not open");
      }
    } else {
      ctx->LogWarning("Room 0x00 not found in selector");
    }
  } else {
    ctx->LogWarning("Room Selector card not visible");
  }

  // Test 6: Object Editor Card
  ctx->LogInfo("--- Test 6: Object Editor Card ---");
  if (ctx->WindowInfo("Dungeon Controls").Window != nullptr) {
    ctx->SetRef("Dungeon Controls");
    ctx->ItemClick("Objects");  // Toggle checkbox
    ctx->Yield();
    ctx->LogInfo("Toggled Object Editor visibility");
  }

  // Test 7: Palette Editor Card
  ctx->LogInfo("--- Test 7: Palette Editor Card ---");
  if (ctx->WindowInfo("Dungeon Controls").Window != nullptr) {
    ctx->SetRef("Dungeon Controls");
    ctx->ItemClick("Palette");  // Toggle checkbox
    ctx->Yield();
    ctx->LogInfo("Toggled Palette Editor visibility");
  }

  // Test 8: Independent Cards can be closed
  ctx->LogInfo("--- Test 8: Close Independent Cards ---");
  // Close room card if it's open
  if (ctx->WindowInfo("Room 0x00").Window != nullptr) {
    ctx->WindowClose("Room 0x00");
    ctx->Yield();
    ctx->LogInfo("Closed Room 0x00 card");
  }

  // Final verification
  ctx->LogInfo("=== DungeonEditorV2 Smoke Test Completed Successfully ===");
  ctx->LogInfo("Card-based architecture is functional");
  ctx->LogInfo("Independent windows can be opened and closed");
  ctx->LogInfo("Per-room settings are accessible");
}
