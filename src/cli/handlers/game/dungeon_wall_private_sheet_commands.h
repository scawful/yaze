#ifndef YAZE_CLI_HANDLERS_GAME_DUNGEON_WALL_PRIVATE_SHEET_COMMANDS_H_
#define YAZE_CLI_HANDLERS_GAME_DUNGEON_WALL_PRIVATE_SHEET_COMMANDS_H_

#include "cli/service/resources/command_handler.h"

namespace yaze::cli::handlers {

// Source-mode WSP1 private-stream admission for room 54. Resolves kit+3bpp
// under explicit context, optionally dry-runs OBJECT_PLAN_54 against asset
// pins, and never writes ROM/project/global sheet tables.
class DungeonWallPrivateSheetCommandHandler : public resources::CommandHandler {
 public:
  std::string GetName() const override { return "dungeon-wall-private-sheet"; }
  std::string GetUsage() const override {
    return "dungeon-wall-private-sheet --kit=<prison_armory.kit.json> "
           "--sheet=<prison_armory.slot3.3bpp> --room=<id> --profile=<id> "
           "--blockset=<id> [--dungeon=<id>] [--outdoor] "
           "[--binding-contract=<BINDING_CONTRACT*.json>] "
           "[--object-plan=<OBJECT_PLAN_54.json>]";
  }
  Descriptor Describe() const override;
  absl::Status ValidateArgs(const resources::ArgumentParser& parser) override;
  absl::Status Execute(Rom* rom, const resources::ArgumentParser& parser,
                       resources::OutputFormatter& formatter) override;

 private:
  std::string GetDefaultFormat() const override { return "json"; }
  std::string GetOutputTitle() const override {
    return "Dungeon Wall Private Sheet";
  }
};

}  // namespace yaze::cli::handlers

#endif  // YAZE_CLI_HANDLERS_GAME_DUNGEON_WALL_PRIVATE_SHEET_COMMANDS_H_
