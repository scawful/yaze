#ifndef YAZE_CLI_SERVICE_AI_VISION_ACTION_REFINER_H_
#define YAZE_CLI_SERVICE_AI_VISION_ACTION_REFINER_H_

#include <filesystem>
#include <string>
#include <vector>

#include "absl/status/statusor.h"
#include "cli/service/ai/ai_action_parser.h"

namespace yaze {
namespace cli {

// Forward declare
class GeminiAIService;

namespace ai {

/**
 * @struct VisionAnalysisResult
 * @brief Result of analyzing a screenshot with Gemini Vision
 */
struct VisionAnalysisResult {
  std::string description;              // What Gemini sees in the image
  std::vector<std::string> widgets;     // Detected UI widgets
  std::vector<std::string> suggestions; // Action suggestions
  bool action_successful = false;       // Whether the last action succeeded
  std::string error_message;            // Error description if action failed
};

/**
 * @struct ActionRefinement
 * @brief Refined action parameters based on vision analysis
 */
struct ActionRefinement {
  bool needs_retry = false;
  bool needs_different_approach = false;
  std::map<std::string, std::string> adjusted_parameters;
  std::string reasoning;
};

/**
 * @class VisionActionRefiner
 * @brief Uses Gemini Vision to analyze GUI screenshots and refine AI actions
 * 
 * This class implements the vision-guided action loop:
 * 1. Take screenshot of current GUI state
 * 2. Send to Gemini Vision with contextual prompt
 * 3. Analyze response to determine next action
 * 4. Verify action success by comparing screenshots
 * 
 * Example usage:
 * ```cpp
 * VisionActionRefiner refiner(gemini_service);
 * 
 * // Analyze current state
 * auto analysis = refiner.AnalyzeCurrentState(
 *     "overworld_editor",
 *     "Looking for tile selector"
 * );
 * 
 * // Verify action was successful
 * auto verification = refiner.VerifyAction(
 *     AIAction(kPlaceTile, {{"x", "5"}, {"y", "7"}}),
 *     before_screenshot,
 *     after_screenshot
 * );
 * 
 * // Refine failed action
 * if (!verification->action_successful) {
 *   auto refinement = refiner.RefineAction(
 *       original_action,
 *       *verification
 *   );
 * }
 * ```
 */
class VisionActionRefiner {
 public:
  /**
   * @brief Construct refiner with Gemini service
   * @param gemini_service Pointer to Gemini AI service (not owned)
   */
  explicit VisionActionRefiner(GeminiAIService* gemini_service);
  
  /**
   * @brief Analyze the current GUI state from a screenshot
   * @param screenshot_path Path to screenshot file
   * @param context Additional context about what we're looking for
   * @return Vision analysis result
   */
  absl::StatusOr<VisionAnalysisResult> AnalyzeScreenshot(
      const std::filesystem::path& screenshot_path,
      const std::string& context = "");
  
  /**
   * @brief Verify an action was successful by comparing before/after screenshots
   * @param action The action that was performed
   * @param before_screenshot Screenshot before action
   * @param after_screenshot Screenshot after action
   * @return Analysis indicating whether action succeeded
   */
  absl::StatusOr<VisionAnalysisResult> VerifyAction(
      const AIAction& action,
      const std::filesystem::path& before_screenshot,
      const std::filesystem::path& after_screenshot);
  
  /**
   * @brief Refine an action based on vision analysis feedback
   * @param original_action The action that failed or needs adjustment
   * @param analysis Vision analysis showing why action failed
   * @return Refined action with adjusted parameters
   */
  absl::StatusOr<ActionRefinement> RefineAction(
      const AIAction& original_action,
      const VisionAnalysisResult& analysis);
  
  /**
   * @brief Find a specific UI element in a screenshot
   * @param screenshot_path Path to screenshot
   * @param element_name Name/description of element to find
   * @return Coordinates or description of where element is located
   */
  absl::StatusOr<std::map<std::string, std::string>> LocateUIElement(
      const std::filesystem::path& screenshot_path,
      const std::string& element_name);
  
  /**
   * @brief Extract all visible widgets from a screenshot
   * @param screenshot_path Path to screenshot
   * @return List of detected widgets with their properties
   */
  absl::StatusOr<std::vector<std::string>> ExtractVisibleWidgets(
      const std::filesystem::path& screenshot_path);
  
 private:
  GeminiAIService* gemini_service_;  // Not owned
  
  // Build prompts for different vision analysis tasks
  std::string BuildAnalysisPrompt(const std::string& context);
  std::string BuildVerificationPrompt(const AIAction& action);
  std::string BuildElementLocationPrompt(const std::string& element_name);
  std::string BuildWidgetExtractionPrompt();
  
  // Parse Gemini vision responses
  VisionAnalysisResult ParseAnalysisResponse(const std::string& response);
  VisionAnalysisResult ParseVerificationResponse(
      const std::string& response, const AIAction& action);
};

}  // namespace ai
}  // namespace cli
}  // namespace yaze

#endif  // YAZE_CLI_SERVICE_AI_VISION_ACTION_REFINER_H_
