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
    status = HandleResourceListCommand(args, rom_context_);
  } else if (tool_call.tool_name == "resource-search") {
    status = HandleResourceSearchCommand(args, rom_context_);
  } else if (tool_call.tool_name == "dungeon-list-sprites") {
    status = HandleDungeonListSpritesCommand(args, rom_context_);
  } else if (tool_call.tool_name == "dungeon-describe-room") {
    status = HandleDungeonDescribeRoomCommand(args, rom_context_);
  } else if (tool_call.tool_name == "overworld-find-tile") {
    status = HandleOverworldFindTileCommand(args, rom_context_);
  } else if (tool_call.tool_name == "overworld-describe-map") {
    status = HandleOverworldDescribeMapCommand(args, rom_context_);
  } else if (tool_call.tool_name == "overworld-list-warps") {
    status = HandleOverworldListWarpsCommand(args, rom_context_);
  } else if (tool_call.tool_name == "overworld-list-sprites") {
    status = HandleOverworldListSpritesCommand(args, rom_context_);
  } else if (tool_call.tool_name == "overworld-get-entrance") {
    status = HandleOverworldGetEntranceCommand(args, rom_context_);
  } else if (tool_call.tool_name == "overworld-tile-stats") {
    status = HandleOverworldTileStatsCommand(args, rom_context_);
  } else if (tool_call.tool_name == "message-list") {
    status = HandleMessageListCommand(args, rom_context_);
  } else if (tool_call.tool_name == "message-read") {
    status = HandleMessageReadCommand(args, rom_context_);
  } else if (tool_call.tool_name == "message-search") {
    status = HandleMessageSearchCommand(args, rom_context_);
  } else if (tool_call.tool_name == "gui-place-tile") {
    // GUI automation tool for placing tiles via test harness
    status = HandleGuiPlaceTileCommand(args, rom_context_);
  } else if (tool_call.tool_name == "gui-click") {
    status = HandleGuiClickCommand(args, rom_context_);
  } else if (tool_call.tool_name == "gui-discover") {
    status = HandleGuiDiscoverToolCommand(args, rom_context_);
  } else if (tool_call.tool_name == "gui-screenshot") {
    status = HandleGuiScreenshotCommand(args, rom_context_);
  } else if (tool_call.tool_name == "dialogue-list") {
    status = HandleDialogueListCommand(args, rom_context_);
  } else if (tool_call.tool_name == "dialogue-read") {
    status = HandleDialogueReadCommand(args, rom_context_);
  } else if (tool_call.tool_name == "dialogue-search") {
    status = HandleDialogueSearchCommand(args, rom_context_);
  } else if (tool_call.tool_name == "music-list") {
    status = HandleMusicListCommand(args, rom_context_);
  } else if (tool_call.tool_name == "music-info") {
    status = HandleMusicInfoCommand(args, rom_context_);
  } else if (tool_call.tool_name == "music-tracks") {
    status = HandleMusicTracksCommand(args, rom_context_);
  } else if (tool_call.tool_name == "sprite-list") {
    status = HandleSpriteListCommand(args, rom_context_);
  } else if (tool_call.tool_name == "sprite-properties") {
    status = HandleSpritePropertiesCommand(args, rom_context_);
  } else if (tool_call.tool_name == "sprite-palette") {
    status = HandleSpritePaletteCommand(args, rom_context_);
  } else if (tool_call.tool_name == "dungeon-export-room") {
    status = HandleDungeonExportRoomCommand(args, rom_context_);
  } else if (tool_call.tool_name == "dungeon-list-objects") {
    status = HandleDungeonListObjectsCommand(args, rom_context_);
  } else if (tool_call.tool_name == "dungeon-get-room-tiles") {
    status = HandleDungeonGetRoomTilesCommand(args, rom_context_);
  } else if (tool_call.tool_name == "dungeon-set-room-property") {
    status = HandleDungeonSetRoomPropertyCommand(args, rom_context_);
  } else if (tool_call.tool_name == "emulator-step") {
    status = HandleEmulatorStepCommand(args, rom_context_);
  } else if (tool_call.tool_name == "emulator-run") {
    status = HandleEmulatorRunCommand(args, rom_context_);
  } else if (tool_call.tool_name == "emulator-pause") {
    status = HandleEmulatorPauseCommand(args, rom_context_);
  } else if (tool_call.tool_name == "emulator-reset") {
    status = HandleEmulatorResetCommand(args, rom_context_);
  } else if (tool_call.tool_name == "emulator-get-state") {
    status = HandleEmulatorGetStateCommand(args, rom_context_);
  } else if (tool_call.tool_name == "emulator-set-breakpoint") {
    status = HandleEmulatorSetBreakpointCommand(args, rom_context_);
  } else if (tool_call.tool_name == "emulator-clear-breakpoint") {
    status = HandleEmulatorClearBreakpointCommand(args, rom_context_);
  } else if (tool_call.tool_name == "emulator-list-breakpoints") {
    status = HandleEmulatorListBreakpointsCommand(args, rom_context_);
  } else if (tool_call.tool_name == "emulator-read-memory") {
    status = HandleEmulatorReadMemoryCommand(args, rom_context_);
  } else if (tool_call.tool_name == "emulator-write-memory") {
    status = HandleEmulatorWriteMemoryCommand(args, rom_context_);
  } else if (tool_call.tool_name == "emulator-get-registers") {
    status = HandleEmulatorGetRegistersCommand(args, rom_context_);
  } else if (tool_call.tool_name == "emulator-get-metrics") {
    status = HandleEmulatorGetMetricsCommand(args, rom_context_);
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
