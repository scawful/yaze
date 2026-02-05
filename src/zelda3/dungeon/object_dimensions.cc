#include "object_dimensions.h"

#include <limits>

#include "core/features.h"
#include "zelda3/dungeon/custom_object.h"
#include "zelda3/dungeon/room_object.h"

namespace yaze {
namespace zelda3 {

namespace {
bool GetSomariaLineDimensions(int object_id, int size, int* width,
                              int* height) {
  if (!width || !height) {
    return false;
  }
  if (!((object_id >= 0xF83 && object_id <= 0xF8C) || object_id == 0xF8E ||
        object_id == 0xF8F)) {
    return false;
  }

  int length = (size & 0x0F) + 1;
  int sub_id = object_id & 0x0F;
  int dx = 1;
  int dy = 0;
  switch (sub_id) {
    case 0x03:
      dx = 1;
      dy = 0;
      break;
    case 0x04:
      dx = 0;
      dy = 1;
      break;
    case 0x05:
      dx = 1;
      dy = 1;
      break;
    case 0x06:
      dx = -1;
      dy = 1;
      break;
    case 0x07:
      dx = 1;
      dy = 0;
      break;
    case 0x08:
      dx = 0;
      dy = 1;
      break;
    case 0x09:
      dx = 1;
      dy = 1;
      break;
    case 0x0A:
      dx = 1;
      dy = 0;
      break;
    case 0x0B:
      dx = 0;
      dy = 1;
      break;
    case 0x0C:
      dx = 1;
      dy = 1;
      break;
    case 0x0E:
      dx = 1;
      dy = 0;
      break;
    case 0x0F:
      dx = 0;
      dy = 1;
      break;
    default:
      dx = 1;
      dy = 0;
      break;
  }

  if (dx != 0 && dy != 0) {
    *width = length;
    *height = length;
  } else if (dx != 0) {
    *width = length;
    *height = 1;
  } else {
    *width = 1;
    *height = length;
  }
  return true;
}
}  // namespace

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

std::pair<int, int> ObjectDimensionTable::GetBaseDimensions(
    int object_id) const {
  auto it = dimensions_.find(object_id);
  if (it != dimensions_.end()) {
    return {it->second.base_width, it->second.base_height};
  }
  return {2, 2};  // Default 16x16 pixels (2x2 tiles)
}

int ObjectDimensionTable::ResolveEffectiveSize(const DimensionEntry& entry,
                                               int size) const {
  if (size != 0) {
    return size;
  }
  if (entry.zero_size_override > 0) {
    return entry.zero_size_override;
  }
  if (entry.use_32_when_zero) {
    return 32;
  }
  return 0;
}

std::pair<int, int> ObjectDimensionTable::GetDimensions(int object_id,
                                                        int size) const {
  int somaria_width = 0;
  int somaria_height = 0;
  if (GetSomariaLineDimensions(object_id, size, &somaria_width,
                               &somaria_height)) {
    return {somaria_width, somaria_height};
  }
  if (object_id == 0xC1) {
    int width = ((size & 0x0F) + 4) * 2;
    int height = (((size >> 4) & 0x0F) + 1) * 3;
    return {width, height};
  }
  if (object_id == 0xDC) {
    int width = (size & 0x0F) + 1;
    int height = (((size >> 4) & 0x0F) * 2) + 7;
    return {width, height};
  }
  if (object_id == 0xD8 || object_id == 0xDA) {
    int size_x = ((size >> 2) & 0x03);
    int size_y = (size & 0x03);
    int width = (size_x + 2) * 4;
    int height = (size_y + 2) * 4;
    return {width, height};
  }
  if (object_id == 0xDD) {
    int size_x = ((size >> 2) & 0x03);
    int size_y = (size & 0x03);
    int width = 4 + (size_x * 2);
    int height = 4 + (size_y * 2);
    return {width, height};
  }
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

  int effective_size = ResolveEffectiveSize(entry, size);

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
      // SuperSquare objects use subtype1 size's 2-bit X/Y fields
      // size_x = ((size >> 2) & 0x03) + 1, size_y = (size & 0x03) + 1
      int size_x = ((size >> 2) & 0x03) + 1;
      int size_y = (size & 0x03) + 1;
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

std::pair<int, int> ObjectDimensionTable::GetSelectionDimensions(
    int object_id, int size) const {
  int somaria_width = 0;
  int somaria_height = 0;
  if (GetSomariaLineDimensions(object_id, size, &somaria_width,
                               &somaria_height)) {
    return {somaria_width, somaria_height};
  }
  if (object_id == 0xC1) {
    int width = ((size & 0x0F) + 4) * 2;
    int height = (((size >> 4) & 0x0F) + 1) * 3;
    return {width, height};
  }
  if (object_id == 0xDC) {
    int width = (size & 0x0F) + 1;
    int height = (((size >> 4) & 0x0F) * 2) + 7;
    return {width, height};
  }
  if (object_id == 0xD8 || object_id == 0xDA) {
    int size_x = ((size >> 2) & 0x03);
    int size_y = (size & 0x03);
    int width = (size_x + 2) * 4;
    int height = (size_y + 2) * 4;
    return {width, height};
  }
  if (object_id == 0xDD) {
    int size_x = ((size >> 2) & 0x03);
    int size_y = (size & 0x03);
    int width = 4 + (size_x * 2);
    int height = 4 + (size_y * 2);
    return {width, height};
  }
  auto it = dimensions_.find(object_id);
  if (it == dimensions_.end()) {
    // Unknown object - use reasonable default based on size
    int s = std::max(1, size + 1);
    return {std::min(s * 2, 16), 2};  // Cap at 16 tiles wide for selection
  }

  const auto& entry = it->second;
  int w = entry.base_width;
  int h = entry.base_height;

  // Selection bounds should match draw-size semantics.
  int effective_size = ResolveEffectiveSize(entry, size);

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
      // SuperSquare: subtype1 size uses 2-bit X/Y fields
      int size_x = ((size >> 2) & 0x03) + 1;
      int size_y = (size & 0x03) + 1;
      w = size_x * entry.extend_multiplier;
      h = size_y * entry.extend_multiplier;
      break;
    }
    case DimensionEntry::ExtendDir::None:
    default:
      break;
  }

  // Ensure minimum visible size (1 tile)
  w = std::max(w, 1);
  h = std::max(h, 1);

  return {w, h};
}

ObjectDimensionTable::SelectionBounds ObjectDimensionTable::GetSelectionBounds(
    int object_id, int size) const {
  if (core::FeatureFlags::get().kEnableCustomObjects) {
    int subtype = size & 0x1F;
    auto custom_or =
        CustomObjectManager::Get().GetObjectInternal(object_id, subtype);
    if (custom_or.ok()) {
      auto custom = custom_or.value();
      if (custom && !custom->IsEmpty()) {
        int min_x = std::numeric_limits<int>::max();
        int min_y = std::numeric_limits<int>::max();
        int max_x = std::numeric_limits<int>::min();
        int max_y = std::numeric_limits<int>::min();

        for (const auto& entry : custom->tiles) {
          min_x = std::min(min_x, entry.rel_x);
          min_y = std::min(min_y, entry.rel_y);
          max_x = std::max(max_x, entry.rel_x);
          max_y = std::max(max_y, entry.rel_y);
        }

        if (min_x != std::numeric_limits<int>::max()) {
          SelectionBounds bounds;
          bounds.offset_x = min_x;
          bounds.offset_y = min_y;
          bounds.width = (max_x - min_x) + 1;
          bounds.height = (max_y - min_y) + 1;
          return bounds;
        }
      }
    }
  }

  auto [w, h] = GetSelectionDimensions(object_id, size);
  SelectionBounds bounds{0, 0, w, h};

  switch (object_id) {
    // Offset +3 (1x1 solid +3)
    case 0x34:
    case 0x11F:
    case 0x120:
    case 0xF96:
      bounds.offset_x = 3;
      break;

    // Rightwards corners +13
    case 0x2F:
      bounds.offset_x = 13;
      break;
    case 0x30:
      bounds.offset_x = 13;
      bounds.offset_y = 1;
      break;

    // Downwards corners +12
    case 0x6C:
    case 0x6D:
      bounds.offset_x = 12;
      break;

    // Moving wall east draws from x to x-2
    case 0xCE:
      bounds.offset_x = -2;
      break;

    // Somaria line diagonal down-left (0xF86 / 0x206)
    case 0xF86: {
      int length = (size & 0x0F) + 1;
      bounds.offset_x = -(length - 1);
      break;
    }

    default:
      break;
  }

  // Acute diagonals extend upward relative to origin.
  if (object_id == 0x09 || object_id == 0x0C || object_id == 0x0D ||
      object_id == 0x10 || object_id == 0x11 || object_id == 0x14) {
    bounds.offset_y = -(bounds.width - 1);
  }
  if (object_id == 0x15 || object_id == 0x18 || object_id == 0x19 ||
      object_id == 0x1C || object_id == 0x1D || object_id == 0x20) {
    bounds.offset_y = -(bounds.width - 1);
  }

  // Diagonal ceiling bottom-right (objects 0xA3, 0xA8, 0xAC) extend upward
  if (object_id == 0xA3 || object_id == 0xA8 || object_id == 0xAC) {
    bounds.offset_y = -(bounds.width - 1);
  }

  return bounds;
}

std::tuple<int, int, int, int> ObjectDimensionTable::GetHitTestBounds(
    const RoomObject& obj) const {
  auto bounds = GetSelectionBounds(obj.id_, obj.size_);
  return {obj.x_ + bounds.offset_x, obj.y_ + bounds.offset_y, bounds.width,
          bounds.height};
}

void ObjectDimensionTable::InitializeDefaults() {
  using Dir = DimensionEntry::ExtendDir;

  // ============================================================================
  // Subtype 1 objects (0x00-0xF7)
  // ============================================================================

  // 0x00: Ceiling 2x2 - uses GetSize_1to15or32
  dimensions_[0x00] = {0, 2, Dir::Horizontal, 2, true};

  // 0x01-0x02: Wall 2x4 - uses GetSize_1to15or26
  for (int id = 0x01; id <= 0x02; id++) {
    dimensions_[id] = {0, 4, Dir::Horizontal, 2, false};  // Use 26 when zero
  }
  for (int id = 0x01; id <= 0x02; id++) {
    dimensions_[id].zero_size_override = 26;
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
    dimensions_[id] = {7, 11, Dir::Diagonal, 1,
                       false};  // base 7 + size*1, height 11 + size*1
  }

  // 0x15-0x20: Diagonal walls - BothBG (count = size + 6)
  for (int id = 0x15; id <= 0x20; id++) {
    dimensions_[id] = {6, 10, Dir::Diagonal, 1,
                       false};  // base 6 + size*1, height 10 + size*1
  }

  // 0x21: Edge 1x3 +2 (width = size*2 + 4)
  dimensions_[0x21] = {4, 3, Dir::Horizontal, 2, false};

  // 0x22: Edge 1x1 +3 (width = size + 4)
  dimensions_[0x22] = {4, 1, Dir::Horizontal, 1, false};

  // 0x23-0x2E: Edge 1x1 +2 (width = size + 3)
  for (int id = 0x23; id <= 0x2E; id++) {
    dimensions_[id] = {3, 1, Dir::Horizontal, 1, false};
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

  // 0x35: Door switcher - uses a single tile in ROM data
  dimensions_[0x35] = {1, 1, Dir::None, 0, false};

  // 0x36-0x37: Decor 4x4 spaced 2 - spacing 6 tiles
  for (int id = 0x36; id <= 0x37; id++) {
    dimensions_[id] = {4, 4, Dir::Horizontal, 6, false};
  }

  // 0x38: Statue 2x3 spaced 2 - spacing 4 tiles
  dimensions_[0x38] = {2, 3, Dir::Horizontal, 4, false};

  // 0x39: Pillar 2x4 spaced 4 - spacing 4 tiles between starts
  dimensions_[0x39] = {2, 4, Dir::Horizontal, 4, false};

  // 0x3A-0x3B: Decor 4x3 spaced 4 - spacing 8 tiles between starts
  for (int id = 0x3A; id <= 0x3B; id++) {
    dimensions_[id] = {4, 3, Dir::Horizontal, 8, false};
  }

  // 0x3C: Doubled 2x2 vertical pair with 2-tile horizontal spacing
  dimensions_[0x3C] = {2, 8, Dir::Horizontal, 4, false};

  // 0x3D: Pillar 2x4 spaced 4 - spacing 4 tiles between starts
  dimensions_[0x3D] = {2, 4, Dir::Horizontal, 4, false};

  // 0x3E: Decor 2x2 spaced 12 - spacing 14 tiles
  dimensions_[0x3E] = {2, 2, Dir::Horizontal, 14, false};

  // 0x3F-0x46: Edge 1x1 +2 (width = size + 3)
  for (int id = 0x3F; id <= 0x46; id++) {
    dimensions_[id] = {3, 1, Dir::Horizontal, 1, false};
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
  dimensions_[0x4C] = {4, 3, Dir::Horizontal, 2, false};

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

  // 0x54-0x5A: Mostly unused, but 0x55-0x56 are wall torches (1x8 column)
  for (int id = 0x54; id <= 0x5A; id++) {
    dimensions_[id] = {1, 1, Dir::None, 0, false};
  }
  // 0x55-0x56: Decor 1x8 spaced 12
  for (int id = 0x55; id <= 0x56; id++) {
    dimensions_[id] = {1, 8, Dir::Horizontal, 12, false};
  }

  // 0x5B-0x5C: Cannon Hole 4x3
  for (int id = 0x5B; id <= 0x5C; id++) {
    dimensions_[id] = {4, 3, Dir::Horizontal, 4, false};
  }

  // 0x5D: Big Rail 1x3 +5 (count = size + 6)
  dimensions_[0x5D] = {6, 3, Dir::Horizontal, 1, false};

  // 0x5E: Block 2x2 spaced 2
  dimensions_[0x5E] = {2, 2, Dir::Horizontal, 4, false};

  // 0x5F: Edge 1x1 +23 (width = size + 23)
  dimensions_[0x5F] = {23, 1, Dir::Horizontal, 1, false};

  // 0x60: Downwards 2x2 - GetSize_1to15or32
  dimensions_[0x60] = {2, 0, Dir::Vertical, 2, true};

  // 0x61-0x62: Downwards 4x2 - GetSize_1to15or26
  for (int id = 0x61; id <= 0x62; id++) {
    dimensions_[id] = {4, 0, Dir::Vertical, 2, false};
  }
  for (int id = 0x61; id <= 0x62; id++) {
    dimensions_[id].zero_size_override = 26;
  }

  // 0x63-0x64: Downwards 4x2 - GetSize_1to15or26 (BothBG)
  for (int id = 0x63; id <= 0x64; id++) {
    dimensions_[id] = {4, 0, Dir::Vertical, 2, false};
    dimensions_[id].zero_size_override = 26;
  }
  // 0x65-0x66: Downwards Decor 4x2 spaced 4 (6-tile spacing)
  for (int id = 0x65; id <= 0x66; id++) {
    dimensions_[id] = {4, 2, Dir::Vertical, 6, false};
  }
  // 0x67-0x68: Downwards 2x2 - GetSize_1to16
  for (int id = 0x67; id <= 0x68; id++) {
    dimensions_[id] = {2, 2, Dir::Vertical, 2, false};
  }

  // 0x69: Downwards edge +3 (height = size + 3)
  dimensions_[0x69] = {1, 3, Dir::Vertical, 1, false};

  // 0x6A-0x6B: Downwards edge
  for (int id = 0x6A; id <= 0x6B; id++) {
    dimensions_[id] = {1, 1, Dir::Vertical, 1, false};
  }

  // 0x6C-0x6D: Downwards corners (+12 offset in draw routine, count = size + 10)
  for (int id = 0x6C; id <= 0x6D; id++) {
    dimensions_[id] = {2, 10, Dir::Vertical, 1, false};
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

  // 0x76-0x77: Downwards Decor 3x4 spaced 4 (8-tile spacing)
  for (int id = 0x76; id <= 0x77; id++) {
    dimensions_[id] = {3, 4, Dir::Vertical, 8, false};
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

  // 0x81-0x84: Downwards Decor 3x4 spaced 2 (6-tile spacing)
  for (int id = 0x81; id <= 0x84; id++) {
    dimensions_[id] = {3, 4, Dir::Vertical, 6, false};
  }

  // 0x85-0x86: Downwards Cannon Hole 3x6
  for (int id = 0x85; id <= 0x86; id++) {
    dimensions_[id] = {3, 6, Dir::Vertical, 6, false};
  }

  // 0x87: Downwards Pillar 2x4 spaced 2
  dimensions_[0x87] = {2, 4, Dir::Vertical, 6, false};

  // 0x88: Downwards Big Rail 3x1 +5
  dimensions_[0x88] = {2, 6, Dir::Vertical, 1, false};

  // 0x89: Downwards Block 2x2 spaced 2
  dimensions_[0x89] = {2, 2, Dir::Vertical, 4, false};

  // 0x8A-0x8C: Edge variants (+23)
  for (int id = 0x8A; id <= 0x8C; id++) {
    dimensions_[id] = {1, 23, Dir::Vertical, 1, false};
  }

  // 0x8D-0x8E: Downwards Edge 1x1
  for (int id = 0x8D; id <= 0x8E; id++) {
    dimensions_[id] = {1, 1, Dir::Vertical, 1, false};
  }

  // 0x8F: Downwards Bar 2x3
  dimensions_[0x8F] = {2, 3, Dir::Vertical, 3, false};

  // 0x90-0x91: Downwards 4x2 - GetSize_1to15or26
  for (int id = 0x90; id <= 0x91; id++) {
    dimensions_[id] = {4, 0, Dir::Vertical, 2, false};
    dimensions_[id].zero_size_override = 26;
  }
  // 0x92-0x93: Downwards 2x2 - GetSize_1to15or32
  for (int id = 0x92; id <= 0x93; id++) {
    dimensions_[id] = {2, 0, Dir::Vertical, 2, true};
  }
  // 0x94: Downwards Floor 4x4 - GetSize_1to16
  dimensions_[0x94] = {4, 4, Dir::Vertical, 4, false};

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
  // ASM: These have fixed patterns, count = (size & 0x0F) + 4
  // TopLeft: 0xA0, 0xA5, 0xA9
  for (int id : {0xA0, 0xA5, 0xA9}) {
    dimensions_[id] = {4, 4, Dir::Diagonal, 1, false};
  }
  // BottomLeft: 0xA1, 0xA6, 0xAA
  for (int id : {0xA1, 0xA6, 0xAA}) {
    dimensions_[id] = {4, 4, Dir::Diagonal, 1, false};
  }
  // TopRight: 0xA2, 0xA7, 0xAB
  for (int id : {0xA2, 0xA7, 0xAB}) {
    dimensions_[id] = {4, 4, Dir::Diagonal, 1, false};
  }
  // BottomRight: 0xA3, 0xA8, 0xAC
  for (int id : {0xA3, 0xA8, 0xAC}) {
    dimensions_[id] = {4, 4, Dir::Diagonal, 1, false};
  }
  // BigHole4x4: 0xA4 - extends both directions
  dimensions_[0xA4] = {4, 4, Dir::Both, 1, false};

  // 0xAD-0xAF, 0xBE-0xBF: Nothing
  for (int id : {0xAD, 0xAE, 0xAF, 0xBE, 0xBF}) {
    dimensions_[id] = {1, 1, Dir::None, 0, false};
  }

  // 0xB0-0xB1: Rightwards Edge 1x1 +7 (count = size + 8)
  for (int id = 0xB0; id <= 0xB1; id++) {
    dimensions_[id] = {8, 1, Dir::Horizontal, 1, false};
  }

  // 0xB2: 4x4 block
  dimensions_[0xB2] = {4, 4, Dir::Horizontal, 4, false};

  // 0xB3-0xB4: Edge 1x1 (+2 variant, width = size + 3)
  for (int id = 0xB3; id <= 0xB4; id++) {
    dimensions_[id] = {3, 1, Dir::Horizontal, 1, false};
  }

  // 0xB5: Weird 2x4 (uses 4x2 downwards routine)
  dimensions_[0xB5] = {4, 0, Dir::Vertical, 2, false};
  dimensions_[0xB5].zero_size_override = 26;

  // 0xB6-0xB7: Rightwards 2x4
  for (int id = 0xB6; id <= 0xB7; id++) {
    dimensions_[id] = {0, 4, Dir::Horizontal, 2, false};
  }
  for (int id = 0xB6; id <= 0xB7; id++) {
    dimensions_[id].zero_size_override = 26;
  }

  // 0xB8-0xB9: Rightwards 2x2
  for (int id = 0xB8; id <= 0xB9; id++) {
    dimensions_[id] = {0, 2, Dir::Horizontal, 2, true};
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

  // 0xC3, 0xD7: 3x3 floor in super square (3x3 spacing)
  dimensions_[0xC3] = {0, 0, Dir::SuperSquare, 3, false};
  dimensions_[0xD7] = {0, 0, Dir::SuperSquare, 3, false};

  // 0xC4: 4x4 floor one
  dimensions_[0xC4] = {0, 0, Dir::SuperSquare, 4, false};

  // 0xC5-0xCA: 4x4 floor patterns
  for (int id = 0xC5; id <= 0xCA; id++) {
    dimensions_[id] = {0, 0, Dir::SuperSquare, 4, false};
  }

  // 0xCB-0xCC: Nothing (RoomDraw_Nothing_E)
  dimensions_[0xCB] = {1, 1, Dir::None, 0, false};
  dimensions_[0xCC] = {1, 1, Dir::None, 0, false};
  // 0xCD-0xCE: Moving walls (3 tiles wide, 4 tiles tall base)
  dimensions_[0xCD] = {3, 4, Dir::None, 0, false};
  dimensions_[0xCE] = {3, 4, Dir::None, 0, false};

  // 0xD1-0xD2: 4x4 floor patterns
  dimensions_[0xD1] = {0, 0, Dir::SuperSquare, 4, false};
  dimensions_[0xD2] = {0, 0, Dir::SuperSquare, 4, false};

  // 0xD9: 4x4 floor pattern
  dimensions_[0xD9] = {0, 0, Dir::SuperSquare, 4, false};

  // 0xDB: 4x4 floor two
  dimensions_[0xDB] = {0, 0, Dir::SuperSquare, 4, false};

  // 0xDD: Table rock 4x4 rightwards
  dimensions_[0xDD] = {4, 4, Dir::Horizontal, 4, false};
  // 0xDE: Spike 2x2 tiling
  dimensions_[0xDE] = {0, 0, Dir::SuperSquare, 2, false};

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
  // Layout corners - 4x4 repeated horizontally
  for (int id = 0x100; id <= 0x107; id++) {
    dimensions_[id] = {4, 4, Dir::Horizontal, 4, false};
  }

  // Other 4x4 patterns
  for (int id = 0x108; id <= 0x10F; id++) {
    dimensions_[id] = {4, 4, Dir::None, 0, false};
  }

  // Weird corners - match 3x4 / 4x3 patterns
  for (int id = 0x110; id <= 0x113; id++) {
    dimensions_[id] = {3, 4, Dir::None, 0, false};
  }
  for (int id = 0x114; id <= 0x117; id++) {
    dimensions_[id] = {4, 3, Dir::None, 0, false};
  }
  // 0x118-0x11B: Rightwards 2x2 (repeatable)
  for (int id = 0x118; id <= 0x11B; id++) {
    dimensions_[id] = {2, 2, Dir::Horizontal, 2, false};
  }
  // 0x11C: Rightwards 4x4 (repeatable)
  dimensions_[0x11C] = {4, 4, Dir::Horizontal, 4, false};
  // 0x11D: 2x3 pillar (repeated)
  dimensions_[0x11D] = {2, 3, Dir::Horizontal, 4, false};
  // 0x11E: Single 2x2
  dimensions_[0x11E] = {2, 2, Dir::Horizontal, 2, false};
  // 0x11F-0x120: Star switch / Torch (1x1 +3)
  dimensions_[0x11F] = {4, 1, Dir::Horizontal, 1, false};
  dimensions_[0x120] = {4, 1, Dir::Horizontal, 1, false};
  // 0x121: 2x3 pillar (repeated)
  dimensions_[0x121] = {2, 3, Dir::Horizontal, 4, false};

  // Tables, beds, etc
  dimensions_[0x122] = {4, 5, Dir::None, 0, false};  // Bed
  dimensions_[0x123] = {4, 3, Dir::Horizontal, 8,
                        false};  // Table (8-tile spacing)
  // 0x124-0x125: 4x4
  dimensions_[0x124] = {4, 4, Dir::Horizontal, 4, false};
  dimensions_[0x125] = {4, 4, Dir::Horizontal, 4, false};
  // 0x126: 2x3 pillar (repeated)
  dimensions_[0x126] = {2, 3, Dir::Horizontal, 4, false};
  // 0x127: Rightwards 2x2 (repeatable)
  dimensions_[0x127] = {2, 2, Dir::Horizontal, 2, false};
  dimensions_[0x128] = {4, 5, Dir::None, 0, false};  // Bed variant
  // 0x129: 4x4
  dimensions_[0x129] = {4, 4, Dir::Horizontal, 4, false};
  // 0x12A-0x12B: Rightwards 2x2 (repeatable)
  dimensions_[0x12A] = {2, 2, Dir::Horizontal, 2, false};
  dimensions_[0x12B] = {2, 2, Dir::Horizontal, 2, false};
  // 0x134: Rightwards 2x2 (repeatable)
  dimensions_[0x134] = {2, 2, Dir::Horizontal, 2, false};
  dimensions_[0x12C] = {3, 6, Dir::None, 0, false};  // 3x6 pattern
  // 0x12D-0x133: Inter-room fat stairs + auto stairs (fixed 4x4)
  for (int id = 0x12D; id <= 0x133; id++) {
    dimensions_[id] = {4, 4, Dir::None, 0, false};
  }
  // 0x135-0x137: Water hop stairs / flood gate (repeatable 4x4)
  for (int id = 0x135; id <= 0x137; id++) {
    dimensions_[id] = {4, 4, Dir::Horizontal, 4, false};
  }
  // 0x138-0x13B: Spiral stairs (fixed 4x3)
  for (int id = 0x138; id <= 0x13B; id++) {
    dimensions_[id] = {4, 3, Dir::None, 0, false};
  }
  // 0x13C: Sanctuary wall (repeatable 4x4)
  dimensions_[0x13C] = {4, 4, Dir::Horizontal, 4, false};
  // 0x13D: Table 4x3 (repeatable with 8-tile spacing)
  dimensions_[0x13D] = {4, 3, Dir::Horizontal, 8, false};
  dimensions_[0x13E] = {6, 3, Dir::None, 0, false};  // Utility 6x3
  // 0x13F: Magic Bat Altar (repeatable 4x4)
  dimensions_[0x13F] = {4, 4, Dir::Horizontal, 4, false};

  // ============================================================================
  // Subtype 3 objects (0xF80-0xFFF)
  // ============================================================================
  // Default for Type 3: most are 2x2 fixed objects
  for (int id = 0xF80; id <= 0xFFF; id++) {
    dimensions_[id] = {2, 2, Dir::None, 0, false};
  }

  // Override specific Type 3 objects with known sizes
  // Prison cell bars (10x4 tiles)
  dimensions_[0xF8D] = {10, 4, Dir::None, 0, false};
  dimensions_[0xF97] = {10, 4, Dir::None, 0, false};
  // Rupee floor pattern
  dimensions_[0xF92] = {6, 8, Dir::None, 0, false};
  // Table/rock 4x3 repeated with 8-tile spacing
  dimensions_[0xF94] = {4, 3, Dir::Horizontal, 8, false};
  // Single hammer peg (1x1 +3)
  dimensions_[0xF96] = {4, 1, Dir::Horizontal, 1, false};
  // Boss shells (repeatable 4x4)
  dimensions_[0xF95] = {4, 4, Dir::Horizontal, 4, false};
  dimensions_[0xFF2] = {4, 4, Dir::Horizontal, 4, false};
  dimensions_[0xFFB] = {4, 4, Dir::Horizontal, 4, false};
  // Auto/straight stairs (fixed 4x4)
  for (int id = 0xF9B; id <= 0xFA1; id++) {
    dimensions_[id] = {4, 4, Dir::None, 0, false};
  }
  for (int id = 0xFA6; id <= 0xFA9; id++) {
    dimensions_[id] = {4, 4, Dir::None, 0, false};
  }
  dimensions_[0xFB3] = {4, 4, Dir::None, 0, false};
  // Repeatable 4x4 patterns
  for (int id = 0xFB4; id <= 0xFB9; id++) {
    dimensions_[id] = {4, 4, Dir::Horizontal, 4, false};
  }
  dimensions_[0xFAA] = {4, 4, Dir::Horizontal, 4, false};
  dimensions_[0xFAD] = {4, 4, Dir::Horizontal, 4, false};
  dimensions_[0xFAE] = {4, 4, Dir::Horizontal, 4, false};
  dimensions_[0xFCB] = {4, 4, Dir::Horizontal, 4, false};
  dimensions_[0xFCC] = {4, 4, Dir::Horizontal, 4, false};
  dimensions_[0xFD4] = {4, 4, Dir::Horizontal, 4, false};
  dimensions_[0xFE2] = {4, 4, Dir::Horizontal, 4, false};
  dimensions_[0xFF4] = {4, 4, Dir::Horizontal, 4, false};
  dimensions_[0xFF6] = {4, 4, Dir::Horizontal, 4, false};
  dimensions_[0xFF7] = {4, 4, Dir::Horizontal, 4, false};
  // Utility + archery patterns
  dimensions_[0xFCD] = {6, 3, Dir::None, 0, false};
  dimensions_[0xFDD] = {6, 3, Dir::None, 0, false};
  dimensions_[0xFD5] = {3, 5, Dir::None, 0, false};
  dimensions_[0xFDB] = {3, 5, Dir::None, 0, false};
  dimensions_[0xFE0] = {3, 6, Dir::None, 0, false};
  dimensions_[0xFE1] = {3, 6, Dir::None, 0, false};
  // Solid wall decor 3x4
  dimensions_[0xFE9] = {3, 4, Dir::None, 0, false};
  dimensions_[0xFEA] = {3, 4, Dir::None, 0, false};
  dimensions_[0xFEE] = {3, 4, Dir::None, 0, false};
  dimensions_[0xFEF] = {3, 4, Dir::None, 0, false};
  // Light beams + Triforce floor
  dimensions_[0xFF0] = {4, 4, Dir::None, 0, false};
  dimensions_[0xFF1] = {6, 6, Dir::None, 0, false};
  dimensions_[0xFF8] = {8, 8, Dir::None, 0, false};
  // Table rock 4x3 (repeatable with 8-tile spacing)
  dimensions_[0xFF9] = {4, 3, Dir::Horizontal, 8, false};
  // Rightwards 4x4 repeated
  dimensions_[0xFC8] = {4, 4, Dir::Horizontal, 4, false};
  // Table/rock 4x3 repeated with 8-tile spacing
  dimensions_[0xFCE] = {4, 3, Dir::Horizontal, 8, false};
  // Actual 4x4 (no repetition)
  dimensions_[0xFE6] = {4, 4, Dir::None, 0, false};
  dimensions_[0xFE7] = {4, 3, Dir::Horizontal, 8, false};
  dimensions_[0xFE8] = {4, 3, Dir::Horizontal, 8, false};
  // Single 4x4 tile8 (large decor)
  dimensions_[0xFEB] = {4, 4, Dir::None, 0, false};
  // Single 4x3
  dimensions_[0xFEC] = {4, 3, Dir::None, 0, false};
  dimensions_[0xFED] = {4, 3, Dir::None, 0, false};
  // Turtle Rock pipes
  dimensions_[0xFBA] = {2, 6, Dir::None, 0, false};
  dimensions_[0xFBB] = {2, 6, Dir::None, 0, false};
  dimensions_[0xFBC] = {6, 2, Dir::None, 0, false};
  dimensions_[0xFBD] = {6, 2, Dir::None, 0, false};
  dimensions_[0xFDC] = {6, 2, Dir::None, 0, false};
  // Rightwards 4x4 repeated
  dimensions_[0xFFA] = {4, 4, Dir::Horizontal, 4, false};
  // 0xFB1-0xFB2: Big Chest 4x3
  dimensions_[0xFB1] = {4, 3, Dir::None, 0, false};
  dimensions_[0xFB2] = {4, 3, Dir::None, 0, false};
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
    if (!offset_result.ok())
      continue;

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
