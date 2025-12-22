#include "object_dimensions.h"

#include "zelda3/dungeon/room_object.h"

namespace yaze {
namespace zelda3 {

ObjectDimensionTable& ObjectDimensionTable::Get() {
  static ObjectDimensionTable instance;
  return instance;
}

absl::Status ObjectDimensionTable::LoadFromRom(Rom* rom) {
  if (!rom || !rom->is_loaded()) {
    return absl::FailedPreconditionError("ROM not loaded");
  }

  dimensions_.clear();
  InitializeDefaults();

  // Parse ROM tables for refinement
  ParseSubtype1Tables(rom);
  ParseSubtype2Tables(rom);
  ParseSubtype3Tables(rom);

  loaded_ = true;
  return absl::OkStatus();
}

std::pair<int, int> ObjectDimensionTable::GetBaseDimensions(int object_id) const {
  auto it = dimensions_.find(object_id);
  if (it != dimensions_.end()) {
    return {it->second.base_width, it->second.base_height};
  }
  return {2, 2};  // Default 16x16 pixels (2x2 tiles)
}

std::pair<int, int> ObjectDimensionTable::GetDimensions(int object_id, int size) const {
  auto it = dimensions_.find(object_id);
  if (it == dimensions_.end()) {
    // Unknown object - estimate from size
    // ASM: When size is 0, default to 32 (not 1)
    int s = (size == 0) ? 32 : size + 1;
    return {2 * s, 2};
  }

  const auto& entry = it->second;
  int w = entry.base_width;
  int h = entry.base_height;

  // ASM: GetSize_1to15or32 uses 32 when combined size is 0
  // This affects variable-size objects like floors
  int effective_size = size;
  if (entry.use_32_when_zero && size == 0) {
    effective_size = 32;
  }

  switch (entry.extend_dir) {
    case DimensionEntry::ExtendDir::Horizontal:
      w += effective_size * entry.extend_multiplier;
      break;
    case DimensionEntry::ExtendDir::Vertical:
      h += effective_size * entry.extend_multiplier;
      break;
    case DimensionEntry::ExtendDir::Both:
      w += effective_size * entry.extend_multiplier;
      h += effective_size * entry.extend_multiplier;
      break;
    case DimensionEntry::ExtendDir::Diagonal:
      // Diagonals extend both dimensions based on count
      // Width = base_width + size, Height = base_height + size
      // ASM: Each iteration draws 5 tiles vertically and advances X by 1
      // Bounding box height = 5 + (count - 1) = count + 4
      w += effective_size * entry.extend_multiplier;
      h += effective_size * entry.extend_multiplier;
      break;
    case DimensionEntry::ExtendDir::SuperSquare: {
      // SuperSquare objects use both size nibbles independently
      // ASM: RoomDraw_4x4BlocksIn4x4SuperSquare and similar
      // size_x = (size & 0x0F) + 1, size_y = ((size >> 4) & 0x0F) + 1
      // width = size_x * block_size, height = size_y * block_size
      int size_x = (size & 0x0F) + 1;
      int size_y = ((size >> 4) & 0x0F) + 1;
      w = size_x * entry.extend_multiplier;
      h = size_y * entry.extend_multiplier;
      break;
    }
    case DimensionEntry::ExtendDir::None:
    default:
      break;
  }

  return {w, h};
}

std::pair<int, int> ObjectDimensionTable::GetSelectionDimensions(int object_id, int size) const {
  auto it = dimensions_.find(object_id);
  if (it == dimensions_.end()) {
    // Unknown object - use reasonable default based on size
    int s = std::max(1, size + 1);
    return {std::min(s * 2, 16), 2};  // Cap at 16 tiles wide for selection
  }

  const auto& entry = it->second;
  int w = entry.base_width;
  int h = entry.base_height;

  // For selection bounds, use actual size (not 32-when-zero)
  // This gives a more accurate visual selection box
  int effective_size = size;
  
  switch (entry.extend_dir) {
    case DimensionEntry::ExtendDir::Horizontal:
      w += effective_size * entry.extend_multiplier;
      break;
    case DimensionEntry::ExtendDir::Vertical:
      h += effective_size * entry.extend_multiplier;
      break;
    case DimensionEntry::ExtendDir::Both:
      w += effective_size * entry.extend_multiplier;
      h += effective_size * entry.extend_multiplier;
      break;
    case DimensionEntry::ExtendDir::Diagonal:
      // Diagonals: both dimensions scale with size
      w += effective_size * entry.extend_multiplier;
      h += effective_size * entry.extend_multiplier;
      break;
    case DimensionEntry::ExtendDir::SuperSquare: {
      // SuperSquare: each nibble controls one dimension
      int size_x = (size & 0x0F) + 1;
      int size_y = ((size >> 4) & 0x0F) + 1;
      w = size_x * entry.extend_multiplier;
      h = size_y * entry.extend_multiplier;
      break;
    }
    case DimensionEntry::ExtendDir::None:
    default:
      break;
  }

  // Ensure minimum visible size
  w = std::max(w, 2);
  h = std::max(h, 2);

  return {w, h};
}

std::tuple<int, int, int, int> ObjectDimensionTable::GetHitTestBounds(
    const RoomObject& obj) const {
  auto [w, h] = GetDimensions(obj.id_, obj.size_);
  return {obj.x_, obj.y_, w, h};
}

void ObjectDimensionTable::InitializeDefaults() {
  using Dir = DimensionEntry::ExtendDir;

  // ============================================================================
  // Subtype 1 objects (0x00-0xF7)
  // ============================================================================

  // 0x00: Ceiling 2x2 - uses GetSize_1to15or32
  dimensions_[0x00] = {2, 2, Dir::Horizontal, 2, true};

  // 0x01-0x02: Wall 2x4 - uses GetSize_1to15or26
  for (int id = 0x01; id <= 0x02; id++) {
    dimensions_[id] = {2, 4, Dir::Horizontal, 2, false};  // Use 26 when zero
  }

  // 0x03-0x04: Wall 2x4 spaced 4 - GetSize_1to16
  for (int id = 0x03; id <= 0x04; id++) {
    dimensions_[id] = {2, 4, Dir::Horizontal, 2, false};
  }

  // 0x05-0x06: Wall 2x4 spaced 4 BothBG - GetSize_1to16
  for (int id = 0x05; id <= 0x06; id++) {
    dimensions_[id] = {2, 4, Dir::Horizontal, 2, false};
  }

  // 0x07-0x08: Floor 2x2 - GetSize_1to16
  for (int id = 0x07; id <= 0x08; id++) {
    dimensions_[id] = {2, 2, Dir::Horizontal, 2, false};
  }

  // 0x09-0x14: Diagonal walls - non-BothBG (count = size + 7)
  // Height = count + 4 tiles (5 tiles per column + diagonal extent)
  for (int id = 0x09; id <= 0x14; id++) {
    // Diagonal pattern: width = count tiles, height = count + 4 tiles
    dimensions_[id] = {7, 11, Dir::Diagonal, 1, false};  // base 7 + size*1, height 11 + size*1
  }

  // 0x15-0x20: Diagonal walls - BothBG (count = size + 6)
  for (int id = 0x15; id <= 0x20; id++) {
    dimensions_[id] = {6, 10, Dir::Diagonal, 1, false};  // base 6 + size*1, height 10 + size*1
  }

  // 0x21: Edge 1x2 +2 (count = size*2 + 1)
  dimensions_[0x21] = {1, 2, Dir::Horizontal, 2, false};

  // 0x22: Edge 1x1 +3 (count = size + 2)
  dimensions_[0x22] = {2, 1, Dir::Horizontal, 1, false};

  // 0x23-0x2E: Edge 1x1 +2 (count = size + 1)
  for (int id = 0x23; id <= 0x2E; id++) {
    dimensions_[id] = {1, 1, Dir::Horizontal, 1, false};
  }

  // 0x2F: Top corners 1x2 +13
  dimensions_[0x2F] = {10, 2, Dir::Horizontal, 1, false};

  // 0x30: Bottom corners 1x2 +13
  dimensions_[0x30] = {10, 2, Dir::Horizontal, 1, false};

  // 0x31-0x32: Nothing
  dimensions_[0x31] = {1, 1, Dir::None, 0, false};
  dimensions_[0x32] = {1, 1, Dir::None, 0, false};

  // 0x33: 4x4 block - GetSize_1to16
  dimensions_[0x33] = {4, 4, Dir::Horizontal, 4, false};

  // 0x34: Solid 1x1 +3 (count = size + 4)
  dimensions_[0x34] = {4, 1, Dir::Horizontal, 1, false};

  // 0x35: Door switcherer - fixed 4x4
  dimensions_[0x35] = {4, 4, Dir::None, 0, false};

  // 0x36-0x37: Decor 4x4 spaced 2 - spacing 6 tiles
  for (int id = 0x36; id <= 0x37; id++) {
    dimensions_[id] = {4, 4, Dir::Horizontal, 6, false};
  }

  // 0x38: Statue 2x3 spaced 2 - spacing 4 tiles
  dimensions_[0x38] = {2, 3, Dir::Horizontal, 4, false};

  // 0x39: Pillar 2x4 spaced 4 - spacing 6 tiles
  dimensions_[0x39] = {2, 4, Dir::Horizontal, 6, false};

  // 0x3A-0x3B: Decor 4x3 spaced 4 - spacing 6 tiles
  for (int id = 0x3A; id <= 0x3B; id++) {
    dimensions_[id] = {4, 3, Dir::Horizontal, 6, false};
  }

  // 0x3C: Doubled 2x2 spaced 2 - spacing 6 tiles
  dimensions_[0x3C] = {4, 2, Dir::Horizontal, 6, false};

  // 0x3D: Pillar 2x4 spaced 4 - spacing 6 tiles
  dimensions_[0x3D] = {2, 4, Dir::Horizontal, 6, false};

  // 0x3E: Decor 2x2 spaced 12 - spacing 14 tiles
  dimensions_[0x3E] = {2, 2, Dir::Horizontal, 14, false};

  // 0x3F-0x46: Edge 1x1 +2 (count = size + 1)
  for (int id = 0x3F; id <= 0x46; id++) {
    dimensions_[id] = {1, 1, Dir::Horizontal, 1, false};
  }

  // 0x47: Waterfall47 - 1x5 columns, width = 2 + (size+1)*2
  dimensions_[0x47] = {4, 5, Dir::Horizontal, 2, false};

  // 0x48: Waterfall48 - 1x3 columns, width = 2 + (size+1)*2
  dimensions_[0x48] = {4, 3, Dir::Horizontal, 2, false};

  // 0x49-0x4A: Floor Tile 4x2 - GetSize_1to16
  for (int id = 0x49; id <= 0x4A; id++) {
    dimensions_[id] = {4, 2, Dir::Horizontal, 4, false};
  }

  // 0x4B: Decor 2x2 spaced 12 - spacing 14 tiles
  dimensions_[0x4B] = {2, 2, Dir::Horizontal, 14, false};

  // 0x4C: Bar 4x3 - spacing 6 tiles
  dimensions_[0x4C] = {4, 3, Dir::Horizontal, 6, false};

  // 0x4D-0x4F: Shelf 4x4 - spacing 6 tiles
  for (int id = 0x4D; id <= 0x4F; id++) {
    dimensions_[id] = {4, 4, Dir::Horizontal, 6, false};
  }

  // 0x50: Line 1x1 +1 (count = size + 2)
  dimensions_[0x50] = {2, 1, Dir::Horizontal, 1, false};

  // 0x51-0x52: Cannon Hole 4x3 - GetSize_1to16
  for (int id = 0x51; id <= 0x52; id++) {
    dimensions_[id] = {4, 3, Dir::Horizontal, 4, false};
  }

  // 0x53: Floor 2x2 - GetSize_1to16
  dimensions_[0x53] = {2, 2, Dir::Horizontal, 2, false};

  // 0x54-0x5A: Nothing
  for (int id = 0x54; id <= 0x5A; id++) {
    dimensions_[id] = {1, 1, Dir::None, 0, false};
  }

  // 0x5B-0x5C: Cannon Hole 4x3
  for (int id = 0x5B; id <= 0x5C; id++) {
    dimensions_[id] = {4, 3, Dir::Horizontal, 4, false};
  }

  // 0x5D: Big Rail 1x3 +5 (count = size + 6)
  dimensions_[0x5D] = {6, 3, Dir::Horizontal, 1, false};

  // 0x5E: Block 2x2 spaced 2
  dimensions_[0x5E] = {2, 2, Dir::Horizontal, 4, false};

  // 0x5F: Edge 1x1 +23 (count = size + 1 but offset +23)
  dimensions_[0x5F] = {1, 1, Dir::Horizontal, 1, false};

  // 0x60: Downwards 2x2 - GetSize_1to15or32
  dimensions_[0x60] = {2, 2, Dir::Vertical, 2, true};

  // 0x61-0x62: Downwards 4x2 - GetSize_1to15or26
  for (int id = 0x61; id <= 0x62; id++) {
    dimensions_[id] = {4, 2, Dir::Vertical, 2, false};
  }

  // 0x63-0x68: Downwards variants - GetSize_1to16
  for (int id = 0x63; id <= 0x68; id++) {
    dimensions_[id] = {4, 2, Dir::Vertical, 2, false};
  }

  // 0x69: Downwards edge +3
  dimensions_[0x69] = {1, 2, Dir::Vertical, 1, false};

  // 0x6A-0x6B: Downwards edge
  for (int id = 0x6A; id <= 0x6B; id++) {
    dimensions_[id] = {1, 1, Dir::Vertical, 1, false};
  }

  // 0x6C-0x6D: Downwards corners
  for (int id = 0x6C; id <= 0x6D; id++) {
    dimensions_[id] = {2, 1, Dir::Vertical, 1, false};
  }

  // 0x6E-0x6F: Nothing
  dimensions_[0x6E] = {1, 1, Dir::None, 0, false};
  dimensions_[0x6F] = {1, 1, Dir::None, 0, false};

  // 0x70: Downwards Floor 4x4
  dimensions_[0x70] = {4, 4, Dir::Vertical, 4, false};

  // 0x71: Downwards Solid 1x1 +3
  dimensions_[0x71] = {1, 4, Dir::Vertical, 1, false};

  // 0x72: Nothing
  dimensions_[0x72] = {1, 1, Dir::None, 0, false};

  // 0x73-0x74: Downwards Decor 4x4 spaced 2
  for (int id = 0x73; id <= 0x74; id++) {
    dimensions_[id] = {4, 4, Dir::Vertical, 6, false};
  }

  // 0x75: Downwards Pillar 2x4 spaced 2
  dimensions_[0x75] = {2, 4, Dir::Vertical, 6, false};

  // 0x76-0x77: Downwards Decor 3x4 spaced 4
  for (int id = 0x76; id <= 0x77; id++) {
    dimensions_[id] = {3, 4, Dir::Vertical, 6, false};
  }

  // 0x78, 0x7B: Downwards Decor 2x2 spaced 12
  dimensions_[0x78] = {2, 2, Dir::Vertical, 14, false};
  dimensions_[0x7B] = {2, 2, Dir::Vertical, 14, false};

  // 0x79-0x7A: Downwards Edge 1x1
  for (int id = 0x79; id <= 0x7A; id++) {
    dimensions_[id] = {1, 1, Dir::Vertical, 1, false};
  }

  // 0x7C: Downwards Line 1x1 +1
  dimensions_[0x7C] = {1, 2, Dir::Vertical, 1, false};

  // 0x7D: Downwards 2x2
  dimensions_[0x7D] = {2, 2, Dir::Vertical, 2, false};

  // 0x7E: Nothing
  dimensions_[0x7E] = {1, 1, Dir::None, 0, false};

  // 0x7F-0x80: Downwards Decor 2x4 spaced 8
  for (int id = 0x7F; id <= 0x80; id++) {
    dimensions_[id] = {2, 4, Dir::Vertical, 10, false};
  }

  // 0x81-0x84: Downwards Decor 3x4 spaced 2
  for (int id = 0x81; id <= 0x84; id++) {
    dimensions_[id] = {3, 4, Dir::Vertical, 5, false};
  }

  // 0x85-0x86: Downwards Cannon Hole 3x6
  for (int id = 0x85; id <= 0x86; id++) {
    dimensions_[id] = {3, 6, Dir::Vertical, 6, false};
  }

  // 0x87: Downwards Pillar 2x4 spaced 2
  dimensions_[0x87] = {2, 4, Dir::Vertical, 6, false};

  // 0x88: Downwards Big Rail 3x1 +5
  dimensions_[0x88] = {3, 6, Dir::Vertical, 1, false};

  // 0x89: Downwards Block 2x2 spaced 2
  dimensions_[0x89] = {2, 2, Dir::Vertical, 4, false};

  // 0x8A-0x8C: Edge variants
  for (int id = 0x8A; id <= 0x8C; id++) {
    dimensions_[id] = {1, 1, Dir::Vertical, 1, false};
  }

  // 0x8D-0x8E: Downwards Edge 1x1
  for (int id = 0x8D; id <= 0x8E; id++) {
    dimensions_[id] = {1, 1, Dir::Vertical, 1, false};
  }

  // 0x8F: Downwards Bar 2x3
  dimensions_[0x8F] = {2, 3, Dir::Vertical, 3, false};

  // 0x90-0x94: Staircase objects - 4x4 or 4x2
  for (int id = 0x90; id <= 0x94; id++) {
    dimensions_[id] = {4, 4, Dir::None, 0, false};
  }

  // 0x95: Downwards Pots 2x2
  dimensions_[0x95] = {2, 2, Dir::Vertical, 2, false};

  // 0x96: Downwards Hammer Pegs 2x2
  dimensions_[0x96] = {2, 2, Dir::Vertical, 2, false};

  // 0x97-0x9F: Nothing
  for (int id = 0x97; id <= 0x9F; id++) {
    dimensions_[id] = {1, 1, Dir::None, 0, false};
  }

  // ============================================================================
  // Diagonal ceilings (0xA0-0xAC)
  // ============================================================================
  // ASM: These have fixed patterns, 4x4 base size
  // TopLeft: 0xA0, 0xA5, 0xA9
  for (int id : {0xA0, 0xA5, 0xA9}) {
    dimensions_[id] = {4, 4, Dir::Diagonal, 4, false};
  }
  // BottomLeft: 0xA1, 0xA6, 0xAA
  for (int id : {0xA1, 0xA6, 0xAA}) {
    dimensions_[id] = {4, 4, Dir::Diagonal, 4, false};
  }
  // TopRight: 0xA2, 0xA7, 0xAB
  for (int id : {0xA2, 0xA7, 0xAB}) {
    dimensions_[id] = {4, 4, Dir::Diagonal, 4, false};
  }
  // BottomRight: 0xA3, 0xA8, 0xAC
  for (int id : {0xA3, 0xA8, 0xAC}) {
    dimensions_[id] = {4, 4, Dir::Diagonal, 4, false};
  }
  // BigHole4x4: 0xA4 - extends both directions
  dimensions_[0xA4] = {4, 4, Dir::Both, 4, true};

  // 0xAD-0xAF, 0xBE-0xBF: Nothing
  for (int id : {0xAD, 0xAE, 0xAF, 0xBE, 0xBF}) {
    dimensions_[id] = {1, 1, Dir::None, 0, false};
  }

  // 0xB0-0xB1: Rightwards Edge 1x1 +7
  for (int id = 0xB0; id <= 0xB1; id++) {
    dimensions_[id] = {7, 1, Dir::Horizontal, 1, false};
  }

  // 0xB2: 4x4 block
  dimensions_[0xB2] = {4, 4, Dir::Horizontal, 4, false};

  // 0xB3-0xB4: Edge 1x1
  for (int id = 0xB3; id <= 0xB4; id++) {
    dimensions_[id] = {1, 1, Dir::Horizontal, 1, false};
  }

  // 0xB5: Weird 2x4
  dimensions_[0xB5] = {2, 4, Dir::Vertical, 2, false};

  // 0xB6-0xB7: Rightwards 2x4
  for (int id = 0xB6; id <= 0xB7; id++) {
    dimensions_[id] = {2, 4, Dir::Horizontal, 2, false};
  }

  // 0xB8-0xB9: Rightwards 2x2
  for (int id = 0xB8; id <= 0xB9; id++) {
    dimensions_[id] = {2, 2, Dir::Horizontal, 2, true};
  }

  // 0xBA: 4x4 block
  dimensions_[0xBA] = {4, 4, Dir::Horizontal, 4, false};

  // 0xBB: Rightwards Block 2x2 spaced 2
  dimensions_[0xBB] = {2, 2, Dir::Horizontal, 4, false};

  // 0xBC: Rightwards Pots 2x2
  dimensions_[0xBC] = {2, 2, Dir::Horizontal, 2, false};

  // 0xBD: Rightwards Hammer Pegs 2x2
  dimensions_[0xBD] = {2, 2, Dir::Horizontal, 2, false};

  // ============================================================================
  // SuperSquare objects (0xC0-0xCF, 0xD0-0xDF, 0xE0-0xEF)
  // These use both size nibbles for independent X/Y sizing.
  // RoomDraw_4x4BlocksIn4x4SuperSquare, RoomDraw_4x4FloorIn4x4SuperSquare, etc.
  // width = ((size & 0x0F) + 1) * 4, height = (((size >> 4) & 0x0F) + 1) * 4
  // ============================================================================

  // 0xC0: Large ceiling (4x4 blocks in super squares)
  // ASM: RoomDraw_4x4BlocksIn4x4SuperSquare ($018B94)
  dimensions_[0xC0] = {0, 0, Dir::SuperSquare, 4, false};

  // 0xC2: 4x4 blocks variant (same routine as 0xC0)
  dimensions_[0xC2] = {0, 0, Dir::SuperSquare, 4, false};

  // 0xC3, 0xD7: 3x3 floor in 4x4 super square
  dimensions_[0xC3] = {0, 0, Dir::SuperSquare, 4, false};
  dimensions_[0xD7] = {0, 0, Dir::SuperSquare, 4, false};

  // 0xC4: 4x4 floor one
  dimensions_[0xC4] = {0, 0, Dir::SuperSquare, 4, false};

  // 0xC5-0xCA: 4x4 floor patterns
  for (int id = 0xC5; id <= 0xCA; id++) {
    dimensions_[id] = {0, 0, Dir::SuperSquare, 4, false};
  }

  // 0xD1-0xD2: 4x4 floor patterns
  dimensions_[0xD1] = {0, 0, Dir::SuperSquare, 4, false};
  dimensions_[0xD2] = {0, 0, Dir::SuperSquare, 4, false};

  // 0xD9: 4x4 floor pattern
  dimensions_[0xD9] = {0, 0, Dir::SuperSquare, 4, false};

  // 0xDB: 4x4 floor two
  dimensions_[0xDB] = {0, 0, Dir::SuperSquare, 4, false};

  // 0xDF-0xE8: 4x4 floor patterns
  for (int id = 0xDF; id <= 0xE8; id++) {
    dimensions_[id] = {0, 0, Dir::SuperSquare, 4, false};
  }

  // ============================================================================
  // Chests - fixed 2x2
  // ============================================================================
  for (int id : {0xF9, 0xFA, 0xFB, 0xFC, 0xFD}) {
    dimensions_[id] = {2, 2, Dir::None, 0, false};
  }

  // ============================================================================
  // Subtype 2 objects (0x100-0x13F)
  // ============================================================================
  // Layout corners - 4x4
  for (int id = 0x100; id <= 0x103; id++) {
    dimensions_[id] = {4, 4, Dir::None, 0, false};
  }

  // Other 4x4 patterns
  for (int id = 0x104; id <= 0x10F; id++) {
    dimensions_[id] = {4, 4, Dir::None, 0, false};
  }

  // Corners - 4x4
  for (int id = 0x110; id <= 0x11F; id++) {
    dimensions_[id] = {4, 4, Dir::None, 0, false};
  }

  // Tables, beds, etc
  dimensions_[0x122] = {4, 5, Dir::None, 0, false};  // Bed
  dimensions_[0x123] = {4, 3, Dir::None, 0, false};  // Table
  dimensions_[0x128] = {4, 5, Dir::None, 0, false};  // Bed variant
  dimensions_[0x12C] = {3, 6, Dir::None, 0, false};  // 3x6 pattern
  dimensions_[0x13E] = {6, 3, Dir::None, 0, false};  // Utility 6x3

  // ============================================================================
  // Subtype 3 objects (0xF80-0xFFF)
  // ============================================================================
  // Default for Type 3: most are 2x2 fixed objects
  for (int id = 0xF80; id <= 0xFFF; id++) {
    dimensions_[id] = {2, 2, Dir::None, 0, false};
  }

  // Override specific Type 3 objects with known sizes
  // 0xFB1-0xFB2: Big Chest 4x3
  dimensions_[0xFB1] = {4, 3, Dir::None, 0, false};
  dimensions_[0xFB2] = {4, 3, Dir::None, 0, false};

  // Light beams and boss shells
  dimensions_[0xFF0] = {2, 4, Dir::None, 0, false};  // Light beam
  dimensions_[0xFF1] = {4, 8, Dir::None, 0, false};  // Big light beam
  dimensions_[0xFF8] = {4, 8, Dir::None, 0, false};  // Ganon Triforce floor
}

void ObjectDimensionTable::ParseSubtype1Tables(Rom* rom) {
  // ROM addresses from ZScream:
  // Tile data offset table: $018000
  // Routine pointer table: $018200
  // These tables help determine object sizes based on tile counts

  constexpr int kSubtype1TileOffsets = 0x8000;
  constexpr int kSubtype1Routines = 0x8200;

  // Read tile count for each object to refine dimensions
  for (int id = 0; id < 0xF8; id++) {
    auto offset_result = rom->ReadWord(kSubtype1TileOffsets + id * 2);
    if (!offset_result.ok()) continue;

    // Tile count can inform base size
    // This is a simplified heuristic - full accuracy requires parsing
    // the actual tile data
  }
}

void ObjectDimensionTable::ParseSubtype2Tables(Rom* rom) {
  // Subtype 2 data offset: $0183F0
  // Subtype 2 routine ptr: $018470
  constexpr int kSubtype2TileOffsets = 0x83F0;
  (void)kSubtype2TileOffsets;
  (void)rom;
  // Similar parsing for subtype 2 objects
}

void ObjectDimensionTable::ParseSubtype3Tables(Rom* rom) {
  // Subtype 3 data offset: $0184F0
  // Subtype 3 routine ptr: $0185F0
  constexpr int kSubtype3TileOffsets = 0x84F0;
  (void)kSubtype3TileOffsets;
  (void)rom;
  // Similar parsing for subtype 3 objects
}

}  // namespace zelda3
}  // namespace yaze
