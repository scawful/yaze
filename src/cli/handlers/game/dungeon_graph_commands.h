#ifndef YAZE_SRC_CLI_HANDLERS_DUNGEON_GRAPH_COMMANDS_H_
#define YAZE_SRC_CLI_HANDLERS_DUNGEON_GRAPH_COMMANDS_H_

#include <string>

#include "absl/status/status.h"
#include "cli/service/resources/command_handler.h"

namespace yaze {
namespace cli {
namespace handlers {

/**
 * @brief Command handler for generating a room connectivity graph
 *
 * Builds a graph representation of dungeon room connections via
 * staircases and hole warps. Useful for analyzing dungeon structure
 * and verifying connectivity.
 */
class DungeonGraphCommandHandler : public resources::CommandHandler {
 public:
  std::string GetName() const { return "dungeon-graph"; }
  std::string GetDescription() const {
    return "Generate room connectivity graph from staircase and holewarp data";
  }
  std::string GetUsage() const {
    return "dungeon-graph --rom <path> [--room <room_id>] [--dungeon <id>] "
           "[--format <json|text>]";
  }

  absl::Status ValidateArgs(
      const resources::ArgumentParser& /*parser*/) override {
    // No required args - can scan all rooms if none specified
    return absl::OkStatus();
  }

  absl::Status Execute(Rom* rom, const resources::ArgumentParser& parser,
                       resources::OutputFormatter& formatter) override;
};

}  // namespace handlers
}  // namespace cli
}  // namespace yaze

#endif  // YAZE_SRC_CLI_HANDLERS_DUNGEON_GRAPH_COMMANDS_H_
