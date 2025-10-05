#ifndef YAZE_CLI_SERVICE_AI_AI_GUI_CONTROLLER_H_
#define YAZE_CLI_SERVICE_AI_AI_GUI_CONTROLLER_H_

#include <filesystem>
#include <memory>
#include <string>
#include <vector>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "cli/service/ai/ai_action_parser.h"
#include "cli/service/ai/vision_action_refiner.h"
#include "cli/service/gui/gui_action_generator.h"

namespace yaze {
namespace cli {

// Forward declares
class GeminiAIService;
class GuiAutomationClient;  // In cli namespace, not cli::gui

namespace ai {

/**
 * @struct ControlLoopConfig
 * @brief Configuration for the AI GUI control loop
 */
struct ControlLoopConfig {
  int max_iterations = 10;              // Max attempts before giving up
  int screenshot_delay_ms = 500;        // Delay before taking screenshots
  bool enable_vision_verification = true; // Use vision to verify actions
  bool enable_iterative_refinement = true; // Retry with refined actions
  int max_retries_per_action = 3;       // Max retries for a single action
  std::string screenshots_dir = "/tmp/yaze/ai_gui_control";
};

/**
 * @struct ControlResult
 * @brief Result of AI-controlled GUI automation
 */
struct ControlResult {
  bool success = false;
  int iterations_performed = 0;
  std::vector<ai::AIAction> actions_executed;
  std::vector<VisionAnalysisResult> vision_analyses;
  std::vector<std::filesystem::path> screenshots_taken;
  std::string error_message;
  std::string final_state_description;
};

/**
 * @class AIGUIController
 * @brief High-level controller for AI-driven GUI automation with vision feedback
 * 
 * This class implements the complete vision-guided control loop:
 * 
 * 1. **Parse Command** → Natural language → AIActions
 * 2. **Take Screenshot** → Capture current GUI state
 * 3. **Analyze Vision** → Gemini analyzes screenshot
 * 4. **Execute Action** → Send gRPC command to GUI
 * 5. **Verify Success** → Compare before/after screenshots
 * 6. **Refine & Retry** → Adjust parameters if action failed
 * 7. **Repeat** → Until goal achieved or max iterations reached
 * 
 * Example usage:
 * ```cpp
 * AIGUIController controller(gemini_service, gui_client);
 * controller.Initialize(config);
 * 
 * auto result = controller.ExecuteCommand(
 *     "Place tile 0x42 at overworld position (5, 7)"
 * );
 * 
 * if (result->success) {
 *   std::cout << "Success! Took " << result->iterations_performed 
 *             << " iterations\n";
 * }
 * ```
 */
class AIGUIController {
 public:
  /**
   * @brief Construct controller with required services
   * @param gemini_service Gemini AI service for vision analysis
   * @param gui_client gRPC client for GUI automation
   */
  AIGUIController(GeminiAIService* gemini_service,
                  GuiAutomationClient* gui_client);
  
  ~AIGUIController() = default;
  
  /**
   * @brief Initialize the controller with configuration
   */
  absl::Status Initialize(const ControlLoopConfig& config);
  
  /**
   * @brief Execute a natural language command with AI vision guidance
   * @param command Natural language command (e.g., "Place tile 0x42 at (5, 7)")
   * @return Result including success status and execution details
   */
  absl::StatusOr<ControlResult> ExecuteCommand(const std::string& command);
  
  /**
   * @brief Execute a sequence of pre-parsed actions
   * @param actions Vector of AI actions to execute
   * @return Result including success status
   */
  absl::StatusOr<ControlResult> ExecuteActions(
      const std::vector<ai::AIAction>& actions);
  
  /**
   * @brief Execute a single action with optional vision verification
   * @param action The action to execute
   * @param verify_with_vision Whether to use vision to verify success
   * @return Success status and vision analysis
   */
  absl::StatusOr<VisionAnalysisResult> ExecuteSingleAction(
      const AIAction& action,
      bool verify_with_vision = true);
  
  /**
   * @brief Analyze the current GUI state without executing actions
   * @param context What to look for in the GUI
   * @return Vision analysis of current state
   */
  absl::StatusOr<VisionAnalysisResult> AnalyzeCurrentGUIState(
      const std::string& context = "");
  
  /**
   * @brief Get the current configuration
   */
  const ControlLoopConfig& config() const { return config_; }
  
  /**
   * @brief Update configuration
   */
  void SetConfig(const ControlLoopConfig& config) { config_ = config; }
  
 private:
  GeminiAIService* gemini_service_;  // Not owned
  GuiAutomationClient* gui_client_;  // Not owned
  std::unique_ptr<VisionActionRefiner> vision_refiner_;
  gui::GuiActionGenerator action_generator_;
  ControlLoopConfig config_;
  std::filesystem::path screenshots_dir_;
  
  // Helper methods
  absl::StatusOr<std::filesystem::path> CaptureCurrentState(
      const std::string& description);
  
  absl::Status ExecuteGRPCAction(const AIAction& action);
  
  absl::StatusOr<VisionAnalysisResult> VerifyActionSuccess(
      const AIAction& action,
      const std::filesystem::path& before_screenshot,
      const std::filesystem::path& after_screenshot);
  
  absl::StatusOr<AIAction> RefineActionWithVision(
      const AIAction& original_action,
      const VisionAnalysisResult& analysis);
  
  void EnsureScreenshotsDirectory();
  std::filesystem::path GenerateScreenshotPath(const std::string& suffix);
};

}  // namespace ai
}  // namespace cli
}  // namespace yaze

#endif  // YAZE_CLI_SERVICE_AI_AI_GUI_CONTROLLER_H_
