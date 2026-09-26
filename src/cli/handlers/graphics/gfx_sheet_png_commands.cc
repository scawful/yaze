#include "cli/handlers/graphics/gfx_sheet_png_commands.h"

#include <algorithm>
#include <filesystem>
#include <string>
#include <vector>

#include "absl/status/status.h"
#include "absl/strings/match.h"
#include "absl/strings/numbers.h"
#include "absl/strings/str_format.h"
#include "absl/strings/str_join.h"
#include "absl/strings/str_split.h"
#include "absl/strings/strip.h"
#include "core/gfx_sheet_policy_adapter.h"
#include "core/project.h"
#include "rom/rom.h"
#include "rom/rom_diff.h"
#include "util/indexed_png.h"
#include "util/macro.h"
#include "zelda3/game_data.h"
#include "zelda3/gfx_sheet_png.h"
#include "zelda3/gfx_sheet_storage.h"

namespace yaze {
namespace cli {
namespace {

absl::StatusOr<int> ParseNumber(absl::string_view text, const char* what) {
  text = absl::StripAsciiWhitespace(text);
  int base = 10;
  if (absl::ConsumePrefix(&text, "0x") || absl::ConsumePrefix(&text, "0X") ||
      absl::ConsumePrefix(&text, "$")) {
    base = 16;
  }
  int value = 0;
  const bool ok = base == 16 ? absl::SimpleHexAtoi(text, &value)
                             : absl::SimpleAtoi(text, &value);
  if (!ok) {
    return absl::InvalidArgumentError(
        absl::StrFormat("%s '%s' is not a number", what, std::string(text)));
  }
  return value;
}

absl::StatusOr<uint16_t> ParseSheet(const resources::ArgumentParser& parser) {
  auto text = parser.GetString("sheet");
  if (!text.has_value()) {
    return absl::InvalidArgumentError("--sheet is required");
  }
  ASSIGN_OR_RETURN(int sheet, ParseNumber(*text, "--sheet"));
  if (sheet < 0 || sheet >= static_cast<int>(zelda3::kGfxSheetCount)) {
    return absl::InvalidArgumentError("--sheet must be 0-222");
  }
  if (zelda3::GetGfxSheetStorageKind(static_cast<uint16_t>(sheet)) ==
      zelda3::GfxSheetStorageKind::kCompressed2bpp) {
    return absl::UnimplementedError(
        "2bpp sheets (113, 114, 218+) are not supported");
  }
  return static_cast<uint16_t>(sheet);
}

absl::StatusOr<int> ParseIntArg(const resources::ArgumentParser& parser,
                                const char* name, int fallback) {
  auto text = parser.GetString(name);
  if (!text.has_value()) {
    return fallback;
  }
  return ParseNumber(*text, name);
}

absl::StatusOr<zelda3::SheetPalette> ResolvePalette(
    Rom& rom, const resources::ArgumentParser& parser) {
  const std::string spec = parser.GetString("palette").value_or("gray");
  if (spec == "gray") {
    return zelda3::GrayscaleSheetPalette();
  }
  if (absl::StartsWith(spec, "rgb:")) {
    std::vector<std::string> parts = absl::StrSplit(spec.substr(4), ',');
    if (parts.size() != 8) {
      return absl::InvalidArgumentError("rgb: palettes need 8 colors");
    }
    zelda3::SheetPalette palette;
    for (int i = 0; i < 8; ++i) {
      absl::string_view hex = absl::StripAsciiWhitespace(parts[i]);
      absl::ConsumePrefix(&hex, "#");
      uint32_t rgb = 0;
      if (hex.size() != 6 || !absl::SimpleHexAtoi(hex, &rgb)) {
        return absl::InvalidArgumentError("rgb: colors are #RRGGBB");
      }
      palette[i] = {static_cast<uint8_t>(rgb >> 16),
                    static_cast<uint8_t>(rgb >> 8), static_cast<uint8_t>(rgb)};
    }
    return palette;
  }
  const bool background = absl::StartsWith(spec, "room-bg:");
  if (background || absl::StartsWith(spec, "room-sprite:")) {
    std::vector<std::string> parts = absl::StrSplit(spec, ':');
    if (parts.size() != 3) {
      return absl::InvalidArgumentError(
          "Room palettes are room-bg:<room>:<row> or room-sprite:<room>:<row>");
    }
    ASSIGN_OR_RETURN(int room, ParseNumber(parts[1], "room"));
    ASSIGN_OR_RETURN(int row, ParseNumber(parts[2], "row"));
    zelda3::GameData data;
    zelda3::LoadOptions options;
    options.load_graphics = false;
    options.expand_rom = false;
    RETURN_IF_ERROR(zelda3::LoadGameData(rom, data, options));
    return background ? zelda3::RoomBackgroundSheetPalette(rom, data, room, row)
                      : zelda3::RoomSpriteSheetPalette(rom, data, room, row);
  }
  return absl::InvalidArgumentError("Unknown --palette " + spec);
}

std::string PaletteText(const zelda3::SheetPalette& palette) {
  return absl::StrJoin(palette, ",", [](std::string* out, const auto& color) {
    out->append(absl::StrFormat("#%02X%02X%02X", color[0], color[1], color[2]));
  });
}

std::string RangeText(const std::vector<std::pair<int, int>>& ranges) {
  return absl::StrJoin(ranges, " ", [](std::string* out, const auto& range) {
    out->append(
        absl::StrFormat("0x%03X-0x%03X", range.first, range.second - 1));
  });
}

}  // namespace

absl::Status GfxExportCommandHandler::ValidateArgs(
    const resources::ArgumentParser& parser) {
  return parser.RequireArgs({"sheet", "png"});
}

absl::Status GfxExportCommandHandler::Execute(
    Rom* rom, const resources::ArgumentParser& parser,
    resources::OutputFormatter& formatter) {
  ASSIGN_OR_RETURN(const uint16_t sheet, ParseSheet(parser));
  ASSIGN_OR_RETURN(const int block, ParseIntArg(parser, "block", 0));
  ASSIGN_OR_RETURN(const int count, ParseIntArg(parser, "count", 16 - block));
  ASSIGN_OR_RETURN(const auto palette, ResolvePalette(*rom, parser));
  ASSIGN_OR_RETURN(const auto data, zelda3::ReadGfxSheetData(*rom, sheet));
  ASSIGN_OR_RETURN(const auto png,
                   zelda3::ExportSheetBlocksPng(data, palette, block, count));
  const std::string path = *parser.GetString("png");
  RETURN_IF_ERROR(util::WriteBinaryFile(path, png));

  formatter.AddField("sheet", absl::StrFormat("0x%02X", sheet));
  formatter.AddField("png", path);
  formatter.AddField("blocks",
                     absl::StrFormat("%d-%d", block, block + count - 1));
  formatter.AddField("width", std::min(count, 8) * 16);
  formatter.AddField("height", (count + 7) / 8 * 16);
  formatter.AddField("palette", PaletteText(palette));
  return absl::OkStatus();
}

absl::Status GfxImportCommandHandler::ValidateArgs(
    const resources::ArgumentParser& parser) {
  RETURN_IF_ERROR(parser.RequireArgs({"sheet", "png"}));
  const bool dry_run = parser.HasFlag("dry-run");
  const bool write = parser.HasFlag("write");
  if (dry_run == write) {
    return absl::InvalidArgumentError("Pass exactly one of --dry-run, --write");
  }
  if (write && !parser.GetString("out").has_value()) {
    return absl::InvalidArgumentError(
        "--write needs --out <rom file>; the input ROM is never overwritten");
  }
  return absl::OkStatus();
}

absl::Status GfxImportCommandHandler::Execute(
    Rom* rom, const resources::ArgumentParser& parser,
    resources::OutputFormatter& formatter) {
  ASSIGN_OR_RETURN(const uint16_t sheet, ParseSheet(parser));
  ASSIGN_OR_RETURN(const int block, ParseIntArg(parser, "block", 0));
  ASSIGN_OR_RETURN(const auto palette, ResolvePalette(*rom, parser));
  ASSIGN_OR_RETURN(const auto current, zelda3::ReadGfxSheetData(*rom, sheet));
  const std::string png_path = *parser.GetString("png");
  ASSIGN_OR_RETURN(const auto png_bytes, util::ReadBinaryFile(png_path));
  ASSIGN_OR_RETURN(const auto png, util::DecodePng(png_bytes));
  ASSIGN_OR_RETURN(const auto imported,
                   zelda3::ImportSheetBlocksPng(current, png, block, palette));

  // Run the real writer on a copy of the ROM buffer; the diff is exactly
  // what --write would change.
  zelda3::GfxSheetWritePolicy policy;
  if (project_ != nullptr) {
    ASSIGN_OR_RETURN(policy, core::BuildGfxSheetWritePolicy(*project_));
  }
  Rom simulated = *rom;
  const auto written =
      zelda3::WriteGfxSheet(simulated, sheet, imported.snes_3bpp, policy);

  formatter.AddField("sheet", absl::StrFormat("0x%02X", sheet));
  formatter.AddField("png", png_path);
  formatter.AddField("changed_blocks",
                     absl::StrJoin(imported.changed_blocks, " "));
  formatter.AddField("changed_tiles",
                     static_cast<int>(imported.changed_tiles.size()));
  formatter.AddField("sheet_byte_ranges", RangeText(imported.changed_ranges));
  if (!written.ok()) {
    // The preview still stands; say why the write would be refused.
    formatter.AddField("write_status", std::string(written.status().message()));
    if (parser.HasFlag("dry-run")) {
      formatter.AddField("mode", "dry-run");
      return absl::OkStatus();
    }
    return written.status();
  }
  formatter.AddField("write_status", "ok");
  const auto rom_diff =
      rom::ComputeDiffRanges(rom->vector(), simulated.vector());
  formatter.AddField("placement",
                     written->placement == zelda3::GfxSheetPlacement::kRelocated
                         ? "relocated"
                         : "in place");
  formatter.AddField("rom_pc", absl::StrFormat("0x%06X", written->new_pc));
  formatter.AddField("stored_bytes",
                     absl::StrFormat("%zu -> %zu", written->old_stored_size,
                                     written->new_stored_size));
  formatter.AddField("rom_bytes_changed",
                     static_cast<int>(rom_diff.total_bytes_changed));
  formatter.AddField(
      "rom_ranges",
      absl::StrJoin(rom_diff.ranges, " ",
                    [](std::string* out, const auto& range) {
                      out->append(absl::StrFormat("0x%06X-0x%06X", range.first,
                                                  range.second - 1));
                    }));

  if (parser.HasFlag("dry-run")) {
    formatter.AddField("mode", "dry-run");
    return absl::OkStatus();
  }
  const std::string out = *parser.GetString("out");
  std::error_code ec;
  if (!rom->filename().empty() &&
      std::filesystem::equivalent(out, rom->filename(), ec)) {
    return absl::InvalidArgumentError(
        "--out is the input ROM; write to a new file");
  }
  RETURN_IF_ERROR(util::WriteBinaryFile(out, simulated.vector()));
  formatter.AddField("mode", "write");
  formatter.AddField("out", out);
  return absl::OkStatus();
}

}  // namespace cli
}  // namespace yaze
