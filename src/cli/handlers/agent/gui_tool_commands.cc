#include "absl/strings/match.h"
#include "cli/handlers/agent/commands.h"

#include <iostream>

#include "absl/status/status.h"
#include "absl/strings/str_format.h"
#include "absl/strings/match.h"
#include "absl/strings/numbers.h"
#include "app/rom.h"

#ifdef YAZE_WITH_GRPC
#include "cli/service/gui/gui_automation_client.h"
#include "cli/service/ai/ai_action_parser.h"
#include "cli/service/gui/gui_action_generator.h"
#endif

namespace yaze {
namespace cli {
namespace agent {

absl::Status HandleGuiPlaceTileCommand(
    const std::vector<std::string>& arg_vec, Rom* rom_context) {
#ifdef YAZE_WITH_GRPC
  // Parse arguments
  int tile_id = -1;
  int x = -1;
  int y = -1;
  
  for (size_t i = 0; i < arg_vec.size(); ++i) {
    const std::string& token = arg_vec[i];
    if (token == "--tile" || token == "--tile-id") {
      if (i + 1 < arg_vec.size()) {
        absl::SimpleAtoi(arg_vec[++i], &tile_id);
      }
    } else if (absl::StartsWith(token, "--tile=")) {
      absl::SimpleAtoi(token.substr(7), &tile_id);
    } else if (token == "--x") {
      if (i + 1 < arg_vec.size()) {
        absl::SimpleAtoi(arg_vec[++i], &x);
      }
    } else if (absl::StartsWith(token, "--x=")) {
      absl::SimpleAtoi(token.substr(4), &x);
    } else if (token == "--y") {
      if (i + 1 < arg_vec.size()) {
        absl::SimpleAtoi(arg_vec[++i], &y);
      }
    } else if (absl::StartsWith(token, "--y=")) {
      absl::SimpleAtoi(token.substr(4), &y);
    }
  }
  
  if (tile_id < 0 || x < 0 || y < 0) {
    return absl::InvalidArgumentError(
        "Usage: gui-place-tile --tile <id> --x <x> --y <y>");
  }
  
  // Create AI actions
  ai::AIAction select_action(ai::AIActionType::kSelectTile, {});
  select_action.parameters["tile_id"] = std::to_string(tile_id);
  
  ai::AIAction place_action(ai::AIActionType::kPlaceTile, {});
  place_action.parameters["x"] = std::to_string(x);
  place_action.parameters["y"] = std::to_string(y);
  
  ai::AIAction save_action(ai::AIActionType::kSaveTile, {});
  
  // Generate test script
  gui::GuiActionGenerator generator;
  std::vector<ai::AIAction> actions = {select_action, place_action, save_action};
  auto script_result = generator.GenerateTestScript(actions);
  if (!script_result.ok()) {
    return script_result.status();
  }
  std::string test_script = *script_result;
  
  // Output as JSON for tool call response
  std::cout << "{\n";
  std::cout << "  \"success\": true,\n";
  std::cout << "  \"tile_id\": " << tile_id << ",\n";
  std::cout << "  \"position\": {\"x\": " << x << ", \"y\": " << y << "},\n";
  std::cout << "  \"test_script\": \"" << test_script << "\",\n";
  std::cout << "  \"message\": \"GUI actions generated for tile placement. Use agent test execute to run.\"\n";
  std::cout << "}\n";
  
  return absl::OkStatus();
#else
  return absl::UnimplementedError("GUI automation requires YAZE_WITH_GRPC=ON");
#endif
}

}  // namespace agent
}  // namespace cli
}  // namespace yaze
