#include "cli/handlers/agent/commands.h"

#include <algorithm>
#include <iostream>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "absl/base/macros.h"
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
#include "cli/handlers/message.h"
#include "cli/handlers/overworld_inspect.h"
#include "cli/service/resources/resource_context_builder.h"
#include "util/macro.h"

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

absl::Status HandleResourceSearchCommand(
  const std::vector<std::string>& arg_vec, Rom* rom_context) {
  std::string query;
  std::string type = "all";
  std::string format = "json";

  for (size_t i = 0; i < arg_vec.size(); ++i) {
    const std::string& token = arg_vec[i];
    if (token == "--query") {
      if (i + 1 >= arg_vec.size()) {
        return absl::InvalidArgumentError("--query requires a value.");
      }
      query = arg_vec[++i];
    } else if (absl::StartsWith(token, "--query=")) {
      query = token.substr(8);
    } else if (token == "--type") {
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

  if (query.empty()) {
    return absl::InvalidArgumentError(
        "Usage: agent resource-search --query <text> [--type <category>] [--format <json|text>]");
  }

  format = absl::AsciiStrToLower(format);
  if (format != "json" && format != "text") {
    return absl::InvalidArgumentError("--format must be either json or text");
  }

  auto normalize_category = [](std::string value) {
    value = absl::AsciiStrToLower(value);
    if (value.size() > 1 && value.back() == 's') {
      value.pop_back();
    }
    if (value == "tile16s") {
      return std::string("tile16");
    }
    return value;
  };

  const std::vector<std::string> known_categories = {
      "overworld", "dungeon", "entrance", "room",
      "sprite", "palette", "item", "tile16"};

  std::vector<std::string> categories;
  std::string normalized_type = normalize_category(type);
  if (normalized_type == "all" || normalized_type.empty()) {
    categories = known_categories;
  } else {
    bool recognized = false;
    for (const auto& candidate : known_categories) {
      if (candidate == normalized_type) {
        recognized = true;
        break;
      }
    }
    if (!recognized) {
      return absl::InvalidArgumentError(
          absl::StrCat("Unknown resource category: ", type,
                        ". Known categories: overworld, dungeon, entrance, room, sprite, palette, item, tile16."));
    }
    categories.push_back(normalized_type);
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

  // Ensure labels are available similar to resource-list
  if (rom->resource_label() && !rom->resource_label()->labels_loaded_) {
    core::YazeProject project;
    auto labels_status = project.InitializeEmbeddedLabels();
    if (labels_status.ok()) {
      rom->resource_label()->labels_ = project.resource_labels;
      rom->resource_label()->labels_loaded_ = true;
    }
  }

  ResourceContextBuilder context_builder(rom);

  struct SearchResult {
    std::string category;
    std::string id;
    std::string label;
  };

  std::vector<SearchResult> results;
  std::string lowered_query = absl::AsciiStrToLower(query);

  for (const auto& category : categories) {
    auto labels_or = context_builder.GetLabels(category);
    if (!labels_or.ok()) {
      // If the category was explicitly requested and not "all", surface the error.
      if (normalized_type != "all") {
        return labels_or.status();
      }
      continue;
    }

    const auto& labels = labels_or.value();
    for (const auto& [id, label] : labels) {
      std::string lowered_label = absl::AsciiStrToLower(label);
      std::string lowered_id = absl::AsciiStrToLower(id);
      if (lowered_label.find(lowered_query) != std::string::npos ||
          lowered_id.find(lowered_query) != std::string::npos) {
        results.push_back({category, id, label});
      }
    }
  }

  std::sort(results.begin(), results.end(),
            [](const SearchResult& a, const SearchResult& b) {
              if (a.category == b.category) {
                return a.id < b.id;
              }
              return a.category < b.category;
            });

  if (results.empty()) {
    if (format == "json") {
      std::cout << "{\n"
                << "  \"query\": \"" << query << "\",\n"
                << "  \"match_count\": 0,\n"
                << "  \"results\": []\n"
                << "}\n";
    } else {
      std::cout << absl::StrFormat(
          "🔍 No matches found for \"%s\" in %s resources.\n",
          query, normalized_type == "all" ? std::string("any") : type);
    }
    return absl::OkStatus();
  }

  if (format == "json") {
    std::cout << "{\n";
    std::cout << "  \"query\": \"" << query << "\",\n";
    std::cout << absl::StrFormat("  \"match_count\": %zu,\n", results.size());
    std::cout << "  \"results\": [\n";
    for (size_t i = 0; i < results.size(); ++i) {
      const auto& result = results[i];
      std::cout << absl::StrFormat(
          "    {\"category\": \"%s\", \"id\": \"%s\", \"label\": \"%s\"}%s\n",
          result.category, result.id, result.label,
          (i + 1 == results.size()) ? "" : ",");
    }
    std::cout << "  ]\n";
    std::cout << "}\n";
  } else {
    std::cout << absl::StrFormat(
        "🔍 %zu match(es) for \"%s\" (categories: %s)\n",
        results.size(), query,
        normalized_type == "all" ? "all" : type);
    std::string current_category;
    for (const auto& result : results) {
      if (result.category != current_category) {
        current_category = result.category;
        std::cout << absl::StrFormat("\n[%s]\n",
                                     absl::AsciiStrToUpper(current_category));
      }
      std::cout << absl::StrFormat("  %-12s → %s\n", result.id, result.label);
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

absl::Status HandleDungeonDescribeRoomCommand(
  const std::vector<std::string>& arg_vec, Rom* rom_context) {
  std::string room_id_str;
  std::string format = "json";
  std::optional<std::string> rom_override;

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
    } else if (token == "--rom") {
      if (i + 1 >= arg_vec.size()) {
        return absl::InvalidArgumentError("--rom requires a value.");
      }
      rom_override = arg_vec[++i];
    } else if (absl::StartsWith(token, "--rom=")) {
      rom_override = token.substr(6);
    }
  }

  if (room_id_str.empty()) {
    return absl::InvalidArgumentError(
        "Usage: agent dungeon-describe-room --room <hex> [--format <json|text>]");
  }

  int room_id = 0;
  if (!absl::SimpleHexAtoi(room_id_str, &room_id)) {
    return absl::InvalidArgumentError(
        absl::StrCat("Invalid room ID: ", room_id_str,
                      " (expected hexadecimal, e.g. 0x02A)"));
  }

  format = absl::AsciiStrToLower(format);
  if (format != "json" && format != "text") {
    return absl::InvalidArgumentError("--format must be either json or text");
  }

  Rom rom_storage;
  Rom* rom = nullptr;
  if (rom_context != nullptr && rom_context->is_loaded() && !rom_override.has_value()) {
    rom = rom_context;
  } else {
    ASSIGN_OR_RETURN(auto rom_or, LoadRomFromPathOrFlag(rom_override));
    rom_storage = std::move(rom_or);
    rom = &rom_storage;
  }

  auto room = zelda3::LoadRoomFromRom(rom, room_id);
  room.LoadObjects();
  room.LoadSprites();

  auto dimensions = room.GetLayout().GetDimensions();
  const auto& sprites = room.GetSprites();
  const auto& chests = room.GetChests();
  const auto& stairs = room.GetStairs();
  const size_t sprite_count = sprites.size();
  const size_t chest_count = chests.size();
  const size_t stair_count = stairs.size();
  const size_t object_count = room.GetTileObjectCount();

  constexpr size_t kRoomNameCount =
      sizeof(zelda3::kRoomNames) / sizeof(zelda3::kRoomNames[0]);
  std::string room_name = "Unknown";
  if (room_id >= 0 && static_cast<size_t>(room_id) < kRoomNameCount) {
    room_name = std::string(zelda3::kRoomNames[room_id]);
    if (room_name.empty()) {
      room_name = "Unnamed";
    }
  }

  constexpr size_t kRoomEffectCount =
      sizeof(zelda3::RoomEffect) / sizeof(zelda3::RoomEffect[0]);
  const size_t effect_index = static_cast<size_t>(room.effect());
  std::string effect_name = "Unknown";
  if (effect_index < kRoomEffectCount) {
    effect_name = zelda3::RoomEffect[effect_index];
  }

  constexpr size_t kRoomTagCount =
      sizeof(zelda3::RoomTag) / sizeof(zelda3::RoomTag[0]);
  const auto tag_name = [&](zelda3::TagKey tag) {
    const size_t index = static_cast<size_t>(tag);
    if (index < kRoomTagCount) {
      return std::string(zelda3::RoomTag[index]);
    }
    return std::string("Unknown");
  };

  constexpr absl::string_view kCollisionNames[] = {
      "Layer 1 Only",
      "Both Layers",
      "Both + Scroll",
      "Moving Floor",
      "Moving Water",
  };
  std::string collision_name = "Unknown";
  const size_t collision_index = static_cast<size_t>(room.collision());
  if (collision_index < ABSL_ARRAYSIZE(kCollisionNames)) {
    collision_name = std::string(kCollisionNames[collision_index]);
  }

  if (format == "json") {
    std::cout << "{\n";
    std::cout << absl::StrFormat("  \"room\": \"0x%03X\",\n", room_id);
    std::cout << absl::StrFormat("  \"name\": \"%s\",\n", room_name);
    std::cout << absl::StrFormat("  \"light\": %s,\n",
                                  room.IsLight() ? "true" : "false");
    std::cout << absl::StrFormat("  \"layout\": {\"width\": %d, \"height\": %d},\n",
                                  dimensions.first, dimensions.second);
    std::cout << absl::StrFormat(
        "  \"counts\": {\"sprites\": %zu, \"chests\": %zu, \"stairs\": %zu, \"tile_objects\": %zu},\n",
        sprite_count, chest_count, stair_count, object_count);
    std::cout << absl::StrFormat(
        "  \"state\": {\"effect\": \"%s\", \"tag1\": \"%s\", \"tag2\": \"%s\", \"collision\": \"%s\", \"layer_merge\": \"%s\"},\n",
        effect_name, tag_name(room.tag1()), tag_name(room.tag2()),
        collision_name, room.layer_merging().Name);
    std::cout << absl::StrFormat(
        "  \"graphics\": {\"blockset\": %u, \"spriteset\": %u, \"palette\": %u},\n",
        room.blockset, room.spriteset, room.palette);
    std::cout << absl::StrFormat(
        "  \"floors\": {\"primary\": %u, \"secondary\": %u},\n",
        room.floor1, room.floor2);
    std::cout << absl::StrFormat(
        "  \"message_id\": \"0x%03X\",\n", room.message_id_);
    std::cout << absl::StrFormat(
        "  \"hole_warp\": \"0x%02X\",\n", room.holewarp);

    std::cout << "  \"staircases\": [";
    for (size_t i = 0; i < stair_count; ++i) {
      const auto& stair = stairs[i];
      std::cout << (i == 0 ? "\n" : ",\n");
      std::cout << absl::StrFormat(
          "    {\"id\": %u, \"target_room\": \"0x%02X\", \"label\": \"%s\"}",
          stair.id, stair.room, stair.label ? stair.label : "");
    }
    if (stair_count > 0) {
      std::cout << "\n  ],\n";
    } else {
      std::cout << "],\n";
    }

    std::cout << "  \"chests\": [";
    for (size_t i = 0; i < chest_count; ++i) {
      const auto& chest = chests[i];
      std::cout << (i == 0 ? "\n" : ",\n");
      std::cout << absl::StrFormat(
          "    {\"item_id\": \"0x%02X\", \"is_big\": %s}",
          chest.id, chest.size ? "true" : "false");
    }
    if (chest_count > 0) {
      std::cout << "\n  ],\n";
    } else {
      std::cout << "],\n";
    }

  const int sample_sprite_count =
    static_cast<int>(std::min<size_t>(sprite_count, 5));
  std::cout << absl::StrFormat(
    "  \"sample_sprites\": %d,\n", sample_sprite_count);
    if (!sprites.empty()) {
      std::cout << "  \"sprites\": [\n";
      const size_t limit = std::min<size_t>(sprites.size(), 5);
      for (size_t i = 0; i < limit; ++i) {
        const auto& spr = sprites[i];
        std::cout << absl::StrFormat(
            "    {\"index\": %zu, \"id\": \"0x%02X\", \"x\": %d, \"y\": %d, \"layer\": %d, \"subtype\": %d}",
            i, spr.id(), spr.x(), spr.y(), spr.layer(), spr.subtype());
        if (i + 1 < limit) {
          std::cout << ",";
        }
        std::cout << "\n";
      }
      std::cout << "  ]\n";
    } else {
      std::cout << "  \"sprites\": []\n";
    }
    std::cout << "}\n";
  } else {
    std::cout << absl::StrFormat("🏰 Room 0x%03X — %s\n", room_id, room_name);
    std::cout << absl::StrFormat(
        "  Layout: %d×%d tiles | Lighting: %s\n",
        dimensions.first, dimensions.second,
        room.IsLight() ? "light" : "dark");
    std::cout << absl::StrFormat(
        "  Sprites: %zu  Chests: %zu  Stairs: %zu  Tile Objects: %zu\n",
        sprite_count, chest_count, stair_count, object_count);
    std::cout << absl::StrFormat(
        "  Effect: %s | Tags: %s / %s | Collision: %s | Layer Merge: %s\n",
        effect_name, tag_name(room.tag1()), tag_name(room.tag2()),
        collision_name, room.layer_merging().Name);
    std::cout << absl::StrFormat(
        "  Graphics → Blockset:%u  Spriteset:%u  Palette:%u\n",
        room.blockset, room.spriteset, room.palette);
    std::cout << absl::StrFormat(
        "  Floors → Main:%u Alt:%u  Message ID:0x%03X  Hole warp:0x%02X\n",
        room.floor1, room.floor2, room.message_id_, room.holewarp);
    if (!stairs.empty()) {
      std::cout << "  Staircases:\n";
      for (const auto& stair : stairs) {
        std::cout << absl::StrFormat("    - ID %u → Room 0x%02X (%s)\n",
                                     stair.id, stair.room,
                                     stair.label ? stair.label : "");
      }
    }
    if (!chests.empty()) {
      std::cout << "  Chests:\n";
      for (size_t i = 0; i < chests.size(); ++i) {
        const auto& chest = chests[i];
        std::cout << absl::StrFormat("    - #%zu Item 0x%02X %s\n", i,
                                     chest.id,
                                     chest.size ? "(big)" : "");
      }
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

absl::Status HandleMessageListCommand(
  const std::vector<std::string>& arg_vec, Rom* rom_context) {
  return yaze::cli::message::HandleMessageListCommand(arg_vec, rom_context);
}

absl::Status HandleMessageReadCommand(
  const std::vector<std::string>& arg_vec, Rom* rom_context) {
  return yaze::cli::message::HandleMessageReadCommand(arg_vec, rom_context);
}

absl::Status HandleMessageSearchCommand(
  const std::vector<std::string>& arg_vec, Rom* rom_context) {
  return yaze::cli::message::HandleMessageSearchCommand(arg_vec, rom_context);
}

}  // namespace agent
}  // namespace cli
}  // namespace yaze
