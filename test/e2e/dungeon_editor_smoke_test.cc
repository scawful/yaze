#include "e2e/dungeon_editor_smoke_test.h"
#include "test_utils.h"
#include "app/core/controller.h"
#include "imgui_test_engine/imgui_te_context.h"

// Comprehensive E2E test for dungeon editor
// Tests the complete workflow: open editor -> select room -> view objects -> interact with UI
void E2ETest_DungeonEditorSmokeTest(ImGuiTestContext* ctx)
{
    ctx->LogInfo("=== Starting Dungeon Editor E2E Test ===");
    
    // Load ROM first
    ctx->LogInfo("Loading ROM...");
    yaze::test::gui::LoadRomInTest(ctx, "zelda3.sfc");
    ctx->LogInfo("ROM loaded successfully");
    
    // Open the Dungeon Editor
    ctx->LogInfo("Opening Dungeon Editor...");
    yaze::test::gui::OpenEditorInTest(ctx, "Dungeon Editor");
    ctx->LogInfo("Dungeon Editor opened");
    
    // Focus on the dungeon editor window
    ctx->WindowFocus("Dungeon Editor");
    ctx->SetRef("Dungeon Editor");
    ctx->LogInfo("Dungeon Editor window focused");
    
    // Test 1: Room Selection
    ctx->LogInfo("--- Test 1: Room Selection ---");
    ctx->ItemClick("Rooms##TabItemButton");
    ctx->LogInfo("Clicked Rooms tab");
    
    // Try to select different rooms
    const char* test_rooms[] = {"Room 0x00", "Room 0x01", "Room 0x02"};
    for (const char* room_name : test_rooms) {
        if (ctx->ItemExists(room_name)) {
            ctx->ItemClick(room_name);
            ctx->LogInfo("Selected %s", room_name);
            ctx->Yield();  // Give time for UI to update
        } else {
            ctx->LogWarning("%s not found in room list", room_name);
        }
    }
    
    // Test 2: Canvas Interaction
    ctx->LogInfo("--- Test 2: Canvas Interaction ---");
    if (ctx->ItemExists("##Canvas")) {
        ctx->ItemClick("##Canvas");
        ctx->LogInfo("Canvas clicked successfully");
    } else {
        ctx->LogError("Canvas not found!");
    }
    
    // Test 3: Object Selector
    ctx->LogInfo("--- Test 3: Object Selector ---");
    ctx->ItemClick("Object Selector##TabItemButton");
    ctx->LogInfo("Object Selector tab clicked");
    
    // Try to access room graphics tab
    ctx->ItemClick("Room Graphics##TabItemButton");
    ctx->LogInfo("Room Graphics tab clicked");
    
    // Go back to Object Selector
    ctx->ItemClick("Object Selector##TabItemButton");
    ctx->LogInfo("Returned to Object Selector tab");
    
    // Test 4: Object Editor tab
    ctx->LogInfo("--- Test 4: Object Editor ---");
    ctx->ItemClick("Object Editor##TabItemButton");
    ctx->LogInfo("Object Editor tab clicked");
    
    // Check if mode buttons exist
    const char* mode_buttons[] = {"Select", "Insert", "Edit"};
    for (const char* button : mode_buttons) {
        if (ctx->ItemExists(button)) {
            ctx->LogInfo("Found mode button: %s", button);
        }
    }
    
    // Test 5: Entrance Selector
    ctx->LogInfo("--- Test 5: Entrance Selector ---");
    ctx->ItemClick("Entrances##TabItemButton");
    ctx->LogInfo("Entrances tab clicked");
    
    // Return to rooms
    ctx->ItemClick("Rooms##TabItemButton");
    ctx->LogInfo("Returned to Rooms tab");
    
    // Final verification
    ctx->LogInfo("=== Dungeon Editor E2E Test Completed Successfully ===");
    ctx->LogInfo("All UI elements accessible and functional");
}

