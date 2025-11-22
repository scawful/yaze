#include "app/test/ai_vision_verifier.h"

#include <chrono>
#include <fstream>
#include <sstream>

#include "absl/strings/str_cat.h"
#include "absl/strings/str_format.h"
#include "util/log.h"

namespace yaze {
namespace test {

AIVisionVerifier::AIVisionVerifier(const VisionVerifierConfig& config)
    : config_(config) {}

AIVisionVerifier::~AIVisionVerifier() = default;

absl::StatusOr<VisionVerificationResult> AIVisionVerifier::Verify(
    const std::string& condition) {
  auto start = std::chrono::steady_clock::now();

  // Capture screenshot
  auto screenshot_result = CaptureAndEncodeScreenshot();
  if (!screenshot_result.ok()) {
    return screenshot_result.status();
  }

  // Build verification prompt
  std::string prompt = absl::StrFormat(
      "Analyze this screenshot and verify the following condition:\n\n"
      "CONDITION: %s\n\n"
      "Respond with:\n"
      "1. PASS or FAIL\n"
      "2. Confidence level (0.0 to 1.0)\n"
      "3. Brief explanation of what you observe\n"
      "4. Any discrepancies if FAIL\n\n"
      "Format your response as:\n"
      "RESULT: [PASS/FAIL]\n"
      "CONFIDENCE: [0.0-1.0]\n"
      "OBSERVATIONS: [what you see]\n"
      "DISCREPANCIES: [if any]",
      condition);

  // Call vision model
  auto ai_response = CallVisionModel(prompt, *screenshot_result);
  if (!ai_response.ok()) {
    return ai_response.status();
  }

  // Parse response
  auto result = ParseAIResponse(*ai_response, "");

  auto end = std::chrono::steady_clock::now();
  result.latency = std::chrono::duration_cast<std::chrono::milliseconds>(
      end - start);

  return result;
}

absl::StatusOr<VisionVerificationResult> AIVisionVerifier::VerifyConditions(
    const std::vector<std::string>& conditions) {
  if (conditions.empty()) {
    return absl::InvalidArgumentError("No conditions provided");
  }

  auto start = std::chrono::steady_clock::now();

  auto screenshot_result = CaptureAndEncodeScreenshot();
  if (!screenshot_result.ok()) {
    return screenshot_result.status();
  }

  // Build multi-condition prompt
  std::ostringstream prompt;
  prompt << "Analyze this screenshot and verify ALL of the following conditions:\n\n";
  for (size_t i = 0; i < conditions.size(); ++i) {
    prompt << (i + 1) << ". " << conditions[i] << "\n";
  }
  prompt << "\nFor EACH condition, respond with:\n"
         << "- PASS or FAIL\n"
         << "- Brief explanation\n\n"
         << "Then provide an OVERALL result (PASS only if ALL conditions pass).\n"
         << "Format:\n"
         << "CONDITION 1: [PASS/FAIL] - [explanation]\n"
         << "...\n"
         << "OVERALL: [PASS/FAIL]\n"
         << "CONFIDENCE: [0.0-1.0]";

  auto ai_response = CallVisionModel(prompt.str(), *screenshot_result);
  if (!ai_response.ok()) {
    return ai_response.status();
  }

  auto result = ParseAIResponse(*ai_response, "");

  auto end = std::chrono::steady_clock::now();
  result.latency = std::chrono::duration_cast<std::chrono::milliseconds>(
      end - start);

  return result;
}

absl::StatusOr<VisionVerificationResult> AIVisionVerifier::CompareToReference(
    const std::string& reference_path, float tolerance) {
  auto start = std::chrono::steady_clock::now();

  auto screenshot_result = CaptureAndEncodeScreenshot();
  if (!screenshot_result.ok()) {
    return screenshot_result.status();
  }

  // For now, use AI vision to compare (could also use pixel-based comparison)
  std::string prompt = absl::StrFormat(
      "Compare this screenshot to the reference image.\n"
      "Tolerance level: %.0f%% (lower = stricter)\n\n"
      "Describe any visual differences you observe.\n"
      "Consider: layout, colors, text, UI elements, game state.\n\n"
      "Format:\n"
      "MATCH: [YES/NO]\n"
      "SIMILARITY: [0.0-1.0]\n"
      "DIFFERENCES: [list any differences found]",
      tolerance * 100);

  auto ai_response = CallVisionModel(prompt, *screenshot_result);
  if (!ai_response.ok()) {
    return ai_response.status();
  }

  auto result = ParseAIResponse(*ai_response, reference_path);

  auto end = std::chrono::steady_clock::now();
  result.latency = std::chrono::duration_cast<std::chrono::milliseconds>(
      end - start);

  return result;
}

absl::StatusOr<std::string> AIVisionVerifier::AskAboutState(
    const std::string& question) {
  auto screenshot_result = CaptureAndEncodeScreenshot();
  if (!screenshot_result.ok()) {
    return screenshot_result.status();
  }

  std::string prompt = absl::StrFormat(
      "Based on this screenshot of the yaze ROM editor, please answer:\n\n%s",
      question);

  return CallVisionModel(prompt, *screenshot_result);
}

absl::StatusOr<VisionVerificationResult> AIVisionVerifier::VerifyTileAt(
    int x, int y, int expected_tile_id) {
  std::string condition = absl::StrFormat(
      "The tile at canvas position (%d, %d) should be tile ID 0x%04X",
      x, y, expected_tile_id);
  return Verify(condition);
}

absl::StatusOr<VisionVerificationResult> AIVisionVerifier::VerifyPanelVisible(
    const std::string& panel_name) {
  std::string condition = absl::StrFormat(
      "The '%s' panel/window should be visible and not obscured",
      panel_name);
  return Verify(condition);
}

absl::StatusOr<VisionVerificationResult> AIVisionVerifier::VerifyEmulatorState(
    const std::string& state_description) {
  std::string condition = absl::StrFormat(
      "In the emulator view, verify: %s", state_description);
  return Verify(condition);
}

absl::StatusOr<VisionVerificationResult> AIVisionVerifier::VerifySpriteAt(
    int x, int y, const std::string& sprite_description) {
  std::string condition = absl::StrFormat(
      "At position (%d, %d), there should be a sprite matching: %s",
      x, y, sprite_description);
  return Verify(condition);
}

absl::StatusOr<std::string> AIVisionVerifier::CaptureScreenshot(
    const std::string& name) {
  if (!screenshot_callback_) {
    return absl::FailedPreconditionError("Screenshot callback not set");
  }

  auto result = screenshot_callback_(&last_width_, &last_height_);
  if (!result.ok()) {
    return result.status();
  }

  last_screenshot_data_ = std::move(*result);

  // Save to file
  std::string path = absl::StrCat(config_.screenshot_dir, "/", name, ".png");
  // TODO: Implement PNG saving
  LOG_DEBUG("AIVisionVerifier", "Screenshot captured: %s (%dx%d)",
            path.c_str(), last_width_, last_height_);

  return path;
}

void AIVisionVerifier::ClearScreenshotCache() {
  last_screenshot_data_.clear();
  last_width_ = 0;
  last_height_ = 0;
}

void AIVisionVerifier::BeginIterativeSession(int max_iterations) {
  in_iterative_session_ = true;
  iterative_max_iterations_ = max_iterations;
  iterative_current_iteration_ = 0;
  iterative_conditions_.clear();
  iterative_results_.clear();
}

absl::Status AIVisionVerifier::AddIterativeCheck(const std::string& condition) {
  if (!in_iterative_session_) {
    return absl::FailedPreconditionError("Not in iterative session");
  }

  if (iterative_current_iteration_ >= iterative_max_iterations_) {
    return absl::ResourceExhaustedError("Max iterations reached");
  }

  iterative_conditions_.push_back(condition);
  iterative_current_iteration_++;

  auto result = Verify(condition);
  if (result.ok()) {
    iterative_results_.push_back(*result);
  }

  return absl::OkStatus();
}

absl::StatusOr<VisionVerificationResult> AIVisionVerifier::CompleteIterativeSession() {
  in_iterative_session_ = false;

  if (iterative_results_.empty()) {
    return absl::NotFoundError("No results in iterative session");
  }

  // Aggregate results
  VisionVerificationResult combined;
  combined.passed = true;
  float total_confidence = 0.0f;

  for (const auto& result : iterative_results_) {
    if (!result.passed) {
      combined.passed = false;
    }
    total_confidence += result.confidence;
    combined.observations.insert(combined.observations.end(),
                                  result.observations.begin(),
                                  result.observations.end());
    combined.discrepancies.insert(combined.discrepancies.end(),
                                   result.discrepancies.begin(),
                                   result.discrepancies.end());
  }

  combined.confidence = total_confidence / iterative_results_.size();

  return combined;
}

absl::StatusOr<std::string> AIVisionVerifier::CaptureAndEncodeScreenshot() {
  if (!screenshot_callback_) {
    return absl::FailedPreconditionError("Screenshot callback not set");
  }

  auto result = screenshot_callback_(&last_width_, &last_height_);
  if (!result.ok()) {
    return result.status();
  }

  last_screenshot_data_ = std::move(*result);

  // TODO: Encode to base64 for API calls
  return "base64_encoded_screenshot_placeholder";
}

absl::StatusOr<std::string> AIVisionVerifier::CallVisionModel(
    const std::string& prompt, const std::string& image_base64) {
  // TODO: Implement actual API calls to Gemini/Ollama/OpenAI
  // This is a placeholder that returns a simulated response

  LOG_DEBUG("AIVisionVerifier", "Calling vision model: %s",
            config_.model_name.c_str());

  // For now, return a placeholder response
  return absl::StrFormat(
      "RESULT: PASS\n"
      "CONFIDENCE: 0.85\n"
      "OBSERVATIONS: Placeholder response - vision API not implemented\n"
      "DISCREPANCIES: None");
}

VisionVerificationResult AIVisionVerifier::ParseAIResponse(
    const std::string& response, const std::string& screenshot_path) {
  VisionVerificationResult result;
  result.ai_response = response;
  result.screenshot_path = screenshot_path;

  // Simple parsing - look for RESULT: PASS/FAIL
  if (response.find("RESULT: PASS") != std::string::npos ||
      response.find("PASS") != std::string::npos) {
    result.passed = true;
  }

  // Look for CONFIDENCE: X.X
  auto conf_pos = response.find("CONFIDENCE:");
  if (conf_pos != std::string::npos) {
    std::string conf_str = response.substr(conf_pos + 11, 4);
    try {
      result.confidence = std::stof(conf_str);
    } catch (...) {
      result.confidence = result.passed ? 0.8f : 0.2f;
    }
  } else {
    result.confidence = result.passed ? 0.8f : 0.2f;
  }

  // Extract observations
  auto obs_pos = response.find("OBSERVATIONS:");
  if (obs_pos != std::string::npos) {
    auto end_pos = response.find('\n', obs_pos);
    if (end_pos == std::string::npos) end_pos = response.length();
    result.observations.push_back(
        response.substr(obs_pos + 13, end_pos - obs_pos - 13));
  }

  // Extract discrepancies
  auto disc_pos = response.find("DISCREPANCIES:");
  if (disc_pos != std::string::npos) {
    auto end_pos = response.find('\n', disc_pos);
    if (end_pos == std::string::npos) end_pos = response.length();
    std::string disc = response.substr(disc_pos + 14, end_pos - disc_pos - 14);
    if (disc != "None" && !disc.empty()) {
      result.discrepancies.push_back(disc);
    }
  }

  return result;
}

}  // namespace test
}  // namespace yaze
