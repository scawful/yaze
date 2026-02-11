#include "zelda3/dungeon/custom_collision.h"

#include <algorithm>
#include <limits>
#include <optional>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#if defined(YAZE_WITH_JSON)
#include "nlohmann/json.hpp"
#endif

#include "absl/status/status.h"
#include "absl/strings/str_format.h"
#include "rom/snes.h"
#include "zelda3/dungeon/dungeon_rom_addresses.h"
#include "zelda3/dungeon/room.h"

namespace yaze {
namespace zelda3 {

namespace {
constexpr int kCollisionMapWidth = 64;
constexpr int kCollisionMapHeight = 64;
constexpr uint16_t kCollisionSingleTileMarker = 0xF0F0;
constexpr uint16_t kCollisionEndMarker = 0xFFFF;
constexpr int kCollisionMapTiles = kCollisionMapWidth * kCollisionMapHeight;
}  // namespace

absl::StatusOr<CustomCollisionMap> LoadCustomCollisionMap(Rom* rom,
                                                          int room_id) {
  if (!rom || !rom->is_loaded()) {
    return absl::InvalidArgumentError("ROM not loaded");
  }
  if (room_id < 0 || room_id >= NumberOfRooms) {
    return absl::OutOfRangeError("Room id out of range");
  }

  const auto& data = rom->vector();
  const int pointer_offset = kCustomCollisionRoomPointers + (room_id * 3);
  if (pointer_offset < 0 ||
      pointer_offset + 2 >= static_cast<int>(data.size())) {
    return absl::OutOfRangeError("Collision pointer table out of range");
  }

  // Oracle of Secrets: reserve a tail region for the WaterFill table.
  // If the reserved region exists in this ROM, refuse to load collision blobs
  // that overlap it (corrupted pointer table / missing terminator).
  const bool has_water_fill_reserved_region =
      (kWaterFillTableEnd <= static_cast<int>(data.size()));
  const size_t collision_safe_end =
      has_water_fill_reserved_region
          ? std::min(static_cast<size_t>(data.size()),
                     static_cast<size_t>(kCustomCollisionDataSoftEnd))
          : static_cast<size_t>(data.size());

  uint32_t snes_ptr = data[pointer_offset] | (data[pointer_offset + 1] << 8) |
                      (data[pointer_offset + 2] << 16);

  CustomCollisionMap result;
  result.tiles.fill(0);

  if (snes_ptr == 0) {
    return result;
  }
  result.has_data = true;

  int pc_ptr = SnesToPc(snes_ptr);
  if (pc_ptr < 0 || pc_ptr >= static_cast<int>(data.size())) {
    return absl::OutOfRangeError("Collision data pointer out of range");
  }
  if (static_cast<size_t>(pc_ptr) >= collision_safe_end) {
    return absl::FailedPreconditionError(absl::StrFormat(
        "Collision data for room 0x%02X overlaps WaterFill reserved region (pc=0x%06X)",
        room_id, pc_ptr));
  }

  size_t cursor = static_cast<size_t>(pc_ptr);
  bool single_tiles_mode = false;
  bool found_end_marker = false;

  while (cursor + 1 < collision_safe_end) {
    uint16_t offset = data[cursor] | (data[cursor + 1] << 8);
    cursor += 2;

    if (offset == kCollisionEndMarker) {
      found_end_marker = true;
      break;
    }

    if (offset == kCollisionSingleTileMarker) {
      single_tiles_mode = true;
      continue;
    }

    if (!single_tiles_mode) {
      if (cursor + 1 >= collision_safe_end) {
        return absl::OutOfRangeError("Collision rectangle header out of range");
      }
      uint8_t width = data[cursor];
      uint8_t height = data[cursor + 1];
      cursor += 2;

      if (width == 0 || height == 0) {
        continue;
      }

      for (uint8_t row = 0; row < height; ++row) {
        int row_offset = static_cast<int>(offset) + (row * kCollisionMapWidth);
        for (uint8_t col = 0; col < width; ++col) {
          if (cursor >= collision_safe_end) {
            return absl::OutOfRangeError(
                "Collision rectangle data out of range");
          }
          uint8_t tile = data[cursor++];
          int idx = row_offset + col;
          if (idx >= 0 && idx < kCollisionMapWidth * kCollisionMapHeight) {
            result.tiles[static_cast<size_t>(idx)] = tile;
          }
        }
      }
    } else {
      if (cursor >= collision_safe_end) {
        return absl::OutOfRangeError("Collision single tile out of range");
      }
      uint8_t tile = data[cursor++];
      int idx = static_cast<int>(offset);
      if (idx >= 0 && idx < kCollisionMapWidth * kCollisionMapHeight) {
        result.tiles[static_cast<size_t>(idx)] = tile;
      }
    }
  }

  if (has_water_fill_reserved_region && !found_end_marker) {
    return absl::FailedPreconditionError(absl::StrFormat(
        "Collision data for room 0x%02X is unterminated before WaterFill reserved region",
        room_id));
  }

  return result;
}

absl::StatusOr<std::string> DumpCustomCollisionRoomsToJsonString(
    const std::vector<CustomCollisionRoomEntry>& rooms) {
#if !defined(YAZE_WITH_JSON)
  return absl::UnimplementedError(
      "JSON support not enabled. Build with -DYAZE_WITH_JSON=ON");
#else
  using json = nlohmann::json;

  std::vector<CustomCollisionRoomEntry> sorted = rooms;
  std::sort(sorted.begin(), sorted.end(),
            [](const CustomCollisionRoomEntry& a,
               const CustomCollisionRoomEntry& b) {
              return a.room_id < b.room_id;
            });

  json root;
  root["version"] = 1;
  json arr = json::array();
  for (const auto& r : sorted) {
    if (r.room_id < 0 || r.room_id >= kNumberOfRooms) {
      continue;
    }

    // Normalize offsets: keep last value for each offset and write in ascending
    // order. Drop value=0 for compactness (0 is treated as "unset" by the editor).
    std::unordered_map<int, int> tile_map;
    tile_map.reserve(r.tiles.size());
    for (const auto& t : r.tiles) {
      if (t.offset >= kCollisionMapTiles) {
        continue;
      }
      tile_map[static_cast<int>(t.offset)] = static_cast<int>(t.value);
    }

    std::vector<std::pair<int, int>> tiles;
    tiles.reserve(tile_map.size());
    for (const auto& [off, val] : tile_map) {
      if (val == 0) {
        continue;
      }
      tiles.emplace_back(off, val);
    }
    if (tiles.empty()) {
      continue;
    }
    std::sort(tiles.begin(), tiles.end(),
              [](const auto& a, const auto& b) { return a.first < b.first; });

    json item;
    item["room_id"] = absl::StrFormat("0x%02X", r.room_id);
    json tiles_arr = json::array();
    for (const auto& [off, val] : tiles) {
      tiles_arr.push_back(json::array({off, val}));
    }
    item["tiles"] = std::move(tiles_arr);
    arr.push_back(std::move(item));
  }
  root["rooms"] = std::move(arr);
  return root.dump(/*indent=*/2);
#endif
}

absl::StatusOr<std::vector<CustomCollisionRoomEntry>>
LoadCustomCollisionRoomsFromJsonString(const std::string& json_content) {
#if !defined(YAZE_WITH_JSON)
  return absl::UnimplementedError(
      "JSON support not enabled. Build with -DYAZE_WITH_JSON=ON");
#else
  using json = nlohmann::json;

  json root;
  try {
    root = json::parse(json_content);
  } catch (const json::parse_error& e) {
    return absl::InvalidArgumentError(
        std::string("JSON parse error: ") + e.what());
  }

  const int version = root.value("version", 1);
  if (version != 1) {
    return absl::InvalidArgumentError(
        absl::StrFormat("Unsupported custom collision JSON version: %d",
                        version));
  }
  if (!root.contains("rooms") || !root["rooms"].is_array()) {
    return absl::InvalidArgumentError("Missing or invalid 'rooms' array");
  }

  auto parse_int = [](const json& v) -> std::optional<int> {
    if (v.is_number_integer()) {
      return v.get<int>();
    }
    if (v.is_number_unsigned()) {
      const auto u = v.get<unsigned int>();
      if (u > static_cast<unsigned int>(std::numeric_limits<int>::max())) {
        return std::nullopt;
      }
      return static_cast<int>(u);
    }
    if (v.is_string()) {
      try {
        size_t idx = 0;
        const std::string s = v.get<std::string>();
        const int parsed = std::stoi(s, &idx, 0);
        if (idx == s.size()) {
          return parsed;
        }
      } catch (...) {
      }
    }
    return std::nullopt;
  };

  std::vector<CustomCollisionRoomEntry> out;
  out.reserve(root["rooms"].size());

  std::unordered_map<int, bool> seen_rooms;
  for (const auto& item : root["rooms"]) {
    if (!item.is_object()) {
      continue;
    }

    const json& room_v =
        item.contains("room_id") ? item["room_id"]
        : item.contains("room")  ? item["room"]
                                 : json();
    const auto room_id_opt = parse_int(room_v);
    if (!room_id_opt.has_value() || *room_id_opt < 0 ||
        *room_id_opt >= kNumberOfRooms) {
      return absl::InvalidArgumentError(
          "Invalid room_id in custom collision JSON");
    }
    const int room_id = *room_id_opt;
    if (seen_rooms.contains(room_id)) {
      return absl::InvalidArgumentError(
          absl::StrFormat("Duplicate room_id in custom collision JSON: 0x%02X",
                          room_id));
    }
    seen_rooms[room_id] = true;

    const json& tiles_v =
        item.contains("tiles")   ? item["tiles"]
        : item.contains("entries") ? item["entries"]
                                   : json::array();
    if (!tiles_v.is_array()) {
      return absl::InvalidArgumentError(
          absl::StrFormat("Invalid tiles array for room 0x%02X", room_id));
    }

    // Last-wins map for duplicate offsets.
    std::unordered_map<int, int> tile_map;
    tile_map.reserve(tiles_v.size());
    for (const auto& entry : tiles_v) {
      int off = -1;
      int val = -1;
      if (entry.is_array() && entry.size() >= 2) {
        const auto off_opt = parse_int(entry[0]);
        const auto val_opt = parse_int(entry[1]);
        if (!off_opt.has_value() || !val_opt.has_value()) {
          return absl::InvalidArgumentError(
              absl::StrFormat("Invalid tile entry for room 0x%02X", room_id));
        }
        off = *off_opt;
        val = *val_opt;
      } else if (entry.is_object()) {
        const json& off_v =
            entry.contains("offset") ? entry["offset"]
            : entry.contains("off")  ? entry["off"]
                                     : json();
        const json& val_v =
            entry.contains("value") ? entry["value"]
            : entry.contains("val") ? entry["val"]
                                    : json();
        const auto off_opt = parse_int(off_v);
        const auto val_opt = parse_int(val_v);
        if (!off_opt.has_value() || !val_opt.has_value()) {
          return absl::InvalidArgumentError(
              absl::StrFormat("Invalid tile entry for room 0x%02X", room_id));
        }
        off = *off_opt;
        val = *val_opt;
      } else {
        return absl::InvalidArgumentError(
            absl::StrFormat("Invalid tile entry for room 0x%02X", room_id));
      }

      if (off < 0 || off >= kCollisionMapTiles) {
        return absl::InvalidArgumentError(
            absl::StrFormat("Invalid tile offset for room 0x%02X", room_id));
      }
      if (val < 0 || val > 0xFF) {
        return absl::InvalidArgumentError(
            absl::StrFormat("Invalid tile value for room 0x%02X", room_id));
      }
      tile_map[off] = val;
    }

    std::vector<std::pair<int, int>> tiles;
    tiles.reserve(tile_map.size());
    for (const auto& [off, val] : tile_map) {
      tiles.emplace_back(off, val);
    }
    std::sort(tiles.begin(), tiles.end(),
              [](const auto& a, const auto& b) { return a.first < b.first; });

    CustomCollisionRoomEntry r;
    r.room_id = room_id;
    r.tiles.reserve(tiles.size());
    for (const auto& [off, val] : tiles) {
      CustomCollisionTileEntry t;
      t.offset = static_cast<uint16_t>(off);
      t.value = static_cast<uint8_t>(val);
      r.tiles.push_back(std::move(t));
    }
    out.push_back(std::move(r));
  }

  std::sort(out.begin(), out.end(),
            [](const CustomCollisionRoomEntry& a,
               const CustomCollisionRoomEntry& b) {
              return a.room_id < b.room_id;
            });
  return out;
#endif
}

}  // namespace zelda3
}  // namespace yaze
