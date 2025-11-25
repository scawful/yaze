#include "cli/service/agent/tool_dispatcher.h"

#include <algorithm>
#include <chrono>
#include <future>
#include <memory>
#include <regex>
#include <sstream>
#include <string>

#include "absl/strings/ascii.h"
#include "absl/strings/str_cat.h"
#include "cli/handlers/game/dialogue_commands.h"
#include "cli/handlers/game/dungeon_commands.h"
#include "cli/handlers/game/message_commands.h"
#include "cli/handlers/game/music_commands.h"
#include "cli/handlers/game/overworld_commands.h"
#include "cli/handlers/graphics/sprite_commands.h"
#include "cli/handlers/tools/test_helpers_commands.h"
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
  // Meta-tools for discoverability
  if (tool_name == "tools-list")
    return ToolCallType::kToolsList;
  if (tool_name == "tools-describe")
    return ToolCallType::kToolsDescribe;
  if (tool_name == "tools-search")
    return ToolCallType::kToolsSearch;

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

  // Test helper tools
  if (tool_name == "tools-helper-list")
    return ToolCallType::kToolsHelperList;
  if (tool_name == "tools-harness-state")
    return ToolCallType::kToolsHarnessState;
  if (tool_name == "tools-extract-values")
    return ToolCallType::kToolsExtractValues;
  if (tool_name == "tools-extract-golden")
    return ToolCallType::kToolsExtractGolden;
  if (tool_name == "tools-patch-v3")
    return ToolCallType::kToolsPatchV3;

  return ToolCallType::kUnknown;
}

// Create the appropriate command handler for a tool call type
std::unique_ptr<resources::CommandHandler> CreateHandler(ToolCallType type) {
  using namespace yaze::cli::handlers;
  using namespace yaze::cli::agent::tools;

  switch (type) {
    // Meta-tools are handled specially in Dispatch()
    case ToolCallType::kToolsList:
    case ToolCallType::kToolsDescribe:
    case ToolCallType::kToolsSearch:
      return nullptr;  // Handled inline

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

    // Test helper tools
    case ToolCallType::kToolsHelperList:
      return std::make_unique<ToolsListCommandHandler>();
    case ToolCallType::kToolsHarnessState:
      return std::make_unique<ToolsHarnessStateCommandHandler>();
    case ToolCallType::kToolsExtractValues:
      return std::make_unique<ToolsExtractValuesCommandHandler>();
    case ToolCallType::kToolsExtractGolden:
      return std::make_unique<ToolsExtractGoldenCommandHandler>();
    case ToolCallType::kToolsPatchV3:
      return std::make_unique<ToolsPatchV3CommandHandler>();

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

    case ToolCallType::kToolsList:
    case ToolCallType::kToolsDescribe:
    case ToolCallType::kToolsSearch:
      return preferences_.meta_tools;

    case ToolCallType::kToolsHelperList:
    case ToolCallType::kToolsHarnessState:
    case ToolCallType::kToolsExtractValues:
    case ToolCallType::kToolsExtractGolden:
    case ToolCallType::kToolsPatchV3:
      return preferences_.test_helpers;

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

  // Handle meta-tools inline
  if (type == ToolCallType::kToolsList) {
    std::ostringstream result;
    result << "{\"tools\": [\n";
    auto tools = GetAvailableTools();
    for (size_t i = 0; i < tools.size(); ++i) {
      result << "  {\"name\": \"" << tools[i].name << "\", "
             << "\"category\": \"" << tools[i].category << "\", "
             << "\"description\": \"" << tools[i].description << "\"}";
      if (i < tools.size() - 1) result << ",";
      result << "\n";
    }
    result << "]}\n";
    return result.str();
  }

  if (type == ToolCallType::kToolsDescribe) {
    auto it = call.args.find("name");
    if (it == call.args.end()) {
      return absl::InvalidArgumentError(
          "tools-describe requires 'name' argument");
    }
    auto info = GetToolInfo(it->second);
    if (!info) {
      return absl::NotFoundError(
          absl::StrCat("Tool not found: ", it->second));
    }
    std::ostringstream result;
    result << "{\"name\": \"" << info->name << "\", "
           << "\"category\": \"" << info->category << "\", "
           << "\"description\": \"" << info->description << "\", "
           << "\"usage\": \"" << info->usage << "\", "
           << "\"requires_rom\": " << (info->requires_rom ? "true" : "false")
           << ", \"examples\": [";
    for (size_t i = 0; i < info->examples.size(); ++i) {
      result << "\"" << info->examples[i] << "\"";
      if (i < info->examples.size() - 1) result << ", ";
    }
    result << "]}\n";
    return result.str();
  }

  if (type == ToolCallType::kToolsSearch) {
    auto it = call.args.find("query");
    if (it == call.args.end()) {
      return absl::InvalidArgumentError(
          "tools-search requires 'query' argument");
    }
    auto matches = SearchTools(it->second);
    std::ostringstream result;
    result << "{\"query\": \"" << it->second << "\", \"matches\": [\n";
    for (size_t i = 0; i < matches.size(); ++i) {
      result << "  {\"name\": \"" << matches[i].name << "\", "
             << "\"category\": \"" << matches[i].category << "\", "
             << "\"description\": \"" << matches[i].description << "\"}";
      if (i < matches.size() - 1) result << ",";
      result << "\n";
    }
    result << "]}\n";
    return result.str();
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
  if (!rom_context_ && handler->RequiresRom()) {
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

std::vector<ToolDispatcher::ToolInfo> ToolDispatcher::GetAvailableTools()
    const {
  std::vector<ToolInfo> tools;

  // Meta-tools
  if (preferences_.meta_tools) {
    tools.push_back({"tools-list", "meta", "List all available tools",
                     "tools-list", {}, false});
    tools.push_back({"tools-describe", "meta",
                     "Get detailed information about a tool",
                     "tools-describe --name=<tool>", {}, false});
    tools.push_back({"tools-search", "meta", "Search tools by keyword",
                     "tools-search --query=<keyword>", {}, false});
  }

  // Resource commands
  if (preferences_.resources) {
    tools.push_back({"resource-list", "resource", "List resource labels",
                     "resource-list --type=<type>",
                     {"resource-list --type=dungeon"}, true});
    tools.push_back({"resource-search", "resource", "Search resource labels",
                     "resource-search --query=<query>",
                     {"resource-search --query=castle"}, true});
  }

  // Dungeon commands
  if (preferences_.dungeon) {
    tools.push_back({"dungeon-list-sprites", "dungeon",
                     "List sprites in a dungeon room",
                     "dungeon-list-sprites --room=<id>", {}, true});
    tools.push_back({"dungeon-describe-room", "dungeon", "Describe a room",
                     "dungeon-describe-room --room=<id>", {}, true});
    tools.push_back({"dungeon-list-objects", "dungeon", "List room objects",
                     "dungeon-list-objects --room=<id>", {}, true});
  }

  // Overworld commands
  if (preferences_.overworld) {
    tools.push_back({"overworld-describe-map", "overworld", "Describe a map",
                     "overworld-describe-map --map=<id>", {}, true});
    tools.push_back({"overworld-find-tile", "overworld", "Find tile locations",
                     "overworld-find-tile --tile=<id>", {}, true});
    tools.push_back({"overworld-list-sprites", "overworld", "List sprites",
                     "overworld-list-sprites --map=<id>", {}, true});
  }

  // Filesystem commands
  if (preferences_.filesystem) {
    tools.push_back({"filesystem-list", "filesystem", "List directory contents",
                     "filesystem-list --path=<path>", {}, false});
    tools.push_back({"filesystem-read", "filesystem", "Read file contents",
                     "filesystem-read --path=<path>", {}, false});
    tools.push_back({"filesystem-exists", "filesystem", "Check file existence",
                     "filesystem-exists --path=<path>", {}, false});
  }

  // Memory inspector
  if (preferences_.memory_inspector) {
    tools.push_back({"memory-analyze", "memory", "Analyze memory region",
                     "memory-analyze --address=<addr> --length=<len>", {},
                     true});
    tools.push_back({"memory-search", "memory", "Search for byte pattern",
                     "memory-search --pattern=<hex>", {}, true});
    tools.push_back({"memory-regions", "memory", "List known memory regions",
                     "memory-regions", {}, false});
  }

  // Test helper tools
  if (preferences_.test_helpers) {
    tools.push_back({"tools-helper-list", "tools", "List test helper tools",
                     "tools-helper-list", {}, false});
    tools.push_back({"tools-harness-state", "tools",
                     "Generate WRAM state for test harness",
                     "tools-harness-state --rom=<path> --output=<path>", {},
                     false});
    tools.push_back({"tools-extract-values", "tools",
                     "Extract vanilla ROM values",
                     "tools-extract-values --rom=<path>", {}, true});
    tools.push_back({"tools-extract-golden", "tools",
                     "Extract golden data for testing",
                     "tools-extract-golden --rom=<path> --output=<path>", {},
                     true});
    tools.push_back({"tools-patch-v3", "tools", "Create v3 patched ROM",
                     "tools-patch-v3 --rom=<input> --output=<output>", {},
                     true});
  }

  return tools;
}

std::optional<ToolDispatcher::ToolInfo> ToolDispatcher::GetToolInfo(
    const std::string& tool_name) const {
  auto tools = GetAvailableTools();
  for (const auto& tool : tools) {
    if (tool.name == tool_name) {
      return tool;
    }
  }
  return std::nullopt;
}

std::vector<ToolDispatcher::ToolInfo> ToolDispatcher::SearchTools(
    const std::string& query) const {
  std::vector<ToolInfo> matches;
  std::string lower_query = absl::AsciiStrToLower(query);

  auto tools = GetAvailableTools();
  for (const auto& tool : tools) {
    std::string lower_name = absl::AsciiStrToLower(tool.name);
    std::string lower_desc = absl::AsciiStrToLower(tool.description);
    std::string lower_category = absl::AsciiStrToLower(tool.category);

    if (lower_name.find(lower_query) != std::string::npos ||
        lower_desc.find(lower_query) != std::string::npos ||
        lower_category.find(lower_query) != std::string::npos) {
      matches.push_back(tool);
    }
  }

  return matches;
}

ToolDispatcher::BatchResult ToolDispatcher::DispatchBatch(
    const BatchToolCall& batch) {
  BatchResult result;
  result.results.resize(batch.calls.size());
  result.statuses.resize(batch.calls.size());

  auto start_time = std::chrono::high_resolution_clock::now();

  if (batch.parallel && batch.calls.size() > 1) {
    // Parallel execution using std::async
    std::vector<std::future<absl::StatusOr<std::string>>> futures;
    futures.reserve(batch.calls.size());

    for (const auto& call : batch.calls) {
      futures.push_back(std::async(std::launch::async, [this, &call]() {
        return this->Dispatch(call);
      }));
    }

    // Collect results
    for (size_t i = 0; i < futures.size(); ++i) {
      auto status_or = futures[i].get();
      if (status_or.ok()) {
        result.results[i] = std::move(status_or.value());
        result.statuses[i] = absl::OkStatus();
        result.successful_count++;
      } else {
        result.results[i] = "";
        result.statuses[i] = status_or.status();
        result.failed_count++;
      }
    }
  } else {
    // Sequential execution
    for (size_t i = 0; i < batch.calls.size(); ++i) {
      auto status_or = Dispatch(batch.calls[i]);
      if (status_or.ok()) {
        result.results[i] = std::move(status_or.value());
        result.statuses[i] = absl::OkStatus();
        result.successful_count++;
      } else {
        result.results[i] = "";
        result.statuses[i] = status_or.status();
        result.failed_count++;
      }
    }
  }

  auto end_time = std::chrono::high_resolution_clock::now();
  result.total_execution_time_ms =
      std::chrono::duration<double, std::milli>(end_time - start_time).count();

  return result;
}

}  // namespace agent
}  // namespace cli
}  // namespace yaze
