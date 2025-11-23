#include "cli/service/agent/tool_dispatcher.h"

#include <algorithm>
#include <memory>
#include <regex>
#include <sstream>
#include <string>

#include "absl/strings/str_cat.h"
#include "cli/handlers/game/dialogue_commands.h"
#include "cli/handlers/game/dungeon_commands.h"
#include "cli/handlers/game/message_commands.h"
#include "cli/handlers/game/music_commands.h"
#include "cli/handlers/game/overworld_commands.h"
#include "cli/handlers/graphics/sprite_commands.h"
#ifdef YAZE_WITH_GRPC
#include "cli/handlers/tools/emulator_commands.h"
#endif
#include "cli/handlers/tools/gui_commands.h"
#include "cli/handlers/tools/resource_commands.h"
#include "cli/service/agent/tools/filesystem_tool.h"
#include "cli/service/agent/tools/memory_inspector_tool.h"
#include "cli/service/resources/command_context.h"
#include "cli/util/terminal_colors.h"

namespace yaze {
namespace cli {
namespace agent {

namespace {

// Map tool name to handler type
ToolCallType GetToolCallType(const std::string& tool_name) {
  // Resource commands
  if (tool_name == "resource-list")
    return ToolCallType::kResourceList;
  if (tool_name == "resource-search")
    return ToolCallType::kResourceSearch;

  // Dungeon commands
  if (tool_name == "dungeon-list-sprites")
    return ToolCallType::kDungeonListSprites;
  if (tool_name == "dungeon-describe-room")
    return ToolCallType::kDungeonDescribeRoom;
  if (tool_name == "dungeon-export-room")
    return ToolCallType::kDungeonExportRoom;
  if (tool_name == "dungeon-list-objects")
    return ToolCallType::kDungeonListObjects;
  if (tool_name == "dungeon-get-room-tiles")
    return ToolCallType::kDungeonGetRoomTiles;
  if (tool_name == "dungeon-set-room-property")
    return ToolCallType::kDungeonSetRoomProperty;

  // Overworld commands
  if (tool_name == "overworld-find-tile")
    return ToolCallType::kOverworldFindTile;
  if (tool_name == "overworld-describe-map")
    return ToolCallType::kOverworldDescribeMap;
  if (tool_name == "overworld-list-warps")
    return ToolCallType::kOverworldListWarps;
  if (tool_name == "overworld-list-sprites")
    return ToolCallType::kOverworldListSprites;
  if (tool_name == "overworld-get-entrance")
    return ToolCallType::kOverworldGetEntrance;
  if (tool_name == "overworld-tile-stats")
    return ToolCallType::kOverworldTileStats;

  // Message & Dialogue commands
  if (tool_name == "message-list")
    return ToolCallType::kMessageList;
  if (tool_name == "message-read")
    return ToolCallType::kMessageRead;
  if (tool_name == "message-search")
    return ToolCallType::kMessageSearch;
  if (tool_name == "dialogue-list")
    return ToolCallType::kDialogueList;
  if (tool_name == "dialogue-read")
    return ToolCallType::kDialogueRead;
  if (tool_name == "dialogue-search")
    return ToolCallType::kDialogueSearch;

  // GUI Automation commands
  if (tool_name == "gui-place-tile")
    return ToolCallType::kGuiPlaceTile;
  if (tool_name == "gui-click")
    return ToolCallType::kGuiClick;
  if (tool_name == "gui-discover-tool")
    return ToolCallType::kGuiDiscover;
  if (tool_name == "gui-screenshot")
    return ToolCallType::kGuiScreenshot;

  // Music commands
  if (tool_name == "music-list")
    return ToolCallType::kMusicList;
  if (tool_name == "music-info")
    return ToolCallType::kMusicInfo;
  if (tool_name == "music-tracks")
    return ToolCallType::kMusicTracks;

  // Sprite commands
  if (tool_name == "sprite-list")
    return ToolCallType::kSpriteList;
  if (tool_name == "sprite-properties")
    return ToolCallType::kSpriteProperties;
  if (tool_name == "sprite-palette")
    return ToolCallType::kSpritePalette;

  // Emulator & Debugger commands
  if (tool_name == "emulator-step")
    return ToolCallType::kEmulatorStep;
  if (tool_name == "emulator-run")
    return ToolCallType::kEmulatorRun;
  if (tool_name == "emulator-pause")
    return ToolCallType::kEmulatorPause;
  if (tool_name == "emulator-reset")
    return ToolCallType::kEmulatorReset;
  if (tool_name == "emulator-get-state")
    return ToolCallType::kEmulatorGetState;
  if (tool_name == "emulator-set-breakpoint")
    return ToolCallType::kEmulatorSetBreakpoint;
  if (tool_name == "emulator-clear-breakpoint")
    return ToolCallType::kEmulatorClearBreakpoint;
  if (tool_name == "emulator-list-breakpoints")
    return ToolCallType::kEmulatorListBreakpoints;
  if (tool_name == "emulator-read-memory")
    return ToolCallType::kEmulatorReadMemory;
  if (tool_name == "emulator-write-memory")
    return ToolCallType::kEmulatorWriteMemory;
  if (tool_name == "emulator-get-registers")
    return ToolCallType::kEmulatorGetRegisters;
  if (tool_name == "emulator-get-metrics")
    return ToolCallType::kEmulatorGetMetrics;

  // Filesystem commands
  if (tool_name == "filesystem-list")
    return ToolCallType::kFilesystemList;
  if (tool_name == "filesystem-read")
    return ToolCallType::kFilesystemRead;
  if (tool_name == "filesystem-exists")
    return ToolCallType::kFilesystemExists;
  if (tool_name == "filesystem-info")
    return ToolCallType::kFilesystemInfo;

  // Build commands (placeholder for future implementation)
  if (tool_name == "build-configure")
    return ToolCallType::kBuildConfigure;
  if (tool_name == "build-compile")
    return ToolCallType::kBuildCompile;
  if (tool_name == "build-test")
    return ToolCallType::kBuildTest;
  if (tool_name == "build-status")
    return ToolCallType::kBuildStatus;

  // Memory inspector commands
  if (tool_name == "memory-analyze")
    return ToolCallType::kMemoryAnalyze;
  if (tool_name == "memory-search")
    return ToolCallType::kMemorySearch;
  if (tool_name == "memory-compare")
    return ToolCallType::kMemoryCompare;
  if (tool_name == "memory-check")
    return ToolCallType::kMemoryCheck;
  if (tool_name == "memory-regions")
    return ToolCallType::kMemoryRegions;

  return ToolCallType::kUnknown;
}

// Create the appropriate command handler for a tool call type
std::unique_ptr<resources::CommandHandler> CreateHandler(ToolCallType type) {
  using namespace yaze::cli::handlers;
  using namespace yaze::cli::agent::tools;

  switch (type) {
    // Resource commands
    case ToolCallType::kResourceList:
      return std::make_unique<ResourceListCommandHandler>();
    case ToolCallType::kResourceSearch:
      return std::make_unique<ResourceSearchCommandHandler>();

    // Dungeon commands
    case ToolCallType::kDungeonListSprites:
      return std::make_unique<DungeonListSpritesCommandHandler>();
    case ToolCallType::kDungeonDescribeRoom:
      return std::make_unique<DungeonDescribeRoomCommandHandler>();
    case ToolCallType::kDungeonExportRoom:
      return std::make_unique<DungeonExportRoomCommandHandler>();
    case ToolCallType::kDungeonListObjects:
      return std::make_unique<DungeonListObjectsCommandHandler>();
    case ToolCallType::kDungeonGetRoomTiles:
      return std::make_unique<DungeonGetRoomTilesCommandHandler>();
    case ToolCallType::kDungeonSetRoomProperty:
      return std::make_unique<DungeonSetRoomPropertyCommandHandler>();

    // Overworld commands
    case ToolCallType::kOverworldFindTile:
      return std::make_unique<OverworldFindTileCommandHandler>();
    case ToolCallType::kOverworldDescribeMap:
      return std::make_unique<OverworldDescribeMapCommandHandler>();
    case ToolCallType::kOverworldListWarps:
      return std::make_unique<OverworldListWarpsCommandHandler>();
    case ToolCallType::kOverworldListSprites:
      return std::make_unique<OverworldListSpritesCommandHandler>();
    case ToolCallType::kOverworldGetEntrance:
      return std::make_unique<OverworldGetEntranceCommandHandler>();
    case ToolCallType::kOverworldTileStats:
      return std::make_unique<OverworldTileStatsCommandHandler>();

    // Message & Dialogue commands
    case ToolCallType::kMessageList:
      return std::make_unique<MessageListCommandHandler>();
    case ToolCallType::kMessageRead:
      return std::make_unique<MessageReadCommandHandler>();
    case ToolCallType::kMessageSearch:
      return std::make_unique<MessageSearchCommandHandler>();
    case ToolCallType::kDialogueList:
      return std::make_unique<DialogueListCommandHandler>();
    case ToolCallType::kDialogueRead:
      return std::make_unique<DialogueReadCommandHandler>();
    case ToolCallType::kDialogueSearch:
      return std::make_unique<DialogueSearchCommandHandler>();

    // GUI Automation commands
    case ToolCallType::kGuiPlaceTile:
      return std::make_unique<GuiPlaceTileCommandHandler>();
    case ToolCallType::kGuiClick:
      return std::make_unique<GuiClickCommandHandler>();
    case ToolCallType::kGuiDiscover:
      return std::make_unique<GuiDiscoverToolCommandHandler>();
    case ToolCallType::kGuiScreenshot:
      return std::make_unique<GuiScreenshotCommandHandler>();

    // Music commands
    case ToolCallType::kMusicList:
      return std::make_unique<MusicListCommandHandler>();
    case ToolCallType::kMusicInfo:
      return std::make_unique<MusicInfoCommandHandler>();
    case ToolCallType::kMusicTracks:
      return std::make_unique<MusicTracksCommandHandler>();

    // Sprite commands
    case ToolCallType::kSpriteList:
      return std::make_unique<SpriteListCommandHandler>();
    case ToolCallType::kSpriteProperties:
      return std::make_unique<SpritePropertiesCommandHandler>();
    case ToolCallType::kSpritePalette:
      return std::make_unique<SpritePaletteCommandHandler>();

      // Emulator & Debugger commands
#ifdef YAZE_WITH_GRPC
    case ToolCallType::kEmulatorStep:
      return std::make_unique<EmulatorStepCommandHandler>();
    case ToolCallType::kEmulatorRun:
      return std::make_unique<EmulatorRunCommandHandler>();
    case ToolCallType::kEmulatorPause:
      return std::make_unique<EmulatorPauseCommandHandler>();
    case ToolCallType::kEmulatorReset:
      return std::make_unique<EmulatorResetCommandHandler>();
    case ToolCallType::kEmulatorGetState:
      return std::make_unique<EmulatorGetStateCommandHandler>();
    case ToolCallType::kEmulatorSetBreakpoint:
      return std::make_unique<EmulatorSetBreakpointCommandHandler>();
    case ToolCallType::kEmulatorClearBreakpoint:
      return std::make_unique<EmulatorClearBreakpointCommandHandler>();
    case ToolCallType::kEmulatorListBreakpoints:
      return std::make_unique<EmulatorListBreakpointsCommandHandler>();
    case ToolCallType::kEmulatorReadMemory:
      return std::make_unique<EmulatorReadMemoryCommandHandler>();
    case ToolCallType::kEmulatorWriteMemory:
      return std::make_unique<EmulatorWriteMemoryCommandHandler>();
    case ToolCallType::kEmulatorGetRegisters:
      return std::make_unique<EmulatorGetRegistersCommandHandler>();
    case ToolCallType::kEmulatorGetMetrics:
      return std::make_unique<EmulatorGetMetricsCommandHandler>();
#endif

    // Filesystem commands
    case ToolCallType::kFilesystemList:
      return std::make_unique<FileSystemListTool>();
    case ToolCallType::kFilesystemRead:
      return std::make_unique<FileSystemReadTool>();
    case ToolCallType::kFilesystemExists:
      return std::make_unique<FileSystemExistsTool>();
    case ToolCallType::kFilesystemInfo:
      return std::make_unique<FileSystemInfoTool>();

    // Build commands (TODO: Implement these handlers)
    // case ToolCallType::kBuildConfigure:
    //   return std::make_unique<BuildConfigureCommandHandler>();
    // case ToolCallType::kBuildCompile:
    //   return std::make_unique<BuildCompileCommandHandler>();
    // case ToolCallType::kBuildTest:
    //   return std::make_unique<BuildTestCommandHandler>();
    // case ToolCallType::kBuildStatus:
    //   return std::make_unique<BuildStatusCommandHandler>();

    // Memory inspector commands
    case ToolCallType::kMemoryAnalyze:
      return std::make_unique<MemoryAnalyzeTool>();
    case ToolCallType::kMemorySearch:
      return std::make_unique<MemorySearchTool>();
    case ToolCallType::kMemoryCompare:
      return std::make_unique<MemoryCompareTool>();
    case ToolCallType::kMemoryCheck:
      return std::make_unique<MemoryCheckTool>();
    case ToolCallType::kMemoryRegions:
      return std::make_unique<MemoryRegionsTool>();

    default:
      return nullptr;
  }
}

// Convert tool call arguments map to command-line style vector
std::vector<std::string> ConvertArgsToVector(
    const std::map<std::string, std::string>& args) {
  std::vector<std::string> result;

  for (const auto& [key, value] : args) {
    // Convert to --key=value format
    result.push_back(absl::StrCat("--", key, "=", value));
  }

  // Always request JSON format for tool calls (easier for AI to parse)
  bool has_format = false;
  for (const auto& arg : result) {
    if (arg.find("--format=") == 0) {
      has_format = true;
      break;
    }
  }
  if (!has_format) {
    result.push_back("--format=json");
  }

  return result;
}

}  // namespace

bool ToolDispatcher::IsToolEnabled(ToolCallType type) const {
  switch (type) {
    case ToolCallType::kResourceList:
    case ToolCallType::kResourceSearch:
      return preferences_.resources;

    case ToolCallType::kDungeonListSprites:
    case ToolCallType::kDungeonDescribeRoom:
    case ToolCallType::kDungeonExportRoom:
    case ToolCallType::kDungeonListObjects:
    case ToolCallType::kDungeonGetRoomTiles:
    case ToolCallType::kDungeonSetRoomProperty:
      return preferences_.dungeon;

    case ToolCallType::kOverworldFindTile:
    case ToolCallType::kOverworldDescribeMap:
    case ToolCallType::kOverworldListWarps:
    case ToolCallType::kOverworldListSprites:
    case ToolCallType::kOverworldGetEntrance:
    case ToolCallType::kOverworldTileStats:
      return preferences_.overworld;

    case ToolCallType::kMessageList:
    case ToolCallType::kMessageRead:
    case ToolCallType::kMessageSearch:
      return preferences_.messages;

    case ToolCallType::kDialogueList:
    case ToolCallType::kDialogueRead:
    case ToolCallType::kDialogueSearch:
      return preferences_.dialogue;

    case ToolCallType::kGuiPlaceTile:
    case ToolCallType::kGuiClick:
    case ToolCallType::kGuiDiscover:
    case ToolCallType::kGuiScreenshot:
      return preferences_.gui;

    case ToolCallType::kMusicList:
    case ToolCallType::kMusicInfo:
    case ToolCallType::kMusicTracks:
      return preferences_.music;

    case ToolCallType::kSpriteList:
    case ToolCallType::kSpriteProperties:
    case ToolCallType::kSpritePalette:
      return preferences_.sprite;

#ifdef YAZE_WITH_GRPC
    case ToolCallType::kEmulatorStep:
    case ToolCallType::kEmulatorRun:
    case ToolCallType::kEmulatorPause:
    case ToolCallType::kEmulatorReset:
    case ToolCallType::kEmulatorGetState:
    case ToolCallType::kEmulatorSetBreakpoint:
    case ToolCallType::kEmulatorClearBreakpoint:
    case ToolCallType::kEmulatorListBreakpoints:
    case ToolCallType::kEmulatorReadMemory:
    case ToolCallType::kEmulatorWriteMemory:
    case ToolCallType::kEmulatorGetRegisters:
    case ToolCallType::kEmulatorGetMetrics:
      return preferences_.emulator;
#endif

    case ToolCallType::kFilesystemList:
    case ToolCallType::kFilesystemRead:
    case ToolCallType::kFilesystemExists:
    case ToolCallType::kFilesystemInfo:
      return preferences_.filesystem;

    case ToolCallType::kBuildConfigure:
    case ToolCallType::kBuildCompile:
    case ToolCallType::kBuildTest:
    case ToolCallType::kBuildStatus:
      return preferences_.build;

    case ToolCallType::kMemoryAnalyze:
    case ToolCallType::kMemorySearch:
    case ToolCallType::kMemoryCompare:
    case ToolCallType::kMemoryCheck:
    case ToolCallType::kMemoryRegions:
      return preferences_.memory_inspector;

    default:
      return true;
  }
}

absl::StatusOr<std::string> ToolDispatcher::Dispatch(const ToolCall& call) {
  // Determine tool call type
  ToolCallType type = GetToolCallType(call.tool_name);

  if (type == ToolCallType::kUnknown) {
    return absl::InvalidArgumentError(
        absl::StrCat("Unknown tool: ", call.tool_name));
  }

  if (!IsToolEnabled(type)) {
    return absl::FailedPreconditionError(absl::StrCat(
        "Tool '", call.tool_name, "' disabled by current agent configuration"));
  }

  // Create the appropriate command handler
  auto handler = CreateHandler(type);
  if (!handler) {
    return absl::InternalError(
        absl::StrCat("Failed to create handler for tool: ", call.tool_name));
  }

  // Convert arguments to command-line style
  std::vector<std::string> args = ConvertArgsToVector(call.args);

  // Check if ROM context is required but not available
  if (!rom_context_) {
    return absl::FailedPreconditionError(
        absl::StrCat("Tool '", call.tool_name,
                     "' requires ROM context but none is available"));
  }

  // Execute the command handler
  // The handler will internally use OutputFormatter to capture output
  // We need to capture stdout to get the formatted output

  // Redirect stdout to capture the output
  std::stringstream output_buffer;
  std::streambuf* old_cout = std::cout.rdbuf(output_buffer.rdbuf());

  // Execute the handler
  absl::Status status = handler->Run(args, rom_context_);

  // Restore stdout
  std::cout.rdbuf(old_cout);

  if (!status.ok()) {
    return status;
  }

  // Return the captured output
  std::string output = output_buffer.str();
  if (output.empty()) {
    return absl::InternalError(
        absl::StrCat("Tool '", call.tool_name, "' produced no output"));
  }

  return output;
}

}  // namespace agent
}  // namespace cli
}  // namespace yaze
