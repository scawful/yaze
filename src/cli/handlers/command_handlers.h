#ifndef YAZE_SRC_CLI_HANDLERS_AGENT_COMMAND_HANDLERS_H_
#define YAZE_SRC_CLI_HANDLERS_AGENT_COMMAND_HANDLERS_H_

#include <memory>
#include <string>
#include <vector>

#include "cli/service/resources/command_handler.h"

namespace yaze {
namespace cli {
namespace handlers {

// Forward declarations for command handler classes
class HexReadCommandHandler;
class HexWriteCommandHandler;
class HexSearchCommandHandler;

class PaletteGetColorsCommandHandler;
class PaletteSetColorCommandHandler;
class PaletteAnalyzeCommandHandler;

class SpriteListCommandHandler;
class SpritePropertiesCommandHandler;
class SpritePaletteCommandHandler;

class MusicListCommandHandler;
class MusicInfoCommandHandler;
class MusicTracksCommandHandler;

class DialogueListCommandHandler;
class DialogueReadCommandHandler;
class DialogueSearchCommandHandler;

class MessageListCommandHandler;
class MessageReadCommandHandler;
class MessageSearchCommandHandler;

class ResourceListCommandHandler;
class ResourceSearchCommandHandler;

class DungeonListSpritesCommandHandler;
class DungeonDescribeRoomCommandHandler;
class DungeonGetEntranceCommandHandler;
class DungeonExportRoomCommandHandler;
class DungeonListObjectsCommandHandler;
class DungeonGetRoomTilesCommandHandler;
class DungeonSetRoomPropertyCommandHandler;

class OverworldFindTileCommandHandler;
class OverworldDescribeMapCommandHandler;
class OverworldListWarpsCommandHandler;
class OverworldListSpritesCommandHandler;
class OverworldGetEntranceCommandHandler;
class OverworldTileStatsCommandHandler;

class GuiPlaceTileCommandHandler;
class GuiClickCommandHandler;
class GuiDiscoverToolCommandHandler;
class GuiScreenshotCommandHandler;

class EmulatorStepCommandHandler;
class EmulatorRunCommandHandler;
class EmulatorPauseCommandHandler;
class EmulatorResetCommandHandler;
class EmulatorGetStateCommandHandler;
class EmulatorSetBreakpointCommandHandler;
class EmulatorClearBreakpointCommandHandler;
class EmulatorListBreakpointsCommandHandler;
class EmulatorReadMemoryCommandHandler;
class EmulatorWriteMemoryCommandHandler;
class EmulatorGetRegistersCommandHandler;
class EmulatorGetMetricsCommandHandler;

// Test helper tools
class ToolsHarnessStateCommandHandler;
class ToolsExtractValuesCommandHandler;
class ToolsExtractGoldenCommandHandler;
class ToolsPatchV3CommandHandler;
class ToolsListCommandHandler;

/**
 * @brief Factory function to create all CLI-level command handlers
 *
 * @return Vector of unique pointers to command handler instances
 */
std::vector<std::unique_ptr<resources::CommandHandler>>
CreateCliCommandHandlers();

}  // namespace handlers
}  // namespace cli
}  // namespace yaze

#endif  // YAZE_SRC_CLI_HANDLERS_AGENT_COMMAND_HANDLERS_H_
