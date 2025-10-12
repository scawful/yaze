#include "cli/handlers/agent/todo_commands.h"
#include "cli/cli.h"

#include <string>
#include <vector>

#include "absl/flags/declare.h"
#include "absl/flags/flag.h"
#include "absl/status/status.h"
#include "absl/strings/str_cat.h"
#include "cli/service/command_registry.h"
#include "cli/handlers/agent/common.h"
#include "cli/handlers/agent/todo_commands.h"
#include "cli/handlers/agent/simple_chat_command.h"

ABSL_DECLARE_FLAG(bool, quiet);

namespace yaze {
namespace cli {

// Forward declarations for special agent commands (not in registry)
namespace agent {
absl::Status HandlePlanCommand(const std::vector<std::string>& args);
absl::Status HandleTestCommand(const std::vector<std::string>& args);
absl::Status HandleTestConversationCommand(const std::vector<std::string>& args);
absl::Status HandleGuiCommand(const std::vector<std::string>& args);
absl::Status HandleLearnCommand(const std::vector<std::string>& args);
absl::Status HandleListCommand();
absl::Status HandleDescribeCommand(const std::vector<std::string>& args);
}  // namespace agent

namespace {

std::string GenerateAgentHelp() {
  auto& registry = CommandRegistry::Instance();
  
  std::ostringstream help;
  help << "Usage: agent <subcommand> [options]\n\n";
  
  help << "AI-Powered Agent Commands:\n";
  help << "  simple-chat              Interactive AI chat\n";
  help << "  test-conversation        Automated test conversation\n";
  help << "  plan                     Generate execution plan\n";
  help << "  learn                    Manage learned knowledge\n";
  help << "  todo                     Task management\n";
  help << "  test                     Run tests\n";
  help << "  list/describe            List/describe proposals\n\n";
  
  // Auto-list available tool commands from registry
  help << "Tool Commands (AI can call these):\n";
  auto agent_commands = registry.GetAgentCommands();
  int count = 0;
  for (const auto& cmd : agent_commands) {
    if (count++ < 10) {  // Show first 10
      auto* meta = registry.GetMetadata(cmd);
      if (meta) {
        help << "  " << cmd;
        for (size_t i = cmd.length(); i < 24; i++) help << " ";
        help << meta->description << "\n";
      }
    }
  }
  help << "  ... and " << (agent_commands.size() - 10) << " more (see z3ed --list-commands)\n\n";
  
  help << "Global Options:\n";
  help << "  --rom=<path>            Path to ROM file\n";
  help << "  --ai_provider=<name>    AI provider: ollama | gemini\n";
  help << "  --format=<type>         Output format: text | json\n\n";
  
  help << "For detailed help: z3ed agent <command> --help\n";
  help << "For all commands:  z3ed --list-commands\n";
  
  return help.str();
}

constexpr absl::string_view kUsage = "";

}  // namespace

namespace handlers {

/**
 * @brief Unified agent command handler using CommandRegistry
 * 
 * Routes commands in this order:
 * 1. Special agent commands (plan, test, learn, todo) - Not in registry
 * 2. Registry commands (resource-*, dungeon-*, overworld-*, emulator-*, etc.)
 * 3. Fallback to error
 */
absl::Status HandleAgentCommand(const std::vector<std::string>& arg_vec) {
  if (arg_vec.empty()) {
    std::cout << GenerateAgentHelp();
    return absl::InvalidArgumentError("No subcommand specified");
  }

  const std::string& subcommand = arg_vec[0];
  std::vector<std::string> subcommand_args(arg_vec.begin() + 1, arg_vec.end());
  
  // === Special Agent Commands (not in registry) ===
  
  if (subcommand == "simple-chat" || subcommand == "chat") {
    auto& registry = CommandRegistry::Instance();
    return registry.Execute("simple-chat", subcommand_args, nullptr);
  }
  
  if (subcommand == "plan") {
    return agent::HandlePlanCommand(subcommand_args);
  }
  
  if (subcommand == "test") {
    return agent::HandleTestCommand(subcommand_args);
  }
  
  if (subcommand == "test-conversation") {
    return agent::HandleTestConversationCommand(subcommand_args);
  }
  
  if (subcommand == "gui") {
    return agent::HandleGuiCommand(subcommand_args);
  }
  
  if (subcommand == "learn") {
    return agent::HandleLearnCommand(subcommand_args);
  }
  
  if (subcommand == "todo") {
    return handlers::HandleTodoCommand(subcommand_args);
  }
  
  if (subcommand == "list") {
    return agent::HandleListCommand();
  }
  
  if (subcommand == "describe") {
    return agent::HandleDescribeCommand(subcommand_args);
  }
  
  // Placeholder for unimplemented workflow commands
  if (subcommand == "run" || subcommand == "diff" || subcommand == "accept" ||
      subcommand == "commit" || subcommand == "revert") {
    return absl::UnimplementedError(
        absl::StrCat("Agent ", subcommand, " command requires ROM context - not yet implemented"));
  }
  
  // === Registry Commands (resource, dungeon, overworld, emulator, etc.) ===
  
  auto& registry = CommandRegistry::Instance();
  
  // Check if this is a registered command
  if (registry.HasCommand(subcommand)) {
    return registry.Execute(subcommand, subcommand_args, nullptr);
  }
  
  // Not found
  std::cout << GenerateAgentHelp();
  return absl::InvalidArgumentError(
      absl::StrCat("Unknown agent command: ", subcommand));
}

// Handler functions are now implemented in command_wrappers.cc

}  // namespace handlers
}  // namespace cli
}  // namespace yaze
