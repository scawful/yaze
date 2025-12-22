#include "cli/handlers/command_handlers.h"

#include <memory>
#include <vector>

#include "cli/handlers/game/dungeon_commands.h"
#include "cli/handlers/game/overworld_commands.h"
#include "cli/handlers/graphics/hex_commands.h"
#include "cli/handlers/graphics/palette_commands.h"
#include "cli/handlers/rom/rom_commands.h"
#include "cli/handlers/tools/resource_commands.h"

namespace yaze {
namespace cli {
namespace handlers {

std::vector<std::unique_ptr<resources::CommandHandler>>
CreateCliCommandHandlers() {
  std::vector<std::unique_ptr<resources::CommandHandler>> handlers;

  // Graphics commands (supported in browser)
  handlers.push_back(std::make_unique<HexReadCommandHandler>());
  handlers.push_back(std::make_unique<HexWriteCommandHandler>());
  handlers.push_back(std::make_unique<HexSearchCommandHandler>());

  // Palette commands (supported in browser)
  handlers.push_back(std::make_unique<PaletteGetColorsCommandHandler>());
  handlers.push_back(std::make_unique<PaletteSetColorCommandHandler>());
  handlers.push_back(std::make_unique<PaletteAnalyzeCommandHandler>());

  // Dungeon commands
  handlers.push_back(std::make_unique<DungeonListSpritesCommandHandler>());
  handlers.push_back(std::make_unique<DungeonDescribeRoomCommandHandler>());
  handlers.push_back(std::make_unique<DungeonExportRoomCommandHandler>());
  handlers.push_back(std::make_unique<DungeonListObjectsCommandHandler>());
  handlers.push_back(std::make_unique<DungeonGetRoomTilesCommandHandler>());
  handlers.push_back(std::make_unique<DungeonSetRoomPropertyCommandHandler>());

  // Overworld commands
  handlers.push_back(std::make_unique<OverworldFindTileCommandHandler>());
  handlers.push_back(std::make_unique<OverworldDescribeMapCommandHandler>());
  handlers.push_back(std::make_unique<OverworldListWarpsCommandHandler>());
  handlers.push_back(std::make_unique<OverworldListSpritesCommandHandler>());
  handlers.push_back(std::make_unique<OverworldGetEntranceCommandHandler>());
  handlers.push_back(std::make_unique<OverworldTileStatsCommandHandler>());

  // Resource commands
  handlers.push_back(std::make_unique<ResourceListCommandHandler>());
  handlers.push_back(std::make_unique<ResourceSearchCommandHandler>());

  // ROM commands
  handlers.push_back(std::make_unique<RomInfoCommandHandler>());
  handlers.push_back(std::make_unique<RomValidateCommandHandler>());
  handlers.push_back(std::make_unique<RomDiffCommandHandler>());
  
  return handlers;
}

std::vector<std::unique_ptr<resources::CommandHandler>>
CreateAgentCommandHandlers() {
  std::vector<std::unique_ptr<resources::CommandHandler>> handlers;

  // Note: Todo and other heavy agent commands are currently disabled in browser build.
  // The todo commands use a function-based API (HandleTodoCommand) rather than
  // CommandHandler classes, and are handled through the terminal bridge directly.
  // Chat and other complex agent commands are also disabled to avoid complex
  // dependency chains (curl, threads, etc).

  return handlers;
}

std::vector<std::unique_ptr<resources::CommandHandler>>
CreateAllCommandHandlers() {
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

}  // namespace handlers
}  // namespace cli
}  // namespace yaze

