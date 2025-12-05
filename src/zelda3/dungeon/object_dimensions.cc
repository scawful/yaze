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
      // Diagonals extend in one direction based on count
      // Width = count, Height = 5 (fixed for diagonal patterns)
      w = effective_size + entry.extend_multiplier;
      h = 5;  // Diagonal patterns draw 5 tiles vertically per column
      break;
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
      // Diagonals: width = size + multiplier, height = 5
      w = effective_size + entry.extend_multiplier;
      h = 5;
      break;
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

  // Subtype 1 objects (0x00-0xF7)
  // Walls and basic structures - horizontal extension
  // ASM: Most walls extend by size+1 tiles horizontally
  for (int id = 0x00; id <= 0x0F; id++) {
    dimensions_[id] = {2, 4, Dir::Horizontal, 2, true};  // 2x4 base, use_32_when_zero
  }

  // Floor patterns - 2x2 repeating with size extension
  // ASM: Floors use GetSize_1to15or32, default to 32 when size=0
  for (int id = 0x10; id <= 0x1F; id++) {
    dimensions_[id] = {2, 2, Dir::Horizontal, 2, true};
  }

  // Rails and edges - horizontal extension
  for (int id = 0x20; id <= 0x2F; id++) {
    dimensions_[id] = {2, 2, Dir::Horizontal, 2, true};
  }

  // Pits and water - both directions
  // ASM: These extend both ways using size parameter
  for (int id = 0x30; id <= 0x3F; id++) {
    dimensions_[id] = {2, 2, Dir::Both, 2, true};
  }

  // Vertical walls
  for (int id = 0x40; id <= 0x4F; id++) {
    dimensions_[id] = {4, 2, Dir::Vertical, 2, true};
  }

  // Diagonal walls (acute angle /) - non-BothBG: multiplier 7
  // ASM: RoomDraw_DiagonalAcute_1to16 uses size+7
  for (int id = 0x50; id <= 0x57; id++) {
    dimensions_[id] = {1, 5, Dir::Diagonal, 7, false};
  }
  // Diagonal walls (acute angle /) - BothBG: multiplier 6
  // ASM: RoomDraw_DiagonalAcute_1to16_BothBG uses size+6
  for (int id = 0x58; id <= 0x5F; id++) {
    dimensions_[id] = {1, 5, Dir::Diagonal, 6, false};
  }

  // Diagonal walls (grave angle \) - non-BothBG: multiplier 7
  // ASM: RoomDraw_DiagonalGrave_1to16 uses size+7
  for (int id = 0x60; id <= 0x67; id++) {
    dimensions_[id] = {1, 5, Dir::Diagonal, 7, false};
  }
  // Diagonal walls (grave angle \) - BothBG: multiplier 6
  // ASM: RoomDraw_DiagonalGrave_1to16_BothBG uses size+6
  for (int id = 0x68; id <= 0x6F; id++) {
    dimensions_[id] = {1, 5, Dir::Diagonal, 6, false};
  }

  // Static objects (pots, blocks, switches) - fixed size
  for (int id = 0x70; id <= 0x8F; id++) {
    dimensions_[id] = {2, 2, Dir::None, 0, false};
  }

  // Staircase objects
  for (int id = 0x90; id <= 0x9F; id++) {
    dimensions_[id] = {4, 4, Dir::None, 0, false};
  }

  // Diagonal ceilings (0xA0-0xAC)
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

  // Chests - fixed 2x2
  for (int id : {0xF9, 0xFA, 0xFB, 0xFC, 0xFD}) {
    dimensions_[id] = {2, 2, Dir::None, 0, false};
  }

  // Subtype 2 objects (0x100-0x1FF)
  // Large decorative objects - mostly 4x4
  for (int id = 0x100; id <= 0x10F; id++) {
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

  // Subtype 3 objects (0xF00+) - special/doors
  // Doors are typically 2x3 or 3x2
  for (int id = 0xF00; id <= 0xFFF; id++) {
    dimensions_[id] = {2, 3, Dir::None, 0, false};
  }
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
