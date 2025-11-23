#ifndef YAZE_TEST_E2E_DUNGEON_LAYER_RENDERING_TEST_H_
#define YAZE_TEST_E2E_DUNGEON_LAYER_RENDERING_TEST_H_

struct ImGuiTestContext;

namespace yaze {
namespace test {

/**
 * @brief Toggle BG1 layer visibility and verify canvas updates
 *
 * Tests that the BG1 (background layer 1) checkbox in the room card
 * properly toggles visibility, and the canvas reflects the change.
 */
void E2ETest_DungeonLayers_ToggleBG1(ImGuiTestContext* ctx);

/**
 * @brief Toggle BG2 layer visibility and verify canvas updates
 *
 * Tests that the BG2 (background layer 2) checkbox in the room card
 * properly toggles visibility, and the canvas reflects the change.
 */
void E2ETest_DungeonLayers_ToggleBG2(ImGuiTestContext* ctx);

/**
 * @brief Turn off all layers and verify blank canvas
 *
 * Tests that when all layer checkboxes (BG1, BG2) are unchecked,
 * the canvas renders with no background layers visible.
 */
void E2ETest_DungeonLayers_AllLayersOff(ImGuiTestContext* ctx);

/**
 * @brief Open two rooms and verify independent layer controls
 *
 * Tests that each room card maintains its own layer visibility settings.
 * Toggling layers in one room should not affect another room's display.
 */
void E2ETest_DungeonLayers_PerRoomSettings(ImGuiTestContext* ctx);

/**
 * @brief Verify objects render above background layers
 *
 * Tests the rendering order: BG1 -> BG2 -> Objects.
 * Objects should always appear on top of background layers.
 */
void E2ETest_DungeonLayers_ObjectsAboveBackground(ImGuiTestContext* ctx);

}  // namespace test
}  // namespace yaze

#endif  // YAZE_TEST_E2E_DUNGEON_LAYER_RENDERING_TEST_H_
