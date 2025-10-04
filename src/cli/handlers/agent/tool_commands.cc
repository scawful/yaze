#include "cli/handlers/agent/commands.h"

#include <iostream>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "absl/flags/declare.h"
#include "absl/flags/flag.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/ascii.h"
#include "absl/strings/match.h"
#include "absl/strings/numbers.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/str_format.h"
#include "absl/strings/str_join.h"
#include "app/core/project.h"
#include "app/rom.h"
#include "app/zelda3/dungeon/room.h"
#include "app/zelda3/overworld/overworld.h"
#include "cli/handlers/overworld_inspect.h"
#include "cli/service/resources/resource_context_builder.h"

ABSL_DECLARE_FLAG(std::string, rom);

namespace yaze {
namespace cli {
namespace agent {

namespace {

absl::StatusOr<Rom> LoadRomFromFlag() {
  std::string rom_path = absl::GetFlag(FLAGS_rom);
  if (rom_path.empty()) {
    return absl::FailedPreconditionError(
        "No ROM loaded. Use --rom=<path> to specify ROM file.");
  }

  Rom rom;
  auto status = rom.LoadFromFile(rom_path);
  if (!status.ok()) {
    return absl::FailedPreconditionError(absl::StrFormat(
        "Failed to load ROM from '%s': %s", rom_path, status.message()));
  }

  return rom;
}

absl::StatusOr<Rom> LoadRomFromPathOrFlag(
    const std::optional<std::string>& override_path) {
  if (override_path.has_value()) {
    Rom rom;
    auto status = rom.LoadFromFile(*override_path);
    if (!status.ok()) {
      return absl::FailedPreconditionError(absl::StrFormat(
          "Failed to load ROM from '%s': %s", *override_path,
          status.message()));
    }
    return rom;
  }
  return LoadRomFromFlag();
}

}  // namespace

absl::Status HandleResourceListCommand(
  const std::vector<std::string>& arg_vec, Rom* rom_context) {
  std::string type;
  std::string format = "table";

  for (size_t i = 0; i < arg_vec.size(); ++i) {
    const std::string& token = arg_vec[i];
    if (token == "--type") {
      if (i + 1 >= arg_vec.size()) {
        return absl::InvalidArgumentError("--type requires a value.");
      }
      type = arg_vec[++i];
    } else if (absl::StartsWith(token, "--type=")) {
      type = token.substr(7);
    } else if (token == "--format") {
      if (i + 1 >= arg_vec.size()) {
        return absl::InvalidArgumentError("--format requires a value.");
      }
      format = arg_vec[++i];
    } else if (absl::StartsWith(token, "--format=")) {
      format = token.substr(9);
    }
  }

  if (type.empty()) {
    return absl::InvalidArgumentError(
        "Usage: agent resource-list --type <type> [--format <table|json>]");
  }

  Rom rom_storage;
  Rom* rom = nullptr;
  if (rom_context != nullptr && rom_context->is_loaded()) {
    rom = rom_context;
  } else {
    auto rom_or = LoadRomFromFlag();
    if (!rom_or.ok()) {
      return rom_or.status();
    }
    rom_storage = std::move(rom_or.value());
    rom = &rom_storage;
  }

  // Initialize embedded labels if not already loaded
  if (rom->resource_label() && !rom->resource_label()->labels_loaded_) {
    core::YazeProject project;
    auto labels_status = project.InitializeEmbeddedLabels();
    if (labels_status.ok()) {
      rom->resource_label()->labels_ = project.resource_labels;
      rom->resource_label()->labels_loaded_ = true;
    }
  }

  ResourceContextBuilder context_builder(rom);
  auto labels_or = context_builder.GetLabels(type);
  if (!labels_or.ok()) {
    return labels_or.status();
  }
  auto labels = std::move(labels_or.value());

  if (format == "json") {
    std::cout << "{\n";
    bool first = true;
    for (const auto& [key, value] : labels) {
      if (!first) {
        std::cout << ",\n";
      }
      std::cout << "  \"" << key << "\": \"" << value << "\"";
      first = false;
    }
    std::cout << "\n}\n";
  } else {
    std::cout << "=== " << absl::AsciiStrToUpper(type) << " Labels ===\n";
    for (const auto& [key, value] : labels) {
      std::cout << absl::StrFormat("  %-10s : %s\n", key, value);
    }
  }

  return absl::OkStatus();
}

absl::Status HandleDungeonListSpritesCommand(
  const std::vector<std::string>& arg_vec, Rom* rom_context) {
  std::string room_id_str;
  std::string format = "table";

  for (size_t i = 0; i < arg_vec.size(); ++i) {
    const std::string& token = arg_vec[i];
    if (token == "--room") {
      if (i + 1 >= arg_vec.size()) {
        return absl::InvalidArgumentError("--room requires a value.");
      }
      room_id_str = arg_vec[++i];
    } else if (absl::StartsWith(token, "--room=")) {
      room_id_str = token.substr(7);
    } else if (token == "--format") {
      if (i + 1 >= arg_vec.size()) {
        return absl::InvalidArgumentError("--format requires a value.");
      }
      format = arg_vec[++i];
    } else if (absl::StartsWith(token, "--format=")) {
      format = token.substr(9);
    }
  }

  if (room_id_str.empty()) {
    return absl::InvalidArgumentError(
        "Usage: agent dungeon-list-sprites --room <id> [--format <table|json>]");
  }

  int room_id;
  if (!absl::SimpleHexAtoi(room_id_str, &room_id)) {
    return absl::InvalidArgumentError(
        "Invalid room ID format. Must be hex.");
  }

  Rom rom_storage;
  Rom* rom = nullptr;
  if (rom_context != nullptr && rom_context->is_loaded()) {
    rom = rom_context;
  } else {
    auto rom_or = LoadRomFromFlag();
    if (!rom_or.ok()) {
      return rom_or.status();
    }
    rom_storage = std::move(rom_or.value());
    rom = &rom_storage;
  }

  auto room = zelda3::LoadRoomFromRom(rom, room_id);
  const auto& sprites = room.GetSprites();

  if (format == "json") {
    std::cout << "[\n";
    for (size_t i = 0; i < sprites.size(); ++i) {
      const auto& sprite = sprites[i];
      std::cout << "  {\n";
      std::cout << "    \"id\": " << sprite.id() << ",\n";
      std::cout << "    \"x\": " << sprite.x() << ",\n";
      std::cout << "    \"y\": " << sprite.y() << "\n";
      std::cout << "  }" << (i + 1 == sprites.size() ? "" : ",") << "\n";
    }
    std::cout << "]\n";
  } else {
    std::cout << "=== Sprites in Room " << room_id_str << " ===\n";
    std::cout << absl::StrFormat("%-10s %-5s %-5s\n", "ID (Hex)", "X", "Y");
    std::cout << std::string(22, '-') << "\n";
    for (const auto& sprite : sprites) {
      std::cout << absl::StrFormat("0x%-8X %-5d %-5d\n", sprite.id(),
                                   sprite.x(), sprite.y());
    }
  }

  return absl::OkStatus();
}

absl::Status HandleOverworldFindTileCommand(
  const std::vector<std::string>& arg_vec, Rom* rom_context) {
  std::optional<std::string> tile_value;
  std::optional<std::string> map_value;
  std::optional<std::string> world_value;
  std::string format = "json";
  std::optional<std::string> rom_override;

  for (size_t i = 0; i < arg_vec.size(); ++i) {
    const std::string& token = arg_vec[i];
    if (token == "--tile") {
      if (i + 1 >= arg_vec.size()) {
        return absl::InvalidArgumentError("--tile requires a value.");
      }
      tile_value = arg_vec[++i];
    } else if (absl::StartsWith(token, "--tile=")) {
      tile_value = token.substr(7);
    } else if (token == "--map") {
      if (i + 1 >= arg_vec.size()) {
        return absl::InvalidArgumentError("--map requires a value.");
      }
      map_value = arg_vec[++i];
    } else if (absl::StartsWith(token, "--map=")) {
      map_value = token.substr(6);
    } else if (token == "--world") {
      if (i + 1 >= arg_vec.size()) {
        return absl::InvalidArgumentError("--world requires a value.");
      }
      world_value = arg_vec[++i];
    } else if (absl::StartsWith(token, "--world=")) {
      world_value = token.substr(8);
    } else if (token == "--format") {
      if (i + 1 >= arg_vec.size()) {
        return absl::InvalidArgumentError("--format requires a value.");
      }
      format = absl::AsciiStrToLower(arg_vec[++i]);
    } else if (absl::StartsWith(token, "--format=")) {
      format = absl::AsciiStrToLower(token.substr(9));
    } else if (token == "--rom") {
      if (i + 1 >= arg_vec.size()) {
        return absl::InvalidArgumentError("--rom requires a value.");
      }
      rom_override = arg_vec[++i];
    } else if (absl::StartsWith(token, "--rom=")) {
      rom_override = token.substr(6);
    }
  }

  if (!tile_value.has_value()) {
    return absl::InvalidArgumentError(
        "Usage: agent overworld-find-tile --tile <id> [--map <id>] [--world <light|dark|special>] [--format <json|text>]");
  }

  ASSIGN_OR_RETURN(int tile_numeric,
                   overworld::ParseNumeric(*tile_value));
  if (tile_numeric < 0 || tile_numeric > 0xFFFF) {
    return absl::InvalidArgumentError(
        absl::StrCat("Tile ID must be between 0x0000 and 0xFFFF (got ",
                      *tile_value, ")"));
  }

  std::optional<int> map_filter;
  if (map_value.has_value()) {
    ASSIGN_OR_RETURN(int parsed_map,
                     overworld::ParseNumeric(*map_value));
    if (parsed_map < 0 || parsed_map >= zelda3::kNumOverworldMaps) {
      return absl::InvalidArgumentError(
          absl::StrCat("Map ID out of range: ", *map_value));
    }
    map_filter = parsed_map;
  }

  std::optional<int> world_filter;
  if (world_value.has_value()) {
    ASSIGN_OR_RETURN(int parsed_world,
                     overworld::ParseWorldSpecifier(*world_value));
    world_filter = parsed_world;
  }

  if (map_filter.has_value()) {
    ASSIGN_OR_RETURN(int inferred_world,
                     overworld::InferWorldFromMapId(*map_filter));
    if (world_filter.has_value() && inferred_world != *world_filter) {
      return absl::InvalidArgumentError(
          absl::StrCat("Map 0x",
                       absl::StrFormat("%02X", *map_filter),
                       " belongs to the ",
                       overworld::WorldName(inferred_world),
                       " World but --world requested ",
                       overworld::WorldName(*world_filter)));
    }
    if (!world_filter.has_value()) {
      world_filter = inferred_world;
    }
  }

  if (format != "json" && format != "text") {
    return absl::InvalidArgumentError(
        absl::StrCat("Unsupported format: ", format));
  }

  Rom rom_storage;
  Rom* rom = nullptr;
  if (rom_context != nullptr && rom_context->is_loaded() && !rom_override.has_value()) {
    rom = rom_context;
  } else {
    ASSIGN_OR_RETURN(auto rom_or,
                     LoadRomFromPathOrFlag(rom_override));
    rom_storage = std::move(rom_or);
    rom = &rom_storage;
  }

  zelda3::Overworld overworld_data(rom);
  auto load_status = overworld_data.Load(rom);
  if (!load_status.ok()) {
    return load_status;
  }

  overworld::TileSearchOptions search_options;
  search_options.map_id = map_filter;
  search_options.world = world_filter;

  ASSIGN_OR_RETURN(auto matches,
                   overworld::FindTileMatches(overworld_data,
                                              static_cast<uint16_t>(tile_numeric),
                                              search_options));

  if (format == "json") {
    std::cout << "{\n";
    std::cout << absl::StrFormat(
        "  \"tile\": \"0x%04X\",\n", tile_numeric);
    std::cout << absl::StrFormat(
        "  \"match_count\": %zu,\n", matches.size());
    std::cout << "  \"matches\": [\n";
    for (size_t i = 0; i < matches.size(); ++i) {
      const auto& match = matches[i];
      std::cout << absl::StrFormat(
          "    {\"map\": \"0x%02X\", \"world\": \"%s\", "
          "\"local\": {\"x\": %d, \"y\": %d}, "
          "\"global\": {\"x\": %d, \"y\": %d}}%s\n",
          match.map_id, overworld::WorldName(match.world), match.local_x,
          match.local_y,
          match.global_x, match.global_y,
          (i + 1 == matches.size()) ? "" : ",");
    }
    std::cout << "  ]\n";
    std::cout << "}\n";
  } else {
    std::cout << absl::StrFormat(
        "🔎 Tile 0x%04X → %zu match(es)\n",
        tile_numeric, matches.size());
    if (matches.empty()) {
      std::cout << "  No matches found." << std::endl;
      return absl::OkStatus();
    }

    for (const auto& match : matches) {
      std::cout << absl::StrFormat(
          "  • Map 0x%02X (%s World) local(%2d,%2d) global(%3d,%3d)\n",
          match.map_id, overworld::WorldName(match.world), match.local_x,
          match.local_y,
          match.global_x, match.global_y);
    }
  }

  return absl::OkStatus();
}

absl::Status HandleOverworldDescribeMapCommand(
  const std::vector<std::string>& arg_vec, Rom* rom_context) {
  std::optional<std::string> map_value;
  std::string format = "json";
  std::optional<std::string> rom_override;

  for (size_t i = 0; i < arg_vec.size(); ++i) {
    const std::string& token = arg_vec[i];
    if (token == "--map") {
      if (i + 1 >= arg_vec.size()) {
        return absl::InvalidArgumentError(
            "--map requires a value. Usage: agent overworld-describe-map --map <id> [--format <json|text>]");
      }
      map_value = arg_vec[++i];
    } else if (absl::StartsWith(token, "--map=")) {
      map_value = token.substr(6);
    } else if (token == "--format") {
      if (i + 1 >= arg_vec.size()) {
        return absl::InvalidArgumentError("--format requires a value.");
      }
      format = absl::AsciiStrToLower(arg_vec[++i]);
    } else if (absl::StartsWith(token, "--format=")) {
      format = absl::AsciiStrToLower(token.substr(9));
    } else if (token == "--rom") {
      if (i + 1 >= arg_vec.size()) {
        return absl::InvalidArgumentError("--rom requires a value.");
      }
      rom_override = arg_vec[++i];
    } else if (absl::StartsWith(token, "--rom=")) {
      rom_override = token.substr(6);
    }
  }

  if (!map_value.has_value()) {
    return absl::InvalidArgumentError(
        "Usage: agent overworld-describe-map --map <id> [--format <json|text>]");
  }

  ASSIGN_OR_RETURN(int map_id,
                   overworld::ParseNumeric(*map_value));
  if (map_id < 0 || map_id >= zelda3::kNumOverworldMaps) {
    return absl::InvalidArgumentError(
        absl::StrCat("Map ID out of range: ", *map_value));
  }

  if (format != "json" && format != "text") {
    return absl::InvalidArgumentError(
        absl::StrCat("Unsupported format: ", format));
  }

  Rom rom_storage;
  Rom* rom = nullptr;
  if (rom_context != nullptr && rom_context->is_loaded() && !rom_override.has_value()) {
    rom = rom_context;
  } else {
    ASSIGN_OR_RETURN(auto rom_or,
                     LoadRomFromPathOrFlag(rom_override));
    rom_storage = std::move(rom_or);
    rom = &rom_storage;
  }

  zelda3::Overworld overworld_data(rom);
  auto load_status = overworld_data.Load(rom);
  if (!load_status.ok()) {
    return load_status;
  }

  ASSIGN_OR_RETURN(auto summary,
                   overworld::BuildMapSummary(overworld_data, map_id));

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
        "  \"grid\": {\"x\": %d, \"y\": %d, \"index\": %d},\n",
        summary.map_x, summary.map_y, summary.local_index);
    std::cout << absl::StrFormat(
        "  \"size\": {\"label\": \"%s\", \"is_large\": %s, \"parent\": \"0x%02X\", \"quadrant\": %d},\n",
        summary.area_size, summary.is_large_map ? "true" : "false",
        summary.parent_map, summary.large_quadrant);
    std::cout << absl::StrFormat(
        "  \"message\": \"0x%04X\",\n", summary.message_id);
    std::cout << absl::StrFormat(
        "  \"area_graphics\": \"0x%02X\",\n", summary.area_graphics);
    std::cout << absl::StrFormat(
        "  \"area_palette\": \"0x%02X\",\n", summary.area_palette);
    std::cout << absl::StrFormat(
        "  \"main_palette\": \"0x%02X\",\n", summary.main_palette);
    std::cout << absl::StrFormat(
        "  \"animated_gfx\": \"0x%02X\",\n", summary.animated_gfx);
    std::cout << absl::StrFormat(
        "  \"subscreen_overlay\": \"0x%04X\",\n",
        summary.subscreen_overlay);
    std::cout << absl::StrFormat(
        "  \"area_specific_bg_color\": \"0x%04X\",\n",
        summary.area_specific_bg_color);
    std::cout << absl::StrFormat(
        "  \"sprite_graphics\": %s,\n", join_hex_json(summary.sprite_graphics));
    std::cout << absl::StrFormat(
        "  \"sprite_palettes\": %s,\n", join_hex_json(summary.sprite_palettes));
    std::cout << absl::StrFormat(
        "  \"area_music\": %s,\n", join_hex_json(summary.area_music));
    std::cout << absl::StrFormat(
        "  \"static_graphics\": %s,\n",
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
        "  Size: %s%s | Parent: 0x%02X | Quadrant: %d\n",
        summary.area_size, summary.is_large_map ? " (large)" : "",
        summary.parent_map, summary.large_quadrant);
    std::cout << absl::StrFormat(
        "  Message: 0x%04X | Area GFX: 0x%02X | Area Palette: 0x%02X\n",
        summary.message_id, summary.area_graphics, summary.area_palette);
    std::cout << absl::StrFormat(
        "  Main Palette: 0x%02X | Animated GFX: 0x%02X | Overlay: %s (0x%04X)\n",
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

absl::Status HandleOverworldListWarpsCommand(
  const std::vector<std::string>& arg_vec, Rom* rom_context) {
  std::optional<std::string> map_value;
  std::optional<std::string> world_value;
  std::optional<std::string> type_value;
  std::string format = "json";
  std::optional<std::string> rom_override;

  for (size_t i = 0; i < arg_vec.size(); ++i) {
    const std::string& token = arg_vec[i];
    if (token == "--map") {
      if (i + 1 >= arg_vec.size()) {
        return absl::InvalidArgumentError("--map requires a value.");
      }
      map_value = arg_vec[++i];
    } else if (absl::StartsWith(token, "--map=")) {
      map_value = token.substr(6);
    } else if (token == "--world") {
      if (i + 1 >= arg_vec.size()) {
        return absl::InvalidArgumentError("--world requires a value.");
      }
      world_value = arg_vec[++i];
    } else if (absl::StartsWith(token, "--world=")) {
      world_value = token.substr(8);
    } else if (token == "--type") {
      if (i + 1 >= arg_vec.size()) {
        return absl::InvalidArgumentError("--type requires a value.");
      }
      type_value = arg_vec[++i];
    } else if (absl::StartsWith(token, "--type=")) {
      type_value = token.substr(7);
    } else if (token == "--format") {
      if (i + 1 >= arg_vec.size()) {
        return absl::InvalidArgumentError("--format requires a value.");
      }
      format = absl::AsciiStrToLower(arg_vec[++i]);
    } else if (absl::StartsWith(token, "--format=")) {
      format = absl::AsciiStrToLower(token.substr(9));
    } else if (token == "--rom") {
      if (i + 1 >= arg_vec.size()) {
        return absl::InvalidArgumentError("--rom requires a value.");
      }
      rom_override = arg_vec[++i];
    } else if (absl::StartsWith(token, "--rom=")) {
      rom_override = token.substr(6);
    }
  }

  if (format != "json" && format != "text") {
    return absl::InvalidArgumentError(
        absl::StrCat("Unsupported format: ", format));
  }

  std::optional<int> map_filter;
  if (map_value.has_value()) {
    ASSIGN_OR_RETURN(int map_id,
                     overworld::ParseNumeric(*map_value));
    if (map_id < 0 || map_id >= zelda3::kNumOverworldMaps) {
      return absl::InvalidArgumentError(
          absl::StrCat("Map ID out of range: ", *map_value));
    }
    map_filter = map_id;
  }

  std::optional<int> world_filter;
  if (world_value.has_value()) {
    ASSIGN_OR_RETURN(int world_id,
                     overworld::ParseWorldSpecifier(*world_value));
    world_filter = world_id;
  }

  std::optional<overworld::WarpType> type_filter;
  if (type_value.has_value()) {
    std::string lower = absl::AsciiStrToLower(*type_value);
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
          absl::StrCat("Unknown warp type: ", *type_value));
    }
  }

  if (map_filter.has_value()) {
    ASSIGN_OR_RETURN(int inferred_world,
                     overworld::InferWorldFromMapId(*map_filter));
    if (world_filter.has_value() && inferred_world != *world_filter) {
      return absl::InvalidArgumentError(
          absl::StrCat("Map 0x",
                       absl::StrFormat("%02X", *map_filter),
                       " belongs to the ",
                       overworld::WorldName(inferred_world),
                       " World but --world requested ",
                       overworld::WorldName(*world_filter)));
    }
    if (!world_filter.has_value()) {
      world_filter = inferred_world;
    }
  }

  Rom rom_storage;
  Rom* rom = nullptr;
  if (rom_context != nullptr && rom_context->is_loaded() && !rom_override.has_value()) {
    rom = rom_context;
  } else {
    ASSIGN_OR_RETURN(auto rom_or,
                     LoadRomFromPathOrFlag(rom_override));
    rom_storage = std::move(rom_or);
    rom = &rom_storage;
  }

  zelda3::Overworld overworld_data(rom);
  auto load_status = overworld_data.Load(rom);
  if (!load_status.ok()) {
    return load_status;
  }

  overworld::WarpQuery query;
  query.map_id = map_filter;
  query.world = world_filter;
  query.type = type_filter;

  ASSIGN_OR_RETURN(auto entries,
                   overworld::CollectWarpEntries(overworld_data, query));

  if (format == "json") {
    std::cout << "{\n";
    std::cout << absl::StrFormat("  \"count\": %zu,\n", entries.size());
    std::cout << "  \"entries\": [\n";
    for (size_t i = 0; i < entries.size(); ++i) {
      const auto& entry = entries[i];
      std::cout << "    {\n";
      std::cout << absl::StrFormat(
          "      \"type\": \"%s\",\n",
          overworld::WarpTypeName(entry.type));
      std::cout << absl::StrFormat(
          "      \"map\": \"0x%02X\",\n", entry.map_id);
      std::cout << absl::StrFormat(
          "      \"world\": \"%s\",\n",
          overworld::WorldName(entry.world));
      std::cout << absl::StrFormat(
          "      \"grid\": {\"x\": %d, \"y\": %d, \"index\": %d},\n",
          entry.map_x, entry.map_y, entry.local_index);
      std::cout << absl::StrFormat(
          "      \"tile16\": {\"x\": %d, \"y\": %d},\n",
          entry.tile16_x, entry.tile16_y);
      std::cout << absl::StrFormat(
          "      \"pixel\": {\"x\": %d, \"y\": %d},\n",
          entry.pixel_x, entry.pixel_y);
      std::cout << absl::StrFormat(
          "      \"map_pos\": \"0x%04X\",\n", entry.map_pos);
      std::cout << absl::StrFormat(
          "      \"deleted\": %s,\n", entry.deleted ? "true" : "false");
      std::cout << absl::StrFormat(
          "      \"is_hole\": %s",
          entry.is_hole ? "true" : "false");
      if (entry.entrance_id.has_value()) {
        std::cout << absl::StrFormat(
            ",\n      \"entrance_id\": \"0x%02X\"",
            *entry.entrance_id);
      }
      if (entry.entrance_name.has_value()) {
        std::cout << absl::StrFormat(
            ",\n      \"entrance_name\": \"%s\"",
            *entry.entrance_name);
      }
      std::cout << "\n    }" << (i + 1 == entries.size() ? "" : ",") << "\n";
    }
    std::cout << "  ]\n";
    std::cout << "}\n";
  } else {
    if (entries.empty()) {
      std::cout << "No overworld warps match the specified filters." << std::endl;
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

}  // namespace agent
}  // namespace cli
}  // namespace yaze
