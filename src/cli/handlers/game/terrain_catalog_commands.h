#ifndef YAZE_CLI_HANDLERS_GAME_TERRAIN_CATALOG_COMMANDS_H_
#define YAZE_CLI_HANDLERS_GAME_TERRAIN_CATALOG_COMMANDS_H_
#include "cli/service/resources/command_handler.h"

namespace yaze::cli::handlers {
// With a project (--project-context or --project-file), the project's
// [files] overworld_graphics_preview transfers are composed over the ordinary
// sheets the way the editor and overworld-render draw the area;
// --graphics-preview=off exports the ROM's sheets only.
class OverworldTerrainCatalogCommandHandler : public resources::CommandHandler {
 public:
  std::string GetName() const override { return "overworld-terrain-catalog"; }
  std::string GetDescription() const {
    return "Export a read-only area tile inventory with project terrain labels";
  }
  std::string GetUsage() const override {
    return "overworld-terrain-catalog --map <hex> --out <new.json> "
           "[--project-context <project.yaze> | --project-file <project.yaze>] "
           "[--catalog <labels.json>] "
           "[--graphics-preview=on|off|<manifest.json>]";
  }
  absl::Status ValidateArgs(const resources::ArgumentParser& parser) override;
  absl::Status Execute(Rom* rom, const resources::ArgumentParser& parser,
                       resources::OutputFormatter& formatter) override;
};
}  // namespace yaze::cli::handlers
#endif
