#ifndef YAZE_SRC_CLI_HANDLERS_DUNGEON_MAP_COMMANDS_H_
#define YAZE_SRC_CLI_HANDLERS_DUNGEON_MAP_COMMANDS_H_

#include <string>

#include "absl/status/status.h"
#include "cli/service/resources/command_handler.h"

namespace yaze {
namespace cli {
namespace handlers {

/**
 * @brief Command handler for ASCII room visualization
 *
 * Generates an ASCII representation of a dungeon room showing
 * walls, floors, pits, doors, and other tile objects. Useful for
 * quick visual inspection of room layouts.
 */
class DungeonMapCommandHandler : public resources::CommandHandler {
 public:
  std::string GetName() const { return "dungeon-map"; }
  std::string GetDescription() const {
    return "Generate ASCII visualization of a dungeon room";
  }
  std::string GetUsage() const {
    return "dungeon-map --rom <path> --room <room_id> [--layer <0|1|2>] "
           "[--format <ascii|json>]";
  }

  absl::Status ValidateArgs(
      const resources::ArgumentParser& parser) override {
    return parser.RequireArgs({"room"});
  }

  absl::Status Execute(Rom* rom, const resources::ArgumentParser& parser,
                       resources::OutputFormatter& formatter) override;
};

}  // namespace handlers
}  // namespace cli
}  // namespace yaze

#endif  // YAZE_SRC_CLI_HANDLERS_DUNGEON_MAP_COMMANDS_H_
