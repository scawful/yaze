#include "cli/handlers/agent/todo_commands.h"
#include "cli/cli.h"

#include <string>
#include <vector>

#include "absl/flags/declare.h"
#include "absl/flags/flag.h"
#include "absl/status/status.h"
#include "cli/handlers/agent/common.h"
#include "cli/handlers/agent/todo_commands.h"
#include "cli/handlers/agent/simple_chat_command.h"
#include "cli/handlers/tools/resource_commands.h"
#include "cli/handlers/game/dungeon_commands.h"
#include "cli/handlers/game/overworld_commands.h"
#include "cli/handlers/tools/gui_commands.h"
#include "cli/handlers/tools/emulator_commands.h"

ABSL_DECLARE_FLAG(bool, quiet);

namespace yaze {
namespace cli {

// Forward declarations from general_commands.cc
namespace agent {
absl::Status HandlePlanCommand(const std::vector<std::string>& args);
absl::Status HandleTestCommand(const std::vector<std::string>& args);
absl::Status HandleTestConversationCommand(const std::vector<std::string>& args);
absl::Status HandleGuiCommand(const std::vector<std::string>& args);
absl::Status HandleLearnCommand(const std::vector<std::string>& args);
absl::Status HandleListCommand();
absl::Status HandleDescribeCommand(const std::vector<std::string>& args);

// Wrapper functions to call CommandHandlers
absl::Status HandleResourceListCommand(const std::vector<std::string>& args, Rom* rom) {
  handlers::ResourceListCommandHandler handler;
  return handler.Run(args, rom);
}

absl::Status HandleResourceSearchCommand(const std::vector<std::string>& args, Rom* rom) {
  handlers::ResourceSearchCommandHandler handler;
  return handler.Run(args, rom);
}

absl::Status HandleDungeonListSpritesCommand(const std::vector<std::string>& args, Rom* rom) {
  handlers::DungeonListSpritesCommandHandler handler;
  return handler.Run(args, rom);
}

absl::Status HandleDungeonDescribeRoomCommand(const std::vector<std::string>& args, Rom* rom) {
  handlers::DungeonDescribeRoomCommandHandler handler;
  return handler.Run(args, rom);
}

absl::Status HandleOverworldFindTileCommand(const std::vector<std::string>& args, Rom* rom) {
  handlers::OverworldFindTileCommandHandler handler;
  return handler.Run(args, rom);
}

absl::Status HandleOverworldDescribeMapCommand(const std::vector<std::string>& args, Rom* rom) {
  handlers::OverworldDescribeMapCommandHandler handler;
  return handler.Run(args, rom);
}

absl::Status HandleOverworldListWarpsCommand(const std::vector<std::string>& args, Rom* rom) {
  handlers::OverworldListWarpsCommandHandler handler;
  return handler.Run(args, rom);
}

}  // namespace agent

namespace {

constexpr absl::string_view kUsage =
  "Usage: agent <subcommand> [options]\n"
  "\n"
  "AI-Powered Agent Subcommands:\n"
  "  simple-chat              Simple text-based chat (recommended for testing)\n"
  "                          Modes: interactive | piped | batch | single-message\n"
  "                          Example: agent simple-chat \"What dungeons exist?\" --rom=zelda3.sfc\n"
  "                          Example: agent simple-chat --rom=zelda3.sfc --ai_provider=ollama\n"
  "                          Example: echo \"List sprites\" | agent simple-chat --rom=zelda3.sfc\n"
  "                          Example: agent simple-chat --file=queries.txt --rom=zelda3.sfc\n"
  "\n"
  "  test-conversation        Run automated test conversation with AI\n"
  "                          Example: agent test-conversation --rom=zelda3.sfc --ai_provider=ollama\n"
  "\n"
  "  chat                    Full FTXUI-based chat interface\n"
  "                          Example: agent chat --rom=zelda3.sfc\n"
  "\n"
  "ROM Inspection Tools (can be called by AI or directly):\n"
  "  resource-list           List labeled resources (dungeons, sprites, etc.)\n"
  "                          Example: agent resource-list --type=dungeon --format=json\n"
  "\n"
  "  resource-search         Search resource labels by fuzzy text\n"
  "                          Example: agent resource-search --query=soldier --type=sprite\n"
  "\n"
  "  dungeon-list-sprites    List sprites in a dungeon room\n"
  "                          Example: agent dungeon-list-sprites --room=5 --format=json\n"
  "\n"
  "  dungeon-describe-room   Summarize metadata for a dungeon room\n"
  "                          Example: agent dungeon-describe-room --room=0x12 --format=text\n"
  "\n"
  "  overworld-find-tile     Search for tile placements in overworld\n"
  "                          Example: agent overworld-find-tile --tile=0x02E --format=json\n"
  "\n"
  "  overworld-describe-map  Get metadata about an overworld map\n"
  "                          Example: agent overworld-describe-map --map=0 --format=json\n"
  "\n"
  "  overworld-list-warps    List entrances/exits/holes in overworld\n"
  "                          Example: agent overworld-list-warps --map=0 --format=json\n"
  "\n"
  "Proposal & Testing Commands:\n"
  "  run                     Execute agent task\n"
  "  plan                    Generate execution plan\n"
  "  diff                    Show ROM differences\n"
  "  accept                  Accept and apply proposal changes\n"
  "  test                    Run agent tests\n"
  "  gui                     Launch GUI components\n"
  "  learn                   Train agent on examples\n"
  "  list                    List available resources\n"
  "  commit                  Commit changes\n"
  "  revert                  Revert changes\n"
  "  describe                Describe agent capabilities\n"
  "  todo                    Manage tasks and project planning\n"
  "\n"
  "Global Options:\n"
  "  --rom=<path>            Path to Zelda3 ROM file (required for most commands)\n"
  "  --ai_provider=<name>    AI provider: mock (default) | ollama | gemini\n"
  "  --ai_model=<name>       Model name (e.g., qwen2.5-coder:7b for Ollama)\n"
  "  --ollama_host=<url>     Ollama server URL (default: http://localhost:11434)\n"
  "  --gemini_api_key=<key>  Gemini API key (or set GEMINI_API_KEY env var)\n"
  "  --format=<type>         Output format: text | markdown | json | compact\n"
  "\n"
  "For more details, see: docs/simple_chat_input_methods.md";

}  // namespace

namespace handlers {

// Legacy Agent class removed - using new CommandHandler system
// This implementation should be moved to a proper AgentCommandHandler
absl::Status HandleAgentCommand(const std::vector<std::string>& arg_vec) {
  if (arg_vec.empty()) {
    return absl::InvalidArgumentError(std::string(kUsage));
  }

  const std::string& subcommand = arg_vec[0];
  std::vector<std::string> subcommand_args(arg_vec.begin() + 1, arg_vec.end());

  if (subcommand == "run") {
    return absl::UnimplementedError("Agent run command requires ROM context - not yet implemented");
  }
  if (subcommand == "plan") {
    return agent::HandlePlanCommand(subcommand_args);
  }
  if (subcommand == "diff") {
    return absl::UnimplementedError("Agent diff command requires ROM context - not yet implemented");
  }
  if (subcommand == "accept") {
    return absl::UnimplementedError("Agent accept command requires ROM context - not yet implemented");
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
  if (subcommand == "list") {
    return agent::HandleListCommand();
  }
  if (subcommand == "commit") {
    return absl::UnimplementedError("Agent commit command requires ROM context - not yet implemented");
  }
  if (subcommand == "revert") {
    return absl::UnimplementedError("Agent revert command requires ROM context - not yet implemented");
  }
  if (subcommand == "describe") {
    return agent::HandleDescribeCommand(subcommand_args);
  }
  if (subcommand == "resource-list") {
    return agent::HandleResourceListCommand(subcommand_args, nullptr);
  }
  if (subcommand == "resource-search") {
    return agent::HandleResourceSearchCommand(subcommand_args, nullptr);
  }
  if (subcommand == "dungeon-list-sprites") {
    return agent::HandleDungeonListSpritesCommand(subcommand_args, nullptr);
  }
  if (subcommand == "dungeon-describe-room") {
    return agent::HandleDungeonDescribeRoomCommand(subcommand_args, nullptr);
  }
  if (subcommand == "overworld-find-tile") {
    return agent::HandleOverworldFindTileCommand(subcommand_args, nullptr);
  }
  if (subcommand == "overworld-describe-map") {
    return agent::HandleOverworldDescribeMapCommand(subcommand_args, nullptr);
  }
  if (subcommand == "overworld-list-warps") {
    return agent::HandleOverworldListWarpsCommand(subcommand_args, nullptr);
  }
  // if (subcommand == "chat") {
  //   return absl::UnimplementedError("Agent chat command requires ROM context - not yet implemented");
  // }
  // if (subcommand == "todo") {
  //   return handlers::HandleTodoCommand(subcommand_args);
  // }
  
  // // Hex manipulation commands
  // if (subcommand == "hex-read") {
  //   return HandleHexRead(subcommand_args, nullptr);
  // }
  // if (subcommand == "hex-write") {
  //   return HandleHexWrite(subcommand_args, nullptr);
  // }
  // if (subcommand == "hex-search") {
  //   return HandleHexSearch(subcommand_args, nullptr);
  // }
  
  // // Palette manipulation commands
  // if (subcommand == "palette-get-colors") {
  //   return HandlePaletteGetColors(subcommand_args, nullptr);
  // }
  // if (subcommand == "palette-set-color") {
  //   return HandlePaletteSetColor(subcommand_args, nullptr);
  // }
  // if (subcommand == "palette-analyze") {
  //   return HandlePaletteAnalyze(subcommand_args, nullptr);
  // }

  return absl::InvalidArgumentError(std::string(kUsage));
}

// Handler functions are now implemented in command_wrappers.cc

}  // namespace handlers
}  // namespace cli
}  // namespace yaze
