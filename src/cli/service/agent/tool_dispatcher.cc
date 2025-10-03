#include "cli/service/agent/tool_dispatcher.h"

#include "absl/strings/str_format.h"
#include "cli/handlers/agent/commands.h"

namespace yaze {
namespace cli {
namespace agent {

absl::StatusOr<std::string> ToolDispatcher::Dispatch(
    const ToolCall& tool_call) {
  std::vector<std::string> args;
  for (const auto& [key, value] : tool_call.args) {
    args.push_back(absl::StrFormat("--%s", key));
    args.push_back(value);
  }

  if (tool_call.tool_name == "resource-list") {
    // Note: This is a simplified approach for now. A more robust solution
    // would capture stdout instead of relying on the handler to return a string.
    auto status = HandleResourceListCommand(args);
    if (!status.ok()) {
      return status;
    }
    return "Successfully listed resources.";
  } else if (tool_call.tool_name == "dungeon-list-sprites") {
    auto status = HandleDungeonListSpritesCommand(args);
    if (!status.ok()) {
      return status;
    }
    return "Successfully listed sprites.";
  }

  return absl::UnimplementedError(
      absl::StrFormat("Unknown tool: %s", tool_call.tool_name));
}

}  // namespace agent
}  // namespace cli
}  // namespace yaze
