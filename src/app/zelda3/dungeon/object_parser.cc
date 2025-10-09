#include "object_parser.h"

#include <algorithm>
#include <cstring>

#include "absl/strings/str_format.h"
#include "app/zelda3/dungeon/room_object.h"

// ROM addresses for object data (PC addresses, not SNES)
static constexpr int kRoomObjectSubtype1 = 0x0A8000;
static constexpr int kRoomObjectSubtype2 = 0x0A9000;
static constexpr int kRoomObjectSubtype3 = 0x0AA000;
static constexpr int kRoomObjectTileAddress = 0x0AB000;

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

ObjectDrawInfo ObjectParser::GetObjectDrawInfo(int16_t object_id) const {
  ObjectDrawInfo info;
  
  // Map object ID to draw routine based on ZScream's subtype1_routines table
  // This is based on the DungeonObjectData.cs mapping from ZScream
  
  if (object_id == 0x00) {
    info.draw_routine_id = 0;  // RoomDraw_Rightwards2x2_1to15or32
    info.routine_name = "Rightwards2x2_1to15or32";
    info.tile_count = 4;
    info.is_horizontal = true;
  }
  else if (object_id >= 0x01 && object_id <= 0x02) {
    info.draw_routine_id = 1;  // RoomDraw_Rightwards2x4_1to15or26
    info.routine_name = "Rightwards2x4_1to15or26";
    info.tile_count = 8;
    info.is_horizontal = true;
  }
  else if (object_id >= 0x03 && object_id <= 0x04) {
    info.draw_routine_id = 2;  // RoomDraw_Rightwards2x4spaced4_1to16
    info.routine_name = "Rightwards2x4spaced4_1to16";
    info.tile_count = 8;
    info.is_horizontal = true;
  }
  else if (object_id >= 0x05 && object_id <= 0x06) {
    info.draw_routine_id = 3;  // RoomDraw_Rightwards2x4spaced4_1to16_BothBG
    info.routine_name = "Rightwards2x4spaced4_1to16_BothBG";
    info.tile_count = 8;
    info.is_horizontal = true;
    info.both_layers = true;
  }
  else if (object_id >= 0x07 && object_id <= 0x08) {
    info.draw_routine_id = 4;  // RoomDraw_Rightwards2x2_1to16
    info.routine_name = "Rightwards2x2_1to16";
    info.tile_count = 4;
    info.is_horizontal = true;
  }
  else if (object_id == 0x09) {
    info.draw_routine_id = 5;  // RoomDraw_DiagonalAcute_1to16
    info.routine_name = "DiagonalAcute_1to16";
    info.tile_count = 5;
    info.is_horizontal = false;
  }
  else if (object_id >= 0x0A && object_id <= 0x0B) {
    info.draw_routine_id = 6;  // RoomDraw_DiagonalGrave_1to16
    info.routine_name = "DiagonalGrave_1to16";
    info.tile_count = 5;
    info.is_horizontal = false;
  }
  else if (object_id >= 0x15 && object_id <= 0x1F) {
    info.draw_routine_id = 7;  // RoomDraw_DiagonalAcute_1to16_BothBG
    info.routine_name = "DiagonalAcute_1to16_BothBG";
    info.tile_count = 5;
    info.is_horizontal = false;
    info.both_layers = true;
  }
  else if (object_id >= 0x16 && object_id <= 0x20) {
    info.draw_routine_id = 8;  // RoomDraw_DiagonalGrave_1to16_BothBG
    info.routine_name = "DiagonalGrave_1to16_BothBG";
    info.tile_count = 5;
    info.is_horizontal = false;
    info.both_layers = true;
  }
  else if (object_id == 0x21) {
    info.draw_routine_id = 9;  // RoomDraw_Rightwards1x2_1to16_plus2
    info.routine_name = "Rightwards1x2_1to16_plus2";
    info.tile_count = 2;
    info.is_horizontal = true;
  }
  else if (object_id == 0x22) {
    info.draw_routine_id = 10; // RoomDraw_RightwardsHasEdge1x1_1to16_plus3
    info.routine_name = "RightwardsHasEdge1x1_1to16_plus3";
    info.tile_count = 1;
    info.is_horizontal = true;
  }
  else if (object_id >= 0x23 && object_id <= 0x2E) {
    info.draw_routine_id = 11; // RoomDraw_RightwardsHasEdge1x1_1to16_plus2
    info.routine_name = "RightwardsHasEdge1x1_1to16_plus2";
    info.tile_count = 1;
    info.is_horizontal = true;
  }
  else if (object_id == 0x2F) {
    info.draw_routine_id = 12; // RoomDraw_RightwardsTopCorners1x2_1to16_plus13
    info.routine_name = "RightwardsTopCorners1x2_1to16_plus13";
    info.tile_count = 2;
    info.is_horizontal = true;
  }
  else if (object_id == 0x30) {
    info.draw_routine_id = 13; // RoomDraw_RightwardsBottomCorners1x2_1to16_plus13
    info.routine_name = "RightwardsBottomCorners1x2_1to16_plus13";
    info.tile_count = 2;
    info.is_horizontal = true;
  }
  else if (object_id >= 0x31 && object_id <= 0x32) {
    info.draw_routine_id = 14; // CustomDraw
    info.routine_name = "CustomDraw";
    info.tile_count = 1;
  }
  else if (object_id == 0x33) {
    info.draw_routine_id = 15; // RoomDraw_Rightwards4x4_1to16
    info.routine_name = "Rightwards4x4_1to16";
    info.tile_count = 16;
    info.is_horizontal = true;
  }
  else if (object_id == 0x34) {
    info.draw_routine_id = 16; // RoomDraw_Rightwards1x1Solid_1to16_plus3
    info.routine_name = "Rightwards1x1Solid_1to16_plus3";
    info.tile_count = 1;
    info.is_horizontal = true;
  }
  else if (object_id == 0x35) {
    info.draw_routine_id = 17; // RoomDraw_DoorSwitcherer
    info.routine_name = "DoorSwitcherer";
    info.tile_count = 1;
  }
  else if (object_id >= 0x36 && object_id <= 0x37) {
    info.draw_routine_id = 18; // RoomDraw_RightwardsDecor4x4spaced2_1to16
    info.routine_name = "RightwardsDecor4x4spaced2_1to16";
    info.tile_count = 16;
    info.is_horizontal = true;
  }
  else if (object_id == 0x38) {
    info.draw_routine_id = 19; // RoomDraw_RightwardsStatue2x3spaced2_1to16
    info.routine_name = "RightwardsStatue2x3spaced2_1to16";
    info.tile_count = 6;
    info.is_horizontal = true;
  }
  else if (object_id == 0x39 || object_id == 0x3D) {
    info.draw_routine_id = 20; // RoomDraw_RightwardsPillar2x4spaced4_1to16
    info.routine_name = "RightwardsPillar2x4spaced4_1to16";
    info.tile_count = 8;
    info.is_horizontal = true;
  }
  else if (object_id >= 0x3A && object_id <= 0x3B) {
    info.draw_routine_id = 21; // RoomDraw_RightwardsDecor4x3spaced4_1to16
    info.routine_name = "RightwardsDecor4x3spaced4_1to16";
    info.tile_count = 12;
    info.is_horizontal = true;
  }
  else if (object_id == 0x3C) {
    info.draw_routine_id = 22; // RoomDraw_RightwardsDoubled2x2spaced2_1to16
    info.routine_name = "RightwardsDoubled2x2spaced2_1to16";
    info.tile_count = 8;
    info.is_horizontal = true;
  }
  else if (object_id == 0x3E) {
    info.draw_routine_id = 23; // RoomDraw_RightwardsDecor2x2spaced12_1to16
    info.routine_name = "RightwardsDecor2x2spaced12_1to16";
    info.tile_count = 4;
    info.is_horizontal = true;
  }
  else if (object_id >= 0x3F && object_id <= 0x40) {
    info.draw_routine_id = 24; // RoomDraw_RightwardsHasEdge1x1_1to16_plus2 (variant)
    info.routine_name = "RightwardsHasEdge1x1_1to16_plus2_variant";
    info.tile_count = 1;
    info.is_horizontal = true;
  }
  else {
    // Default to simple 1x1 solid for unmapped objects
    info.draw_routine_id = 16; // Use solid block routine
    info.routine_name = "DefaultSolid";
    info.tile_count = 1;
    info.is_horizontal = true;
  }
  
  return info;
}

}  // namespace zelda3
}  // namespace yaze