#ifndef YAZE_CLI_HANDLERS_GAME_TERRAIN_CATALOG_COMMANDS_H_
#define YAZE_CLI_HANDLERS_GAME_TERRAIN_CATALOG_COMMANDS_H_
#include "cli/service/resources/command_handler.h"

namespace yaze::cli::handlers {
class OverworldTerrainCatalogCommandHandler : public resources::CommandHandler {
 public:
  std::string GetName() const override { return "overworld-terrain-catalog"; }
  std::string GetDescription() const {
    return "Export a read-only area tile inventory with project terrain labels";
  }
  std::string GetUsage() const override {
    return "overworld-terrain-catalog --map <hex> --out <new.json> "
           "[--project-file <project.yaze>] [--catalog <labels.json>]";
  }
  absl::Status ValidateArgs(const resources::ArgumentParser& parser) override;
  absl::Status Execute(Rom* rom, const resources::ArgumentParser& parser,
                       resources::OutputFormatter& formatter) override;
};
}  // namespace yaze::cli::handlers
#endif
