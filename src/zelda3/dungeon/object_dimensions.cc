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
    int s = size + 1;
    return {2 * s, 2};
  }

  const auto& entry = it->second;
  int w = entry.base_width;
  int h = entry.base_height;

  switch (entry.extend_dir) {
    case DimensionEntry::ExtendDir::Horizontal:
      w += size * entry.extend_multiplier;
      break;
    case DimensionEntry::ExtendDir::Vertical:
      h += size * entry.extend_multiplier;
      break;
    case DimensionEntry::ExtendDir::Both:
      w += size * entry.extend_multiplier;
      h += size * entry.extend_multiplier;
      break;
    case DimensionEntry::ExtendDir::None:
    default:
      break;
  }

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
  for (int id = 0x00; id <= 0x0F; id++) {
    dimensions_[id] = {2, 4, Dir::Horizontal, 2};  // 2x4 base, extend right
  }

  // Floor patterns - 2x2 repeating
  for (int id = 0x10; id <= 0x1F; id++) {
    dimensions_[id] = {2, 2, Dir::Horizontal, 2};
  }

  // Rails and edges
  for (int id = 0x20; id <= 0x2F; id++) {
    dimensions_[id] = {2, 2, Dir::Horizontal, 2};
  }

  // Pits and water - both directions
  for (int id = 0x30; id <= 0x3F; id++) {
    dimensions_[id] = {2, 2, Dir::Both, 2};
  }

  // Vertical walls
  for (int id = 0x40; id <= 0x4F; id++) {
    dimensions_[id] = {4, 2, Dir::Vertical, 2};
  }

  // Diagonal walls (acute angle /)
  for (int id = 0x50; id <= 0x5F; id++) {
    dimensions_[id] = {2, 2, Dir::Both, 2};  // Diagonals extend both ways
  }

  // Diagonal walls (grave angle \)
  for (int id = 0x60; id <= 0x6F; id++) {
    dimensions_[id] = {2, 2, Dir::Both, 2};
  }

  // Static objects (pots, blocks, switches) - fixed size
  for (int id = 0x70; id <= 0x8F; id++) {
    dimensions_[id] = {2, 2, Dir::None, 0};
  }

  // Chests - fixed 2x2
  dimensions_[0xF9] = {2, 2, Dir::None, 0};
  dimensions_[0xFB] = {2, 2, Dir::None, 0};

  // Subtype 2 objects (0x100-0x1FF)
  // Large decorative objects - mostly 4x4
  for (int id = 0x100; id <= 0x10F; id++) {
    dimensions_[id] = {4, 4, Dir::None, 0};
  }

  // Corners - 4x4
  for (int id = 0x110; id <= 0x11F; id++) {
    dimensions_[id] = {4, 4, Dir::None, 0};
  }

  // Tables, beds, etc
  dimensions_[0x122] = {4, 5, Dir::None, 0};  // Bed
  dimensions_[0x123] = {4, 3, Dir::None, 0};  // Table
  dimensions_[0x128] = {4, 5, Dir::None, 0};  // Bed variant

  // Subtype 3 objects (0xF00+) - special/doors
  // Doors are typically 2x3 or 3x2
  for (int id = 0xF00; id <= 0xFFF; id++) {
    dimensions_[id] = {2, 3, Dir::None, 0};
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
