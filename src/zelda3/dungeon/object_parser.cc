#include "object_parser.h"

#include <algorithm>
#include <cstring>

#include "absl/strings/str_format.h"
#include "util/log.h"
#include "zelda3/dungeon/draw_routines/draw_routine_registry.h"
#include "zelda3/dungeon/room_object.h"

// ROM addresses for object data are defined in room_object.h:
// - kRoomObjectSubtype1 = 0x8000 (SNES $01:8000)
// - kRoomObjectSubtype2 = 0x83F0 (SNES $01:83F0)
// - kRoomObjectSubtype3 = 0x84F0 (SNES $01:84F0)
// - kRoomObjectTileAddress = 0x1B52 (SNES $00:9B52)

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

  // Validate object ID is non-negative
  if (object_id < 0) {
    return absl::InvalidArgumentError(
        absl::StrFormat("Invalid object ID: %d", object_id));
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
      break;
    }
    case 2: {
      // Type 2 objects: 0x100-0x13F (64 objects only)
      // Index mask 0x3F ensures we stay within 64-entry table bounds
      int index = (object_id - 0x100) & 0x3F;
      info.subtype_ptr = kRoomObjectSubtype2 + (index * 2);
      // Routine table starts 128 bytes (64 entries * 2 bytes) after data table
      info.routine_ptr = kRoomObjectSubtype2 + 0x80 + (index * 2);
      break;
    }
    case 3: {
      // Type 3 object IDs are 0xF80-0xFFF (128 objects)
      // Table index should be 0-127
      int index = (object_id - 0xF80) & 0x7F;
      info.subtype_ptr = kRoomObjectSubtype3 + (index * 2);
      info.routine_ptr = kRoomObjectSubtype3 + 0x100 + (index * 2);
      break;
    }
    default:
      return absl::InvalidArgumentError(
          absl::StrFormat("Invalid object subtype for ID: %#04x", object_id));
  }

  info.max_tile_count = ResolveTileCountForObject(object_id);

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

  // Read tile data pointer (16-bit little-endian offset)
  uint8_t low = rom_->data()[tile_ptr];
  uint8_t high = rom_->data()[tile_ptr + 1];
  int16_t offset = (int16_t)((high << 8) | low);  // Signed offset!
  int tile_data_ptr = kRoomObjectTileAddress + offset;

  // DEBUG: Log wall objects 0x61/0x62 and ceiling 0xC0 to verify tile data
  bool is_debug_object = (object_id == 0x61 || object_id == 0x62 ||
                          object_id == 0xC0 || object_id == 0xC2);
  static int debug_count = 0;
  if (debug_count < 10 || is_debug_object) {
    LOG_DEBUG("ObjectParser",
              "ParseSubtype1: obj=0x%02X%s tile_ptr=0x%04X (SNES $01:%04X)",
              object_id, is_debug_object ? " (DEBUG)" : "", tile_ptr, tile_ptr);
    LOG_DEBUG("ObjectParser",
              "  ROM[0x%04X..0x%04X]=0x%02X 0x%02X offset=%d (0x%04X)",
              tile_ptr, tile_ptr + 1, low, high, offset, (uint16_t)offset);
    LOG_DEBUG("ObjectParser",
              "  tile_data_ptr=0x%04X+0x%04X=0x%04X (SNES $00:%04X)",
              kRoomObjectTileAddress, (uint16_t)offset, tile_data_ptr,
              tile_data_ptr + 0x8000);

    // Fix: Check for negative tile_data_ptr to prevent SIGSEGV on corrupted ROMs
    if (tile_data_ptr >= 0 && tile_data_ptr + 8 < (int)rom_->size()) {
      uint16_t tw0 =
          rom_->data()[tile_data_ptr] | (rom_->data()[tile_data_ptr + 1] << 8);
      uint16_t tw1 = rom_->data()[tile_data_ptr + 2] |
                     (rom_->data()[tile_data_ptr + 3] << 8);
      LOG_DEBUG("ObjectParser", "  First 2 tiles: $%04X(id=%d) $%04X(id=%d)",
                tw0, tw0 & 0x3FF, tw1, tw1 & 0x3FF);
    } else {
      LOG_DEBUG("ObjectParser", "  Tile data at 0x%04X: <OUT OF BOUNDS>",
                tile_data_ptr);
    }
    if (!is_debug_object)
      debug_count++;
  }

  // Use lookup table for correct tile count per object ID
  int tile_count = GetSubtype1TileCount(object_id);
  return ReadTileData(tile_data_ptr, tile_count);
}

absl::StatusOr<std::vector<gfx::TileInfo>> ObjectParser::ParseSubtype2(
    int16_t object_id) {
  // Type 2 objects: 0x100-0x13F (64 objects only)
  int index = (object_id - 0x100) & 0x3F;
  int tile_ptr = kRoomObjectSubtype2 + (index * 2);

  if (tile_ptr + 1 >= (int)rom_->size()) {
    return absl::OutOfRangeError(
        absl::StrFormat("Tile pointer out of range: %#06x", tile_ptr));
  }

  // Read tile data pointer
  uint8_t low = rom_->data()[tile_ptr];
  uint8_t high = rom_->data()[tile_ptr + 1];
  int tile_data_ptr = kRoomObjectTileAddress + ((high << 8) | low);

  // Determine tile count based on object ID
  int tile_count = GetSubtype2TileCount(object_id);

  // DEBUG: Log corner tile loading (0x100-0x103)
  bool is_corner = (object_id >= 0x100 && object_id <= 0x103);
  if (is_corner) {
    LOG_DEBUG("ObjectParser",
              "ParseSubtype2: CORNER obj=0x%03X index=%d tile_ptr=0x%04X",
              object_id, index, tile_ptr);
    LOG_DEBUG("ObjectParser",
              "  ROM[0x%04X..0x%04X]=0x%02X 0x%02X offset=0x%04X", tile_ptr,
              tile_ptr + 1, low, high, (high << 8) | low);
    LOG_DEBUG(
        "ObjectParser", "  tile_data_ptr=0x%04X+0x%04X=0x%04X (tile_count=%d)",
        kRoomObjectTileAddress, (high << 8) | low, tile_data_ptr, tile_count);

    // Show first 2 tile words
    if (tile_data_ptr >= 0 && tile_data_ptr + 4 < (int)rom_->size()) {
      uint16_t tw0 =
          rom_->data()[tile_data_ptr] | (rom_->data()[tile_data_ptr + 1] << 8);
      uint16_t tw1 = rom_->data()[tile_data_ptr + 2] |
                     (rom_->data()[tile_data_ptr + 3] << 8);
      LOG_DEBUG("ObjectParser", "  First 2 tiles: $%04X(id=%d) $%04X(id=%d)",
                tw0, tw0 & 0x3FF, tw1, tw1 & 0x3FF);
    }
  }

  return ReadTileData(tile_data_ptr, tile_count);
}

absl::StatusOr<std::vector<gfx::TileInfo>> ObjectParser::ParseSubtype3(
    int16_t object_id) {
  // Type 3 object IDs are 0xF80-0xFFF (128 objects)
  // Table index should be 0-127, calculated by subtracting base offset 0xF80
  int index = (object_id - 0xF80) & 0x7F;
  int tile_ptr = kRoomObjectSubtype3 + (index * 2);

  if (tile_ptr + 1 >= (int)rom_->size()) {
    return absl::OutOfRangeError(
        absl::StrFormat("Tile pointer out of range: %#06x", tile_ptr));
  }

  // Read tile data pointer
  uint8_t low = rom_->data()[tile_ptr];
  uint8_t high = rom_->data()[tile_ptr + 1];
  int tile_data_ptr = kRoomObjectTileAddress + ((high << 8) | low);

  // Determine tile count based on object ID
  int tile_count = GetSubtype3TileCount(object_id);
  return ReadTileData(tile_data_ptr, tile_count);
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
      LOG_DEBUG("ObjectParser",
                "ReadTile[%d]: addr=0x%06X word=0x%04X id=0x%03X pal=%d "
                "mirror=(h:%d,v:%d)",
                i, tile_offset, tile_word, tile_info.id_, tile_info.palette_,
                tile_info.horizontal_mirror_, tile_info.vertical_mirror_);
    }
  }

  if (should_log) {
    LOG_DEBUG("ObjectParser",
              "ReadTileData: addr=0x%06X count=%d loaded %zu tiles", address,
              tile_count, tiles.size());
    debug_read_count++;
  }

  return tiles;
}

int ObjectParser::GetSubtype2TileCount(int16_t object_id) const {
  // 4x4 corners (0x100-0x10F): 16 tiles (32 bytes)
  // These are RoomDraw_4x4 and RoomDraw_4x4Corner_BothBG routines
  if (object_id >= 0x100 && object_id <= 0x10F) {
    return 16;
  }
  // Weird corners (0x110-0x117): 12 tiles (24 bytes)
  // These are RoomDraw_WeirdCornerBottom_BothBG and RoomDraw_WeirdCornerTop_BothBG
  if (object_id >= 0x110 && object_id <= 0x117) {
    return 12;
  }
  // 4x4 fixed patterns (stairs/altars/walls)
  if (object_id == 0x11C || object_id == 0x124 || object_id == 0x125 ||
      object_id == 0x129 || (object_id >= 0x12D && object_id <= 0x133) ||
      (object_id >= 0x135 && object_id <= 0x137) || object_id == 0x13C ||
      object_id == 0x13F) {
    return 16;
  }
  // Beds (4x5)
  if (object_id == 0x122 || object_id == 0x128) {
    return 20;
  }
  // Tables (4x3)
  if (object_id == 0x123 || object_id == 0x13D) {
    return 12;
  }
  // 3x6 and 6x3 utility patterns
  if (object_id == 0x12C || object_id == 0x13E) {
    return 18;
  }
  // Spiral stairs (4x3)
  if (object_id >= 0x138 && object_id <= 0x13B) {
    return 12;
  }
  // Default: 8 tiles for other subtype 2 objects
  return 8;
}

int ObjectParser::GetSubtype3TileCount(int16_t object_id) const {
  // Water face variants:
  // - 0xF80 (Empty) may branch to the 0xF81 tile block at runtime, so we load
  //   both spans to preserve parity for state-dependent rendering.
  // - 0xF81 (Spitting): 4x5
  // - 0xF82 (Drenching): 4x7
  if (object_id == 0xF80) {
    return 32;
  }
  if (object_id == 0xF81) {
    return 20;
  }
  if (object_id == 0xF82) {
    return 28;
  }

  // BigChest (0xFB1 = ASM 0x231) and OpenBigChest (0xFB2 = ASM 0x232): 12 tiles
  // These use RoomDraw_1x3N_rightwards with N=4 (4 columns × 3 rows)
  if (object_id == 0xFB1 || object_id == 0xFB2) {
    return 12;
  }
  // TableRock4x3 variants (0xF94, 0xFCE, 0xFE7-0xFE8, 0xFEC-0xFED): 12 tiles
  if (object_id == 0xF94 || object_id == 0xFCE ||
      (object_id >= 0xFE7 && object_id <= 0xFE8) ||
      (object_id >= 0xFEC && object_id <= 0xFED)) {
    return 12;
  }
  // 4x4 pattern objects: 16 tiles
  // (0xFC8 = 0x248, 0xFE6 = 0x266, 0xFEB = 0x26B, 0xFFA = 0x27A)
  if (object_id == 0xFC8 || object_id == 0xFE6 || object_id == 0xFEB ||
      object_id == 0xFFA) {
    return 16;
  }
  // Boss shells (4x4)
  if (object_id == 0xF95 || object_id == 0xFF2 || object_id == 0xFFB) {
    return 16;
  }
  // Auto stairs (4x4)
  if ((object_id >= 0xF9B && object_id <= 0xF9D) || object_id == 0xFB3) {
    return 16;
  }
  // Straight inter-room stairs (4x4)
  if ((object_id >= 0xF9E && object_id <= 0xFA1) ||
      (object_id >= 0xFA6 && object_id <= 0xFA9)) {
    return 16;
  }
  // 4x4 single-pattern objects
  if (object_id == 0xFAA || object_id == 0xFAD || object_id == 0xFAE ||
      (object_id >= 0xFB4 && object_id <= 0xFB9) || object_id == 0xFCB ||
      object_id == 0xFCC || object_id == 0xFD4 || object_id == 0xFE2 ||
      object_id == 0xFF4 || object_id == 0xFF6 || object_id == 0xFF7) {
    return 16;
  }
  // Utility 6x3 (18 tiles)
  if (object_id == 0xFCD || object_id == 0xFDD) {
    return 18;
  }
  // Utility 3x5 (15 tiles)
  if (object_id == 0xFD5 || object_id == 0xFDB) {
    return 15;
  }
  // Archery game target door (3x6, 18 tiles)
  if (object_id >= 0xFE0 && object_id <= 0xFE1) {
    return 18;
  }
  // Solid wall decor 3x4 (12 tiles)
  if (object_id == 0xFE9 || object_id == 0xFEA || object_id == 0xFEE ||
      object_id == 0xFEF) {
    return 12;
  }
  // Light beams
  if (object_id == 0xFF0) {
    return 16;
  }
  if (object_id == 0xFF1) {
    return 36;
  }
  // Ganon Triforce floor decor (two 4x4 blocks -> 32 tiles)
  if (object_id == 0xFF8) {
    return 32;
  }
  // Table rock 4x3
  if (object_id == 0xFF9) {
    return 12;
  }
  // Default: 8 tiles for most subtype 3 objects
  return 8;
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

int ObjectParser::ResolveTileCountForObject(int16_t object_id) const {
  if (object_id < 0) {
    return 1;
  }

  switch (DetermineSubtype(object_id)) {
    case 1:
      return GetSubtype1TileCount(object_id);
    case 2:
      return GetSubtype2TileCount(object_id);
    case 3:
      return GetSubtype3TileCount(object_id);
    default:
      return 1;
  }
}

ObjectDrawInfo ObjectParser::GetObjectDrawInfo(int16_t object_id) const {
  ObjectDrawInfo info;

  // Keep draw-info tile counts sourced from subtype tables (ROM-facing data),
  // while routine selection comes from DrawRoutineRegistry (renderer-facing
  // canonical mapping).
  info.tile_count = std::max(1, ResolveTileCountForObject(object_id));
  const bool is_vertical_band = (object_id >= 0x60 && object_id <= 0x6F);
  info.is_horizontal = !is_vertical_band;
  info.is_vertical = is_vertical_band;

  info.draw_routine_id = DrawRoutineIds::kRightwards1x1Solid_1to16_plus3;
  info.routine_name = "DefaultSolid";
  info.both_layers = false;

  auto& registry = DrawRoutineRegistry::Get();
  // Feature flags (custom objects) can be toggled at runtime by project
  // context, so keep parser metadata aligned with current registry mappings.
  registry.RefreshFeatureFlagMappings();

  int registry_routine = registry.GetRoutineIdForObject(object_id);
  if (registry_routine < 0) {
    return info;
  }

  info.draw_routine_id = registry_routine;
  if (const DrawRoutineInfo* routine_info =
          registry.GetRoutineInfo(registry_routine);
      routine_info != nullptr) {
    info.routine_name = routine_info->name;
    info.both_layers = routine_info->draws_to_both_bgs;

    switch (routine_info->category) {
      case DrawRoutineInfo::Category::Downwards:
        info.is_horizontal = false;
        info.is_vertical = true;
        break;
      case DrawRoutineInfo::Category::Diagonal:
        info.is_horizontal = false;
        info.is_vertical = false;
        break;
      default:
        info.is_horizontal = true;
        info.is_vertical = false;
        break;
    }
  }

  return info;
}

}  // namespace zelda3
}  // namespace yaze
