#include "cli/handlers/agent/commands.h"
#include "cli/z3ed.h"

#include <string>
#include <vector>

#include "absl/status/status.h"

namespace yaze {
namespace cli {
namespace agent {
namespace {

constexpr absl::string_view kUsage =
    "Usage: agent <run|plan|diff|accept|test|gui|learn|list|commit|revert|describe> "
    "[options]";

}  // namespace
}  // namespace agent

absl::Status Agent::Run(const std::vector<std::string>& arg_vec) {
  if (arg_vec.empty()) {
    return absl::InvalidArgumentError(std::string(agent::kUsage));
  }

  const std::string& subcommand = arg_vec[0];
  std::vector<std::string> subcommand_args(arg_vec.begin() + 1, arg_vec.end());

  if (subcommand == "run") {
    return agent::HandleRunCommand(subcommand_args, rom_);
  }
  if (subcommand == "plan") {
    return agent::HandlePlanCommand(subcommand_args);
  }
  if (subcommand == "diff") {
    return agent::HandleDiffCommand(rom_, subcommand_args);
  }
  if (subcommand == "accept") {
    return agent::HandleAcceptCommand(subcommand_args, rom_);
  }
  if (subcommand == "test") {
    return agent::HandleTestCommand(subcommand_args);
  }
  if (subcommand == "gui") {
    return agent::HandleGuiCommand(subcommand_args);
  }
  if (subcommand == "learn") {
    return agent::HandleLearnCommand();
  }
  if (subcommand == "list") {
    return agent::HandleListCommand();
  }
  if (subcommand == "commit") {
    return agent::HandleCommitCommand(rom_);
  }
  if (subcommand == "revert") {
    return agent::HandleRevertCommand(rom_);
  }
  if (subcommand == "describe") {
    return agent::HandleDescribeCommand(subcommand_args);
  }

  return absl::InvalidArgumentError(std::string(agent::kUsage));
}

}  // namespace cli
}  // namespace yaze
