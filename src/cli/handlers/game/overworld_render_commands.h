#ifndef YAZE_SRC_CLI_HANDLERS_GAME_OVERWORLD_RENDER_COMMANDS_H_
#define YAZE_SRC_CLI_HANDLERS_GAME_OVERWORLD_RENDER_COMMANDS_H_

#include <optional>
#include <string>

#include "absl/status/statusor.h"
#include "cli/service/resources/command_handler.h"
#include "zelda3/overworld/graphics_preview.h"

namespace yaze {
namespace cli {
namespace handlers {

// Raw runtime graphics an overworld render applies before drawing.
struct OverworldRenderGraphicsPreview {
  std::string mode;      // project, file, off, none
  std::string manifest;  // manifest path; empty for off and none
  zelda3::OverworldGraphicsPreview preview;
};

// Resolves --graphics-preview. Unset or "on" loads the project's
// [files] overworld_graphics_preview ("on" fails without one), "off" renders
// the ROM's graphics only, and any other value is a manifest path.
absl::StatusOr<OverworldRenderGraphicsPreview>
ResolveOverworldRenderGraphicsPreview(const project::YazeProject* project,
                                      const std::optional<std::string>& flag);

// Render the overworld area containing a screen to a PNG file from current
// ROM data. A child screen of a large/wide/tall area resolves to its parent
// and the whole area is rendered.
//
// Usage:
//   overworld-render --screen=<hex> --out=<path.png>
//       [--overlays=sprites,entrances,exits,holes,items,grid,all]
//       [--phase=0|1|2] [--scale=<float>] [--area-overlay=on|off]
//       [--graphics-preview=on|off|<manifest.json>]
//
// The area's subscreen overlay (sky, fog, lava, canopy, rain, ...) is
// composited by default the way the game layers it; --area-overlay=off
// renders the bare area.
//
// With --project-context, graphics a hack DMAs at runtime (the project's
// [files] overworld_graphics_preview manifest) replace the area's tiles the
// way the editor canvas shows them; --graphics-preview=off draws the ROM's
// sheets only.
class OverworldRenderCommandHandler : public resources::CommandHandler {
 public:
  std::string GetName() const override { return "overworld-render"; }
  std::string GetDescription() const {
    return "Render an overworld area (with overlays) to PNG";
  }
  std::string GetUsage() const override {
    return "overworld-render --screen=<hex> --out=<path.png> "
           "[--overlays=sprites,entrances,exits,holes,items,grid,all] "
           "[--phase=0|1|2] [--scale=<float>] [--area-overlay=on|off] "
           "[--graphics-preview=on|off|<manifest.json>]";
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

// Build-time validation of a project's managed terrain against native ROM data.
// Emits verified inputs for the Oracle compiler without rendering or writing.
class LyraVerifyProjectCommandHandler : public resources::CommandHandler {
 public:
  std::string GetName() const override { return "lyra-verify-project"; }
  std::string GetUsage() const override {
    return "lyra-verify-project --project-context=<project.yaze> "
           "--rom=<source>";
  }
  absl::Status ValidateArgs(const resources::ArgumentParser&) override {
    return absl::OkStatus();
  }
  absl::Status Execute(Rom* rom, const resources::ArgumentParser& parser,
                       resources::OutputFormatter& formatter) override;

 private:
  std::string GetDefaultFormat() const override { return "json"; }
  std::string GetOutputTitle() const override { return "Lyra Project Inputs"; }
};

}  // namespace handlers
}  // namespace cli
}  // namespace yaze

#endif  // YAZE_SRC_CLI_HANDLERS_GAME_OVERWORLD_RENDER_COMMANDS_H_
