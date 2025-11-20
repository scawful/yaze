#ifndef YAZE_SRC_CLI_SERVICE_AGENT_TOOL_DISPATCHER_H_
#define YAZE_SRC_CLI_SERVICE_AGENT_TOOL_DISPATCHER_H_

#include <string>

#include "absl/status/statusor.h"
#include "cli/service/ai/common.h"

namespace yaze {

class Rom;

namespace cli {
namespace agent {

enum class ToolCallType {
  kUnknown,
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
  };

  ToolDispatcher() = default;

  // Execute a tool call and return the result as a string.
  absl::StatusOr<std::string> Dispatch(const ToolCall& tool_call);
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
