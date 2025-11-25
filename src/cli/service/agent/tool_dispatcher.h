#ifndef YAZE_SRC_CLI_SERVICE_AGENT_TOOL_DISPATCHER_H_
#define YAZE_SRC_CLI_SERVICE_AGENT_TOOL_DISPATCHER_H_

#include <optional>
#include <string>
#include <vector>

#include "absl/status/statusor.h"
#include "cli/service/ai/common.h"

namespace yaze {

class Rom;

namespace cli {
namespace agent {

enum class ToolCallType {
  kUnknown,
  // Meta-tools for discoverability
  kToolsList,
  kToolsDescribe,
  kToolsSearch,
  // Resources
  kResourceList,
  kResourceSearch,
  // Dungeon
  kDungeonListSprites,
  kDungeonDescribeRoom,
  kDungeonExportRoom,
  kDungeonListObjects,
  kDungeonGetRoomTiles,
  kDungeonSetRoomProperty,
  // Overworld
  kOverworldFindTile,
  kOverworldDescribeMap,
  kOverworldListWarps,
  kOverworldListSprites,
  kOverworldGetEntrance,
  kOverworldTileStats,
  // Messages & Dialogue
  kMessageList,
  kMessageRead,
  kMessageSearch,
  kDialogueList,
  kDialogueRead,
  kDialogueSearch,
  // GUI Automation
  kGuiPlaceTile,
  kGuiClick,
  kGuiDiscover,
  kGuiScreenshot,
  // Music
  kMusicList,
  kMusicInfo,
  kMusicTracks,
  // Sprites
  kSpriteList,
  kSpriteProperties,
  kSpritePalette,
  // Emulator & Debugger
  kEmulatorStep,
  kEmulatorRun,
  kEmulatorPause,
  kEmulatorReset,
  kEmulatorGetState,
  kEmulatorSetBreakpoint,
  kEmulatorClearBreakpoint,
  kEmulatorListBreakpoints,
  kEmulatorReadMemory,
  kEmulatorWriteMemory,
  kEmulatorGetRegisters,
  kEmulatorGetMetrics,
  // Filesystem
  kFilesystemList,
  kFilesystemRead,
  kFilesystemExists,
  kFilesystemInfo,
  // Build Tools
  kBuildConfigure,
  kBuildCompile,
  kBuildTest,
  kBuildStatus,
  // Memory Inspector Tools
  kMemoryAnalyze,
  kMemorySearch,
  kMemoryCompare,
  kMemoryCheck,
  kMemoryRegions,
  // Test Helper Tools
  kToolsHelperList,
  kToolsHarnessState,
  kToolsExtractValues,
  kToolsExtractGolden,
  kToolsPatchV3,
};

class ToolDispatcher {
 public:
  struct ToolPreferences {
    bool resources = true;
    bool dungeon = true;
    bool overworld = true;
    bool messages = true;
    bool dialogue = true;
    bool gui = true;
    bool music = true;
    bool sprite = true;
#ifdef YAZE_WITH_GRPC
    bool emulator = true;
#else
    bool emulator = false;
#endif
    bool filesystem = true;
    bool build = true;
    bool memory_inspector = true;
    bool test_helpers = true;
    bool meta_tools = true;  // tools-list, tools-describe, tools-search
  };

  /**
   * @brief Tool information for discoverability
   */
  struct ToolInfo {
    std::string name;
    std::string category;
    std::string description;
    std::string usage;
    std::vector<std::string> examples;
    bool requires_rom;
  };

  /**
   * @brief Get list of all available tools
   */
  std::vector<ToolInfo> GetAvailableTools() const;

  /**
   * @brief Get detailed information about a specific tool
   */
  std::optional<ToolInfo> GetToolInfo(const std::string& tool_name) const;

  /**
   * @brief Search tools by keyword
   */
  std::vector<ToolInfo> SearchTools(const std::string& query) const;

  /**
   * @brief Batch tool call request
   */
  struct BatchToolCall {
    std::vector<ToolCall> calls;
    bool parallel = false;  // If true, execute independent calls in parallel
  };

  /**
   * @brief Result of a batch tool call
   */
  struct BatchResult {
    std::vector<std::string> results;
    std::vector<absl::Status> statuses;
    double total_execution_time_ms = 0.0;
    size_t successful_count = 0;
    size_t failed_count = 0;
  };

  /**
   * @brief Execute multiple tool calls in a batch
   *
   * When parallel=false, calls are executed sequentially.
   * When parallel=true, calls are executed concurrently (if no dependencies).
   */
  BatchResult DispatchBatch(const BatchToolCall& batch);

  ToolDispatcher() = default;

  // Execute a tool call and return the result as a string.
  absl::StatusOr<std::string> Dispatch(const ::yaze::cli::ToolCall& tool_call);
  // Provide a ROM context for tool calls that require ROM access.
  void SetRomContext(Rom* rom) { rom_context_ = rom; }
  void SetToolPreferences(const ToolPreferences& prefs) {
    preferences_ = prefs;
  }
  const ToolPreferences& preferences() const { return preferences_; }

 private:
  bool IsToolEnabled(ToolCallType type) const;

  Rom* rom_context_ = nullptr;
  ToolPreferences preferences_;
};

}  // namespace agent
}  // namespace cli
}  // namespace yaze

#endif  // YAZE_SRC_CLI_SERVICE_AGENT_TOOL_DISPATCHER_H_
