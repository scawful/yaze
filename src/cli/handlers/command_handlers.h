#ifndef YAZE_SRC_CLI_HANDLERS_AGENT_COMMAND_HANDLERS_H_
#define YAZE_SRC_CLI_HANDLERS_AGENT_COMMAND_HANDLERS_H_

#include <memory>
#include <string>
#include <vector>

#include "cli/service/resources/command_handler.h"

#include "cli/handlers/agent/simple_chat_command.h"

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

/**
 * @brief Factory function to create all CLI-level command handlers
 * 
 * @return Vector of unique pointers to command handler instances
 */
std::vector<std::unique_ptr<resources::CommandHandler>>
CreateCliCommandHandlers();

/**
 * @brief Factory function to create all agent-specific command handlers
 * 
 * @return Vector of unique pointers to command handler instances
 */
std::vector<std::unique_ptr<resources::CommandHandler>>
CreateAgentCommandHandlers();

/**
 * @brief Factory function to create all command handlers (CLI + agent)
 * 
 * @return Vector of unique pointers to command handler instances
 */
std::vector<std::unique_ptr<resources::CommandHandler>>
CreateAllCommandHandlers();

}  // namespace handlers
}  // namespace cli
}  // namespace yaze

#endif  // YAZE_SRC_CLI_HANDLERS_AGENT_COMMAND_HANDLERS_H_
