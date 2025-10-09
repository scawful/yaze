#ifndef YAZE_TEST_E2E_DUNGEON_EDITOR_SMOKE_TEST_H
#define YAZE_TEST_E2E_DUNGEON_EDITOR_SMOKE_TEST_H

#include "imgui_test_engine/imgui_te_context.h"

/**
 * @brief Quick smoke test for DungeonEditorV2 card-based UI
 * 
 * Tests basic functionality:
 * - Opening dungeon editor
 * - Opening independent cards (Rooms, Matrix, Objects, etc.)
 * - Opening room cards
 * - Basic interaction with canvas
 */
void E2ETest_DungeonEditorV2SmokeTest(ImGuiTestContext* ctx);

#endif  // YAZE_TEST_E2E_DUNGEON_EDITOR_SMOKE_TEST_H

