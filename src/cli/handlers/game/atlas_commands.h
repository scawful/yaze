#ifndef YAZE_SRC_CLI_HANDLERS_GAME_ATLAS_COMMANDS_H_
#define YAZE_SRC_CLI_HANDLERS_GAME_ATLAS_COMMANDS_H_

#include <string>

#include "cli/service/resources/command_handler.h"

namespace yaze::cli::handlers {

// Atlas map views, headless. Atlas P1: the dungeon model as JSON
// (--view dungeon --json). The overworld view (P2) and PNG output (--out, P3)
// are not implemented yet and fail with a clear error.
//
// Usage:
//   atlas-render --rom <sfc> --view dungeon --json <path>
//       [--project <.yaze>] [--annotations <file>[,<file>...]]
//       [--defaults vanilla|none] [--symbols <sym>] [--vanilla <rom>]
//
// --project opens the project with YazeProject::Open: the dungeon registry
// (Docs/Dev/Planning/dungeons.json, with the same fallbacks as the census),
// the hack manifest's warp tags and the [atlas] annotation files. A project
// with neither a registry nor [atlas] annotations is an error. Read-only: the
// ROM is never written, and --json may not name the ROM.
class AtlasRenderCommandHandler : public resources::CommandHandler {
 public:
  std::string GetName() const override { return "atlas-render"; }
  std::string GetDescription() const {
    return "Atlas map views: dungeon room model as JSON (overworld and PNG "
           "follow)";
  }
  std::string GetUsage() const override {
    return "atlas-render --rom <sfc> --view dungeon --json <path> "
           "[--project <.yaze>] [--annotations <file>[,<file>...]] "
           "[--defaults vanilla|none] [--symbols <sym>] [--vanilla <rom>]";
  }

  absl::Status ValidateArgs(const resources::ArgumentParser& parser) override;
  absl::Status Execute(Rom* rom, const resources::ArgumentParser& parser,
                       resources::OutputFormatter& formatter) override;
  absl::Status ExecuteWithContext(
      Rom* rom, const resources::ArgumentParser& parser,
      resources::OutputFormatter& formatter,
      const resources::CommandInvocationContext& invocation_context) override;

 private:
  std::string GetDefaultFormat() const override { return "json"; }
  std::string GetOutputTitle() const override { return "Atlas Render"; }
};

}  // namespace yaze::cli::handlers

#endif  // YAZE_SRC_CLI_HANDLERS_GAME_ATLAS_COMMANDS_H_
