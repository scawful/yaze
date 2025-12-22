#define IMGUI_DEFINE_MATH_OPERATORS
#include "e2e/imgui_test_engine_demo.h"

#include "app/controller.h"
#include "app/test/screenshot_assertion.h"
#include "imgui.h"
#include "imgui_test_engine/imgui_te_context.h"
#include "imgui_test_engine/imgui_te_engine.h" // Added include for ImGuiTestEngine
#include "test_utils.h"

namespace yaze {
namespace test {

/**
 * @brief Demo: Basic ImGui Test Engine interaction patterns.
 *
 * This test demonstrates fundamental ImGui Test Engine features:
 * - Window focus and navigation
 * - Menu interaction
 * - Button clicks
 * - Keyboard input
 * - Mouse movement and dragging
 */
void E2ETest_ImGuiBasicInteraction(ImGuiTestContext* ctx) {
  // Set reference window for navigation
  ctx->SetRef("Yaze");

  // Test 1: Menu navigation
  ctx->LogInfo("Testing menu navigation...");
  ctx->MenuCheck("View/Show Demo Window");
  ctx->Yield(5);

  // Test 2: Window focus
  ctx->WindowFocus("Dear ImGui Demo");
  ctx->Yield(3);

  // Test 3: Tree node expansion
  ctx->ItemOpen("Widgets");
  ctx->Yield(2);

  // Test 4: Button click
  ctx->ItemClick("Basic/Button");
  ctx->Yield(2);

  // Test 5: Close the demo window
  ctx->WindowClose("Dear ImGui Demo");

  ctx->LogInfo("Basic interaction test completed");
}

/**
 * @brief Demo: Mouse interaction and coordinate verification.
 *
 * Shows how to:
 * - Move mouse to specific coordinates
 * - Perform drag operations
 * - Verify mouse position
 */
void E2ETest_ImGuiMouseInteraction(ImGuiTestContext* ctx) {
  ctx->SetRef("Yaze");

  // Open a canvas-based editor
  ctx->MenuClick("View/Overworld Editor");
  ctx->Yield(10);

  ctx->WindowFocus("Overworld Editor");

  // Find a canvas widget
  ImGuiTestItemInfo canvas = ctx->ItemInfo("##OverworldCanvas");
  if (canvas.ID != 0) {
    // Get canvas bounds
    ImRect bounds = canvas.RectClipped;

    // Test: Click at canvas center
    ImVec2 center((bounds.Min.x + bounds.Max.x) / 2,
                  (bounds.Min.y + bounds.Max.y) / 2);
    ctx->MouseMoveToPos(center);
    ctx->MouseClick(0);
    ctx->LogInfo("Clicked canvas at (%.1f, %.1f)", center.x, center.y);

    // Test: Drag selection
    ImVec2 drag_start(bounds.Min.x + 50, bounds.Min.y + 50);
    ImVec2 drag_end(bounds.Min.x + 150, bounds.Min.y + 150);
    ctx->MouseMoveToPos(drag_start);
    ctx->MouseDown(0);
    ctx->MouseMoveToPos(drag_end);
    ctx->MouseUp(0);
    ctx->LogInfo("Dragged from (%.1f, %.1f) to (%.1f, %.1f)", drag_start.x,
                 drag_start.y, drag_end.x, drag_end.y);
  } else {
    ctx->LogWarning("Canvas not found - skipping mouse interaction test");
  }
}

/**
 * @brief Demo: Keyboard shortcuts and input.
 *
 * Tests common keyboard shortcuts like:
 * - Ctrl+S (Save)
 * - Ctrl+Z (Undo)
 * - Ctrl+Y (Redo)
 * - Arrow keys for navigation
 */
void E2ETest_ImGuiKeyboardShortcuts(ImGuiTestContext* ctx) {
  gui::LoadRomInTest(ctx, yaze::test::TestRomManager::GetTestRomPath());

  ctx->SetRef("Yaze");
  ctx->WindowFocus("Overworld Editor");
  ctx->Yield(5);

  // Test: Undo shortcut
  ctx->LogInfo("Testing Ctrl+Z (Undo)...");
  ctx->KeyDown(ImGuiMod_Ctrl);
  ctx->KeyPress(ImGuiKey_Z);
  ctx->KeyUp(ImGuiMod_Ctrl);
  ctx->Yield(2);

  // Test: Redo shortcut
  ctx->LogInfo("Testing Ctrl+Y (Redo)...");
  ctx->KeyDown(ImGuiMod_Ctrl);
  ctx->KeyPress(ImGuiKey_Y);
  ctx->KeyUp(ImGuiMod_Ctrl);
  ctx->Yield(2);

  // Test: Arrow key navigation
  ctx->LogInfo("Testing arrow key navigation...");
  ctx->KeyPress(ImGuiKey_RightArrow);
  ctx->KeyPress(ImGuiKey_DownArrow);
  ctx->KeyPress(ImGuiKey_LeftArrow);
  ctx->KeyPress(ImGuiKey_UpArrow);

  ctx->LogInfo("Keyboard shortcuts test completed");
}

/**
 * @brief Demo: Widget state verification.
 *
 * Demonstrates how to check widget states:
 * - Checkbox states
 * - Input field values
 * - Combo box selection
 */
void E2ETest_ImGuiWidgetState(ImGuiTestContext* ctx) {
  ctx->SetRef("Yaze");

  // Open settings dialog
  ctx->MenuClick("Edit/Settings");
  ctx->Yield(5);

  ctx->WindowFocus("Settings");

  // Test: Check if a checkbox exists and interact with it
  ImGuiTestItemInfo item = ctx->ItemInfo("**/Dark Mode");
  if (item.ID != 0) {
    // Toggle the checkbox
    ctx->ItemClick("**/Dark Mode");
    ctx->Yield(2);
    ctx->LogInfo("Toggled Dark Mode checkbox");
  }

  // Close settings
  ctx->WindowClose("Settings");
}

/**
 * @brief Demo: Combining ImGui Test Engine with Screenshot Assertions.
 *
 * This test shows how to integrate our AI test infrastructure with
 * the ImGui Test Engine for comprehensive visual verification.
 */
void E2ETest_ImGuiWithScreenshotAssertion(ImGuiTestContext* ctx) {
  gui::LoadRomInTest(ctx, yaze::test::TestRomManager::GetTestRomPath());

  ctx->SetRef("Yaze");
  ctx->MenuClick("View/Graphics Editor");
  ctx->Yield(10);

  ctx->WindowFocus("Graphics Editor");

  // Set up screenshot assertion (would need actual framebuffer capture)
  ComparisonConfig config;
  config.tolerance = 0.95f;
  config.algorithm = ComparisonConfig::Algorithm::kPixelExact;

  ScreenshotAssertion asserter;
  asserter.SetConfig(config);

  // Register capture callback that uses ImGui's framebuffer
  // In real implementation, this would capture from SDL/OpenGL
  asserter.SetCaptureCallback([]() -> absl::StatusOr<Screenshot> {
    Screenshot shot;
    // Would capture actual framebuffer here
    shot.width = 1280;
    shot.height = 720;
    shot.data.resize(shot.width * shot.height * 4, 128);
    return shot;
  });

  // Capture baseline before interaction
  auto baseline_status = asserter.CaptureBaseline("graphics_baseline");
  IM_CHECK(baseline_status.ok());
  ctx->LogInfo("Captured baseline screenshot");

  // Perform an action - click on a graphics sheet
  ctx->ItemClick("**/Sheet_0");
  ctx->Yield(5);

  // Verify the UI changed
  auto change_result = asserter.AssertChanged("graphics_baseline");
  if (change_result.ok()) {
    ctx->LogInfo("Change detection: %s (similarity: %.2f%%)",
                 change_result->passed ? "CHANGED" : "UNCHANGED",
                 change_result->similarity * 100.0f);
    // In a real test, we'd assert change->passed is true
  }

  ctx->LogInfo("Screenshot assertion test completed");
}

/**
 * @brief Demo: Test timing and synchronization.
 *
 * Shows how to handle timing-sensitive operations:
 * - Waiting for UI updates
 * - Yielding for animations
 * - Verifying state after delays
 */
void E2ETest_ImGuiTimingDemo(ImGuiTestContext* ctx) {
  ctx->SetRef("Yaze");

  // Test speed affects how long Yield() waits
  ctx->LogInfo("Current test speed: %d", ImGuiTestEngine_GetIO(ctx->Engine).ConfigRunSpeed);

  // Open an editor that has loading time
  ctx->MenuClick("View/Dungeon Editor");

  // Yield for the editor to fully load
  ctx->LogInfo("Waiting for editor to load...");
  ctx->Yield(20);

  // Check if the editor window appeared
  ImGuiWindow* window = ctx->GetWindowByRef("Dungeon Editor");
  if (window) {
    ctx->LogInfo("Dungeon Editor loaded successfully");
    IM_CHECK(window->Active || window->WasActive);
  } else {
    ctx->LogWarning("Dungeon Editor window not found");
  }

  // Yield can also be used with frame counts
  ctx->LogInfo("Yielding for 10 frames...");
  ctx->Yield(10);

  ctx->LogInfo("Timing demo completed");
}

/**
 * @brief Demo: Error handling and recovery.
 *
 * Shows how to handle:
 * - Missing widgets gracefully
 * - Failed operations
 * - Test recovery
 */
void E2ETest_ImGuiErrorHandling(ImGuiTestContext* ctx) {
  ctx->SetRef("Yaze");

  // Attempt to interact with a non-existent widget
  ctx->LogInfo("Testing missing widget handling...");

  ImGuiTestItemInfo item = ctx->ItemInfo("NonExistentWidget");
  if (item.ID == 0) {
    ctx->LogInfo("Correctly detected missing widget");
  }

  // Try to click something that might not exist
  // Using ItemClick with ImGuiTestOpFlags_NoError to suppress errors
  ctx->ItemClick("**/MaybeExistsButton", 0, ImGuiTestOpFlags_NoError);

  // Recovery: ensure we're in a known good state
  ctx->SetRef("Yaze");
  ctx->WindowFocus("");  // Focus main window

  ctx->LogInfo("Error handling demo completed");
}

}  // namespace test
}  // namespace yaze
