#include "cli/handlers/command_handlers.h"

#include "cli/handlers/tools/resource_commands.h"
#include "cli/handlers/tools/gui_commands.h"
#ifdef YAZE_WITH_GRPC
#include "cli/handlers/tools/emulator_commands.h"
#endif
#include "cli/handlers/game/dungeon_commands.h"
#include "cli/handlers/game/overworld_commands.h"
#include "cli/handlers/game/message_commands.h"
#include "cli/handlers/game/dialogue_commands.h"
#include "cli/handlers/game/music_commands.h"
#include "cli/handlers/graphics/hex_commands.h"
#include "cli/handlers/graphics/palette_commands.h"
#include "cli/handlers/graphics/sprite_commands.h"

#include <memory>
#include <unordered_map>

namespace yaze {
namespace cli {
namespace handlers {

// Static command registry
namespace {
std::unordered_map<std::string, resources::CommandHandler*> g_command_registry;
}

std::vector<std::unique_ptr<resources::CommandHandler>> CreateCliCommandHandlers() {
  std::vector<std::unique_ptr<resources::CommandHandler>> handlers;
  
  // Graphics commands
  handlers.push_back(std::make_unique<HexReadCommandHandler>());
  handlers.push_back(std::make_unique<HexWriteCommandHandler>());
  handlers.push_back(std::make_unique<HexSearchCommandHandler>());
  
  // Palette commands
  handlers.push_back(std::make_unique<PaletteGetColorsCommandHandler>());
  handlers.push_back(std::make_unique<PaletteSetColorCommandHandler>());
  handlers.push_back(std::make_unique<PaletteAnalyzeCommandHandler>());
  
  // Sprite commands
  handlers.push_back(std::make_unique<SpriteListCommandHandler>());
  handlers.push_back(std::make_unique<SpritePropertiesCommandHandler>());
  handlers.push_back(std::make_unique<SpritePaletteCommandHandler>());
  
  // Music commands
  handlers.push_back(std::make_unique<MusicListCommandHandler>());
  handlers.push_back(std::make_unique<MusicInfoCommandHandler>());
  handlers.push_back(std::make_unique<MusicTracksCommandHandler>());
  
  // Dialogue commands
  handlers.push_back(std::make_unique<DialogueListCommandHandler>());
  handlers.push_back(std::make_unique<DialogueReadCommandHandler>());
  handlers.push_back(std::make_unique<DialogueSearchCommandHandler>());
  
  // Message commands
  handlers.push_back(std::make_unique<MessageListCommandHandler>());
  handlers.push_back(std::make_unique<MessageReadCommandHandler>());
  handlers.push_back(std::make_unique<MessageSearchCommandHandler>());
  
  return handlers;
}

#include "cli/handlers/agent/simple_chat_command.h"

std::vector<std::unique_ptr<resources::CommandHandler>> CreateAgentCommandHandlers() {
  std::vector<std::unique_ptr<resources::CommandHandler>> handlers;
  
  // Add simple-chat command handler
  handlers.push_back(std::make_unique<SimpleChatCommandHandler>());

  // Resource inspection tools
  handlers.push_back(std::make_unique<ResourceListCommandHandler>());
  handlers.push_back(std::make_unique<ResourceSearchCommandHandler>());
  
  // Dungeon inspection
  handlers.push_back(std::make_unique<DungeonListSpritesCommandHandler>());
  handlers.push_back(std::make_unique<DungeonDescribeRoomCommandHandler>());
  handlers.push_back(std::make_unique<DungeonExportRoomCommandHandler>());
  handlers.push_back(std::make_unique<DungeonListObjectsCommandHandler>());
  handlers.push_back(std::make_unique<DungeonGetRoomTilesCommandHandler>());
  handlers.push_back(std::make_unique<DungeonSetRoomPropertyCommandHandler>());
  
  // Overworld inspection
  handlers.push_back(std::make_unique<OverworldFindTileCommandHandler>());
  handlers.push_back(std::make_unique<OverworldDescribeMapCommandHandler>());
  handlers.push_back(std::make_unique<OverworldListWarpsCommandHandler>());
  handlers.push_back(std::make_unique<OverworldListSpritesCommandHandler>());
  handlers.push_back(std::make_unique<OverworldGetEntranceCommandHandler>());
  handlers.push_back(std::make_unique<OverworldTileStatsCommandHandler>());
  
  // GUI automation tools
  handlers.push_back(std::make_unique<GuiPlaceTileCommandHandler>());
  handlers.push_back(std::make_unique<GuiClickCommandHandler>());
  handlers.push_back(std::make_unique<GuiDiscoverToolCommandHandler>());
  handlers.push_back(std::make_unique<GuiScreenshotCommandHandler>());
  
  // Emulator & debugger commands
#ifdef YAZE_WITH_GRPC
  handlers.push_back(std::make_unique<EmulatorStepCommandHandler>());
  handlers.push_back(std::make_unique<EmulatorRunCommandHandler>());
  handlers.push_back(std::make_unique<EmulatorPauseCommandHandler>());
  handlers.push_back(std::make_unique<EmulatorResetCommandHandler>());
  handlers.push_back(std::make_unique<EmulatorGetStateCommandHandler>());
  handlers.push_back(std::make_unique<EmulatorSetBreakpointCommandHandler>());
  handlers.push_back(std::make_unique<EmulatorClearBreakpointCommandHandler>());
  handlers.push_back(std::make_unique<EmulatorListBreakpointsCommandHandler>());
  handlers.push_back(std::make_unique<EmulatorReadMemoryCommandHandler>());
  handlers.push_back(std::make_unique<EmulatorWriteMemoryCommandHandler>());
  handlers.push_back(std::make_unique<EmulatorGetRegistersCommandHandler>());
  handlers.push_back(std::make_unique<EmulatorGetMetricsCommandHandler>());
#endif
  
  return handlers;
}

std::vector<std::unique_ptr<resources::CommandHandler>> CreateAllCommandHandlers() {
  std::vector<std::unique_ptr<resources::CommandHandler>> handlers;
  
  // Add CLI handlers
  auto cli_handlers = CreateCliCommandHandlers();
  for (auto& handler : cli_handlers) {
    handlers.push_back(std::move(handler));
  }
  
  // Add agent handlers
  auto agent_handlers = CreateAgentCommandHandlers();
  for (auto& handler : agent_handlers) {
    handlers.push_back(std::move(handler));
  }
  
  return handlers;
}

resources::CommandHandler* GetCommandHandler(const std::string& name) {
  auto it = g_command_registry.find(name);
  if (it != g_command_registry.end()) {
    return it->second;
  }
  return nullptr;
}

}  // namespace handlers
}  // namespace cli
}  // namespace yaze

