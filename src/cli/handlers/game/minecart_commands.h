#ifndef YAZE_SRC_CLI_HANDLERS_GAME_MINECART_COMMANDS_H_
#define YAZE_SRC_CLI_HANDLERS_GAME_MINECART_COMMANDS_H_

#include "cli/service/resources/command_handler.h"

namespace yaze {
namespace cli {
namespace handlers {

class DungeonMinecartAuditCommandHandler : public resources::CommandHandler {
 public:
  std::string GetName() const override { return "dungeon-minecart-audit"; }
  std::string GetDescription() const {
    return "Audit minecart-related objects/sprites/collision in dungeon rooms";
  }
  std::string GetUsage() const override {
    return "dungeon-minecart-audit [--room <room_id> | --rooms <hex,hex,...> | "
           "--all] [--only-issues] [--only-matches] [--include-track-objects] "
           "[--track-object-id <hex>] [--minecart-sprite-id <hex>] "
           "[--format <json|text>]";
  }

  absl::Status ValidateArgs(const resources::ArgumentParser& parser) override;

  absl::Status Execute(Rom* rom, const resources::ArgumentParser& parser,
                       resources::OutputFormatter& formatter) override;
};

// Enumerate all track tile positions and types from a room's custom collision
// data. Outputs a tile list with (x, y, value, type, category) for every
// non-zero track tile, plus a bounded ASCII grid for spatial reasoning.
class DungeonMinecartMapCommandHandler : public resources::CommandHandler {
 public:
  std::string GetName() const override { return "dungeon-minecart-map"; }
  std::string GetDescription() const {
    return "Enumerate track tile positions and types from custom collision "
           "data";
  }
  std::string GetUsage() const override {
    return "dungeon-minecart-map --room <hex> [--format <json|text>]";
  }

  absl::Status ValidateArgs(const resources::ArgumentParser& parser) override {
    return parser.RequireArgs({"room"});
  }

  absl::Status Execute(Rom* rom, const resources::ArgumentParser& parser,
                       resources::OutputFormatter& formatter) override;
};

}  // namespace handlers
}  // namespace cli
}  // namespace yaze

#endif  // YAZE_SRC_CLI_HANDLERS_GAME_MINECART_COMMANDS_H_
