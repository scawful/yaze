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
 * Lists each room's header links (stair slots, warp tags, holewarp, teleport
 * doors) from zelda3::CollectRoomLinks, the links the room census uses.
 * Each edge says whether the game can take it (`strong`); header bytes that
 * nothing uses are weak. Destinations past 0x127 go to `out_of_range`.
 * --dungeon keeps rooms the census assigns to that dungeon ID.
 */
class DungeonGraphCommandHandler : public resources::CommandHandler {
 public:
  std::string GetName() const { return "dungeon-graph"; }
  std::string GetDescription() const {
    return "Generate room connectivity graph from staircase and holewarp data";
  }
  std::string GetUsage() const {
    return "dungeon-graph --rom <path> [--room <room_id>] [--dungeon <id>] "
           "[--project <code folder|.yaze>] [--format <json|text>]";
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

  absl::Status ValidateArgs(const resources::ArgumentParser& parser) override {
    return parser.RequireArgs({"entrance"});
  }

  absl::Status Execute(Rom* rom, const resources::ArgumentParser& parser,
                       resources::OutputFormatter& formatter) override;
};

/**
 * @brief Command handler for auto-discovering dungeon rooms
 *
 * Starting from an entrance (read from the table the game reads, ZScream's
 * bank $0F copy when present), performs BFS over the header links the game
 * can take (stairs, warp tags, live holes, teleport doors). Weak links are
 * listed but not followed.
 */
class DungeonDiscoverCommandHandler : public resources::CommandHandler {
 public:
  std::string GetName() const override { return "dungeon-discover"; }
  std::string GetDescription() const {
    return "Auto-discover all rooms reachable from an entrance";
  }
  std::string GetUsage() const override {
    return "dungeon-discover --rom <path> --entrance <entrance_id> "
           "[--depth <max_depth>] [--project <code folder|.yaze>] "
           "[--format <json|text>]";
  }

  absl::Status ValidateArgs(const resources::ArgumentParser& parser) override {
    return parser.RequireArgs({"entrance"});
  }

  absl::Status Execute(Rom* rom, const resources::ArgumentParser& parser,
                       resources::OutputFormatter& formatter) override;
};

/**
 * @brief Full room connectivity graph including door edges
 *
 * Starting from an entrance, performs BFS over the room links the census
 * uses: mutual grid-neighbor doors (no wrap across rows or pages), teleport
 * doors (type 0x46: stair slot 4 east, 3 west), and stair/holewarp links the
 * game can take. Door edges include tile coordinates so the Python navigator
 * can teleport Link to a door tile and press the direction to trigger the
 * transition.
 *
 * Exit-type doors (FancyDungeonExit, CaveExit, etc.) are included in the
 * output but NOT followed during BFS (marked is_exit=true, to="exit").
 * Key-stair doors are listed with to="stairs"; their stair object is the
 * link.
 *
 * Usage:
 *   dungeon-room-graph --entrance=0x27 [--depth=50] [--same-blockset]
 *
 * --same-blockset: only follow doors to rooms with the same blockset as the
 *   starting room. Prevents cross-dungeon cascade on the shared ALTTP grid.
 */
class DungeonRoomGraphCommandHandler : public resources::CommandHandler {
 public:
  std::string GetName() const override { return "dungeon-room-graph"; }
  std::string GetDescription() const {
    return "Build full room graph (doors + staircases) from an entrance";
  }
  std::string GetUsage() const override {
    return "dungeon-room-graph --entrance <id> [--depth <max>] "
           "[--same-blockset] [--project <code folder|.yaze>]";
  }

  absl::Status ValidateArgs(const resources::ArgumentParser& parser) override {
    return parser.RequireArgs({"entrance"});
  }

  absl::Status Execute(Rom* rom, const resources::ArgumentParser& parser,
                       resources::OutputFormatter& formatter) override;
};

}  // namespace handlers
}  // namespace cli
}  // namespace yaze

#endif  // YAZE_SRC_CLI_HANDLERS_DUNGEON_GRAPH_COMMANDS_H_
