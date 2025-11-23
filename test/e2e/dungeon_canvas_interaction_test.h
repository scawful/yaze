#ifndef YAZE_TEST_E2E_DUNGEON_CANVAS_INTERACTION_TEST_H
#define YAZE_TEST_E2E_DUNGEON_CANVAS_INTERACTION_TEST_H

#include "imgui_test_engine/imgui_te_context.h"

/**
 * @brief Tests for dungeon canvas pan and zoom interactions
 *
 * Verifies that the dungeon room canvas supports:
 * - Mouse wheel zoom in/out
 * - Right-click drag to pan the view
 * - View reset functionality
 */
void E2ETest_DungeonCanvas_PanZoom(ImGuiTestContext* ctx);

/**
 * @brief Tests for object selection on the dungeon canvas
 *
 * Verifies that clicking on the canvas:
 * - Selects objects at the clicked position
 * - Deselects when clicking empty space
 * - Shows selection highlight on selected objects
 */
void E2ETest_DungeonCanvas_ObjectSelection(ImGuiTestContext* ctx);

/**
 * @brief Tests for grid snap behavior when placing objects
 *
 * Verifies that objects snap to the 8x8 grid:
 * - Object placement aligns to grid boundaries
 * - Object movement respects grid snapping
 */
void E2ETest_DungeonCanvas_GridSnap(ImGuiTestContext* ctx);

/**
 * @brief Tests for multi-select functionality with shift-click
 *
 * Verifies that shift-click:
 * - Adds objects to current selection
 * - Allows selecting multiple objects
 * - Selection can be cleared with click on empty space
 */
void E2ETest_DungeonCanvas_MultiSelect(ImGuiTestContext* ctx);

#endif  // YAZE_TEST_E2E_DUNGEON_CANVAS_INTERACTION_TEST_H
