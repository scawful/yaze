#ifndef YAZE_SRC_CLI_HANDLERS_GAME_DUNGEON_CENSUS_COMMANDS_H_
#define YAZE_SRC_CLI_HANDLERS_GAME_DUNGEON_CENSUS_COMMANDS_H_

#include <string>

#include "absl/status/status.h"
#include "cli/service/resources/command_handler.h"

namespace yaze::cli::handlers {

/**
 * @brief Room census: owner, FREE / RECLAIMABLE / IN USE status and reasons
 * for rooms 0x000-0x127. Backed by zelda3::ComputeRoomCensus, the same model
 * as the Room Matrix "Census" overlay.
 */
class DungeonRoomCensusCommandHandler : public resources::CommandHandler {
 public:
  std::string GetName() const override { return "dungeon-room-census"; }
  std::string GetDescription() const {
    return "Classify every dungeon room as free, reclaimable or in use, with "
           "owner and reasons";
  }
  std::string GetUsage() const override {
    return "dungeon-room-census --rom <path> [--project <code folder|.yaze>] "
           "[--vanilla <vanilla rom>] [--room <hex>] [--status "
           "<free|reclaimable|in_use>] [--format <json|table>] "
           "[--emit-vanilla-fingerprints <out.inc>]";
  }

  absl::Status ValidateArgs(const resources::ArgumentParser& parser) override;

  absl::Status Execute(Rom* rom, const resources::ArgumentParser& parser,
                       resources::OutputFormatter& formatter) override;
};

}  // namespace yaze::cli::handlers

#endif  // YAZE_SRC_CLI_HANDLERS_GAME_DUNGEON_CENSUS_COMMANDS_H_
