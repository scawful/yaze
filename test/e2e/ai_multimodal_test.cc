#include "e2e/ai_multimodal_test.h"

#include "app/test/ai_vision_verifier.h"
#include "app/test/screenshot_assertion.h"
#include "imgui.h"
#include "imgui_test_engine/imgui_te_context.h"
#include "test_utils.h"

namespace yaze {
namespace test {

/**
 * @brief AI Vision verification test demonstrating LLM-based UI testing.
 *
 * This test shows how to use the AIVisionVerifier to verify UI state
 * using natural language conditions that are evaluated by a vision model.
 *
 * Prerequisites:
 * - Vision model endpoint configured (Gemini/Ollama/OpenAI)
 * - Screenshot capture callback registered
 */
void E2ETest_AIVisionVerification(ImGuiTestContext* ctx) {
  // Load ROM first
  gui::LoadRomInTest(ctx, "zelda3.sfc");

  // Open the overworld editor
  gui::OpenEditorInTest(ctx, "Overworld");
  ctx->Yield(10);  // Let the editor render

  // Configure AI vision verifier
  VisionVerifierConfig config;
  config.model_name = "gemini-pro-vision";
  config.screenshot_dir = "/tmp/yaze_test_screenshots";
  config.confidence_threshold = 0.7f;

  AIVisionVerifier verifier(config);

  // Register screenshot capture callback
  // In production, this would capture from the actual window
  verifier.SetScreenshotCallback(
      [ctx](int* width, int* height) -> absl::StatusOr<std::vector<uint8_t>> {
        // Placeholder - in real test this captures the ImGui framebuffer
        *width = 1280;
        *height = 720;
        return std::vector<uint8_t>(1280 * 720 * 4, 0);
      });

  // Test 1: Verify panel visibility using natural language
  auto panel_result = verifier.VerifyPanelVisible("Overworld Canvas");
  if (panel_result.ok()) {
    IM_CHECK(panel_result->passed);
    IM_CHECK_GT(panel_result->confidence, 0.5f);
  }

  // Test 2: Verify multiple conditions at once
  std::vector<std::string> conditions = {
      "The overworld map is visible in the main canvas area",
      "There is a tile selector panel on the left or right side",
      "The menu bar is visible at the top of the window"};

  auto multi_result = verifier.VerifyConditions(conditions);
  if (multi_result.ok()) {
    ctx->LogInfo("AI Vision Test: %s (confidence: %.2f)",
                 multi_result->passed ? "PASSED" : "FAILED",
                 multi_result->confidence);
  }

  // Test 3: Ask open-ended question about UI state
  auto state_result = verifier.AskAboutState(
      "What map area is currently selected in the overworld editor?");
  if (state_result.ok()) {
    ctx->LogInfo("AI State Query Response: %s", state_result->c_str());
  }
}

/**
 * @brief Screenshot comparison test demonstrating pixel-based UI testing.
 *
 * This test shows how to use ScreenshotAssertion to verify UI state
 * using pixel-level comparison against reference images.
 */
void E2ETest_ScreenshotAssertion(ImGuiTestContext* ctx) {
  gui::LoadRomInTest(ctx, "zelda3.sfc");
  gui::OpenEditorInTest(ctx, "Graphics");
  ctx->Yield(10);

  // Configure screenshot assertion
  ComparisonConfig config;
  config.tolerance = 0.95f;  // 95% similarity required
  config.algorithm = ComparisonConfig::Algorithm::kPixelExact;
  config.color_threshold = 5;  // Allow small color variations
  config.generate_diff_image = true;
  config.diff_output_dir = "/tmp/yaze_test_diffs";

  ScreenshotAssertion asserter;
  asserter.SetConfig(config);

  // Register capture callback
  asserter.SetCaptureCallback(
      []() -> absl::StatusOr<Screenshot> {
        Screenshot shot;
        shot.width = 1280;
        shot.height = 720;
        shot.data.resize(shot.width * shot.height * 4, 128);
        return shot;
      });

  // Test 1: Capture baseline
  auto baseline_status = asserter.CaptureBaseline("graphics_editor_initial");
  IM_CHECK(baseline_status.ok());

  // Perform some action (simulated)
  ctx->ItemClick("**/GraphicsSheet_0");
  ctx->Yield(5);

  // Test 2: Verify change occurred
  auto change_result = asserter.AssertChanged("graphics_editor_initial");
  if (change_result.ok()) {
    ctx->LogInfo("Screenshot changed: %s (similarity: %.2f%%)",
                 change_result->passed ? "YES" : "NO",
                 change_result->similarity * 100);
  }

  // Test 3: Region-based comparison
  ScreenRegion tile_region{0, 0, 256, 256};  // Tile selector region
  auto region_result = asserter.AssertRegionMatches(
      "/tmp/reference_tile_selector.raw", tile_region);

  // Test 4: Color presence verification
  auto color_result = asserter.AssertRegionContainsColor(
      tile_region,
      0, 128, 0,  // Green (typical grass tile color)
      0.1f);      // At least 10% coverage

  if (color_result.ok()) {
    IM_CHECK(*color_result);  // Green should be present in tile graphics
  }
}

/**
 * @brief Combined AI and screenshot test demonstrating hybrid approach.
 *
 * This test shows how to combine AI vision and screenshot assertions
 * for comprehensive UI testing.
 */
void E2ETest_HybridAIScreenshotTest(ImGuiTestContext* ctx) {
  gui::LoadRomInTest(ctx, "zelda3.sfc");
  gui::OpenEditorInTest(ctx, "Dungeon");
  ctx->Yield(10);

  // Screenshot assertion for pixel-level verification
  ScreenshotAssertion screenshot;
  screenshot.SetCaptureCallback([]() -> absl::StatusOr<Screenshot> {
    Screenshot shot;
    shot.width = 1280;
    shot.height = 720;
    shot.data.resize(shot.width * shot.height * 4, 64);
    return shot;
  });

  // AI vision for semantic verification
  VisionVerifierConfig vision_config;
  vision_config.model_name = "ollama/llava";
  AIVisionVerifier vision(vision_config);
  vision.SetScreenshotCallback(
      [](int* w, int* h) -> absl::StatusOr<std::vector<uint8_t>> {
        *w = 1280;
        *h = 720;
        return std::vector<uint8_t>(*w * *h * 4, 0);
      });

  // Step 1: Capture initial state
  screenshot.CaptureBaseline("dungeon_initial");

  // Step 2: Perform edit action
  ctx->SetRef("Dungeon Editor");
  ctx->ItemClick("**/RoomSelector_0");
  ctx->Yield(5);

  // Step 3: Pixel verification - something changed
  auto pixel_result = screenshot.AssertChanged("dungeon_initial");
  if (pixel_result.ok()) {
    IM_CHECK(pixel_result->passed);
  }

  // Step 4: AI verification - correct change
  auto ai_result = vision.Verify(
      "A dungeon room is displayed in the editor canvas showing tile layout");
  if (ai_result.ok()) {
    ctx->LogInfo("AI verified room display: %s",
                 ai_result->passed ? "PASS" : "FAIL");
  }
}

}  // namespace test
}  // namespace yaze
