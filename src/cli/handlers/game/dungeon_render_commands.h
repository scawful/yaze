#ifndef YAZE_SRC_CLI_HANDLERS_GAME_DUNGEON_RENDER_COMMANDS_H_
#define YAZE_SRC_CLI_HANDLERS_GAME_DUNGEON_RENDER_COMMANDS_H_

#include "cli/service/resources/command_handler.h"

namespace yaze {
namespace cli {
namespace handlers {

// Render a dungeon room to a PNG file without requiring the HTTP server.
//
// Usage:
//   dungeon-render --room=<id> --output=<path.png>
//           [--overlays=collision,track,sprites,objects,grid,camera,all]
//           [--scale=<float>]
//
// Writes a PNG file to --output. Exits non-zero on failure.
// Designed for agentic workflows where a long-lived server is inconvenient.
class DungeonRenderCommandHandler : public resources::CommandHandler {
 public:
  std::string GetName() const override { return "dungeon-render"; }
  std::string GetUsage() const override {
    return "dungeon-render --room=<id> --output=<path.png> "
           "[--overlays=collision,track,sprites,objects,grid,camera,all] "
           "[--scale=<float>]";
  }

  absl::Status ValidateArgs(const resources::ArgumentParser& parser) override;
  absl::Status Execute(Rom* rom, const resources::ArgumentParser& parser,
                       resources::OutputFormatter& formatter) override;

 private:
  std::string GetDefaultFormat() const override { return "json"; }
  std::string GetOutputTitle() const override { return "Dungeon Render"; }
};

}  // namespace handlers
}  // namespace cli
}  // namespace yaze

#endif  // YAZE_SRC_CLI_HANDLERS_GAME_DUNGEON_RENDER_COMMANDS_H_
