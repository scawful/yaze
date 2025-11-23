#ifndef YAZE_TEST_E2E_DUNGEON_OBJECT_DRAWING_TEST_H
#define YAZE_TEST_E2E_DUNGEON_OBJECT_DRAWING_TEST_H

#include "imgui_test_engine/imgui_te_context.h"

/**
 * @file dungeon_object_drawing_test.h
 * @brief E2E tests for dungeon object drawing and manipulation
 *
 * Tests the object drawing system in the dungeon editor:
 * - Basic object placement
 * - Multi-layer object placement (BG1, BG2, BG3)
 * - Object deletion
 * - Object repositioning via drag
 *
 * Requires:
 * - ROM file for testing (zelda3.sfc)
 * - GUI test mode (--ui flag)
 */

namespace yaze {
namespace test {

/**
 * @brief Test basic object placement in a dungeon room
 *
 * Steps:
 * 1. Load ROM and open dungeon editor
 * 2. Open a room card
 * 3. Open object editor panel
 * 4. Select an object from the object selector
 * 5. Click on canvas to place the object
 * 6. Verify the object appears in the room's object list
 */
void E2ETest_DungeonObjectDrawing_BasicPlacement(ImGuiTestContext* ctx);

/**
 * @brief Test placing objects on multiple background layers
 *
 * Steps:
 * 1. Load ROM and open dungeon editor
 * 2. Open a room card
 * 3. Place objects on BG1, BG2, and BG3 layers
 * 4. Toggle layer visibility
 * 5. Verify objects appear/disappear based on layer visibility
 */
void E2ETest_DungeonObjectDrawing_MultiLayerObjects(ImGuiTestContext* ctx);

/**
 * @brief Test deleting objects from a dungeon room
 *
 * Steps:
 * 1. Load ROM and open dungeon editor
 * 2. Open a room with existing objects
 * 3. Select an object on the canvas
 * 4. Delete the object using the Delete key
 * 5. Verify the object is removed from the room's object list
 */
void E2ETest_DungeonObjectDrawing_ObjectDeletion(ImGuiTestContext* ctx);

/**
 * @brief Test repositioning objects via drag operation
 *
 * Steps:
 * 1. Load ROM and open dungeon editor
 * 2. Open a room with existing objects
 * 3. Click and drag an object to a new position
 * 4. Release the mouse button
 * 5. Verify the object's position has changed
 */
void E2ETest_DungeonObjectDrawing_ObjectRepositioning(ImGuiTestContext* ctx);

}  // namespace test
}  // namespace yaze

#endif  // YAZE_TEST_E2E_DUNGEON_OBJECT_DRAWING_TEST_H
