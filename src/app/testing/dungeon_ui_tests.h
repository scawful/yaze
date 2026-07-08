#ifndef YAZE_APP_TEST_DUNGEON_UI_TESTS_H_
#define YAZE_APP_TEST_DUNGEON_UI_TESTS_H_

// Tier 1 ImGui Test Engine tests for dungeon editor panels.
//
// These tests run inside the live GUI loop and exercise panel-level
// interactions: opening/closing panels, toggling layer visibility,
// verifying widget state after actions, and basic keyboard shortcuts.
//
// Requirements:
//   - YAZE_ENABLE_IMGUI_TEST_ENGINE must be defined
//   - ImGui Test Engine must be initialized (TestManager::InitializeUITesting)
//   - A ROM must be loaded with a dungeon editor active
//
// Tests are registered via RegisterDungeonUITests() called from
// TestManager::InitializeUITesting().

struct ImGuiTestEngine;

namespace yaze::test {

/// Register all Tier 1 dungeon editor UI tests with the test engine.
/// Call this after ImGuiTestEngine_Start() during InitializeUITesting().
void RegisterDungeonUITests(ImGuiTestEngine* engine);

}  // namespace yaze::test

#endif  // YAZE_APP_TEST_DUNGEON_UI_TESTS_H_
