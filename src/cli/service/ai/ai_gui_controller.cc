#include "cli/service/ai/ai_gui_controller.h"

#include <chrono>
#include <thread>

#include "absl/strings/str_cat.h"
#include "absl/strings/str_format.h"
#include "absl/time/clock.h"
#include "absl/time/time.h"
#include "cli/service/ai/gemini_ai_service.h"

#ifdef YAZE_WITH_GRPC
#include "cli/service/gui/gui_automation_client.h"
#include "app/service/screenshot_utils.h"
#endif

namespace yaze {
namespace cli {
namespace ai {

AIGUIController::AIGUIController(GeminiAIService* gemini_service,
                                 GuiAutomationClient* gui_client)
    : gemini_service_(gemini_service),
      gui_client_(gui_client),
      vision_refiner_(std::make_unique<VisionActionRefiner>(gemini_service)) {
  
  if (!gemini_service_) {
    throw std::invalid_argument("Gemini service cannot be null");
  }
  
  if (!gui_client_) {
    throw std::invalid_argument("GUI client cannot be null");
  }
}

absl::Status AIGUIController::Initialize(const ControlLoopConfig& config) {
  config_ = config;
  screenshots_dir_ = config.screenshots_dir;
  
  EnsureScreenshotsDirectory();
  
  return absl::OkStatus();
}

absl::StatusOr<ControlResult> AIGUIController::ExecuteCommand(
    const std::string& command) {
  
  // Parse natural language command into actions
  auto actions_result = AIActionParser::ParseCommand(command);
  if (!actions_result.ok()) {
    return actions_result.status();
  }
  
  return ExecuteActions(*actions_result);
}

absl::StatusOr<ControlResult> AIGUIController::ExecuteActions(
    const std::vector<AIAction>& actions) {
  
  ControlResult result;
  result.success = false;
  
  for (const auto& action : actions) {
    int retry_count = 0;
    bool action_succeeded = false;
    AIAction current_action = action;
    
    while (retry_count < config_.max_retries_per_action && !action_succeeded) {
      result.iterations_performed++;
      
      if (result.iterations_performed > config_.max_iterations) {
        result.error_message = "Max iterations reached";
        return result;
      }
      
      // Execute the action with vision verification
      auto execute_result = ExecuteSingleAction(
          current_action,
          config_.enable_vision_verification
      );
      
      if (!execute_result.ok()) {
        result.error_message = std::string(execute_result.status().message());
        return result;
      }
      
      result.vision_analyses.push_back(*execute_result);
      result.actions_executed.push_back(current_action);
      
      if (execute_result->action_successful) {
        action_succeeded = true;
      }
      else if (config_.enable_iterative_refinement) {
        // Refine action and retry
        auto refinement = vision_refiner_->RefineAction(
            current_action,
            *execute_result
        );
        
        if (!refinement.ok()) {
          result.error_message = 
              absl::StrCat("Failed to refine action: ",
                          refinement.status().message());
          return result;
        }
        
        if (refinement->needs_different_approach) {
          result.error_message = 
              absl::StrCat("Action requires different approach: ",
                          refinement->reasoning);
          return result;
        }
        
        if (refinement->needs_retry) {
          // Update action parameters
          for (const auto& [key, value] : refinement->adjusted_parameters) {
            current_action.parameters[key] = value;
          }
        }
        
        retry_count++;
      }
      else {
        // No refinement, just fail
        result.error_message = execute_result->error_message;
        return result;
      }
    }
    
    if (!action_succeeded) {
      result.error_message = 
          absl::StrFormat("Action failed after %d retries", retry_count);
      return result;
    }
  }
  
  result.success = true;
  
  // Capture final state
  auto final_screenshot = CaptureCurrentState("final_state");
  if (final_screenshot.ok()) {
    result.screenshots_taken.push_back(*final_screenshot);
    
    // Analyze final state
    auto final_analysis = vision_refiner_->AnalyzeScreenshot(
        *final_screenshot,
        "Verify all actions completed successfully"
    );
    
    if (final_analysis.ok()) {
      result.final_state_description = final_analysis->description;
    }
  }
  
  return result;
}

absl::StatusOr<VisionAnalysisResult> AIGUIController::ExecuteSingleAction(
    const AIAction& action,
    bool verify_with_vision) {
  
  VisionAnalysisResult result;
  
  // Capture before screenshot
  std::filesystem::path before_screenshot;
  if (verify_with_vision) {
    auto before_result = CaptureCurrentState("before_action");
    if (!before_result.ok()) {
      return before_result.status();
    }
    before_screenshot = *before_result;
  }
  
  // Wait for UI to settle
  if (config_.screenshot_delay_ms > 0) {
    std::this_thread::sleep_for(
        std::chrono::milliseconds(config_.screenshot_delay_ms));
  }
  
  // Execute the action via gRPC
  auto execute_status = ExecuteGRPCAction(action);
  if (!execute_status.ok()) {
    result.action_successful = false;
    result.error_message = std::string(execute_status.message());
    return result;
  }
  
  // Wait for action to complete
  std::this_thread::sleep_for(
      std::chrono::milliseconds(config_.screenshot_delay_ms));
  
  if (verify_with_vision) {
    // Capture after screenshot
    auto after_result = CaptureCurrentState("after_action");
    if (!after_result.ok()) {
      return after_result.status();
    }
    
    // Verify with vision
    return VerifyActionSuccess(action, before_screenshot, *after_result);
  }
  else {
    // Assume success without verification
    result.action_successful = true;
    result.description = "Action executed (no vision verification)";
    return result;
  }
}

absl::StatusOr<VisionAnalysisResult> AIGUIController::AnalyzeCurrentGUIState(
    const std::string& context) {
  
  auto screenshot = CaptureCurrentState("analysis");
  if (!screenshot.ok()) {
    return screenshot.status();
  }
  
  return vision_refiner_->AnalyzeScreenshot(*screenshot, context);
}

// Private helper methods

absl::StatusOr<std::filesystem::path> AIGUIController::CaptureCurrentState(
    const std::string& description) {
  
#ifdef YAZE_WITH_GRPC
  std::filesystem::path path = GenerateScreenshotPath(description);
  
  auto result = yaze::test::CaptureHarnessScreenshot(path.string());
  if (!result.ok()) {
    return result.status();
  }
  
  return std::filesystem::path(result->file_path);
#else
  return absl::UnimplementedError("Screenshot capture requires gRPC support");
#endif
}

absl::Status AIGUIController::ExecuteGRPCAction(const AIAction& action) {
  // Convert AI action to gRPC test commands
  auto test_script_result = action_generator_.GenerateTestScript({action});
  
  if (!test_script_result.ok()) {
    return test_script_result.status();
  }
  
#ifdef YAZE_WITH_GRPC
  if (!gui_client_) {
    return absl::FailedPreconditionError("GUI automation client not initialized");
  }
  
  // Execute the action based on its type
  if (action.type == AIActionType::kClickButton) {
    // Extract target from parameters
    std::string target = "button:Unknown";
    if (action.parameters.count("target") > 0) {
      target = action.parameters.at("target");
    }
    
    // Determine click type
    ClickType click_type = ClickType::kLeft;
    if (action.parameters.count("click_type") > 0) {
      const std::string& type = action.parameters.at("click_type");
      if (type == "right") {
        click_type = ClickType::kRight;
      } else if (type == "middle") {
        click_type = ClickType::kMiddle;
      } else if (type == "double") {
        click_type = ClickType::kDouble;
      }
    }
    
    auto result = gui_client_->Click(target, click_type);
    if (!result.ok()) {
      return result.status();
    }
    
    if (!result->success) {
      return absl::InternalError(
          absl::StrCat("Click action failed: ", result->message));
    }
    
    return absl::OkStatus();
  }
  else if (action.type == AIActionType::kSelectTile) {
    // Extract target and text from parameters (treating select as a type-like action)
    std::string target = "input:Unknown";
    std::string text = "";
    bool clear_first = true;
    
    if (action.parameters.count("target") > 0) {
      target = action.parameters.at("target");
    }
    if (action.parameters.count("text") > 0) {
      text = action.parameters.at("text");
    }
    if (action.parameters.count("clear_first") > 0) {
      clear_first = (action.parameters.at("clear_first") == "true");
    }
    
    auto result = gui_client_->Type(target, text, clear_first);
    if (!result.ok()) {
      return result.status();
    }
    
    if (!result->success) {
      return absl::InternalError(
          absl::StrCat("Type action failed: ", result->message));
    }
    
    return absl::OkStatus();
  }
  else if (action.type == AIActionType::kWait) {
    // Extract condition and timeout from parameters
    std::string condition = "visible";
    int timeout_ms = 5000;
    int poll_interval_ms = 100;
    
    if (action.parameters.count("condition") > 0) {
      condition = action.parameters.at("condition");
    }
    if (action.parameters.count("timeout_ms") > 0) {
      timeout_ms = std::stoi(action.parameters.at("timeout_ms"));
    }
    if (action.parameters.count("poll_interval_ms") > 0) {
      poll_interval_ms = std::stoi(action.parameters.at("poll_interval_ms"));
    }
    
    auto result = gui_client_->Wait(condition, timeout_ms, poll_interval_ms);
    if (!result.ok()) {
      return result.status();
    }
    
    if (!result->success) {
      return absl::InternalError(
          absl::StrCat("Wait action failed: ", result->message));
    }
    
    return absl::OkStatus();
  }
  else if (action.type == AIActionType::kVerifyTile) {
    // Extract condition from parameters (treating verify as assert)
    std::string condition = "";
    if (action.parameters.count("condition") > 0) {
      condition = action.parameters.at("condition");
    }
    
    auto result = gui_client_->Assert(condition);
    if (!result.ok()) {
      return result.status();
    }
    
    if (!result->success) {
      return absl::InternalError(
          absl::StrCat("Assert action failed: ", result->message,
                      " (expected: ", result->expected_value,
                      ", actual: ", result->actual_value, ")"));
    }
    
    return absl::OkStatus();
  }
  else if (action.type == AIActionType::kPlaceTile) {
    // This is a special action for setting overworld tiles
    // Extract map_id, x, y, tile from parameters
    if (action.parameters.count("map_id") == 0 ||
        action.parameters.count("x") == 0 ||
        action.parameters.count("y") == 0 ||
        action.parameters.count("tile") == 0) {
      return absl::InvalidArgumentError(
          "set_tile action requires map_id, x, y, and tile parameters");
    }
    
    int map_id = std::stoi(action.parameters.at("map_id"));
    int x = std::stoi(action.parameters.at("x"));
    int y = std::stoi(action.parameters.at("y"));
    std::string tile_str = action.parameters.at("tile");
    
    // Navigate to overworld editor
    auto click_result = gui_client_->Click("menu:Overworld", ClickType::kLeft);
    if (!click_result.ok() || !click_result->success) {
      return absl::InternalError("Failed to open Overworld editor");
    }
    
    // Wait for overworld editor to be visible
    auto wait_result = gui_client_->Wait("window:Overworld Editor", 2000, 100);
    if (!wait_result.ok() || !wait_result->success) {
      return absl::InternalError("Overworld editor did not appear");
    }
    
    // Set the map ID
    auto type_result = gui_client_->Type("input:Map ID", std::to_string(map_id), true);
    if (!type_result.ok() || !type_result->success) {
      return absl::InternalError("Failed to set map ID");
    }
    
    // Click on the tile position (approximate based on editor layout)
    // This is a simplified implementation
    std::string target = absl::StrCat("canvas:overworld@", x * 16, ",", y * 16);
    click_result = gui_client_->Click(target, ClickType::kLeft);
    if (!click_result.ok() || !click_result->success) {
      return absl::InternalError("Failed to click tile position");
    }
    
    return absl::OkStatus();
  }
  else {
    return absl::UnimplementedError(
        absl::StrCat("Action type not implemented: ",
                    static_cast<int>(action.type)));
  }
#else
  return absl::UnimplementedError(
      "gRPC GUI automation requires building with -DYAZE_WITH_GRPC=ON");
#endif
}


absl::StatusOr<VisionAnalysisResult> AIGUIController::VerifyActionSuccess(
    const AIAction& action,
    const std::filesystem::path& before_screenshot,
    const std::filesystem::path& after_screenshot) {
  
  return vision_refiner_->VerifyAction(action, before_screenshot, after_screenshot);
}

absl::StatusOr<AIAction> AIGUIController::RefineActionWithVision(
    const AIAction& original_action,
    const VisionAnalysisResult& analysis) {
  
  auto refinement = vision_refiner_->RefineAction(original_action, analysis);
  if (!refinement.ok()) {
    return refinement.status();
  }
  
  AIAction refined_action = original_action;
  
  // Apply adjusted parameters
  for (const auto& [key, value] : refinement->adjusted_parameters) {
    refined_action.parameters[key] = value;
  }
  
  return refined_action;
}

void AIGUIController::EnsureScreenshotsDirectory() {
  std::error_code ec;
  std::filesystem::create_directories(screenshots_dir_, ec);
  
  if (ec) {
    std::cerr << "Warning: Failed to create screenshots directory: " 
              << ec.message() << std::endl;
  }
}

std::filesystem::path AIGUIController::GenerateScreenshotPath(
    const std::string& suffix) {
  
  int64_t timestamp = absl::ToUnixMillis(absl::Now());
  
  std::string filename = absl::StrFormat(
      "ai_gui_%s_%lld.png",
      suffix,
      static_cast<long long>(timestamp)
  );
  
  return screenshots_dir_ / filename;
}

}  // namespace ai
}  // namespace cli
}  // namespace yaze
