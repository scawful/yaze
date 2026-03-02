#include "zelda3/overworld/tile16_usage_index.h"

namespace yaze::zelda3 {

namespace {

const gfx::TileInfo& TileInfoForQuadrant(const gfx::Tile16& tile, int quadrant) {
  switch (quadrant) {
    case 0:
      return tile.tile0_;
    case 1:
      return tile.tile1_;
    case 2:
      return tile.tile2_;
    default:
      return tile.tile3_;
  }
}

}  // namespace

void ClearTile8UsageIndex(Tile8UsageIndex* usage_index) {
  if (!usage_index) {
    return;
  }
  for (auto& hits : *usage_index) {
    hits.clear();
  }
}

void AddTile16ToUsageIndex(const gfx::Tile16& tile_data, int tile16_id,
                           Tile8UsageIndex* usage_index) {
  if (!usage_index) {
    return;
  }

  for (int quadrant = 0; quadrant < 4; ++quadrant) {
    const int tile8_id = TileInfoForQuadrant(tile_data, quadrant).id_;
    if (tile8_id < 0 || tile8_id >= kMaxTile8UsageId) {
      continue;
    }
    (*usage_index)[tile8_id].push_back({tile16_id, quadrant});
  }
}

absl::Status BuildTile8UsageIndex(
    int total_tiles,
    const std::function<absl::StatusOr<gfx::Tile16>(int)>& tile_provider,
    Tile8UsageIndex* usage_index) {
  if (!usage_index) {
    return absl::InvalidArgumentError("Usage index pointer is null");
  }
  if (!tile_provider) {
    return absl::InvalidArgumentError("Tile provider callback is not set");
  }
  if (total_tiles < 0) {
    return absl::InvalidArgumentError("Total tiles cannot be negative");
  }

  ClearTile8UsageIndex(usage_index);
  for (int tile_id = 0; tile_id < total_tiles; ++tile_id) {
    auto tile_result = tile_provider(tile_id);
    if (!tile_result.ok()) {
      continue;
    }
    AddTile16ToUsageIndex(*tile_result, tile_id, usage_index);
  }
  return absl::OkStatus();
}

}  // namespace yaze::zelda3
