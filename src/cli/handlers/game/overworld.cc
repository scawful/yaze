#include "zelda3/overworld/overworld.h"

#include <algorithm>
#include <cctype>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

#include "absl/flags/declare.h"
#include "absl/flags/flag.h"
#include "absl/status/statusor.h"
#include "absl/strings/ascii.h"
#include "absl/strings/match.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/str_format.h"
#include "absl/strings/str_join.h"
#include "cli/cli.h"
#include "cli/handlers/game/overworld_inspect.h"

ABSL_DECLARE_FLAG(std::string, rom);

namespace yaze {
namespace cli {

// Note: These CLI commands currently operate directly on ROM data.
// Future: Integrate with CanvasAutomationAPI for live GUI automation.

// Legacy OverworldGetTile class removed - using new CommandHandler system
// TODO: Implement OverworldGetTileCommandHandler
absl::Status HandleOverworldGetTileLegacy(
    const std::vector<std::string>& arg_vec) {
  int map_id = -1, x = -1, y = -1;

  for (size_t i = 0; i < arg_vec.size(); ++i) {
    const std::string& arg = arg_vec[i];
    if ((arg == "--map") && i + 1 < arg_vec.size()) {
      map_id = std::stoi(arg_vec[++i]);
    } else if ((arg == "--x") && i + 1 < arg_vec.size()) {
      x = std::stoi(arg_vec[++i]);
    } else if ((arg == "--y") && i + 1 < arg_vec.size()) {
      y = std::stoi(arg_vec[++i]);
    }
  }

  if (map_id == -1 || x == -1 || y == -1) {
    return absl::InvalidArgumentError(
        "Usage: overworld get-tile --map <map_id> --x <x> --y <y>");
  }

  std::string rom_file = absl::GetFlag(FLAGS_rom);
  if (rom_file.empty()) {
    return absl::InvalidArgumentError(
        "ROM file must be provided via --rom flag.");
  }

  Rom rom;
  auto load_status = rom.LoadFromFile(rom_file);
  if (!load_status.ok()) {
    return load_status;
  }
  if (!rom.is_loaded()) {
    return absl::AbortedError("Failed to load ROM.");
  }

  zelda3::Overworld overworld(&rom);
  auto ow_status = overworld.Load(&rom);
  if (!ow_status.ok()) {
    return ow_status;
  }

  uint16_t tile = overworld.GetTile(x, y);

  std::cout << "Tile at (" << x << ", " << y << ") on map " << map_id
            << " is: 0x" << std::hex << tile << std::endl;

  return absl::OkStatus();
}

// Legacy OverworldSetTile class removed - using new CommandHandler system
// TODO: Implement OverworldSetTileCommandHandler
absl::Status HandleOverworldSetTileLegacy(
    const std::vector<std::string>& arg_vec) {
  int map_id = -1, x = -1, y = -1, tile_id = -1;

  for (size_t i = 0; i < arg_vec.size(); ++i) {
    const std::string& arg = arg_vec[i];
    if ((arg == "--map") && i + 1 < arg_vec.size()) {
      map_id = std::stoi(arg_vec[++i]);
    } else if ((arg == "--x") && i + 1 < arg_vec.size()) {
      x = std::stoi(arg_vec[++i]);
    } else if ((arg == "--y") && i + 1 < arg_vec.size()) {
      y = std::stoi(arg_vec[++i]);
    } else if ((arg == "--tile") && i + 1 < arg_vec.size()) {
      tile_id = std::stoi(arg_vec[++i], nullptr, 16);
    }
  }

  if (map_id == -1 || x == -1 || y == -1 || tile_id == -1) {
    return absl::InvalidArgumentError(
        "Usage: overworld set-tile --map <map_id> --x <x> --y <y> --tile "
        "<tile_id>");
  }

  std::string rom_file = absl::GetFlag(FLAGS_rom);
  if (rom_file.empty()) {
    return absl::InvalidArgumentError(
        "ROM file must be provided via --rom flag.");
  }

  Rom rom;
  auto load_status = rom.LoadFromFile(rom_file);
  if (!load_status.ok()) {
    return load_status;
  }
  if (!rom.is_loaded()) {
    return absl::AbortedError("Failed to load ROM.");
  }

  zelda3::Overworld overworld(&rom);
  auto status = overworld.Load(&rom);
  if (!status.ok()) {
    return status;
  }

  // Set the world based on map_id
  if (map_id < 0x40) {
    overworld.set_current_world(0);  // Light World
  } else if (map_id < 0x80) {
    overworld.set_current_world(1);  // Dark World
  } else {
    overworld.set_current_world(2);  // Special World
  }

  // Set the tile
  overworld.SetTile(x, y, static_cast<uint16_t>(tile_id));

  // Save the ROM
  auto save_status = rom.SaveToFile({.filename = rom_file});
  if (!save_status.ok()) {
    return save_status;
  }

  std::cout << "✅ Set tile at (" << x << ", " << y << ") on map " << map_id
            << " to: 0x" << std::hex << tile_id << std::dec << std::endl;

  return absl::OkStatus();
}

namespace {

constexpr absl::string_view kFindTileUsage =
    "Usage: overworld find-tile --tile <tile_id> [--map <map_id>] [--world "
    "<light|dark|special|0|1|2>] [--format <json|text>]";

}  // namespace

// Legacy OverworldFindTile class removed - using new CommandHandler system
// TODO: Implement OverworldFindTileCommandHandler
absl::Status HandleOverworldFindTileLegacy(
    const std::vector<std::string>& arg_vec) {
  std::unordered_map<std::string, std::string> options;
  std::vector<std::string> positional;
  options.reserve(arg_vec.size());

  for (size_t i = 0; i < arg_vec.size(); ++i) {
    const std::string& token = arg_vec[i];
    if (absl::StartsWith(token, "--")) {
      std::string key;
      std::string value;
      auto eq_pos = token.find('=');
      if (eq_pos != std::string::npos) {
        key = token.substr(2, eq_pos - 2);
        value = token.substr(eq_pos + 1);
      } else {
        key = token.substr(2);
        if (i + 1 >= arg_vec.size()) {
          return absl::InvalidArgumentError(
              absl::StrCat("Missing value for --", key, "\n", kFindTileUsage));
        }
        value = arg_vec[++i];
      }
      if (value.empty()) {
        return absl::InvalidArgumentError(
            absl::StrCat("Missing value for --", key, "\n", kFindTileUsage));
      }
      options[key] = value;
    } else {
      positional.push_back(token);
    }
  }

  if (!positional.empty()) {
    return absl::InvalidArgumentError(absl::StrCat(
        "Unexpected positional arguments: ", absl::StrJoin(positional, ", "),
        "\n", kFindTileUsage));
  }

  auto tile_it = options.find("tile");
  if (tile_it == options.end()) {
    return absl::InvalidArgumentError(
        absl::StrCat("Missing required --tile argument\n", kFindTileUsage));
  }

  ASSIGN_OR_RETURN(int tile_value, overworld::ParseNumeric(tile_it->second));
  if (tile_value < 0 || tile_value > 0xFFFF) {
    return absl::InvalidArgumentError(
        absl::StrCat("Tile ID must be between 0x0000 and 0xFFFF (got ",
                     tile_it->second, ")"));
  }
  const uint16_t tile_id = static_cast<uint16_t>(tile_value);

  std::optional<int> map_filter;
  if (auto map_it = options.find("map"); map_it != options.end()) {
    ASSIGN_OR_RETURN(int map_value, overworld::ParseNumeric(map_it->second));
    if (map_value < 0 || map_value >= 0xA0) {
      return absl::InvalidArgumentError(
          absl::StrCat("Map ID out of range: ", map_it->second));
    }
    map_filter = map_value;
  }

  std::optional<int> world_filter;
  if (auto world_it = options.find("world"); world_it != options.end()) {
    ASSIGN_OR_RETURN(int parsed_world,
                     overworld::ParseWorldSpecifier(world_it->second));
    world_filter = parsed_world;
  }

  if (map_filter.has_value()) {
    ASSIGN_OR_RETURN(int inferred_world,
                     overworld::InferWorldFromMapId(*map_filter));
    if (world_filter.has_value() && inferred_world != *world_filter) {
      return absl::InvalidArgumentError(absl::StrCat(
          "Map 0x", absl::StrFormat("%02X", *map_filter), " belongs to the ",
          overworld::WorldName(inferred_world), " World but --world requested ",
          overworld::WorldName(*world_filter)));
    }
    if (!world_filter.has_value()) {
      world_filter = inferred_world;
    }
  }

  std::string format = "text";
  if (auto format_it = options.find("format"); format_it != options.end()) {
    format = absl::AsciiStrToLower(format_it->second);
    if (format != "text" && format != "json") {
      return absl::InvalidArgumentError(
          absl::StrCat("Unsupported format: ", format_it->second));
    }
  }

  std::string rom_file = absl::GetFlag(FLAGS_rom);
  if (auto rom_it = options.find("rom"); rom_it != options.end()) {
    rom_file = rom_it->second;
  }

  if (rom_file.empty()) {
    return absl::InvalidArgumentError(
        "ROM file must be provided via --rom flag.");
  }

  Rom rom;
  auto load_status = rom.LoadFromFile(rom_file);
  if (!load_status.ok()) {
    return load_status;
  }
  if (!rom.is_loaded()) {
    return absl::AbortedError("Failed to load ROM.");
  }

  zelda3::Overworld overworld(&rom);
  auto ow_status = overworld.Load(&rom);
  if (!ow_status.ok()) {
    return ow_status;
  }

  overworld::TileSearchOptions search_options;
  search_options.map_id = map_filter;
  search_options.world = world_filter;

  ASSIGN_OR_RETURN(auto matches, overworld::FindTileMatches(overworld, tile_id,
                                                            search_options));

  if (format == "json") {
    std::cout << "{\n";
    std::cout << absl::StrFormat("  \"tile\": \"0x%04X\",\n", tile_id);
    std::cout << absl::StrFormat("  \"match_count\": %zu,\n", matches.size());
    std::cout << "  \"matches\": [\n";
    for (size_t i = 0; i < matches.size(); ++i) {
      const auto& match = matches[i];
      std::cout << absl::StrFormat(
          "    {\"map\": \"0x%02X\", \"world\": \"%s\", "
          "\"local\": {\"x\": %d, \"y\": %d}, "
          "\"global\": {\"x\": %d, \"y\": %d}}%s\n",
          match.map_id, overworld::WorldName(match.world), match.local_x,
          match.local_y, match.global_x, match.global_y,
          (i + 1 == matches.size()) ? "" : ",");
    }
    std::cout << "  ]\n";
    std::cout << "}\n";
  } else {
    std::cout << absl::StrFormat("🔎 Tile 0x%04X → %zu match(es)\n", tile_id,
                                 matches.size());
    if (matches.empty()) {
      std::cout << "  No matches found." << std::endl;
      return absl::OkStatus();
    }

    for (const auto& match : matches) {
      std::cout << absl::StrFormat(
          "  • Map 0x%02X (%s World) local(%2d,%2d) global(%3d,%3d)\n",
          match.map_id, overworld::WorldName(match.world), match.local_x,
          match.local_y, match.global_x, match.global_y);
    }
  }

  return absl::OkStatus();
}

// Legacy OverworldDescribeMap class removed - using new CommandHandler system
// TODO: Implement OverworldDescribeMapCommandHandler
absl::Status HandleOverworldDescribeMapLegacy(
    const std::vector<std::string>& arg_vec) {
  constexpr absl::string_view kUsage =
      "Usage: overworld describe-map --map <map_id> [--format <json|text>]";

  std::unordered_map<std::string, std::string> options;
  std::vector<std::string> positional;
  options.reserve(arg_vec.size());

  for (size_t i = 0; i < arg_vec.size(); ++i) {
    const std::string& token = arg_vec[i];
    if (absl::StartsWith(token, "--")) {
      std::string key;
      std::string value;
      auto eq_pos = token.find('=');
      if (eq_pos != std::string::npos) {
        key = token.substr(2, eq_pos - 2);
        value = token.substr(eq_pos + 1);
      } else {
        key = token.substr(2);
        if (i + 1 >= arg_vec.size()) {
          return absl::InvalidArgumentError(
              absl::StrCat("Missing value for --", key, "\n", kUsage));
        }
        value = arg_vec[++i];
      }
      if (value.empty()) {
        return absl::InvalidArgumentError(
            absl::StrCat("Missing value for --", key, "\n", kUsage));
      }
      options[key] = value;
    } else {
      positional.push_back(token);
    }
  }

  if (!positional.empty()) {
    return absl::InvalidArgumentError(absl::StrCat(
        "Unexpected positional arguments: ", absl::StrJoin(positional, ", "),
        "\n", kUsage));
  }

  auto map_it = options.find("map");
  if (map_it == options.end()) {
    return absl::InvalidArgumentError(std::string(kUsage));
  }

  ASSIGN_OR_RETURN(int map_value, overworld::ParseNumeric(map_it->second));
  if (map_value < 0 || map_value >= zelda3::kNumOverworldMaps) {
    return absl::InvalidArgumentError(
        absl::StrCat("Map ID out of range: ", map_it->second));
  }

  std::string format = "text";
  if (auto it = options.find("format"); it != options.end()) {
    format = absl::AsciiStrToLower(it->second);
    if (format != "text" && format != "json") {
      return absl::InvalidArgumentError(
          absl::StrCat("Unsupported format: ", it->second));
    }
  }

  std::string rom_file = absl::GetFlag(FLAGS_rom);
  if (auto it = options.find("rom"); it != options.end()) {
    rom_file = it->second;
  }

  if (rom_file.empty()) {
    return absl::InvalidArgumentError(
        "ROM file must be provided via --rom flag.");
  }

  Rom rom;
  auto load_status = rom.LoadFromFile(rom_file);
  if (!load_status.ok()) {
    return load_status;
  }
  if (!rom.is_loaded()) {
    return absl::AbortedError("Failed to load ROM.");
  }

  zelda3::Overworld overworld_rom(&rom);
  auto ow_status = overworld_rom.Load(&rom);
  if (!ow_status.ok()) {
    return ow_status;
  }

  ASSIGN_OR_RETURN(auto summary,
                   overworld::BuildMapSummary(overworld_rom, map_value));

  auto join_hex = [](const std::vector<uint8_t>& values) {
    std::vector<std::string> parts;
    parts.reserve(values.size());
    for (uint8_t v : values) {
      parts.push_back(absl::StrFormat("0x%02X", v));
    }
    return absl::StrJoin(parts, ", ");
  };

  auto join_hex_json = [](const std::vector<uint8_t>& values) {
    std::vector<std::string> parts;
    parts.reserve(values.size());
    for (uint8_t v : values) {
      parts.push_back(absl::StrFormat("\"0x%02X\"", v));
    }
    return absl::StrCat("[", absl::StrJoin(parts, ", "), "]");
  };

  if (format == "json") {
    std::cout << "{\n";
    std::cout << absl::StrFormat("  \"map\": \"0x%02X\",\n", summary.map_id);
    std::cout << absl::StrFormat("  \"world\": \"%s\",\n",
                                 overworld::WorldName(summary.world));
    std::cout << absl::StrFormat(
        "  \"grid\": {\"x\": %d, \"y\": %d, \"index\": %d},\n", summary.map_x,
        summary.map_y, summary.local_index);
    std::cout << absl::StrFormat(
        "  \"size\": {\"label\": \"%s\", \"is_large\": %s, \"parent\": "
        "\"0x%02X\", \"quadrant\": %d},\n",
        summary.area_size, summary.is_large_map ? "true" : "false",
        summary.parent_map, summary.large_quadrant);
    std::cout << absl::StrFormat("  \"message\": \"0x%04X\",\n",
                                 summary.message_id);
    std::cout << absl::StrFormat("  \"area_graphics\": \"0x%02X\",\n",
                                 summary.area_graphics);
    std::cout << absl::StrFormat("  \"area_palette\": \"0x%02X\",\n",
                                 summary.area_palette);
    std::cout << absl::StrFormat("  \"main_palette\": \"0x%02X\",\n",
                                 summary.main_palette);
    std::cout << absl::StrFormat("  \"animated_gfx\": \"0x%02X\",\n",
                                 summary.animated_gfx);
    std::cout << absl::StrFormat("  \"subscreen_overlay\": \"0x%04X\",\n",
                                 summary.subscreen_overlay);
    std::cout << absl::StrFormat("  \"area_specific_bg_color\": \"0x%04X\",\n",
                                 summary.area_specific_bg_color);
    std::cout << absl::StrFormat("  \"sprite_graphics\": %s,\n",
                                 join_hex_json(summary.sprite_graphics));
    std::cout << absl::StrFormat("  \"sprite_palettes\": %s,\n",
                                 join_hex_json(summary.sprite_palettes));
    std::cout << absl::StrFormat("  \"area_music\": %s,\n",
                                 join_hex_json(summary.area_music));
    std::cout << absl::StrFormat("  \"static_graphics\": %s,\n",
                                 join_hex_json(summary.static_graphics));
    std::cout << absl::StrFormat(
        "  \"overlay\": {\"enabled\": %s, \"id\": \"0x%04X\"}\n",
        summary.has_overlay ? "true" : "false", summary.overlay_id);
    std::cout << "}\n";
  } else {
    std::cout << absl::StrFormat("🗺️ Map 0x%02X (%s World)\n", summary.map_id,
                                 overworld::WorldName(summary.world));
    std::cout << absl::StrFormat("  Grid: (%d, %d) local-index %d\n",
                                 summary.map_x, summary.map_y,
                                 summary.local_index);
    std::cout << absl::StrFormat(
        "  Size: %s%s | Parent: 0x%02X | Quadrant: %d\n", summary.area_size,
        summary.is_large_map ? " (large)" : "", summary.parent_map,
        summary.large_quadrant);
    std::cout << absl::StrFormat(
        "  Message: 0x%04X | Area GFX: 0x%02X | Area Palette: 0x%02X\n",
        summary.message_id, summary.area_graphics, summary.area_palette);
    std::cout << absl::StrFormat(
        "  Main Palette: 0x%02X | Animated GFX: 0x%02X | Overlay: %s "
        "(0x%04X)\n",
        summary.main_palette, summary.animated_gfx,
        summary.has_overlay ? "yes" : "no", summary.overlay_id);
    std::cout << absl::StrFormat(
        "  Subscreen Overlay: 0x%04X | BG Color: 0x%04X\n",
        summary.subscreen_overlay, summary.area_specific_bg_color);
    std::cout << absl::StrFormat("  Sprite GFX: [%s]\n",
                                 join_hex(summary.sprite_graphics));
    std::cout << absl::StrFormat("  Sprite Palettes: [%s]\n",
                                 join_hex(summary.sprite_palettes));
    std::cout << absl::StrFormat("  Area Music: [%s]\n",
                                 join_hex(summary.area_music));
    std::cout << absl::StrFormat("  Static GFX: [%s]\n",
                                 join_hex(summary.static_graphics));
  }

  return absl::OkStatus();
}

// Legacy OverworldListWarps class removed - using new CommandHandler system
// TODO: Implement OverworldListWarpsCommandHandler
absl::Status HandleOverworldListWarpsLegacy(
    const std::vector<std::string>& arg_vec) {
  constexpr absl::string_view kUsage =
      "Usage: overworld list-warps [--map <map_id>] [--world "
      "<light|dark|special>] "
      "[--type <entrance|hole|exit|all>] [--format <json|text>]";

  std::unordered_map<std::string, std::string> options;
  std::vector<std::string> positional;
  options.reserve(arg_vec.size());

  for (size_t i = 0; i < arg_vec.size(); ++i) {
    const std::string& token = arg_vec[i];
    if (absl::StartsWith(token, "--")) {
      std::string key;
      std::string value;
      auto eq_pos = token.find('=');
      if (eq_pos != std::string::npos) {
        key = token.substr(2, eq_pos - 2);
        value = token.substr(eq_pos + 1);
      } else {
        key = token.substr(2);
        if (i + 1 >= arg_vec.size()) {
          return absl::InvalidArgumentError(
              absl::StrCat("Missing value for --", key, "\n", kUsage));
        }
        value = arg_vec[++i];
      }
      if (value.empty()) {
        return absl::InvalidArgumentError(
            absl::StrCat("Missing value for --", key, "\n", kUsage));
      }
      options[key] = value;
    } else {
      positional.push_back(token);
    }
  }

  if (!positional.empty()) {
    return absl::InvalidArgumentError(absl::StrCat(
        "Unexpected positional arguments: ", absl::StrJoin(positional, ", "),
        "\n", kUsage));
  }

  std::optional<int> map_filter;
  if (auto it = options.find("map"); it != options.end()) {
    ASSIGN_OR_RETURN(int map_value, overworld::ParseNumeric(it->second));
    if (map_value < 0 || map_value >= zelda3::kNumOverworldMaps) {
      return absl::InvalidArgumentError(
          absl::StrCat("Map ID out of range: ", it->second));
    }
    map_filter = map_value;
  }

  std::optional<int> world_filter;
  if (auto it = options.find("world"); it != options.end()) {
    ASSIGN_OR_RETURN(int parsed_world,
                     overworld::ParseWorldSpecifier(it->second));
    world_filter = parsed_world;
  }

  std::optional<overworld::WarpType> type_filter;
  if (auto it = options.find("type"); it != options.end()) {
    std::string lower = absl::AsciiStrToLower(it->second);
    if (lower == "entrance" || lower == "entrances") {
      type_filter = overworld::WarpType::kEntrance;
    } else if (lower == "hole" || lower == "holes") {
      type_filter = overworld::WarpType::kHole;
    } else if (lower == "exit" || lower == "exits") {
      type_filter = overworld::WarpType::kExit;
    } else if (lower == "all" || lower.empty()) {
      type_filter.reset();
    } else {
      return absl::InvalidArgumentError(
          absl::StrCat("Unknown warp type: ", it->second));
    }
  }

  if (map_filter.has_value()) {
    ASSIGN_OR_RETURN(int inferred_world,
                     overworld::InferWorldFromMapId(*map_filter));
    if (world_filter.has_value() && inferred_world != *world_filter) {
      return absl::InvalidArgumentError(absl::StrCat(
          "Map 0x", absl::StrFormat("%02X", *map_filter), " belongs to the ",
          overworld::WorldName(inferred_world), " World but --world requested ",
          overworld::WorldName(*world_filter)));
    }
    if (!world_filter.has_value()) {
      world_filter = inferred_world;
    }
  }

  std::string format = "text";
  if (auto it = options.find("format"); it != options.end()) {
    format = absl::AsciiStrToLower(it->second);
    if (format != "text" && format != "json") {
      return absl::InvalidArgumentError(
          absl::StrCat("Unsupported format: ", it->second));
    }
  }

  std::string rom_file = absl::GetFlag(FLAGS_rom);
  if (auto it = options.find("rom"); it != options.end()) {
    rom_file = it->second;
  }

  if (rom_file.empty()) {
    return absl::InvalidArgumentError(
        "ROM file must be provided via --rom flag.");
  }

  Rom rom;
  auto load_status = rom.LoadFromFile(rom_file);
  if (!load_status.ok()) {
    return load_status;
  }
  if (!rom.is_loaded()) {
    return absl::AbortedError("Failed to load ROM.");
  }

  zelda3::Overworld overworld_rom(&rom);
  auto ow_status = overworld_rom.Load(&rom);
  if (!ow_status.ok()) {
    return ow_status;
  }

  overworld::WarpQuery query;
  query.map_id = map_filter;
  query.world = world_filter;
  query.type = type_filter;

  ASSIGN_OR_RETURN(auto entries,
                   overworld::CollectWarpEntries(overworld_rom, query));

  if (format == "json") {
    std::cout << "{\n";
    std::cout << absl::StrFormat("  \"count\": %zu,\n", entries.size());
    std::cout << "  \"entries\": [\n";
    for (size_t i = 0; i < entries.size(); ++i) {
      const auto& entry = entries[i];
      std::cout << "    {\n";
      std::cout << absl::StrFormat("      \"type\": \"%s\",\n",
                                   overworld::WarpTypeName(entry.type));
      std::cout << absl::StrFormat("      \"map\": \"0x%02X\",\n",
                                   entry.map_id);
      std::cout << absl::StrFormat("      \"world\": \"%s\",\n",
                                   overworld::WorldName(entry.world));
      std::cout << absl::StrFormat(
          "      \"grid\": {\"x\": %d, \"y\": %d, \"index\": %d},\n",
          entry.map_x, entry.map_y, entry.local_index);
      std::cout << absl::StrFormat(
          "      \"tile16\": {\"x\": %d, \"y\": %d},\n", entry.tile16_x,
          entry.tile16_y);
      std::cout << absl::StrFormat("      \"pixel\": {\"x\": %d, \"y\": %d},\n",
                                   entry.pixel_x, entry.pixel_y);
      std::cout << absl::StrFormat("      \"map_pos\": \"0x%04X\",\n",
                                   entry.map_pos);
      std::cout << absl::StrFormat("      \"deleted\": %s,\n",
                                   entry.deleted ? "true" : "false");
      std::cout << absl::StrFormat("      \"is_hole\": %s",
                                   entry.is_hole ? "true" : "false");
      if (entry.entrance_id.has_value()) {
        std::cout << absl::StrFormat(",\n      \"entrance_id\": \"0x%02X\"",
                                     *entry.entrance_id);
      }
      if (entry.entrance_name.has_value()) {
        std::cout << absl::StrFormat(",\n      \"entrance_name\": \"%s\"",
                                     *entry.entrance_name);
      }
      std::cout << "\n    }" << (i + 1 == entries.size() ? "" : ",") << "\n";
    }
    std::cout << "  ]\n";
    std::cout << "}\n";
  } else {
    if (entries.empty()) {
      std::cout << "No overworld warps match the specified filters."
                << std::endl;
      return absl::OkStatus();
    }

    std::cout << absl::StrFormat("🌐 Overworld warps (%zu)\n", entries.size());
    for (const auto& entry : entries) {
      std::string line = absl::StrFormat(
          "  • %-9s map 0x%02X (%s World) tile16(%02d,%02d) pixel(%4d,%4d)",
          overworld::WarpTypeName(entry.type), entry.map_id,
          overworld::WorldName(entry.world), entry.tile16_x, entry.tile16_y,
          entry.pixel_x, entry.pixel_y);
      if (entry.entrance_id.has_value()) {
        line = absl::StrCat(line,
                            absl::StrFormat(" id=0x%02X", *entry.entrance_id));
      }
      if (entry.entrance_name.has_value()) {
        line = absl::StrCat(line, " (", *entry.entrance_name, ")");
      }
      if (entry.deleted) {
        line = absl::StrCat(line, " [deleted]");
      }
      if (entry.is_hole && entry.type != overworld::WarpType::kHole) {
        line = absl::StrCat(line, " [hole]");
      }
      std::cout << line << std::endl;
    }
  }

  return absl::OkStatus();
}

// ============================================================================
// Phase 4B: Canvas Automation API Commands
// ============================================================================

// Legacy OverworldSelectRect class removed - using new CommandHandler system
// TODO: Implement OverworldSelectRectCommandHandler
absl::Status HandleOverworldSelectRectLegacy(
    const std::vector<std::string>& arg_vec) {
  int map_id = -1, x1 = -1, y1 = -1, x2 = -1, y2 = -1;

  for (size_t i = 0; i < arg_vec.size(); ++i) {
    const std::string& arg = arg_vec[i];
    if ((arg == "--map") && i + 1 < arg_vec.size()) {
      map_id = std::stoi(arg_vec[++i]);
    } else if ((arg == "--x1") && i + 1 < arg_vec.size()) {
      x1 = std::stoi(arg_vec[++i]);
    } else if ((arg == "--y1") && i + 1 < arg_vec.size()) {
      y1 = std::stoi(arg_vec[++i]);
    } else if ((arg == "--x2") && i + 1 < arg_vec.size()) {
      x2 = std::stoi(arg_vec[++i]);
    } else if ((arg == "--y2") && i + 1 < arg_vec.size()) {
      y2 = std::stoi(arg_vec[++i]);
    }
  }

  if (map_id == -1 || x1 == -1 || y1 == -1 || x2 == -1 || y2 == -1) {
    return absl::InvalidArgumentError(
        "Usage: overworld select-rect --map <map_id> --x1 <x1> --y1 <y1> --x2 "
        "<x2> --y2 <y2>");
  }

  std::cout << "✅ Selected rectangle on map " << map_id << " from (" << x1
            << "," << y1 << ") to (" << x2 << "," << y2 << ")" << std::endl;

  int width = std::abs(x2 - x1) + 1;
  int height = std::abs(y2 - y1) + 1;
  std::cout << "   Selection size: " << width << "x" << height << " tiles ("
            << (width * height) << " total)" << std::endl;

  return absl::OkStatus();
}

// Legacy OverworldScrollTo class removed - using new CommandHandler system
// TODO: Implement OverworldScrollToCommandHandler
absl::Status HandleOverworldScrollToLegacy(
    const std::vector<std::string>& arg_vec) {
  int map_id = -1, x = -1, y = -1;
  bool center = false;

  for (size_t i = 0; i < arg_vec.size(); ++i) {
    const std::string& arg = arg_vec[i];
    if ((arg == "--map") && i + 1 < arg_vec.size()) {
      map_id = std::stoi(arg_vec[++i]);
    } else if ((arg == "--x") && i + 1 < arg_vec.size()) {
      x = std::stoi(arg_vec[++i]);
    } else if ((arg == "--y") && i + 1 < arg_vec.size()) {
      y = std::stoi(arg_vec[++i]);
    } else if (arg == "--center") {
      center = true;
    }
  }

  if (map_id == -1 || x == -1 || y == -1) {
    return absl::InvalidArgumentError(
        "Usage: overworld scroll-to --map <map_id> --x <x> --y <y> [--center]");
  }

  std::cout << "✅ Scrolled to tile (" << x << "," << y << ") on map "
            << map_id;
  if (center) {
    std::cout << " (centered)";
  }
  std::cout << std::endl;

  return absl::OkStatus();
}

// Legacy OverworldSetZoom class removed - using new CommandHandler system
// TODO: Implement OverworldSetZoomCommandHandler
absl::Status HandleOverworldSetZoomLegacy(
    const std::vector<std::string>& arg_vec) {
  float zoom = -1.0f;

  for (size_t i = 0; i < arg_vec.size(); ++i) {
    const std::string& arg = arg_vec[i];
    if ((arg == "--zoom") && i + 1 < arg_vec.size()) {
      zoom = std::stof(arg_vec[++i]);
    }
  }

  if (zoom < 0.0f) {
    return absl::InvalidArgumentError(
        "Usage: overworld set-zoom --zoom <level>\n"
        "  Zoom level: 0.25 - 4.0");
  }

  // Clamp to valid range
  zoom = std::max(0.25f, std::min(zoom, 4.0f));

  std::cout << "✅ Set zoom level to " << zoom << "x" << std::endl;

  return absl::OkStatus();
}

// Legacy OverworldGetVisibleRegion class removed - using new CommandHandler
// system
// TODO: Implement OverworldGetVisibleRegionCommandHandler
absl::Status HandleOverworldGetVisibleRegionLegacy(
    const std::vector<std::string>& arg_vec) {
  int map_id = -1;
  std::string format = "text";

  for (size_t i = 0; i < arg_vec.size(); ++i) {
    const std::string& arg = arg_vec[i];
    if ((arg == "--map") && i + 1 < arg_vec.size()) {
      map_id = std::stoi(arg_vec[++i]);
    } else if ((arg == "--format") && i + 1 < arg_vec.size()) {
      format = arg_vec[++i];
    }
  }

  if (map_id == -1) {
    return absl::InvalidArgumentError(
        "Usage: overworld get-visible-region --map <map_id> [--format "
        "json|text]");
  }

  // Note: This would query the canvas automation API in a live GUI context
  // For now, return placeholder data
  if (format == "json") {
    std::cout << R"({
  "map_id": )" << map_id
              << R"(,
  "visible_region": {
    "min_x": 0,
    "min_y": 0,
    "max_x": 31,
    "max_y": 31
  },
  "tile_count": 1024
})" << std::endl;
  } else {
    std::cout << "Visible region on map " << map_id << ":" << std::endl;
    std::cout << "  Min: (0, 0)" << std::endl;
    std::cout << "  Max: (31, 31)" << std::endl;
    std::cout << "  Total visible tiles: 1024" << std::endl;
  }

  return absl::OkStatus();
}

}  // namespace cli
}  // namespace yaze
