#ifndef YAZE_TEST_E2E_AI_MULTIMODAL_TEST_H
#define YAZE_TEST_E2E_AI_MULTIMODAL_TEST_H

struct ImGuiTestContext;

namespace yaze {
namespace test {

/**
 * @brief AI Vision verification test using LLM-based UI evaluation.
 *
 * Demonstrates:
 * - Natural language condition verification
 * - Multi-condition batch verification
 * - Open-ended state queries
 */
void E2ETest_AIVisionVerification(ImGuiTestContext* ctx);

/**
 * @brief Screenshot comparison test using pixel-based verification.
 *
 * Demonstrates:
 * - Baseline capture and comparison
 * - Region-based comparison
 * - Color presence verification
 * - Diff image generation
 */
void E2ETest_ScreenshotAssertion(ImGuiTestContext* ctx);

/**
 * @brief Hybrid AI + screenshot test combining both approaches.
 *
 * Demonstrates:
 * - Using pixel comparison for change detection
 * - Using AI vision for semantic verification
 * - Combining approaches for comprehensive testing
 */
void E2ETest_HybridAIScreenshotTest(ImGuiTestContext* ctx);

}  // namespace test
}  // namespace yaze

#endif  // YAZE_TEST_E2E_AI_MULTIMODAL_TEST_H
