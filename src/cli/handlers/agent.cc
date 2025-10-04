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
  "  dungeon-list-sprites    List sprites in a dungeon room\n"
  "                          Example: agent dungeon-list-sprites --room=5 --format=json\n"
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
  "\n"
  "Global Options:\n"
  "  --rom=<path>            Path to Zelda3 ROM file (required for most commands)\n"
  "  --ai_provider=<name>    AI provider: mock (default) | ollama | gemini\n"
  "  --ai_model=<name>       Model name (e.g., qwen2.5-coder:7b for Ollama)\n"
  "  --ollama_host=<url>     Ollama server URL (default: http://localhost:11434)\n"
  "  --gemini_api_key=<key>  Gemini API key (or set GEMINI_API_KEY env var)\n"
  "  --format=<type>         Output format: json | table | yaml\n"
  "\n"
  "For more details, see: docs/simple_chat_input_methods.md";

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
  if (subcommand == "test-conversation") {
    return agent::HandleTestConversationCommand(subcommand_args);
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
  if (subcommand == "resource-list") {
    return agent::HandleResourceListCommand(subcommand_args);
  }
  if (subcommand == "dungeon-list-sprites") {
    return agent::HandleDungeonListSpritesCommand(subcommand_args);
  }
  if (subcommand == "overworld-find-tile") {
    return agent::HandleOverworldFindTileCommand(subcommand_args);
  }
  if (subcommand == "overworld-describe-map") {
    return agent::HandleOverworldDescribeMapCommand(subcommand_args);
  }
  if (subcommand == "overworld-list-warps") {
    return agent::HandleOverworldListWarpsCommand(subcommand_args);
  }
  if (subcommand == "chat") {
    return agent::HandleChatCommand(rom_);
  }
  if (subcommand == "simple-chat") {
    return agent::HandleSimpleChatCommand(subcommand_args, rom_);
  }

  return absl::InvalidArgumentError(std::string(agent::kUsage));
}

}  // namespace cli
}  // namespace yaze
