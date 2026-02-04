#ifndef YAZE_SRC_CLI_HANDLERS_DUNGEON_GROUP_COMMANDS_H_
#define YAZE_SRC_CLI_HANDLERS_DUNGEON_GROUP_COMMANDS_H_

#include <string>

#include "absl/status/status.h"
#include "cli/service/resources/command_handler.h"

namespace yaze {
namespace cli {
namespace handlers {

/**
 * @brief Command handler for organizing rooms by dungeon ID
 *
 * Groups dungeon rooms by their associated dungeon, showing start rooms,
 * boss rooms, and room counts. Useful for understanding dungeon structure
 * and verifying room assignments.
 */
class DungeonGroupCommandHandler : public resources::CommandHandler {
 public:
  std::string GetName() const { return "dungeon-group"; }
  std::string GetDescription() const {
    return "Organize and list rooms grouped by dungeon ID";
  }
  std::string GetUsage() const {
    return "dungeon-group --rom <path> [--dungeon <id>] [--list] "
           "[--format <json|text>]";
  }

  absl::Status ValidateArgs(
      const resources::ArgumentParser& /*parser*/) override {
    // No required args - lists all dungeons by default
    return absl::OkStatus();
  }

  absl::Status Execute(Rom* rom, const resources::ArgumentParser& parser,
                       resources::OutputFormatter& formatter) override;
};

}  // namespace handlers
}  // namespace cli
}  // namespace yaze

#endif  // YAZE_SRC_CLI_HANDLERS_DUNGEON_GROUP_COMMANDS_H_
