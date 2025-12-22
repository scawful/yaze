#include "zelda3/overworld/diggable_tiles.h"

#include <cstring>

namespace yaze::zelda3 {

bool DiggableTiles::IsDiggable(uint16_t tile_id) const {
  if (tile_id >= kMaxDiggableTileId) {
    return false;
  }
  int byte_index = tile_id / 8;
  int bit_index = tile_id % 8;
  return (bitfield_[byte_index] & (1 << bit_index)) != 0;
}

void DiggableTiles::SetDiggable(uint16_t tile_id, bool diggable) {
  if (tile_id >= kMaxDiggableTileId) {
    return;
  }
  int byte_index = tile_id / 8;
  int bit_index = tile_id % 8;
  if (diggable) {
    bitfield_[byte_index] |= (1 << bit_index);
  } else {
    bitfield_[byte_index] &= ~(1 << bit_index);
  }
}

void DiggableTiles::Clear() {
  bitfield_.fill(0);
}

void DiggableTiles::SetVanillaDefaults() {
  Clear();
  for (int i = 0; i < kNumVanillaDiggableTiles; ++i) {
    SetDiggable(kVanillaDiggableTiles[i], true);
  }
}

std::vector<uint16_t> DiggableTiles::GetAllDiggableTileIds() const {
  std::vector<uint16_t> result;
  for (uint16_t tile_id = 0; tile_id < kMaxDiggableTileId; ++tile_id) {
    if (IsDiggable(tile_id)) {
      result.push_back(tile_id);
    }
  }
  return result;
}

int DiggableTiles::GetDiggableCount() const {
  int count = 0;
  for (uint16_t tile_id = 0; tile_id < kMaxDiggableTileId; ++tile_id) {
    if (IsDiggable(tile_id)) {
      ++count;
    }
  }
  return count;
}

void DiggableTiles::FromBytes(const uint8_t* data) {
  std::memcpy(bitfield_.data(), data, kDiggableTilesBitfieldSize);
}

void DiggableTiles::ToBytes(uint8_t* data) const {
  std::memcpy(data, bitfield_.data(), kDiggableTilesBitfieldSize);
}

bool DiggableTiles::IsTile16Diggable(
    const gfx::Tile16& tile16,
    const std::array<uint8_t, 0x200>& all_tiles_types) {
  // Check all 4 component tiles
  // A Tile16 is diggable only if ALL components are diggable types
  auto is_tile_diggable = [&all_tiles_types](const gfx::TileInfo& tile_info) {
    uint16_t tile_id = tile_info.id_;
    if (tile_id >= all_tiles_types.size()) {
      return false;
    }
    uint8_t tile_type = all_tiles_types[tile_id];
    return tile_type == kTileTypeDiggable1 || tile_type == kTileTypeDiggable2;
  };

  return is_tile_diggable(tile16.tile0_) &&
         is_tile_diggable(tile16.tile1_) &&
         is_tile_diggable(tile16.tile2_) &&
         is_tile_diggable(tile16.tile3_);
}

}  // namespace yaze::zelda3
