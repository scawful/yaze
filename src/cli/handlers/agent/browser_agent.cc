#include <vector>
#include <string>
#include <iostream>

#include "absl/status/status.h"
#include "absl/strings/str_cat.h"
#include "cli/cli.h"
#include "cli/service/command_registry.h"
#include "cli/handlers/agent/todo_commands.h"
#include "app/rom.h"

namespace yaze {
namespace cli {
namespace handlers {

// Forward declarations if needed
namespace agent {
  // Stubs for unsupported commands
  absl::Status HandleRunCommand(const std::vector<std::string>&, Rom&) {
    return absl::UnimplementedError("Agent run command not available in browser yet");
  }
  absl::Status HandlePlanCommand(const std::vector<std::string>&) {
    return absl::UnimplementedError("Agent plan command not available in browser yet");
  }
  absl::Status HandleDiffCommand(Rom&, const std::vector<std::string>&) {
    return absl::UnimplementedError("Agent diff command not available in browser yet");
  }
  absl::Status HandleCommitCommand(Rom&) {
    return absl::UnimplementedError("Agent commit command not available in browser yet");
  }
  absl::Status HandleRevertCommand(Rom&) {
    return absl::UnimplementedError("Agent revert command not available in browser yet");
  }
  absl::Status HandleAcceptCommand(const std::vector<std::string>&, Rom&) {
    return absl::UnimplementedError("Agent accept command not available in browser yet");
  }
  absl::Status HandleTestCommand(const std::vector<std::string>&) {
    return absl::UnimplementedError("Agent test command not available in browser");
  }
  absl::Status HandleTestConversationCommand(const std::vector<std::string>&) {
    return absl::UnimplementedError("Agent test-conversation command not available in browser");
  }
  absl::Status HandleLearnCommand(const std::vector<std::string>&) {
    return absl::UnimplementedError("Agent learn command not available in browser");
  }
  absl::Status HandleListCommand() {
    return absl::UnimplementedError("Agent list command not available in browser");
  }
  absl::Status HandleDescribeCommand(const std::vector<std::string>&) {
    return absl::UnimplementedError("Agent describe command not available in browser");
  }
}

std::string GenerateAgentHelp() {
  return "Available Agent Commands (Browser):\n"
         "  todo               - Manage todo list\n"
         "  chat, simple-chat  - Chat with the agent (Not yet implemented)\n";
}

Rom& AgentRom() {
  static Rom rom;
  return rom;
}

absl::Status HandleAgentCommand(const std::vector<std::string>& arg_vec) {
  if (arg_vec.empty()) {
    std::cout << GenerateAgentHelp();
    return absl::InvalidArgumentError("No subcommand specified");
  }

  const std::string& subcommand = arg_vec[0];
  std::vector<std::string> subcommand_args(arg_vec.begin() + 1, arg_vec.end());

  if (subcommand == "simple-chat" || subcommand == "chat") {
    // Chat not currently supported in browser build
    return absl::UnimplementedError("Chat not yet available in browser");
  }

  if (subcommand == "todo") {
    return handlers::HandleTodoCommand(subcommand_args);
  }

  // Check registry for other commands
  auto& registry = CommandRegistry::Instance();
  if (registry.HasCommand(subcommand)) {
    return registry.Execute(subcommand, subcommand_args, nullptr);
  }

  std::cout << GenerateAgentHelp();
  return absl::InvalidArgumentError(
      absl::StrCat("Unknown or unsupported agent command in browser: ", subcommand));
}

}  // namespace handlers
}  // namespace cli
}  // namespace yaze
