#ifndef YAZE_APP_TEST_OVERWORLD_UI_TESTS_H_
#define YAZE_APP_TEST_OVERWORLD_UI_TESTS_H_

// Tier 1 ImGui Test Engine tests for overworld editor subsystems.
//
// These tests run inside the live GUI loop and exercise:
//   - Keyboard shortcut mode switching (1/2/B/F keys)
//   - Canvas navigation smoke tests (pan, zoom)
//   - Toolbar toolbar existence and interaction
//   - Map lock toggle (Ctrl+L)
//   - Fullscreen toggle (F11)
//
// Requirements:
//   - YAZE_ENABLE_IMGUI_TEST_ENGINE must be defined
//   - ImGui Test Engine must be initialized (TestManager::InitializeUITesting)
//   - A ROM must be loaded with the overworld editor active
//
// Tests are registered via RegisterOverworldUITests() called from
// TestManager::InitializeUITesting().

struct ImGuiTestEngine;

namespace yaze::test {

/// Register all Tier 1 overworld editor UI tests with the test engine.
/// Call this after ImGuiTestEngine_Start() during InitializeUITesting().
void RegisterOverworldUITests(ImGuiTestEngine* engine);

}  // namespace yaze::test

#endif  // YAZE_APP_TEST_OVERWORLD_UI_TESTS_H_
