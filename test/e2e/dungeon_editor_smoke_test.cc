#include "e2e/dungeon_editor_smoke_test.h"
#include "test_utils.h"
#include "app/core/controller.h"
#include "imgui_test_engine/imgui_te_context.h"

// Simple smoke test for dungeon editor
// Verifies that the editor opens and basic UI elements are present
void E2ETest_DungeonEditorSmokeTest(ImGuiTestContext* ctx)
{
    // Load ROM first
    yaze::test::gui::LoadRomInTest(ctx, "zelda3.sfc");
    
    // Open the Dungeon Editor
    yaze::test::gui::OpenEditorInTest(ctx, "Dungeon Editor");
    
    // Focus on the dungeon editor window
    ctx->WindowFocus("Dungeon Editor");
    
    // Log that we opened the editor
    ctx->LogInfo("Dungeon Editor window opened successfully");
    
    // Verify the 3-column layout exists
    // Check for Room Selector on the left
    ctx->SetRef("Dungeon Editor");
    
    // Check basic tabs exist
    ctx->ItemClick("Rooms##TabItemButton");
    ctx->LogInfo("Room selector tab clicked");
    
    // Check that we can see room list
    // Room 0 should exist in any valid zelda3.sfc
    ctx->ItemClick("Room 0x00");
    ctx->LogInfo("Selected room 0x00");
    
    // Verify canvas is present (center column)
    // The canvas should be focusable
    ctx->ItemClick("##Canvas");
    ctx->LogInfo("Canvas clicked");
    
    // Check Object Selector tab on right
    ctx->ItemClick("Object Selector##TabItemButton");
    ctx->LogInfo("Object selector tab clicked");
    
    // Log success
    ctx->LogInfo("Dungeon Editor smoke test completed successfully");
}

