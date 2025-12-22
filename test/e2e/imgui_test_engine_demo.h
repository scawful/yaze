#ifndef YAZE_TEST_E2E_IMGUI_TEST_ENGINE_DEMO_H
#define YAZE_TEST_E2E_IMGUI_TEST_ENGINE_DEMO_H

struct ImGuiTestContext;

namespace yaze {
namespace test {

/**
 * @brief Demo: Basic ImGui Test Engine interaction patterns.
 *
 * Demonstrates: Window focus, menu navigation, button clicks, keyboard input.
 */
void E2ETest_ImGuiBasicInteraction(ImGuiTestContext* ctx);

/**
 * @brief Demo: Mouse interaction and coordinate verification.
 *
 * Shows: Mouse movement, click, drag operations, coordinate verification.
 */
void E2ETest_ImGuiMouseInteraction(ImGuiTestContext* ctx);

/**
 * @brief Demo: Keyboard shortcuts and input.
 *
 * Tests: Ctrl+S, Ctrl+Z, Ctrl+Y, arrow keys, and other shortcuts.
 */
void E2ETest_ImGuiKeyboardShortcuts(ImGuiTestContext* ctx);

/**
 * @brief Demo: Widget state verification.
 *
 * Demonstrates: Checking checkbox states, input values, combo selection.
 */
void E2ETest_ImGuiWidgetState(ImGuiTestContext* ctx);

/**
 * @brief Demo: Combining ImGui Test Engine with Screenshot Assertions.
 *
 * Shows integration of AI test infrastructure with ImGui Test Engine.
 */
void E2ETest_ImGuiWithScreenshotAssertion(ImGuiTestContext* ctx);

/**
 * @brief Demo: Test timing and synchronization.
 *
 * Demonstrates: Yield, waiting for UI updates, handling animations.
 */
void E2ETest_ImGuiTimingDemo(ImGuiTestContext* ctx);

/**
 * @brief Demo: Error handling and recovery.
 *
 * Shows: Handling missing widgets, failed operations, test recovery.
 */
void E2ETest_ImGuiErrorHandling(ImGuiTestContext* ctx);

}  // namespace test
}  // namespace yaze

#endif  // YAZE_TEST_E2E_IMGUI_TEST_ENGINE_DEMO_H
