#include "cli/handlers/graphics/gfx_tilemap_render_command.h"

#include <array>
#include <string>
#include <vector>

#include "absl/strings/str_format.h"
#include "app/emu/debug/symbol_provider.h"
#include "rom/rom.h"
#include "util/indexed_png.h"
#include "util/macro.h"
#include "zelda3/game_data.h"
#include "zelda3/screen/menu_tilemap.h"
#include "zelda3/screen/menu_tilemap_sources.h"

namespace yaze {
namespace cli {
namespace {

constexpr char kDefaultPaletteLabel[] = "Oracle_Menu_Palette";

std::vector<std::array<uint8_t, 4>> ToRgbaPalette(
    const std::array<gfx::SnesColor, 32>& colors) {
  std::vector<std::array<uint8_t, 4>> out(32);
  for (int i = 0; i < 32; ++i) {
    auto rgb = colors[i].rgb();
    uint8_t alpha = (i % 4 == 0) ? 0 : 255;  // color 0 of each sub-palette
    out[i] = {static_cast<uint8_t>(rgb.x), static_cast<uint8_t>(rgb.y),
              static_cast<uint8_t>(rgb.z), alpha};
  }
  return out;
}

absl::StatusOr<Rom> LoadRomArg(const resources::ArgumentParser& parser,
                               const char* why) {
  auto rom_path = parser.GetString("rom");
  if (!rom_path.has_value()) {
    return absl::InvalidArgumentError(
        absl::StrFormat("--rom is required (%s)", why));
  }
  Rom rom;
  RETURN_IF_ERROR(rom.LoadFromFile(*rom_path));
  return rom;
}

}  // namespace

absl::Status GfxTilemapRenderCommandHandler::ValidateArgs(
    const resources::ArgumentParser& parser) {
  RETURN_IF_ERROR(parser.RequireArgs({"tilemap", "out"}));

  std::string chr_source = parser.GetString("chr-source").value_or("rom");
  if (chr_source != "rom" && chr_source != "file") {
    return absl::InvalidArgumentError("--chr-source must be 'rom' or 'file'");
  }
  if (chr_source == "file" && !parser.GetString("chr-file").has_value()) {
    return absl::InvalidArgumentError("--chr-source file needs --chr-file");
  }

  std::string palette_source =
      parser.GetString("palette-source").value_or("symbol");
  if (palette_source != "symbol" && palette_source != "hud" &&
      palette_source != "file") {
    return absl::InvalidArgumentError(
        "--palette-source must be 'symbol', 'hud' or 'file'");
  }
  if (palette_source == "file" &&
      !parser.GetString("palette-file").has_value()) {
    return absl::InvalidArgumentError(
        "--palette-source file needs --palette-file");
  }
  if (palette_source == "symbol" && !parser.GetString("symbols").has_value()) {
    return absl::InvalidArgumentError(
        "--palette-source symbol needs --symbols <sym file> (and --rom)");
  }
  return absl::OkStatus();
}

absl::Status GfxTilemapRenderCommandHandler::Execute(
    Rom* /*rom*/, const resources::ArgumentParser& parser,
    resources::OutputFormatter& formatter) {
  const std::string tilemap_path = *parser.GetString("tilemap");
  const std::string out_path = *parser.GetString("out");

  zelda3::MenuTilemapDocument doc;
  RETURN_IF_ERROR(doc.LoadFromFile(tilemap_path));

  // --- CHR source ---
  const std::string chr_source = parser.GetString("chr-source").value_or("rom");
  std::vector<uint8_t> chr_sheet;
  if (chr_source == "file") {
    ASSIGN_OR_RETURN(chr_sheet, zelda3::ResolveMenuChrFromFile(
                                    *parser.GetString("chr-file")));
  } else {
    ASSIGN_OR_RETURN(Rom chr_rom,
                     LoadRomArg(parser, "--chr-source rom needs a ROM"));
    ASSIGN_OR_RETURN(chr_sheet, zelda3::ResolveMenuChrFromRom(chr_rom));
  }

  // --- Palette source ---
  const std::string palette_source =
      parser.GetString("palette-source").value_or("symbol");
  std::array<gfx::SnesColor, 32> colors{};
  std::string palette_label;
  if (palette_source == "file") {
    int offset = 2;
    if (auto off = parser.GetString("palette-file-offset"); off.has_value()) {
      ASSIGN_OR_RETURN(offset, parser.GetInt("palette-file-offset"));
    }
    ASSIGN_OR_RETURN(colors, zelda3::ResolveMenuPaletteFromFile(
                                 *parser.GetString("palette-file"),
                                 static_cast<size_t>(offset)));
  } else if (palette_source == "hud") {
    ASSIGN_OR_RETURN(Rom pal_rom,
                     LoadRomArg(parser, "--palette-source hud needs a ROM"));
    zelda3::GameData game_data;
    zelda3::LoadOptions options;
    options.load_graphics = false;
    options.load_gfx_groups = false;
    options.expand_rom = false;
    options.populate_metadata = false;
    RETURN_IF_ERROR(zelda3::LoadGameData(pal_rom, game_data, options));
    ASSIGN_OR_RETURN(colors, zelda3::ResolveMenuPaletteFromHud(game_data));
  } else {
    palette_label =
        parser.GetString("palette-label").value_or(kDefaultPaletteLabel);
    ASSIGN_OR_RETURN(Rom pal_rom,
                     LoadRomArg(parser, "--palette-source symbol needs a ROM"));
    emu::debug::SymbolProvider symbols;
    RETURN_IF_ERROR(symbols.LoadSymbolFile(*parser.GetString("symbols"),
                                           emu::debug::SymbolFormat::kAuto));
    ASSIGN_OR_RETURN(colors, zelda3::ResolveMenuPaletteFromSymbol(
                                 pal_rom, symbols, palette_label));
  }

  // --- Render ---
  auto chr_fn = zelda3::MakeChrPixelFn(chr_sheet);
  std::vector<uint8_t> indexed = doc.RenderIndexed(chr_fn);
  std::vector<std::array<uint8_t, 4>> palette = ToRgbaPalette(colors);

  ASSIGN_OR_RETURN(
      std::vector<uint8_t> png,
      util::EncodeIndexedPng(doc.render_width(), doc.render_height(), indexed,
                             palette));
  RETURN_IF_ERROR(util::WriteBinaryFile(out_path, png));

  formatter.AddField("tilemap", tilemap_path);
  formatter.AddField("out", out_path);
  formatter.AddField("rows", doc.rows());
  formatter.AddField("cols", doc.cols());
  formatter.AddField("width", doc.render_width());
  formatter.AddField("height", doc.render_height());
  formatter.AddField("chr_source", chr_source);
  formatter.AddField("palette_source", palette_source);
  if (!palette_label.empty()) {
    formatter.AddField("palette_label", palette_label);
  }
  return absl::OkStatus();
}

}  // namespace cli
}  // namespace yaze
