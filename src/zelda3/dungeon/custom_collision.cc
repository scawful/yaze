#include "zelda3/dungeon/custom_collision.h"

#include <algorithm>

#include "absl/status/status.h"
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

  uint32_t snes_ptr = data[pointer_offset] |
                      (data[pointer_offset + 1] << 8) |
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

  size_t cursor = static_cast<size_t>(pc_ptr);
  bool single_tiles_mode = false;

  while (cursor + 1 < data.size()) {
    uint16_t offset = data[cursor] | (data[cursor + 1] << 8);
    cursor += 2;

    if (offset == kCollisionEndMarker) {
      break;
    }

    if (offset == kCollisionSingleTileMarker) {
      single_tiles_mode = true;
      continue;
    }

    if (!single_tiles_mode) {
      if (cursor + 1 >= data.size()) {
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
          if (cursor >= data.size()) {
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
      if (cursor >= data.size()) {
        return absl::OutOfRangeError("Collision single tile out of range");
      }
      uint8_t tile = data[cursor++];
      int idx = static_cast<int>(offset);
      if (idx >= 0 && idx < kCollisionMapWidth * kCollisionMapHeight) {
        result.tiles[static_cast<size_t>(idx)] = tile;
      }
    }
  }

  return result;
}

}  // namespace zelda3
}  // namespace yaze
