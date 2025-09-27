#include "object_parser.h"

#include <algorithm>
#include <cstring>

#include "absl/strings/str_format.h"
#include "app/zelda3/dungeon/room_object.h"

namespace yaze {
namespace zelda3 {

absl::StatusOr<std::vector<gfx::Tile16>> ObjectParser::ParseObject(int16_t object_id) {
  if (rom_ == nullptr) {
    return absl::InvalidArgumentError("ROM is null");
  }

  int subtype = DetermineSubtype(object_id);
  
  switch (subtype) {
    case 1:
      return ParseSubtype1(object_id);
    case 2:
      return ParseSubtype2(object_id);
    case 3:
      return ParseSubtype3(object_id);
    default:
      return absl::InvalidArgumentError(
          absl::StrFormat("Invalid object subtype for ID: %#04x", object_id));
  }
}

absl::StatusOr<ObjectRoutineInfo> ObjectParser::ParseObjectRoutine(int16_t object_id) {
  if (rom_ == nullptr) {
    return absl::InvalidArgumentError("ROM is null");
  }

  auto subtype_info = GetObjectSubtype(object_id);
  if (!subtype_info.ok()) {
    return subtype_info.status();
  }

  ObjectRoutineInfo routine_info;
  routine_info.routine_ptr = subtype_info->routine_ptr;
  routine_info.tile_ptr = subtype_info->subtype_ptr;
  routine_info.tile_count = subtype_info->max_tile_count;
  routine_info.is_repeatable = true;
  routine_info.is_orientation_dependent = true;

  return routine_info;
}

absl::StatusOr<ObjectSubtypeInfo> ObjectParser::GetObjectSubtype(int16_t object_id) {
  ObjectSubtypeInfo info;
  info.subtype = DetermineSubtype(object_id);

  switch (info.subtype) {
    case 1: {
      int index = object_id & 0xFF;
      info.subtype_ptr = kRoomObjectSubtype1 + (index * 2);
      info.routine_ptr = kRoomObjectSubtype1 + 0x200 + (index * 2);
      info.max_tile_count = 8; // Most subtype 1 objects use 8 tiles
      break;
    }
    case 2: {
      int index = object_id & 0x7F;
      info.subtype_ptr = kRoomObjectSubtype2 + (index * 2);
      info.routine_ptr = kRoomObjectSubtype2 + 0x80 + (index * 2);
      info.max_tile_count = 8;
      break;
    }
    case 3: {
      int index = object_id & 0xFF;
      info.subtype_ptr = kRoomObjectSubtype3 + (index * 2);
      info.routine_ptr = kRoomObjectSubtype3 + 0x100 + (index * 2);
      info.max_tile_count = 8;
      break;
    }
    default:
      return absl::InvalidArgumentError(
          absl::StrFormat("Invalid object subtype for ID: %#04x", object_id));
  }

  return info;
}

absl::StatusOr<ObjectSizeInfo> ObjectParser::ParseObjectSize(int16_t object_id, uint8_t size_byte) {
  ObjectSizeInfo info;
  
  // Extract size bits (0-3 for X, 4-7 for Y)
  int size_x = size_byte & 0x03;
  int size_y = (size_byte >> 2) & 0x03;
  
  info.width_tiles = (size_x + 1) * 2;  // Convert to tile count
  info.height_tiles = (size_y + 1) * 2;
  
  // Determine orientation based on object ID and size
  // This is a heuristic based on the object naming patterns
  if (object_id >= 0x80 && object_id <= 0xFF) {
    // Objects 0x80-0xFF are typically vertical
    info.is_horizontal = false;
  } else {
    // Objects 0x00-0x7F are typically horizontal
    info.is_horizontal = true;
  }
  
  // Determine if object is repeatable
  info.is_repeatable = (size_byte != 0);
  info.repeat_count = size_byte == 0 ? 32 : size_byte;
  
  return info;
}

absl::StatusOr<std::vector<gfx::Tile16>> ObjectParser::ParseSubtype1(int16_t object_id) {
  int index = object_id & 0xFF;
  int tile_ptr = kRoomObjectSubtype1 + (index * 2);
  
  if (tile_ptr + 1 >= (int)rom_->size()) {
    return absl::OutOfRangeError(
        absl::StrFormat("Tile pointer out of range: %#06x", tile_ptr));
  }
  
  // Read tile data pointer
  uint8_t low = rom_->data()[tile_ptr];
  uint8_t high = rom_->data()[tile_ptr + 1];
  int tile_data_ptr = kRoomObjectTileAddress + ((high << 8) | low);
  
  // Read 8 tiles (most subtype 1 objects use 8 tiles)
  return ReadTileData(tile_data_ptr, 8);
}

absl::StatusOr<std::vector<gfx::Tile16>> ObjectParser::ParseSubtype2(int16_t object_id) {
  int index = object_id & 0x7F;
  int tile_ptr = kRoomObjectSubtype2 + (index * 2);
  
  if (tile_ptr + 1 >= (int)rom_->size()) {
    return absl::OutOfRangeError(
        absl::StrFormat("Tile pointer out of range: %#06x", tile_ptr));
  }
  
  // Read tile data pointer
  uint8_t low = rom_->data()[tile_ptr];
  uint8_t high = rom_->data()[tile_ptr + 1];
  int tile_data_ptr = kRoomObjectTileAddress + ((high << 8) | low);
  
  // Read 8 tiles
  return ReadTileData(tile_data_ptr, 8);
}

absl::StatusOr<std::vector<gfx::Tile16>> ObjectParser::ParseSubtype3(int16_t object_id) {
  int index = object_id & 0xFF;
  int tile_ptr = kRoomObjectSubtype3 + (index * 2);
  
  if (tile_ptr + 1 >= (int)rom_->size()) {
    return absl::OutOfRangeError(
        absl::StrFormat("Tile pointer out of range: %#06x", tile_ptr));
  }
  
  // Read tile data pointer
  uint8_t low = rom_->data()[tile_ptr];
  uint8_t high = rom_->data()[tile_ptr + 1];
  int tile_data_ptr = kRoomObjectTileAddress + ((high << 8) | low);
  
  // Read 8 tiles
  return ReadTileData(tile_data_ptr, 8);
}

absl::StatusOr<std::vector<gfx::Tile16>> ObjectParser::ReadTileData(int address, int tile_count) {
  if (address < 0 || address + (tile_count * 8) >= (int)rom_->size()) {
    return absl::OutOfRangeError(
        absl::StrFormat("Tile data address out of range: %#06x", address));
  }
  
  std::vector<gfx::Tile16> tiles;
  tiles.reserve(tile_count);
  
  for (int i = 0; i < tile_count; i++) {
    int tile_offset = address + (i * 8);
    
    // Read 4 words (8 bytes) per tile
    uint16_t w0 = rom_->data()[tile_offset] | (rom_->data()[tile_offset + 1] << 8);
    uint16_t w1 = rom_->data()[tile_offset + 2] | (rom_->data()[tile_offset + 3] << 8);
    uint16_t w2 = rom_->data()[tile_offset + 4] | (rom_->data()[tile_offset + 5] << 8);
    uint16_t w3 = rom_->data()[tile_offset + 6] | (rom_->data()[tile_offset + 7] << 8);
    
    tiles.emplace_back(
        gfx::WordToTileInfo(w0),
        gfx::WordToTileInfo(w1),
        gfx::WordToTileInfo(w2),
        gfx::WordToTileInfo(w3)
    );
  }
  
  return tiles;
}

int ObjectParser::DetermineSubtype(int16_t object_id) const {
  if (object_id >= 0x200) {
    return 3;
  } else if (object_id >= 0x100) {
    return 2;
  } else {
    return 1;
  }
}

}  // namespace zelda3
}  // namespace yaze