#include "cli/z3ed.h"
#include "app/zelda3/overworld/overworld.h"

#include <algorithm>
#include <cctype>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

#include "absl/flags/flag.h"
#include "absl/flags/declare.h"
#include "absl/status/statusor.h"
#include "absl/strings/ascii.h"
#include "absl/strings/match.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/str_join.h"
#include "absl/strings/str_format.h"

ABSL_DECLARE_FLAG(std::string, rom);

namespace yaze {
namespace cli {

absl::Status OverworldGetTile::Run(const std::vector<std::string>& arg_vec) {
  if (arg_vec.size() < 3) {
    return absl::InvalidArgumentError("Usage: overworld get-tile --map <map_id> --x <x> --y <y>");
  }

  // TODO: Implement proper argument parsing
  int map_id = std::stoi(arg_vec[0]);
  int x = std::stoi(arg_vec[1]);
  int y = std::stoi(arg_vec[2]);

  std::string rom_file = absl::GetFlag(FLAGS_rom);
  if (rom_file.empty()) {
      return absl::InvalidArgumentError("ROM file must be provided via --rom flag.");
  }

  auto load_status = rom_.LoadFromFile(rom_file);
  if (!load_status.ok()) {
    return load_status;
  }
  if (!rom_.is_loaded()) {
      return absl::AbortedError("Failed to load ROM.");
  }

  zelda3::Overworld overworld(&rom_);
  auto ow_status = overworld.Load(&rom_);
  if (!ow_status.ok()) {
    return ow_status;
  }

  uint16_t tile = overworld.GetTile(x, y);

  std::cout << "Tile at (" << x << ", " << y << ") on map " << map_id << " is: 0x" << std::hex << tile << std::endl;

  return absl::OkStatus();
}

absl::Status OverworldSetTile::Run(const std::vector<std::string>& arg_vec) {
  if (arg_vec.size() < 4) {
    return absl::InvalidArgumentError("Usage: overworld set-tile --map <map_id> --x <x> --y <y> --tile <tile_id>");
  }

  // TODO: Implement proper argument parsing
  int map_id = std::stoi(arg_vec[0]);
  int x = std::stoi(arg_vec[1]);
  int y = std::stoi(arg_vec[2]);
  int tile_id = std::stoi(arg_vec[3], nullptr, 16);

  std::string rom_file = absl::GetFlag(FLAGS_rom);
  if (rom_file.empty()) {
      return absl::InvalidArgumentError("ROM file must be provided via --rom flag.");
  }

  auto load_status = rom_.LoadFromFile(rom_file);
  if (!load_status.ok()) {
    return load_status;
  }
  if (!rom_.is_loaded()) {
      return absl::AbortedError("Failed to load ROM.");
  }

  zelda3::Overworld overworld(&rom_);
  auto status = overworld.Load(&rom_);
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
  auto save_status = rom_.SaveToFile({.filename = rom_file});
  if (!save_status.ok()) {
    return save_status;
  }

  std::cout << "✅ Set tile at (" << x << ", " << y << ") on map " << map_id 
            << " to: 0x" << std::hex << tile_id << std::dec << std::endl;

  return absl::OkStatus();
}

namespace {

constexpr absl::string_view kFindTileUsage =
    "Usage: overworld find-tile --tile <tile_id> [--map <map_id>] [--world <light|dark|special|0|1|2>] [--format <json|text>]";

absl::StatusOr<int> ParseNumeric(const std::string& value, int base = 0) {
  try {
    size_t processed = 0;
    int result = std::stoi(value, &processed, base);
    if (processed != value.size()) {
      return absl::InvalidArgumentError(
          absl::StrCat("Invalid numeric value: ", value));
    }
    return result;
  } catch (const std::exception&) {
    return absl::InvalidArgumentError(
        absl::StrCat("Invalid numeric value: ", value));
  }
}

absl::StatusOr<int> WorldFromString(const std::string& value) {
  std::string lower = absl::AsciiStrToLower(value);
  if (lower == "0" || lower == "light") {
    return 0;
  }
  if (lower == "1" || lower == "dark") {
    return 1;
  }
  if (lower == "2" || lower == "special") {
    return 2;
  }
  return absl::InvalidArgumentError(
      absl::StrCat("Unknown world value: ", value));
}

absl::StatusOr<int> WorldFromMapId(int map_id) {
  if (map_id < 0) {
    return absl::InvalidArgumentError("Map ID must be non-negative");
  }
  if (map_id < 0x40) {
    return 0;
  }
  if (map_id < 0x80) {
    return 1;
  }
  if (map_id < 0xA0) {
    return 2;
  }
  return absl::InvalidArgumentError(
      absl::StrCat("Map ID out of range: 0x", absl::StrFormat("%02X", map_id)));
}

std::string WorldName(int world) {
  switch (world) {
    case 0:
      return "Light";
    case 1:
      return "Dark";
    case 2:
      return "Special";
    default:
      return absl::StrCat("Unknown(", world, ")");
  }
}

}  // namespace

absl::Status OverworldFindTile::Run(const std::vector<std::string>& arg_vec) {
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
    return absl::InvalidArgumentError(
        absl::StrCat("Unexpected positional arguments: ",
                     absl::StrJoin(positional, ", "), "\n", kFindTileUsage));
  }

  auto tile_it = options.find("tile");
  if (tile_it == options.end()) {
    return absl::InvalidArgumentError(
        absl::StrCat("Missing required --tile argument\n", kFindTileUsage));
  }

  ASSIGN_OR_RETURN(int tile_value, ParseNumeric(tile_it->second));
  if (tile_value < 0 || tile_value > 0xFFFF) {
    return absl::InvalidArgumentError(
        absl::StrCat("Tile ID must be between 0x0000 and 0xFFFF (got ",
                     tile_it->second, ")"));
  }
  const uint16_t tile_id = static_cast<uint16_t>(tile_value);

  std::optional<int> map_filter;
  if (auto map_it = options.find("map"); map_it != options.end()) {
    ASSIGN_OR_RETURN(int map_value, ParseNumeric(map_it->second));
    if (map_value < 0 || map_value >= 0xA0) {
      return absl::InvalidArgumentError(
          absl::StrCat("Map ID out of range: ", map_it->second));
    }
    map_filter = map_value;
  }

  std::optional<int> world_filter;
  if (auto world_it = options.find("world"); world_it != options.end()) {
    ASSIGN_OR_RETURN(int parsed_world, WorldFromString(world_it->second));
    world_filter = parsed_world;
  }

  if (map_filter.has_value()) {
    ASSIGN_OR_RETURN(int inferred_world, WorldFromMapId(*map_filter));
    if (world_filter.has_value() && inferred_world != *world_filter) {
      return absl::InvalidArgumentError(
          absl::StrCat("Map 0x",
                       absl::StrFormat("%02X", *map_filter),
                       " belongs to the ", WorldName(inferred_world),
                       " World but --world requested ", WorldName(*world_filter)));
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

  auto load_status = rom_.LoadFromFile(rom_file);
  if (!load_status.ok()) {
    return load_status;
  }
  if (!rom_.is_loaded()) {
    return absl::AbortedError("Failed to load ROM.");
  }

  zelda3::Overworld overworld(&rom_);
  auto ow_status = overworld.Load(&rom_);
  if (!ow_status.ok()) {
    return ow_status;
  }

  struct TileMatch {
    int map_id;
    int world;
    int local_x;
    int local_y;
    int global_x;
    int global_y;
  };

  std::vector<int> worlds_to_search;
  if (world_filter.has_value()) {
    worlds_to_search.push_back(*world_filter);
  } else {
    worlds_to_search = {0, 1, 2};
  }

  std::vector<TileMatch> matches;

  for (int world : worlds_to_search) {
    int world_start = 0;
    int world_maps = 0;
    switch (world) {
      case 0:
        world_start = 0x00;
        world_maps = 0x40;
        break;
      case 1:
        world_start = 0x40;
        world_maps = 0x40;
        break;
      case 2:
        world_start = 0x80;
        world_maps = 0x20;
        break;
      default:
        return absl::InvalidArgumentError(
            absl::StrCat("Unknown world index: ", world));
    }

    overworld.set_current_world(world);

    for (int local_map = 0; local_map < world_maps; ++local_map) {
      int map_id = world_start + local_map;
      if (map_filter.has_value() && map_id != *map_filter) {
        continue;
      }

      int map_x_index = local_map % 8;
      int map_y_index = local_map / 8;

      int global_x_start = map_x_index * 32;
      int global_y_start = map_y_index * 32;

      for (int local_y = 0; local_y < 32; ++local_y) {
        for (int local_x = 0; local_x < 32; ++local_x) {
          int global_x = global_x_start + local_x;
          int global_y = global_y_start + local_y;

          uint16_t tile = overworld.GetTile(global_x, global_y);
          if (tile == tile_id) {
            matches.push_back({map_id, world, local_x, local_y, global_x,
                              global_y});
          }
        }
      }
    }
  }

  if (format == "json") {
    std::cout << "{\n";
    std::cout << absl::StrFormat(
        "  \"tile\": \"0x%04X\",\n", tile_id);
    std::cout << absl::StrFormat(
        "  \"match_count\": %zu,\n", matches.size());
    std::cout << "  \"matches\": [\n";
    for (size_t i = 0; i < matches.size(); ++i) {
      const auto& match = matches[i];
      std::cout << absl::StrFormat(
          "    {\"map\": \"0x%02X\", \"world\": \"%s\", "
          "\"local\": {\"x\": %d, \"y\": %d}, "
          "\"global\": {\"x\": %d, \"y\": %d}}%s\n",
          match.map_id, WorldName(match.world), match.local_x, match.local_y,
          match.global_x, match.global_y,
          (i + 1 == matches.size()) ? "" : ",");
    }
    std::cout << "  ]\n";
    std::cout << "}\n";
  } else {
    std::cout << absl::StrFormat(
        "🔎 Tile 0x%04X → %zu match(es)\n", tile_id, matches.size());
    if (matches.empty()) {
      std::cout << "  No matches found." << std::endl;
      return absl::OkStatus();
    }

    for (const auto& match : matches) {
      std::cout << absl::StrFormat(
          "  • Map 0x%02X (%s World) local(%2d,%2d) global(%3d,%3d)\n",
          match.map_id, WorldName(match.world), match.local_x, match.local_y,
          match.global_x, match.global_y);
    }
  }

  return absl::OkStatus();
}

} // namespace cli
} // namespace yaze
