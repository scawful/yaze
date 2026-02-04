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

/**
 * @brief Command handler for reading entrance table data
 *
 * Reads the entrance table at ROM address $14813 and returns comprehensive
 * entrance data including room ID, position, camera settings, and dungeon ID.
 * This is an alias for dungeon-get-entrance with a simpler name.
 */
class EntranceInfoCommandHandler : public resources::CommandHandler {
 public:
  std::string GetName() const override { return "entrance-info"; }
  std::string GetDescription() const {
    return "Get entrance table data for an entrance ID";
  }
  std::string GetUsage() const override {
    return "entrance-info --rom <path> --entrance <entrance_id> [--spawn] "
           "[--format <json|text>]";
  }

  absl::Status ValidateArgs(
      const resources::ArgumentParser& parser) override {
    return parser.RequireArgs({"entrance"});
  }

  absl::Status Execute(Rom* rom, const resources::ArgumentParser& parser,
                       resources::OutputFormatter& formatter) override;
};

/**
 * @brief Command handler for auto-discovering dungeon rooms
 *
 * Starting from an entrance ID, performs BFS/DFS through stair, holewarp,
 * and door connections to discover all reachable rooms. Returns the room
 * list and connection graph.
 */
class DungeonDiscoverCommandHandler : public resources::CommandHandler {
 public:
  std::string GetName() const override { return "dungeon-discover"; }
  std::string GetDescription() const {
    return "Auto-discover all rooms reachable from an entrance";
  }
  std::string GetUsage() const override {
    return "dungeon-discover --rom <path> --entrance <entrance_id> "
           "[--depth <max_depth>] [--format <json|text>]";
  }

  absl::Status ValidateArgs(
      const resources::ArgumentParser& parser) override {
    return parser.RequireArgs({"entrance"});
  }

  absl::Status Execute(Rom* rom, const resources::ArgumentParser& parser,
                       resources::OutputFormatter& formatter) override;
};

}  // namespace handlers
}  // namespace cli
}  // namespace yaze

#endif  // YAZE_SRC_CLI_HANDLERS_DUNGEON_GRAPH_COMMANDS_H_
