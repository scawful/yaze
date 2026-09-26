#ifndef YAZE_CLI_HANDLERS_GRAPHICS_GFX_SHEET_PNG_COMMANDS_H_
#define YAZE_CLI_HANDLERS_GRAPHICS_GFX_SHEET_PNG_COMMANDS_H_

#include "cli/service/resources/command_handler.h"

namespace yaze {
namespace cli {

// Palette specs shared by gfx-export and gfx-import:
//   gray                      8-step grayscale (default)
//   rgb:#RRGGBB,...           exactly 8 colors for indices 0-7
//   room-bg:<room>:<row>      dungeon room background CGRAM row 0-7
//   room-sprite:<room>:<row>  dungeon room sprite palette (OAM row 0-7)

/**
 * @brief Export 16x16 blocks of a 3bpp graphics sheet to an indexed PNG.
 */
class GfxExportCommandHandler : public resources::CommandHandler {
 public:
  std::string GetName() const override { return "gfx-export"; }
  std::string GetUsage() const override {
    return "gfx-export --sheet <id> --png <file> [--block <0-15>] "
           "[--count <n>] [--palette gray|rgb:...|room-bg:<room>:<row>|"
           "room-sprite:<room>:<row>]";
  }
  std::string GetDefaultFormat() const override { return "json"; }
  std::string GetOutputTitle() const override { return "Graphics Export"; }
  bool RequiresRom() const override { return true; }
  Descriptor Describe() const override {
    Descriptor desc;
    desc.display_name = "gfx-export";
    desc.summary =
        "Export a graphics sheet (or 16x16 block range) to an "
        "indexed PNG with a chosen palette.";
    return desc;
  }
  absl::Status ValidateArgs(const resources::ArgumentParser& parser) override;
  absl::Status Execute(Rom* rom, const resources::ArgumentParser& parser,
                       resources::OutputFormatter& formatter) override;
};

/**
 * @brief Import an indexed PNG into a sheet at a 16x16 block. --dry-run
 * reports the blocks, tiles and ROM bytes that would change; --write saves
 * the result through the sheet writer to --out (never the input ROM).
 */
class GfxImportCommandHandler : public resources::CommandHandler {
 public:
  std::string GetName() const override { return "gfx-import"; }
  std::string GetUsage() const override {
    return "gfx-import --sheet <id> --png <file> [--block <0-15>] "
           "[--palette ...] (--dry-run | --write --out <rom file>)";
  }
  std::string GetDefaultFormat() const override { return "json"; }
  std::string GetOutputTitle() const override { return "Graphics Import"; }
  bool RequiresRom() const override { return true; }
  Descriptor Describe() const override {
    Descriptor desc;
    desc.display_name = "gfx-import";
    desc.summary =
        "Import an indexed PNG into a graphics sheet: preview the "
        "exact changes, then write a new ROM file.";
    return desc;
  }
  absl::Status ValidateArgs(const resources::ArgumentParser& parser) override;
  absl::Status Execute(Rom* rom, const resources::ArgumentParser& parser,
                       resources::OutputFormatter& formatter) override;
};

}  // namespace cli
}  // namespace yaze

#endif  // YAZE_CLI_HANDLERS_GRAPHICS_GFX_SHEET_PNG_COMMANDS_H_
