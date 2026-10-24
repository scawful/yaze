#ifndef YAZE_ZELDA3_OVERWORLD_TILE16_USAGE_INDEX_H
#define YAZE_ZELDA3_OVERWORLD_TILE16_USAGE_INDEX_H

#include <array>
#include <cstdint>
#include <functional>
#include <vector>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "app/gfx/types/snes_tile.h"

namespace yaze::zelda3 {

struct Tile8UsageHit {
  int tile16_id = 0;
  int quadrant = 0;
};

constexpr int kMaxTile8UsageId = 1024;
using Tile8UsageIndex =
    std::array<std::vector<Tile8UsageHit>, kMaxTile8UsageId>;

void ClearTile8UsageIndex(Tile8UsageIndex* usage_index);

void AddTile16ToUsageIndex(const gfx::Tile16& tile_data, int tile16_id,
                           Tile8UsageIndex* usage_index);

absl::Status BuildTile8UsageIndex(
    int total_tiles,
    const std::function<absl::StatusOr<gfx::Tile16>(int)>& tile_provider,
    Tile8UsageIndex* usage_index);

// Adds one overworld map's tile16 placements to `counts` (indexed by tile16
// ID). `world_tiles` is a world blockset indexed [x][y]; `local_map` is the
// map's index within its world (map_id % 0x40, 8 maps per row, 32x32 tiles
// each). IDs outside `counts` are ignored.
void AccumulateTile16MapUsage(
    const std::vector<std::vector<uint16_t>>& world_tiles, int local_map,
    std::vector<int>* counts);

enum class Tile16SortMode { kUsage, kTileId };

// Tile16 IDs in display order. kUsage puts the most placed tiles first (ties
// by ID); kTileId keeps ID order. `hide_unused` drops tiles with no placements.
std::vector<int> BuildTile16DisplayOrder(const std::vector<int>& counts,
                                         Tile16SortMode sort_mode,
                                         bool hide_unused);

}  // namespace yaze::zelda3

#endif  // YAZE_ZELDA3_OVERWORLD_TILE16_USAGE_INDEX_H
