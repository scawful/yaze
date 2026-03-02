#ifndef YAZE_SRC_CLI_HANDLERS_PALETTE_COMMANDS_H_
#define YAZE_SRC_CLI_HANDLERS_PALETTE_COMMANDS_H_

#include "cli/service/resources/command_handler.h"

namespace yaze {
namespace cli {
namespace handlers {

/**
 * @brief Command handler for getting palette colors from a ROM.
 *
 * Loads palette data from the ROM and outputs color information in JSON or text
 * format. Supports querying by palette group name and optional palette index.
 *
 * Usage:
 *   palette-get-colors --group <group_name> [--index <palette_index>]
 *                      [--format <json|text>]
 *
 * Groups: ow_main, ow_aux, ow_animated, hud, global_sprites, armors, swords,
 *         shields, sprites_aux1, sprites_aux2, sprites_aux3, dungeon_main,
 *         grass, 3d_object, ow_mini_map
 */
class PaletteGetColorsCommandHandler : public resources::CommandHandler {
 public:
  std::string GetName() const override { return "palette-get-colors"; }
  std::string GetDescription() const {
    return "Get colors from a ROM palette group";
  }
  std::string GetUsage() const override {
    return "palette-get-colors --group <group_name> [--index <palette_index>] "
           "[--format <json|text>]";
  }

  absl::Status ValidateArgs(const resources::ArgumentParser& parser) override {
    return parser.RequireArgs({"group"});
  }

  absl::Status Execute(Rom* rom, const resources::ArgumentParser& parser,
                       resources::OutputFormatter& formatter) override;
};

/**
 * @brief Command handler for setting a palette color in the ROM.
 *
 * Modifies a specific color entry within a palette group and writes it back
 * to the ROM. The color can be specified as an RGB hex string (e.g., FF0000
 * for red) or as a 15-bit SNES color value.
 *
 * Usage:
 *   palette-set-color --group <group_name> --palette <palette_index>
 *                     --index <color_index> --color <RRGGBB|snes_hex>
 *                     [--write] [--format <json|text>]
 */
class PaletteSetColorCommandHandler : public resources::CommandHandler {
 public:
  std::string GetName() const override { return "palette-set-color"; }
  std::string GetDescription() const {
    return "Set a color in a ROM palette entry";
  }
  std::string GetUsage() const override {
    return "palette-set-color --group <group_name> --palette <palette_index> "
           "--index <color_index> --color <RRGGBB> [--write] "
           "[--format <json|text>]";
  }

  absl::Status ValidateArgs(const resources::ArgumentParser& parser) override {
    return parser.RequireArgs({"group", "palette", "index", "color"});
  }

  absl::Status Execute(Rom* rom, const resources::ArgumentParser& parser,
                       resources::OutputFormatter& formatter) override;
};

/**
 * @brief Command handler for analyzing palette usage statistics.
 *
 * Reports per-group palette counts, color counts, unique color counts,
 * brightness distribution, and ROM address ranges. Can analyze a single
 * group or all groups.
 *
 * Usage:
 *   palette-analyze [--group <group_name>] [--format <json|text>]
 */
class PaletteAnalyzeCommandHandler : public resources::CommandHandler {
 public:
  std::string GetName() const override { return "palette-analyze"; }
  std::string GetDescription() const {
    return "Analyze palette usage statistics and color properties";
  }
  std::string GetUsage() const override {
    return "palette-analyze [--group <group_name>] [--format <json|text>]";
  }

  absl::Status ValidateArgs(const resources::ArgumentParser& parser) override {
    return absl::OkStatus();  // No required args
  }

  absl::Status Execute(Rom* rom, const resources::ArgumentParser& parser,
                       resources::OutputFormatter& formatter) override;
};

}  // namespace handlers
}  // namespace cli
}  // namespace yaze

#endif  // YAZE_SRC_CLI_HANDLERS_PALETTE_COMMANDS_H_
