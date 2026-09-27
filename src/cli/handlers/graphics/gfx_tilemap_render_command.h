#ifndef YAZE_CLI_HANDLERS_GRAPHICS_GFX_TILEMAP_RENDER_COMMAND_H_
#define YAZE_CLI_HANDLERS_GRAPHICS_GFX_TILEMAP_RENDER_COMMAND_H_

#include "cli/service/resources/command_handler.h"

namespace yaze {
namespace cli {

/**
 * @brief Renders a 2bpp SNES BG tilemap file (e.g. an Oracle of Secrets
 * Menu/tilemaps/*.tilemap file) to a true-color-equivalent indexed PNG,
 * using real CHR graphics and a real palette pulled from a ROM (or file
 * sources). This is the headless counterpart of the Screen editor's
 * "Menu Tilemap (2bpp)" panel preview, used for scripted/agent pixel
 * comparisons against reference screenshots.
 *
 * ROM access is optional at the command level (RequiresRom() is false)
 * because a fully file-backed invocation (--chr-source file --palette-
 * source file) needs no ROM at all; --rom is loaded on demand for the
 * sources that need it.
 */
class GfxTilemapRenderCommandHandler : public resources::CommandHandler {
 public:
  std::string GetName() const override { return "gfx-tilemap-render"; }
  std::string GetUsage() const override {
    return "gfx-tilemap-render --tilemap <file> --out <png> [--rom <rom>] "
           "[--symbols <sym file>] "
           "[--palette-source symbol|hud|file] "
           "[--palette-label <label>] [--palette-file <path>] "
           "[--palette-file-offset <n>] "
           "[--chr-source rom|file] [--chr-file <path>]";
  }
  std::string GetDefaultFormat() const override { return "json"; }
  std::string GetOutputTitle() const override { return "Tilemap Render"; }
  bool RequiresRom() const override { return false; }
  Descriptor Describe() const override {
    Descriptor desc;
    desc.display_name = "gfx-tilemap-render";
    desc.summary =
        "Render a 2bpp menu tilemap file to an indexed PNG using real CHR "
        "and palette sources, for headless pixel comparisons.";
    return desc;
  }
  absl::Status ValidateArgs(const resources::ArgumentParser& parser) override;
  absl::Status Execute(Rom* rom, const resources::ArgumentParser& parser,
                       resources::OutputFormatter& formatter) override;
};

}  // namespace cli
}  // namespace yaze

#endif  // YAZE_CLI_HANDLERS_GRAPHICS_GFX_TILEMAP_RENDER_COMMAND_H_
