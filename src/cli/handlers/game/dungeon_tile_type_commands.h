#ifndef YAZE_SRC_CLI_HANDLERS_GAME_DUNGEON_TILE_TYPE_COMMANDS_H_
#define YAZE_SRC_CLI_HANDLERS_GAME_DUNGEON_TILE_TYPE_COMMANDS_H_

#include "cli/service/resources/command_handler.h"

namespace yaze {
namespace cli {
namespace handlers {

// Prints a dungeon room's vanilla tile attribute ("tile type") maps: COLMAPA
// ($7F2000, from BG1) and COLMAPB ($7F3000, from BG2), 64x64 each, as
// Underworld_LoadAttributeTable builds them when the room loads on a new
// file. Pits are type 0x20. The ZScream custom collision map ($128090) is a
// separate table and is reported only as a count; see
// dungeon-list-custom-collision.
//
// Usage:
//   dungeon-tile-types --room <hex> [--layer upper|lower|both]
//       [--type <hex>] [--no-rows] [--format <json|text>]
class DungeonTileTypesCommandHandler : public resources::CommandHandler {
 public:
  std::string GetName() const override { return "dungeon-tile-types"; }
  std::string GetDescription() const {
    return "Vanilla collision tile types ($7F2000/$7F3000) for a dungeon room";
  }
  std::string GetUsage() const override {
    return "dungeon-tile-types --room <hex> [--layer upper|lower|both] "
           "[--type <hex>] [--no-rows] [--format <json|text>]";
  }

  absl::Status ValidateArgs(const resources::ArgumentParser& parser) override;
  absl::Status Execute(Rom* rom, const resources::ArgumentParser& parser,
                       resources::OutputFormatter& formatter) override;

 private:
  std::string GetDefaultFormat() const override { return "json"; }
  std::string GetOutputTitle() const override { return "Dungeon Tile Types"; }
};

}  // namespace handlers
}  // namespace cli
}  // namespace yaze

#endif  // YAZE_SRC_CLI_HANDLERS_GAME_DUNGEON_TILE_TYPE_COMMANDS_H_
