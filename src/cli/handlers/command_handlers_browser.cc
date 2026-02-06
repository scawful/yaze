#include "cli/handlers/command_handlers.h"

#include <memory>
#include <vector>

#include "cli/handlers/game/dungeon_collision_commands.h"
#include "cli/handlers/game/dungeon_commands.h"
#include "cli/handlers/game/minecart_commands.h"
#include "cli/handlers/game/overworld_commands.h"
#include "cli/handlers/game/overworld_graph_commands.h"
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
  handlers.push_back(std::make_unique<DungeonListChestsCommandHandler>());
  handlers.push_back(std::make_unique<DungeonGetEntranceCommandHandler>());
  handlers.push_back(std::make_unique<DungeonExportRoomCommandHandler>());
  handlers.push_back(std::make_unique<DungeonListObjectsCommandHandler>());
  handlers.push_back(std::make_unique<DungeonListCustomCollisionCommandHandler>());
  handlers.push_back(std::make_unique<DungeonGetRoomTilesCommandHandler>());
  handlers.push_back(std::make_unique<DungeonSetRoomPropertyCommandHandler>());
  handlers.push_back(std::make_unique<DungeonMinecartAuditCommandHandler>());

  // Overworld commands
  handlers.push_back(std::make_unique<OverworldFindTileCommandHandler>());
  handlers.push_back(std::make_unique<OverworldDescribeMapCommandHandler>());
  handlers.push_back(std::make_unique<OverworldListWarpsCommandHandler>());
  handlers.push_back(std::make_unique<OverworldListSpritesCommandHandler>());
  handlers.push_back(std::make_unique<OverworldListItemsCommandHandler>());
  handlers.push_back(std::make_unique<OverworldGetEntranceCommandHandler>());
  handlers.push_back(std::make_unique<OverworldTileStatsCommandHandler>());
  handlers.push_back(std::make_unique<OverworldExportGraphCommandHandler>());

  // Resource commands
  handlers.push_back(std::make_unique<ResourceListCommandHandler>());
  handlers.push_back(std::make_unique<ResourceSearchCommandHandler>());

  // ROM commands
  handlers.push_back(std::make_unique<RomInfoCommandHandler>());
  handlers.push_back(std::make_unique<RomValidateCommandHandler>());
  handlers.push_back(std::make_unique<RomDiffCommandHandler>());

  return handlers;
}

}  // namespace handlers
}  // namespace cli
}  // namespace yaze
