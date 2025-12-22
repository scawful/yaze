#ifndef YAZE_APP_TEST_AI_VISION_VERIFIER_H
#define YAZE_APP_TEST_AI_VISION_VERIFIER_H

#include <chrono>
#include <functional>
#include <memory>
#include <string>
#include <vector>

#include "absl/status/status.h"
#include "absl/status/statusor.h"

// Forward declare AI service types (avoid circular dependency)
namespace yaze {
namespace cli {
class AIService;
}
}  // namespace yaze

namespace yaze {
namespace test {

/**
 * @brief Result of an AI vision verification check.
 *
 * Contains both the AI's assessment and metadata about the verification.
 */
struct VisionVerificationResult {
  bool passed = false;
  float confidence = 0.0f;  // 0.0 to 1.0
  std::string ai_response;
  std::string screenshot_path;
  std::chrono::milliseconds latency{0};
  std::string error_message;

  // Detailed findings from the AI
  std::vector<std::string> observations;
  std::vector<std::string> discrepancies;
};

/**
 * @brief Configuration for vision verification.
 */
struct VisionVerifierConfig {
  // AI model settings
  std::string model_provider = "gemini";  // "gemini", "ollama", "openai"
  std::string model_name = "gemini-1.5-flash";
  float temperature = 0.1f;  // Low temperature for consistent verification

  // Screenshot settings
  std::string screenshot_dir = "/tmp/yaze_test_screenshots";
  bool save_all_screenshots = true;
  bool include_screenshot_in_response = false;

  // Verification settings
  float confidence_threshold = 0.8f;
  int max_retries = 2;
  std::chrono::seconds timeout{30};

  // Prompt templates
  std::string system_prompt =
      "You are a visual verification assistant for a SNES ROM editor called "
      "yaze. "
      "Your task is to analyze screenshots and verify that UI elements, game "
      "states, "
      "and visual properties match expected conditions. Be precise and "
      "objective.";
};

/**
 * @brief Callback for custom screenshot capture.
 *
 * Allows integration with different rendering backends (SDL, OpenGL, etc.)
 */
using ScreenshotCaptureCallback =
    std::function<absl::StatusOr<std::vector<uint8_t>>(int* width, int* height)>;

/**
 * @class AIVisionVerifier
 * @brief AI-powered visual verification for GUI testing.
 *
 * This class provides multimodal AI capabilities for verifying GUI state
 * through screenshot analysis. It integrates with Gemini, Ollama, or other
 * vision-capable models to enable intelligent test assertions.
 *
 * Usage example:
 * @code
 *   AIVisionVerifier verifier(config);
 *   verifier.SetScreenshotCallback(my_capture_func);
 *
 *   // Simple verification
 *   auto result = verifier.Verify("The overworld map should show Link at
 * position (100, 200)");
 *
 *   // Structured verification
 *   auto result = verifier.VerifyConditions({
 *     "The tile selector shows 16x16 tiles",
 *     "The palette panel is visible on the right",
 *     "No error dialogs are displayed"
 *   });
 * @endcode
 */
class AIVisionVerifier {
 public:
  explicit AIVisionVerifier(const VisionVerifierConfig& config = {});
  ~AIVisionVerifier();

  // Configuration
  void SetConfig(const VisionVerifierConfig& config) { config_ = config; }
  const VisionVerifierConfig& GetConfig() const { return config_; }

  // Screenshot capture setup
  void SetScreenshotCallback(ScreenshotCaptureCallback callback) {
    screenshot_callback_ = std::move(callback);
  }

  /**
   * @brief Set the AI service to use for vision verification.
   *
   * When set, uses the provided AIService (e.g., GeminiAIService) for
   * multimodal requests. When not set, uses placeholder responses.
   *
   * @param service Pointer to an AIService instance (caller owns memory)
   */
  void SetAIService(cli::AIService* service) { ai_service_ = service; }

  // --- Core Verification Methods ---

  /**
   * @brief Verify a single condition using AI vision.
   * @param condition Natural language description of expected state.
   * @return Verification result with AI assessment.
   */
  absl::StatusOr<VisionVerificationResult> Verify(const std::string& condition);

  /**
   * @brief Verify multiple conditions in a single screenshot.
   * @param conditions List of expected conditions.
   * @return Combined verification result.
   */
  absl::StatusOr<VisionVerificationResult> VerifyConditions(
      const std::vector<std::string>& conditions);

  /**
   * @brief Compare current state against a reference screenshot.
   * @param reference_path Path to reference screenshot.
   * @param tolerance Visual difference tolerance (0.0 = exact, 1.0 = any).
   * @return Verification result with comparison details.
   */
  absl::StatusOr<VisionVerificationResult> CompareToReference(
      const std::string& reference_path, float tolerance = 0.1f);

  /**
   * @brief Ask the AI an open-ended question about the current state.
   * @param question Question to ask about the screenshot.
   * @return AI's response.
   */
  absl::StatusOr<std::string> AskAboutState(const std::string& question);

  // --- Specialized Verifications for yaze ---

  /**
   * @brief Verify tile at canvas position matches expected tile ID.
   */
  absl::StatusOr<VisionVerificationResult> VerifyTileAt(int x, int y,
                                                        int expected_tile_id);

  /**
   * @brief Verify that a specific editor panel is visible.
   */
  absl::StatusOr<VisionVerificationResult> VerifyPanelVisible(
      const std::string& panel_name);

  /**
   * @brief Verify game state in emulator matches expected values.
   */
  absl::StatusOr<VisionVerificationResult> VerifyEmulatorState(
      const std::string& state_description);

  /**
   * @brief Verify sprite rendering at specific location.
   */
  absl::StatusOr<VisionVerificationResult> VerifySpriteAt(
      int x, int y, const std::string& sprite_description);

  // --- Screenshot Management ---

  /**
   * @brief Capture and save a screenshot.
   * @param name Name for the screenshot file.
   * @return Path to saved screenshot.
   */
  absl::StatusOr<std::string> CaptureScreenshot(const std::string& name);

  /**
   * @brief Get the last captured screenshot data.
   */
  const std::vector<uint8_t>& GetLastScreenshotData() const {
    return last_screenshot_data_;
  }

  /**
   * @brief Clear cached screenshots to free memory.
   */
  void ClearScreenshotCache();

  // --- Iterative Refinement ---

  /**
   * @brief Begin an iterative verification session.
   *
   * Useful for complex verifications where the AI may need multiple
   * screenshots to confirm a condition (e.g., animation completed).
   */
  void BeginIterativeSession(int max_iterations = 5);

  /**
   * @brief Add a verification to the iterative session.
   */
  absl::Status AddIterativeCheck(const std::string& condition);

  /**
   * @brief Complete the iterative session and get results.
   */
  absl::StatusOr<VisionVerificationResult> CompleteIterativeSession();

 private:
  // Internal helpers
  absl::StatusOr<std::string> CaptureAndEncodeScreenshot();
  absl::StatusOr<std::string> CallVisionModel(const std::string& prompt,
                                              const std::string& image_base64);
  VisionVerificationResult ParseAIResponse(const std::string& response,
                                           const std::string& screenshot_path);

  VisionVerifierConfig config_;
  ScreenshotCaptureCallback screenshot_callback_;
  cli::AIService* ai_service_ = nullptr;  // Optional AI service for real API calls
  std::vector<uint8_t> last_screenshot_data_;
  int last_width_ = 0;
  int last_height_ = 0;

  // Iterative session state
  bool in_iterative_session_ = false;
  int iterative_max_iterations_ = 5;
  int iterative_current_iteration_ = 0;
  std::vector<std::string> iterative_conditions_;
  std::vector<VisionVerificationResult> iterative_results_;
};

/**
 * @brief RAII helper for iterative verification sessions.
 */
class ScopedIterativeVerification {
 public:
  explicit ScopedIterativeVerification(AIVisionVerifier& verifier,
                                       int max_iterations = 5)
      : verifier_(verifier) {
    verifier_.BeginIterativeSession(max_iterations);
  }

  ~ScopedIterativeVerification() {
    if (!completed_) {
      (void)verifier_.CompleteIterativeSession();
    }
  }

  absl::Status Check(const std::string& condition) {
    return verifier_.AddIterativeCheck(condition);
  }

  absl::StatusOr<VisionVerificationResult> Complete() {
    completed_ = true;
    return verifier_.CompleteIterativeSession();
  }

 private:
  AIVisionVerifier& verifier_;
  bool completed_ = false;
};

}  // namespace test
}  // namespace yaze

#endif  // YAZE_APP_TEST_AI_VISION_VERIFIER_H
