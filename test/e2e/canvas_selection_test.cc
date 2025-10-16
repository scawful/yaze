#define IMGUI_DEFINE_MATH_OPERATORS
#include "e2e/canvas_selection_test.h"
#include "app/controller.h"
#include "test_utils.h"

void E2ETest_CanvasSelectionTest(ImGuiTestContext* ctx)
{
    yaze::test::gui::LoadRomInTest(ctx, "zelda3.sfc");
    yaze::Controller* controller = (yaze::Controller*)ctx->Test->UserData;
    yaze::zelda3::Overworld* overworld = controller->overworld();

    // 1. Open the Overworld Editor
    yaze::test::gui::OpenEditorInTest(ctx, "Overworld Editor");

    // 2. Find the canvas
    ctx->WindowFocus("Overworld Editor");
    ctx->ItemClick("##Canvas");

    // 3. Get the original tile data
    // We'll check the 2x2 tile area at the paste location (600, 300)
    // The tile at (600, 300) is at (75, 37) in tile coordinates.
    // The overworld map is 128x128 tiles.
    uint16_t orig_tile1 = overworld->GetTile(75, 37);
    uint16_t orig_tile2 = overworld->GetTile(76, 37);
    uint16_t orig_tile3 = overworld->GetTile(75, 38);
    uint16_t orig_tile4 = overworld->GetTile(76, 38);

    // 4. Perform a rectangle selection that crosses a 512px boundary
    // The canvas is 1024x1024, with the top-left at (0,0).
    // We'll select a 2x2 tile area from (510, 256) to (514, 258).
    // This will cross the 512px boundary.
    ctx->MouseMoveToPos(ImVec2(510, 256));
    ctx->MouseDown(0);
    ctx->MouseMoveToPos(ImVec2(514, 258));
    ctx->MouseUp(0);

    // 5. Copy the selection
    ctx->KeyDown(ImGuiKey_LeftCtrl);
    ctx->KeyPress(ImGuiKey_C);
    ctx->KeyUp(ImGuiKey_LeftCtrl);

    // 6. Paste the selection
    ctx->MouseMoveToPos(ImVec2(600, 300));
    ctx->KeyDown(ImGuiKey_LeftCtrl);
    ctx->KeyPress(ImGuiKey_V);
    ctx->KeyUp(ImGuiKey_LeftCtrl);

    // 7. Verify that the pasted tiles are correct
    uint16_t new_tile1 = overworld->GetTile(75, 37);
    uint16_t new_tile2 = overworld->GetTile(76, 37);
    uint16_t new_tile3 = overworld->GetTile(75, 38);
    uint16_t new_tile4 = overworld->GetTile(76, 38);

    // The bug is that the selection wraps around, so the pasted tiles are incorrect.
    // We expect the new tiles to be different from the original tiles.
    IM_CHECK_NE(orig_tile1, new_tile1);
    IM_CHECK_NE(orig_tile2, new_tile2);
    IM_CHECK_NE(orig_tile3, new_tile3);
    IM_CHECK_NE(orig_tile4, new_tile4);

    // We also expect the pasted tiles to be the same as the selected tiles.
    // The selected tiles are at (63, 32) and (64, 32), (63, 33) and (64, 33).
    uint16_t selected_tile1 = overworld->GetTile(63, 32);
    uint16_t selected_tile2 = overworld->GetTile(64, 32);
    uint16_t selected_tile3 = overworld->GetTile(63, 33);
    uint16_t selected_tile4 = overworld->GetTile(64, 33);

    IM_CHECK_EQ(new_tile1, selected_tile1);
    IM_CHECK_EQ(new_tile2, selected_tile2);
    IM_CHECK_EQ(new_tile3, selected_tile3);
    IM_CHECK_EQ(new_tile4, selected_tile4);

    ctx->LogInfo("Original tiles: %d, %d, %d, %d", orig_tile1, orig_tile2, orig_tile3, orig_tile4);
    ctx->LogInfo("Selected tiles: %d, %d, %d, %d", selected_tile1, selected_tile2, selected_tile3, selected_tile4);
    ctx->LogInfo("New tiles: %d, %d, %d, %d", new_tile1, new_tile2, new_tile3, new_tile4);
}
