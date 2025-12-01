#include "object_parser.h"

#include <algorithm>
#include <cstring>

#include "absl/strings/str_format.h"
#include "util/log.h"
#include "zelda3/dungeon/room_object.h"

// ROM addresses for object data (PC addresses, not SNES)
// ALTTP US 1.0 ROM addresses - these are the actual addresses from the game
// SNES addresses are shown in comments for reference
static constexpr int kRoomObjectSubtype1 = 0x0F8000;      // SNES: $08:8000
static constexpr int kRoomObjectSubtype2 = 0x0F83F0;      // SNES: $08:83F0
static constexpr int kRoomObjectSubtype3 = 0x0F84F0;      // SNES: $08:84F0
static constexpr int kRoomObjectTileAddress = 0x091B52;   // SNES: $09:1B52

// Subtype 1 tile count lookup table (from ZScream's DungeonObjectData.cs)
// Each entry specifies how many tiles to read for that object ID (0x00-0xF7)
// Index directly by (object_id & 0xFF) for subtype 1 objects
// clang-format off
static constexpr uint8_t kSubtype1TileLengths[0xF8] = {
     4,  8,  8,  8,  8,  8,  8,  4,  4,  5,  5,  5,  5,  5,  5,  5,  // 0x00-0x0F
     5,  5,  5,  5,  5,  5,  5,  5,  5,  5,  5,  5,  5,  5,  5,  5,  // 0x10-0x1F
     5,  9,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  6,  // 0x20-0x2F
     6,  1,  1, 16,  1,  1, 16, 16,  6,  8, 12, 12,  4,  8,  4,  3,  // 0x30-0x3F
     3,  3,  3,  3,  3,  3,  3,  0,  0,  8,  8,  4,  9, 16, 16, 16,  // 0x40-0x4F
     1, 18, 18,  4,  1,  8,  8,  1,  1,  1,  1, 18, 18, 15,  4,  3,  // 0x50-0x5F
     4,  8,  8,  8,  8,  8,  8,  4,  4,  3,  1,  1,  6,  6,  1,  1,  // 0x60-0x6F
    16,  1,  1, 16, 16,  8, 16, 16,  4,  1,  1,  4,  1,  4,  1,  8,  // 0x70-0x7F
     8, 12, 12, 12, 12, 18, 18,  8, 12,  4,  3,  3,  3,  1,  1,  6,  // 0x80-0x8F
     8,  8,  4,  4, 16,  4,  4,  1,  1,  1,  1,  1,  1,  1,  1,  1,  // 0x90-0x9F
     1,  1,  1,  1, 24,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  // 0xA0-0xAF
     1,  1, 16,  3,  3,  8,  8,  8,  4,  4, 16,  4,  4,  4,  1,  1,  // 0xB0-0xBF
     1, 68,  1,  1,  8,  8,  8,  8,  8,  8,  8,  1,  1, 28, 28,  1,  // 0xC0-0xCF
     1,  8,  8,  0,  0,  0,  0,  1,  8,  8,  8,  8, 21, 16,  4,  8,  // 0xD0-0xDF
     8,  8,  8,  8,  8,  8,  8,  8,  8,  1,  1,  1,  1,  1,  1,  1,  // 0xE0-0xEF
     1,  1,  1,  1,  1,  1,  1,  1                                   // 0xF0-0xF7
};
// clang-format on

// Helper function to get tile count for Subtype 1 objects
static inline int GetSubtype1TileCount(int object_id) {
  int index = object_id & 0xFF;
  if (index < 0xF8) {
    int count = kSubtype1TileLengths[index];
    return (count > 0) ? count : 8;  // Default to 8 if table has 0
  }
  return 8;  // Default for IDs >= 0xF8
}

namespace yaze {
namespace zelda3 {

absl::StatusOr<std::vector<gfx::TileInfo>> ObjectParser::ParseObject(
    int16_t object_id) {
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

absl::StatusOr<ObjectRoutineInfo> ObjectParser::ParseObjectRoutine(
    int16_t object_id) {
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

absl::StatusOr<ObjectSubtypeInfo> ObjectParser::GetObjectSubtype(
    int16_t object_id) {
  ObjectSubtypeInfo info;
  info.subtype = DetermineSubtype(object_id);

  switch (info.subtype) {
    case 1: {
      int index = object_id & 0xFF;
      info.subtype_ptr = kRoomObjectSubtype1 + (index * 2);
      info.routine_ptr = kRoomObjectSubtype1 + 0x200 + (index * 2);
      info.max_tile_count = GetSubtype1TileCount(object_id);  // Use lookup table
      break;
    }
    case 2: {
      int index = (object_id - 0x100) & 0xFF;  // was: object_id & 0x7F
      info.subtype_ptr = kRoomObjectSubtype2 + (index * 2);
      info.routine_ptr = kRoomObjectSubtype2 + 0x100 + (index * 2);  // adjusted for 256 entries
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

absl::StatusOr<ObjectSizeInfo> ObjectParser::ParseObjectSize(
    int16_t object_id, uint8_t size_byte) {
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

absl::StatusOr<std::vector<gfx::TileInfo>> ObjectParser::ParseSubtype1(
    int16_t object_id) {
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

  // Use lookup table for correct tile count per object ID
  int tile_count = GetSubtype1TileCount(object_id);
  return ReadTileData(tile_data_ptr, tile_count);
}

absl::StatusOr<std::vector<gfx::TileInfo>> ObjectParser::ParseSubtype2(
    int16_t object_id) {
  int index = (object_id - 0x100) & 0xFF;  // was: object_id & 0x7F
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

absl::StatusOr<std::vector<gfx::TileInfo>> ObjectParser::ParseSubtype3(
    int16_t object_id) {
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

absl::StatusOr<std::vector<gfx::TileInfo>> ObjectParser::ReadTileData(
    int address, int tile_count) {
  // Each tile is stored as a 16-bit word (2 bytes), not 8 bytes!
  // ZScream: tiles.Add(new Tile(ROM.DATA[pos + ((i * 2))], ROM.DATA[pos + ((i *
  // 2)) + 1]));
  if (address < 0 || address + (tile_count * 2) >= (int)rom_->size()) {
    return absl::OutOfRangeError(
        absl::StrFormat("Tile data address out of range: %#06x", address));
  }

  std::vector<gfx::TileInfo> tiles;
  tiles.reserve(tile_count);

  // DEBUG: Log first tile read
  static int debug_read_count = 0;
  bool should_log = (debug_read_count < 3);

  for (int i = 0; i < tile_count; i++) {
    int tile_offset = address + (i * 2);  // 2 bytes per tile word

    // Read 1 word (2 bytes) per tile - this is the SNES tile format
    uint16_t tile_word =
        rom_->data()[tile_offset] | (rom_->data()[tile_offset + 1] << 8);

    auto tile_info = gfx::WordToTileInfo(tile_word);
    tiles.push_back(tile_info);

    // DEBUG: Log first few tiles
    if (should_log && i < 4) {
      printf(
          "[ObjectParser] ReadTile[%d]: addr=0x%06X word=0x%04X → id=0x%03X "
          "pal=%d mirror=(h:%d,v:%d)\n",
          i, tile_offset, tile_word, tile_info.id_, tile_info.palette_,
          tile_info.horizontal_mirror_, tile_info.vertical_mirror_);
    }
  }

  if (should_log) {
    printf(
        "[ObjectParser] ReadTileData: addr=0x%06X count=%d → loaded %zu "
        "tiles\n",
        address, tile_count, tiles.size());
    debug_read_count++;
  }

  return tiles;
}

int ObjectParser::DetermineSubtype(int16_t object_id) const {
  // Type 3 IDs from decoding are 0xF80-0xFFF (b3 0xF8-0xFF shifted).
  if (object_id >= 0xF80) {
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
  } else if (object_id >= 0x01 && object_id <= 0x02) {
    info.draw_routine_id = 1;  // RoomDraw_Rightwards2x4_1to15or26
    info.routine_name = "Rightwards2x4_1to15or26";
    info.tile_count = 8;
    info.is_horizontal = true;
  } else if (object_id >= 0x03 && object_id <= 0x04) {
    info.draw_routine_id = 2;  // RoomDraw_Rightwards2x4spaced4_1to16
    info.routine_name = "Rightwards2x4spaced4_1to16";
    info.tile_count = 8;
    info.is_horizontal = true;
  } else if (object_id >= 0x05 && object_id <= 0x06) {
    info.draw_routine_id = 3;  // RoomDraw_Rightwards2x4spaced4_1to16_BothBG
    info.routine_name = "Rightwards2x4spaced4_1to16_BothBG";
    info.tile_count = 8;
    info.is_horizontal = true;
    info.both_layers = true;
  } else if (object_id >= 0x07 && object_id <= 0x08) {
    info.draw_routine_id = 4;  // RoomDraw_Rightwards2x2_1to16
    info.routine_name = "Rightwards2x2_1to16";
    info.tile_count = 4;
    info.is_horizontal = true;
  } else if (object_id == 0x09) {
    info.draw_routine_id = 5;  // RoomDraw_DiagonalAcute_1to16
    info.routine_name = "DiagonalAcute_1to16";
    info.tile_count = 5;
    info.is_horizontal = false;
  } else if (object_id >= 0x0A && object_id <= 0x0B) {
    info.draw_routine_id = 6;  // RoomDraw_DiagonalGrave_1to16
    info.routine_name = "DiagonalGrave_1to16";
    info.tile_count = 5;
    info.is_horizontal = false;
  } else if (object_id >= 0x0C && object_id <= 0x0D) {
    info.draw_routine_id = 17; // RoomDraw_DiagonalAcute_1to16_BothBG
    info.routine_name = "DiagonalAcute_1to16_BothBG";
    info.tile_count = 5;
    info.is_horizontal = false;
    info.both_layers = true;
  } else if (object_id >= 0x0E && object_id <= 0x0F) {
    info.draw_routine_id = 18; // RoomDraw_DiagonalGrave_1to16_BothBG
    info.routine_name = "DiagonalGrave_1to16_BothBG";
    info.tile_count = 5;
    info.is_horizontal = false;
    info.both_layers = true;
  } else if (object_id >= 0x10 && object_id <= 0x11) {
    info.draw_routine_id = 17; // RoomDraw_DiagonalAcute_1to16_BothBG
    info.routine_name = "DiagonalAcute_1to16_BothBG";
    info.tile_count = 5;
    info.is_horizontal = false;
    info.both_layers = true;
  } else if (object_id >= 0x12 && object_id <= 0x13) {
    info.draw_routine_id = 18; // RoomDraw_DiagonalGrave_1to16_BothBG
    info.routine_name = "DiagonalGrave_1to16_BothBG";
    info.tile_count = 5;
    info.is_horizontal = false;
    info.both_layers = true;
  } else if (object_id >= 0x14 && object_id <= 0x15) {
    info.draw_routine_id = 17; // RoomDraw_DiagonalAcute_1to16_BothBG
    info.routine_name = "DiagonalAcute_1to16_BothBG";
    info.tile_count = 5;
    info.is_horizontal = false;
    info.both_layers = true;
  } else if (object_id >= 0x16 && object_id <= 0x17) {
    info.draw_routine_id = 18; // RoomDraw_DiagonalGrave_1to16_BothBG
    info.routine_name = "DiagonalGrave_1to16_BothBG";
    info.tile_count = 5;
    info.is_horizontal = false;
    info.both_layers = true;
  } else if (object_id >= 0x18 && object_id <= 0x19) {
    info.draw_routine_id = 17; // RoomDraw_DiagonalAcute_1to16_BothBG
    info.routine_name = "DiagonalAcute_1to16_BothBG";
    info.tile_count = 5;
    info.is_horizontal = false;
    info.both_layers = true;
  } else if (object_id >= 0x1A && object_id <= 0x1B) {
    info.draw_routine_id = 18; // RoomDraw_DiagonalGrave_1to16_BothBG
    info.routine_name = "DiagonalGrave_1to16_BothBG";
    info.tile_count = 5;
    info.is_horizontal = false;
    info.both_layers = true;
  } else if (object_id >= 0x1C && object_id <= 0x1D) {
    info.draw_routine_id = 17; // RoomDraw_DiagonalAcute_1to16_BothBG
    info.routine_name = "DiagonalAcute_1to16_BothBG";
    info.tile_count = 5;
    info.is_horizontal = false;
    info.both_layers = true;
  } else if (object_id >= 0x1E && object_id <= 0x1F) {
    info.draw_routine_id = 18; // RoomDraw_DiagonalGrave_1to16_BothBG
    info.routine_name = "DiagonalGrave_1to16_BothBG";
    info.tile_count = 5;
    info.is_horizontal = false;
    info.both_layers = true;
  } else if (object_id == 0x20) {
    info.draw_routine_id = 17; // RoomDraw_DiagonalAcute_1to16_BothBG
    info.routine_name = "DiagonalAcute_1to16_BothBG";
    info.tile_count = 5;
    info.is_horizontal = false;
    info.both_layers = true;
  } else if (object_id == 0x21) {
    info.draw_routine_id = 20;  // RoomDraw_Rightwards1x2_1to16_plus2
    info.routine_name = "Rightwards1x2_1to16_plus2";
    info.tile_count = 2;
    info.is_horizontal = true;
  } else if (object_id == 0x22) {
    info.draw_routine_id = 20;  // RoomDraw_Rightwards1x2_1to16_plus2
    info.routine_name = "Rightwards1x2_1to16_plus2";
    info.tile_count = 2;
    info.is_horizontal = true;
  } else if (object_id >= 0x23 && object_id <= 0x24) {
    info.draw_routine_id = 21;  // RoomDraw_RightwardsHasEdge1x1_1to16_plus3
    info.routine_name = "RightwardsHasEdge1x1_1to16_plus3";
    info.tile_count = 1;
    info.is_horizontal = true;
  } else if (object_id >= 0x25 && object_id <= 0x26) {
    info.draw_routine_id = 22;  // RoomDraw_RightwardsHasEdge1x1_1to16_plus2
    info.routine_name = "RightwardsHasEdge1x1_1to16_plus2";
    info.tile_count = 1;
    info.is_horizontal = true;
  } else if (object_id == 0x27) {
    info.draw_routine_id = 23;  // RoomDraw_RightwardsTopCorners1x2_1to16_plus13
    info.routine_name = "RightwardsTopCorners1x2_1to16_plus13";
    info.tile_count = 2;
    info.is_horizontal = true;
  } else if (object_id == 0x28) {
    info.draw_routine_id = 24;  // RoomDraw_RightwardsBottomCorners1x2_1to16_plus13
    info.routine_name = "RightwardsBottomCorners1x2_1to16_plus13";
    info.tile_count = 2;
    info.is_horizontal = true;
  } else if (object_id >= 0x29 && object_id <= 0x2E) {
    info.draw_routine_id = 22;  // RoomDraw_RightwardsHasEdge1x1_1to16_plus2
    info.routine_name = "RightwardsHasEdge1x1_1to16_plus2";
    info.tile_count = 1;
    info.is_horizontal = true;
  } else if (object_id == 0x2F) {
    info.draw_routine_id = 23;  // RoomDraw_RightwardsTopCorners1x2_1to16_plus13
    info.routine_name = "RightwardsTopCorners1x2_1to16_plus13";
    info.tile_count = 2;
    info.is_horizontal = true;
  } else if (object_id == 0x30) {
    info.draw_routine_id = 24;  // RoomDraw_RightwardsBottomCorners1x2_1to16_plus13
    info.routine_name = "RightwardsBottomCorners1x2_1to16_plus13";
    info.tile_count = 2;
    info.is_horizontal = true;
  } else if (object_id >= 0x31 && object_id <= 0x32) {
    info.draw_routine_id = 14;  // CustomDraw
    info.routine_name = "CustomDraw";
    info.tile_count = 1;
  } else if (object_id == 0x33) {
    info.draw_routine_id = 16;  // RoomDraw_Rightwards4x4_1to16
    info.routine_name = "Rightwards4x4_1to16";
    info.tile_count = 16;
    info.is_horizontal = true;
  } else if (object_id == 0x34) {
    info.draw_routine_id = 25;  // RoomDraw_Rightwards1x1Solid_1to16_plus3
    info.routine_name = "Rightwards1x1Solid_1to16_plus3";
    info.tile_count = 1;
    info.is_horizontal = true;
  } else if (object_id == 0x35) {
    info.draw_routine_id = 26;  // RoomDraw_DoorSwitcherer
    info.routine_name = "DoorSwitcherer";
    info.tile_count = 1;
  } else if (object_id >= 0x36 && object_id <= 0x37) {
    info.draw_routine_id = 27;  // RoomDraw_RightwardsDecor4x4spaced2_1to16
    info.routine_name = "RightwardsDecor4x4spaced2_1to16";
    info.tile_count = 16;
    info.is_horizontal = true;
  } else if (object_id == 0x38) {
    info.draw_routine_id = 28;  // RoomDraw_RightwardsStatue2x3spaced2_1to16
    info.routine_name = "RightwardsStatue2x3spaced2_1to16";
    info.tile_count = 6;
    info.is_horizontal = true;
  } else if (object_id == 0x39 || object_id == 0x3D) {
    info.draw_routine_id = 29;  // RoomDraw_RightwardsPillar2x4spaced4_1to16
    info.routine_name = "RightwardsPillar2x4spaced4_1to16";
    info.tile_count = 8;
    info.is_horizontal = true;
  } else if (object_id >= 0x3A && object_id <= 0x3B) {
    info.draw_routine_id = 30;  // RoomDraw_RightwardsDecor4x3spaced4_1to16
    info.routine_name = "RightwardsDecor4x3spaced4_1to16";
    info.tile_count = 12;
    info.is_horizontal = true;
  } else if (object_id == 0x3C) {
    info.draw_routine_id = 31;  // RoomDraw_RightwardsDoubled2x2spaced2_1to16
    info.routine_name = "RightwardsDoubled2x2spaced2_1to16";
    info.tile_count = 8;
    info.is_horizontal = true;
  } else if (object_id == 0x3E) {
    info.draw_routine_id = 32;  // RoomDraw_RightwardsDecor2x2spaced12_1to16
    info.routine_name = "RightwardsDecor2x2spaced12_1to16";
    info.tile_count = 4;
    info.is_horizontal = true;
  } else if (object_id >= 0x3F && object_id <= 0x40) {
    info.draw_routine_id = 22;  // RoomDraw_RightwardsHasEdge1x1_1to16_plus2 (variant)
    info.routine_name = "RightwardsHasEdge1x1_1to16_plus2";
    info.tile_count = 1;
    info.is_horizontal = true;
  } else if (object_id >= 0x40 && object_id <= 0x4F) {
    info.draw_routine_id = 19;  // RoomDraw_Corner4x4 (Type 2 corners)
    info.routine_name = "Corner4x4";
    info.tile_count = 16;
    info.is_horizontal = true;
  } else if (object_id >= 0x60 && object_id <= 0x6F) {
      // Vertical objects (Subtype 1)
      if (object_id == 0x60) {
          info.draw_routine_id = 7; // RoomDraw_Downwards2x2_1to15or32
          info.routine_name = "Downwards2x2_1to15or32";
          info.tile_count = 4;
          info.is_horizontal = false;
      } else if (object_id >= 0x61 && object_id <= 0x62) {
          info.draw_routine_id = 8; // RoomDraw_Downwards4x2_1to15or26
          info.routine_name = "Downwards4x2_1to15or26";
          info.tile_count = 8;
          info.is_horizontal = false;
      } else if (object_id >= 0x63 && object_id <= 0x64) {
          info.draw_routine_id = 9; // RoomDraw_Downwards4x2_1to16_BothBG
          info.routine_name = "Downwards4x2_1to16_BothBG";
          info.tile_count = 8;
          info.is_horizontal = false;
          info.both_layers = true;
      } else if (object_id >= 0x65 && object_id <= 0x66) {
          info.draw_routine_id = 10; // RoomDraw_DownwardsDecor4x2spaced4_1to16
          info.routine_name = "DownwardsDecor4x2spaced4_1to16";
          info.tile_count = 8;
          info.is_horizontal = false;
      } else if (object_id >= 0x67 && object_id <= 0x68) {
          info.draw_routine_id = 11; // RoomDraw_Downwards2x2_1to16
          info.routine_name = "Downwards2x2_1to16";
          info.tile_count = 4;
          info.is_horizontal = false;
      } else if (object_id == 0x69) {
          info.draw_routine_id = 12; // RoomDraw_DownwardsHasEdge1x1_1to16_plus3
          info.routine_name = "DownwardsHasEdge1x1_1to16_plus3";
          info.tile_count = 1;
          info.is_horizontal = false;
      } else if (object_id >= 0x6A && object_id <= 0x6B) {
          info.draw_routine_id = 13; // RoomDraw_DownwardsEdge1x1_1to16
          info.routine_name = "DownwardsEdge1x1_1to16";
          info.tile_count = 1;
          info.is_horizontal = false;
      } else if (object_id == 0x6C) {
          info.draw_routine_id = 14; // RoomDraw_DownwardsLeftCorners2x1_1to16_plus12
          info.routine_name = "DownwardsLeftCorners2x1_1to16_plus12";
          info.tile_count = 2;
          info.is_horizontal = false;
      } else if (object_id == 0x6D) {
          info.draw_routine_id = 15; // RoomDraw_DownwardsRightCorners2x1_1to16_plus12
          info.routine_name = "DownwardsRightCorners2x1_1to16_plus12";
          info.tile_count = 2;
          info.is_horizontal = false;
      } else {
          info.draw_routine_id = 16; // Default
          info.routine_name = "DefaultSolid";
          info.tile_count = 1;
      }
  } else if (object_id >= 0x100 && object_id <= 0x10F) {
      info.draw_routine_id = 16; // RoomDraw_Rightwards4x4_1to16 (Type 2)
      info.routine_name = "Rightwards4x4_1to16";
      info.tile_count = 16;
      info.is_horizontal = true;
  } else {
    // Default to simple 1x1 solid for unmapped objects
    info.draw_routine_id = 25;  // Use solid block routine (0x34)
    info.routine_name = "DefaultSolid";
    info.tile_count = 1;
    info.is_horizontal = true;
  }

  return info;
}

}  // namespace zelda3
}  // namespace yaze