#ifndef YAZE_SRC_CLI_HANDLERS_GAME_OVERWORLD_RENDER_COMMANDS_H_
#define YAZE_SRC_CLI_HANDLERS_GAME_OVERWORLD_RENDER_COMMANDS_H_

#include "cli/service/resources/command_handler.h"

namespace yaze {
namespace cli {
namespace handlers {

// Render the overworld area containing a screen to a PNG file from current
// ROM data. A child screen of a large/wide/tall area resolves to its parent
// and the whole area is rendered.
//
// Usage:
//   overworld-render --screen=<hex> --out=<path.png>
//       [--overlays=sprites,entrances,exits,holes,items,grid,all]
//       [--phase=0|1|2] [--scale=<float>]
class OverworldRenderCommandHandler : public resources::CommandHandler {
 public:
  std::string GetName() const override { return "overworld-render"; }
  std::string GetDescription() const {
    return "Render an overworld area (with overlays) to PNG";
  }
  std::string GetUsage() const override {
    return "overworld-render --screen=<hex> --out=<path.png> "
           "[--overlays=sprites,entrances,exits,holes,items,grid,all] "
           "[--phase=0|1|2] [--scale=<float>]";
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
  std::string GetOutputTitle() const override { return "Overworld Render"; }
};

}  // namespace handlers
}  // namespace cli
}  // namespace yaze

#endif  // YAZE_SRC_CLI_HANDLERS_GAME_OVERWORLD_RENDER_COMMANDS_H_
