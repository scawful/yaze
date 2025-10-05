#ifndef YAZE_CLI_SERVICE_GUI_GUI_ACTION_GENERATOR_H_
#define YAZE_CLI_SERVICE_GUI_GUI_ACTION_GENERATOR_H_

#include <string>
#include <vector>

#include "absl/status/statusor.h"
#include "cli/service/ai/ai_action_parser.h"

#ifdef YAZE_WITH_JSON
#include "nlohmann/json.hpp"
#endif

namespace yaze {
namespace cli {
namespace gui {

/**
 * @class GuiActionGenerator  
 * @brief Converts high-level AI actions into executable GUI test scripts
 * 
 * Takes parsed AI actions and generates gRPC test harness commands or
 * JSON test scripts that can be executed to control the GUI.
 */
class GuiActionGenerator {
 public:
  GuiActionGenerator() = default;
  
  /**
   * Generate a test script from a sequence of AI actions
   * @param actions Vector of actions to convert
   * @return JSON-formatted test script, or error status
   */
  absl::StatusOr<std::string> GenerateTestScript(
      const std::vector<ai::AIAction>& actions);
  
#ifdef YAZE_WITH_JSON
  /**
   * Generate a JSON test object from actions
   * @param actions Vector of actions to convert
   * @return JSON object with test steps
   */
  absl::StatusOr<nlohmann::json> GenerateTestJSON(
      const std::vector<ai::AIAction>& actions);
#endif
  
  /**
   * Convert a single action to a test step
   */
  std::string ActionToTestStep(const ai::AIAction& action, int step_number);
  
 private:
  // Helper functions for specific action types
  std::string GenerateOpenEditorStep(const ai::AIAction& action);
  std::string GenerateSelectTileStep(const ai::AIAction& action);
  std::string GeneratePlaceTileStep(const ai::AIAction& action);
  std::string GenerateSaveTileStep(const ai::AIAction& action);
  std::string GenerateClickButtonStep(const ai::AIAction& action);
  std::string GenerateWaitStep(const ai::AIAction& action);
  std::string GenerateScreenshotStep(const ai::AIAction& action);
  
#ifdef YAZE_WITH_JSON
  nlohmann::json ActionToJSON(const ai::AIAction& action);
#endif
};

}  // namespace gui
}  // namespace cli
}  // namespace yaze

#endif  // YAZE_CLI_SERVICE_GUI_GUI_ACTION_GENERATOR_H_
