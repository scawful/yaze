#include "cli/handlers/graphics/palette_commands.h"

#include <algorithm>
#include <cmath>
#include <set>
#include <string>

#include "absl/status/status.h"
#include "absl/strings/numbers.h"
#include "absl/strings/str_format.h"
#include "app/gfx/types/snes_color.h"
#include "app/gfx/types/snes_palette.h"
#include "util/macro.h"
#include "zelda3/game_data.h"

namespace yaze {
namespace cli {
namespace handlers {

namespace {

// All valid palette group names for iteration and validation.
constexpr const char* kAllGroupNames[] = {
    "ow_main",        "ow_aux",       "ow_animated",  "hud",
    "global_sprites", "armors",       "swords",       "shields",
    "sprites_aux1",   "sprites_aux2", "sprites_aux3", "dungeon_main",
    "grass",          "3d_object",    "ow_mini_map",
};
constexpr int kNumGroups = sizeof(kAllGroupNames) / sizeof(kAllGroupNames[0]);

// Load game data (palettes only) from the ROM.
absl::Status LoadPalettes(Rom* rom, zelda3::GameData& game_data) {
  game_data.set_rom(rom);
  zelda3::LoadOptions opts;
  opts.load_graphics = false;
  opts.load_palettes = true;
  opts.load_gfx_groups = false;
  opts.expand_rom = false;
  opts.populate_metadata = false;
  return zelda3::LoadGameData(*rom, game_data, opts);
}

// Format a single SnesColor as a hex RGB string (#RRGGBB).
std::string FormatColorHex(const gfx::SnesColor& color) {
  auto rgb = color.rgb();  // 0-255 stored in ImVec4
  return absl::StrFormat("#%02X%02X%02X", static_cast<int>(rgb.x),
                         static_cast<int>(rgb.y), static_cast<int>(rgb.z));
}

// Compute perceptual brightness (0-255) from an SnesColor.
int ColorBrightness(const gfx::SnesColor& color) {
  auto rgb = color.rgb();
  // ITU-R BT.601 luma
  return static_cast<int>(0.299 * rgb.x + 0.587 * rgb.y + 0.114 * rgb.z);
}

// Emit the colors of a single SnesPalette to the formatter.
void FormatPaletteColors(const gfx::SnesPalette& palette,
                         resources::OutputFormatter& formatter) {
  formatter.BeginArray("colors");
  for (size_t c = 0; c < palette.size(); ++c) {
    const auto& color = palette[c];
    auto rgb = color.rgb();
    std::string entry = absl::StrFormat(
        "%zu: %s (R=%d G=%d B=%d, SNES=0x%04X)", c, FormatColorHex(color),
        static_cast<int>(rgb.x), static_cast<int>(rgb.y),
        static_cast<int>(rgb.z), color.snes());
    formatter.AddArrayItem(entry);
  }
  formatter.EndArray();
}

// Analyze a single palette group and emit stats.
void AnalyzeGroup(const std::string& group_name, const gfx::PaletteGroup& group,
                  resources::OutputFormatter& formatter) {
  formatter.BeginObject(group_name);
  formatter.AddField("group", group_name);
  formatter.AddField("palette_count", static_cast<int>(group.size()));

  int total_colors = 0;
  std::set<uint16_t> unique_snes;
  int brightness_sum = 0;
  int darkest = 255;
  int brightest = 0;

  for (size_t p = 0; p < group.size(); ++p) {
    const auto& pal = group.palette_ref(p);
    total_colors += static_cast<int>(pal.size());
    for (size_t c = 0; c < pal.size(); ++c) {
      uint16_t snes_val = pal[c].snes();
      unique_snes.insert(snes_val);
      int br = ColorBrightness(pal[c]);
      brightness_sum += br;
      darkest = std::min(darkest, br);
      brightest = std::max(brightest, br);
    }
  }

  formatter.AddField("total_colors", total_colors);
  formatter.AddField("unique_colors", static_cast<int>(unique_snes.size()));

  if (total_colors > 0) {
    formatter.BeginObject("brightness");
    formatter.AddField("average", brightness_sum / total_colors);
    formatter.AddField("darkest", darkest);
    formatter.AddField("brightest", brightest);
    formatter.EndObject();
  }

  // ROM address range for the group
  try {
    uint32_t first_addr = gfx::GetPaletteAddress(group_name, 0, 0);
    formatter.AddHexField("rom_address_start", first_addr, 6);
  } catch (...) {
    // GetPaletteAddress may throw for grass (single-color group).
    // Gracefully omit the address.
  }

  formatter.EndObject();
}

}  // namespace

// ---------------------------------------------------------------------------
// palette-get-colors
// ---------------------------------------------------------------------------
absl::Status PaletteGetColorsCommandHandler::Execute(
    Rom* rom, const resources::ArgumentParser& parser,
    resources::OutputFormatter& formatter) {
  auto group_name = parser.GetString("group").value();

  // Load palette data from ROM.
  zelda3::GameData game_data;
  RETURN_IF_ERROR(LoadPalettes(rom, game_data));

  const auto* group = game_data.palette_groups.get_group(group_name);
  if (!group) {
    return absl::NotFoundError(
        absl::StrFormat("Unknown palette group: '%s'. Valid groups: ow_main, "
                        "ow_aux, ow_animated, hud, global_sprites, armors, "
                        "swords, shields, sprites_aux1, sprites_aux2, "
                        "sprites_aux3, dungeon_main, grass, 3d_object, "
                        "ow_mini_map",
                        group_name));
  }

  // Optional: narrow to a single palette index within the group.
  auto index_result = parser.GetInt("index");
  if (!index_result.ok() && !absl::IsNotFound(index_result.status())) {
    return index_result.status();
  }

  formatter.BeginObject("Palette Colors");
  formatter.AddField("group", group_name);
  formatter.AddField("palette_count", static_cast<int>(group->size()));

  if (index_result.ok()) {
    int idx = index_result.value();
    if (idx < 0 || idx >= static_cast<int>(group->size())) {
      return absl::InvalidArgumentError(absl::StrFormat(
          "Palette index %d out of range [0, %d)", idx, group->size()));
    }

    const auto& pal = group->palette_ref(idx);
    formatter.AddField("palette_index", idx);
    formatter.AddField("color_count", static_cast<int>(pal.size()));
    FormatPaletteColors(pal, formatter);
  } else {
    // Dump all palettes in the group.
    formatter.BeginArray("palettes");
    for (size_t p = 0; p < group->size(); ++p) {
      const auto& pal = group->palette_ref(p);
      std::string label =
          absl::StrFormat("palette %zu (%zu colors)", p, pal.size());
      formatter.AddArrayItem(label);
    }
    formatter.EndArray();

    // If the group is small enough, also dump all colors inline.
    if (group->size() <= 6) {
      for (size_t p = 0; p < group->size(); ++p) {
        const auto& pal = group->palette_ref(p);
        formatter.BeginObject(absl::StrFormat("palette_%zu", p));
        formatter.AddField("index", static_cast<int>(p));
        formatter.AddField("color_count", static_cast<int>(pal.size()));
        FormatPaletteColors(pal, formatter);
        formatter.EndObject();
      }
    }
  }

  formatter.EndObject();
  return absl::OkStatus();
}

// ---------------------------------------------------------------------------
// palette-set-color
// ---------------------------------------------------------------------------
absl::Status PaletteSetColorCommandHandler::Execute(
    Rom* rom, const resources::ArgumentParser& parser,
    resources::OutputFormatter& formatter) {
  auto group_name = parser.GetString("group").value();
  auto palette_str = parser.GetString("palette").value();
  auto index_str = parser.GetString("index").value();
  auto color_str = parser.GetString("color").value();
  bool write = parser.HasFlag("write");

  int palette_idx;
  if (!absl::SimpleAtoi(palette_str, &palette_idx)) {
    return absl::InvalidArgumentError(
        absl::StrFormat("Invalid palette index: '%s'", palette_str));
  }

  int color_idx;
  if (!absl::SimpleAtoi(index_str, &color_idx)) {
    return absl::InvalidArgumentError(
        absl::StrFormat("Invalid color index: '%s'", index_str));
  }

  // Parse RGB hex color (RRGGBB, with or without leading # or 0x).
  std::string hex = color_str;
  if (!hex.empty() && hex[0] == '#')
    hex = hex.substr(1);
  if (hex.size() >= 2 && hex[0] == '0' && (hex[1] == 'x' || hex[1] == 'X'))
    hex = hex.substr(2);

  if (hex.size() != 6) {
    return absl::InvalidArgumentError(absl::StrFormat(
        "Color must be a 6-digit hex RGB value (e.g., FF0000). Got: '%s'",
        color_str));
  }

  unsigned int r_val, g_val, b_val;
  if (sscanf(hex.c_str(), "%02x%02x%02x", &r_val, &g_val, &b_val) != 3) {
    return absl::InvalidArgumentError(
        absl::StrFormat("Failed to parse color hex: '%s'", color_str));
  }

  uint8_t r = static_cast<uint8_t>(r_val);
  uint8_t g = static_cast<uint8_t>(g_val);
  uint8_t b = static_cast<uint8_t>(b_val);

  // Load palette data from ROM.
  zelda3::GameData game_data;
  RETURN_IF_ERROR(LoadPalettes(rom, game_data));

  auto* group = game_data.palette_groups.get_group(group_name);
  if (!group) {
    return absl::NotFoundError(
        absl::StrFormat("Unknown palette group: '%s'", group_name));
  }

  if (palette_idx < 0 || palette_idx >= static_cast<int>(group->size())) {
    return absl::InvalidArgumentError(absl::StrFormat(
        "Palette index %d out of range [0, %d)", palette_idx, group->size()));
  }

  const auto& pal = group->palette_ref(palette_idx);
  if (color_idx < 0 || color_idx >= static_cast<int>(pal.size())) {
    return absl::InvalidArgumentError(absl::StrFormat(
        "Color index %d out of range [0, %d)", color_idx, pal.size()));
  }

  // Build the new SNES color.
  gfx::SnesColor new_color(r, g, b);

  // Read the old color for reporting.
  auto old_color = group->GetColor(palette_idx, color_idx);
  std::string old_hex = FormatColorHex(old_color);

  formatter.BeginObject("Palette Color Set");
  formatter.AddField("group", group_name);
  formatter.AddField("palette_index", palette_idx);
  formatter.AddField("color_index", color_idx);
  formatter.AddField("old_color", old_hex);
  formatter.AddField("new_color", FormatColorHex(new_color));
  formatter.AddHexField("old_snes", old_color.snes(), 4);
  formatter.AddHexField("new_snes", new_color.snes(), 4);

  if (write) {
    // Compute the ROM address for this color entry and write 2 bytes.
    uint32_t addr = gfx::GetPaletteAddress(group_name, palette_idx, color_idx);
    uint16_t snes_val = new_color.snes();
    RETURN_IF_ERROR(rom->WriteShort(addr, snes_val));

    formatter.AddField("status", "written");
    formatter.AddHexField("rom_address", addr, 6);
  } else {
    formatter.AddField("status", "dry_run");
    formatter.AddField("message", "Use --write to apply the change to the ROM");
  }

  formatter.EndObject();
  return absl::OkStatus();
}

// ---------------------------------------------------------------------------
// palette-analyze
// ---------------------------------------------------------------------------
absl::Status PaletteAnalyzeCommandHandler::Execute(
    Rom* rom, const resources::ArgumentParser& parser,
    resources::OutputFormatter& formatter) {
  auto group_name_opt = parser.GetString("group");

  // Load palette data from ROM.
  zelda3::GameData game_data;
  RETURN_IF_ERROR(LoadPalettes(rom, game_data));

  formatter.BeginObject("Palette Analysis");

  if (group_name_opt.has_value()) {
    // Analyze a single group.
    const std::string& group_name = group_name_opt.value();
    const auto* group = game_data.palette_groups.get_group(group_name);
    if (!group) {
      return absl::NotFoundError(
          absl::StrFormat("Unknown palette group: '%s'", group_name));
    }
    formatter.AddField("analysis_type", "single_group");
    AnalyzeGroup(group_name, *group, formatter);
  } else {
    // Analyze all groups.
    formatter.AddField("analysis_type", "all_groups");

    int total_palettes = 0;
    int total_colors = 0;
    std::set<uint16_t> global_unique;

    formatter.BeginArray("groups");
    for (int i = 0; i < kNumGroups; ++i) {
      const std::string name = kAllGroupNames[i];
      const auto* group = game_data.palette_groups.get_group(name);
      if (!group)
        continue;

      int group_colors = 0;
      for (size_t p = 0; p < group->size(); ++p) {
        const auto& pal = group->palette_ref(p);
        group_colors += static_cast<int>(pal.size());
        for (size_t c = 0; c < pal.size(); ++c) {
          global_unique.insert(pal[c].snes());
        }
      }
      total_palettes += static_cast<int>(group->size());
      total_colors += group_colors;

      std::string summary = absl::StrFormat("%s: %zu palettes, %d colors", name,
                                            group->size(), group_colors);
      formatter.AddArrayItem(summary);
    }
    formatter.EndArray();

    formatter.AddField("total_groups", kNumGroups);
    formatter.AddField("total_palettes", total_palettes);
    formatter.AddField("total_colors", total_colors);
    formatter.AddField("unique_colors_global",
                       static_cast<int>(global_unique.size()));

    // Brightness histogram (buckets of 32).
    formatter.BeginObject("brightness_distribution");
    int buckets[8] = {};
    for (int i = 0; i < kNumGroups; ++i) {
      const auto* group = game_data.palette_groups.get_group(kAllGroupNames[i]);
      if (!group)
        continue;
      for (size_t p = 0; p < group->size(); ++p) {
        const auto& pal = group->palette_ref(p);
        for (size_t c = 0; c < pal.size(); ++c) {
          int br = ColorBrightness(pal[c]);
          int bucket = std::min(br / 32, 7);
          buckets[bucket]++;
        }
      }
    }
    formatter.AddField("0-31_dark", buckets[0]);
    formatter.AddField("32-63", buckets[1]);
    formatter.AddField("64-95", buckets[2]);
    formatter.AddField("96-127", buckets[3]);
    formatter.AddField("128-159", buckets[4]);
    formatter.AddField("160-191", buckets[5]);
    formatter.AddField("192-223", buckets[6]);
    formatter.AddField("224-255_bright", buckets[7]);
    formatter.EndObject();
  }

  formatter.EndObject();
  return absl::OkStatus();
}

}  // namespace handlers
}  // namespace cli
}  // namespace yaze
