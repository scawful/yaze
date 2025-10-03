#include "cli/service/agent/tool_dispatcher.h"

#include <iostream>
#include <sstream>

#include "absl/strings/match.h"
#include "absl/strings/str_format.h"
#include "cli/handlers/agent/commands.h"

namespace yaze {
namespace cli {
namespace agent {

absl::StatusOr<std::string> ToolDispatcher::Dispatch(
    const ToolCall& tool_call) {
  std::vector<std::string> args;
  bool has_format = false;
  for (const auto& [key, value] : tool_call.args) {
    args.push_back(absl::StrFormat("--%s", key));
    args.push_back(value);
    if (absl::EqualsIgnoreCase(key, "format")) {
      has_format = true;
    }
  }

  if (!has_format) {
    args.push_back("--format");
    args.push_back("json");
  }

  // Capture stdout
  std::stringstream buffer;
  auto old_cout_buf = std::cout.rdbuf();
  std::cout.rdbuf(buffer.rdbuf());

  absl::Status status;
  if (tool_call.tool_name == "resource-list") {
    status = HandleResourceListCommand(args);
  } else if (tool_call.tool_name == "dungeon-list-sprites") {
    status = HandleDungeonListSpritesCommand(args);
  } else {
    status = absl::UnimplementedError(
        absl::StrFormat("Unknown tool: %s", tool_call.tool_name));
  }

  // Restore stdout
  std::cout.rdbuf(old_cout_buf);

  if (!status.ok()) {
    return status;
  }

  return buffer.str();
}

}  // namespace agent
}  // namespace cli
}  // namespace yaze
