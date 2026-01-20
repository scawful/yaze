#include <algorithm>
#include <sstream>
#include <string>
#include <vector>

#include "absl/flags/declare.h"
#include "absl/flags/flag.h"
#include "absl/status/status.h"
#include "absl/strings/str_cat.h"
#include "cli/cli.h"
#include "cli/handlers/agent_command_registration.h"
#include "cli/handlers/agent/common.h"
#include "cli/handlers/agent/simple_chat_command.h"
#include "cli/handlers/agent/todo_commands.h"
#include "cli/service/command_registry.h"
#include "rom/rom.h"

ABSL_DECLARE_FLAG(bool, quiet);

namespace yaze {
namespace cli {

// Forward declarations for special agent commands (not in registry)
namespace agent {
absl::Status HandleRunCommand(const std::vector<std::string>& args, Rom& rom);
absl::Status HandlePlanCommand(const std::vector<std::string>& args);
absl::Status HandleTestCommand(const std::vector<std::string>& args);
absl::Status HandleTestConversationCommand(
    const std::vector<std::string>& args);
absl::Status HandleLearnCommand(const std::vector<std::string>& args);
absl::Status HandleListCommand();
absl::Status HandleDiffCommand(Rom& rom, const std::vector<std::string>& args);
absl::Status HandleCommitCommand(Rom& rom);
absl::Status HandleRevertCommand(Rom& rom);
absl::Status HandleAcceptCommand(const std::vector<std::string>& args,
                                 Rom& rom);
absl::Status HandleDescribeCommand(const std::vector<std::string>& args);
}  // namespace agent

namespace {

Rom& AgentRom() {
  static Rom rom;
  return rom;
}

std::string GenerateAgentHelp() {
  auto& registry = CommandRegistry::Instance();

  std::ostringstream help;
  help << "Usage: agent <subcommand> [options]\n\n";

  help << "AI-Powered Agent Commands:\n";
  help << "  simple-chat              Interactive AI chat\n";
  help << "  test-conversation        Automated test conversation\n";
  help << "  plan                     Generate execution plan\n";
  help << "  run                      Execute plan in sandbox\n";
  help << "  diff                     Review the latest proposal diff\n";
  help << "  accept                   Apply proposal changes to ROM\n";
  help << "  commit                   Save current ROM changes\n";
  help << "  revert                   Reload ROM from disk\n";
  help << "  learn                    Manage learned knowledge\n";
  help << "  todo                     Task management\n";
  help << "  test                     Run tests\n";
  help << "  list/describe            List/describe proposals\n\n";

  // Auto-list available tool commands from registry
  help << "Tool Commands (AI can call these):\n";
  auto agent_commands = registry.GetAgentCommands();
  const size_t preview_count = std::min<size_t>(10, agent_commands.size());
  for (size_t i = 0; i < preview_count; ++i) {
    const auto& cmd = agent_commands[i];
    if (auto* meta = registry.GetMetadata(cmd); meta != nullptr) {
      help << "  " << cmd;
      for (size_t pad = cmd.length(); pad < 24; ++pad)
        help << " ";
      help << meta->description << "\n";
    }
  }
  if (agent_commands.size() > preview_count) {
    help << "  ... and " << (agent_commands.size() - preview_count)
         << " more (see z3ed --list-commands)\n";
  }
  help << "\n";

  help << "Global Options:\n";
  help << "  --rom=<path>            Path to ROM file\n";
  help << "  --ai_provider=<name>    AI provider: auto | ollama | gemini | "
          "openai | anthropic | mock\n";
  help << "  --ai_model=<name>       Provider-specific model override\n";
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
  RegisterAgentCommandHandlers();
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

  auto& agent_rom = AgentRom();

  if (subcommand == "run") {
    return agent::HandleRunCommand(subcommand_args, agent_rom);
  }

  if (subcommand == "plan") {
    return agent::HandlePlanCommand(subcommand_args);
  }

  if (subcommand == "diff") {
    return agent::HandleDiffCommand(agent_rom, subcommand_args);
  }

  if (subcommand == "accept") {
    return agent::HandleAcceptCommand(subcommand_args, agent_rom);
  }

  if (subcommand == "commit") {
    return agent::HandleCommitCommand(agent_rom);
  }

  if (subcommand == "revert") {
    return agent::HandleRevertCommand(agent_rom);
  }

  if (subcommand == "test") {
    return agent::HandleTestCommand(subcommand_args);
  }

  if (subcommand == "test-conversation") {
    return agent::HandleTestConversationCommand(subcommand_args);
  }

  if (subcommand == "gui") {
    // GUI commands are in the registry (gui-place-tile, gui-click, etc.)
    // Route to registry instead
    return absl::InvalidArgumentError(
        "Use 'z3ed gui-<command>' or see 'z3ed --help gui' for available GUI "
        "automation commands");
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

// Handler implementations live in general_commands.cc

}  // namespace handlers
}  // namespace cli
}  // namespace yaze
