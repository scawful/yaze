#include "zelda3/overworld/tile16_usage_index.h"

#include <algorithm>

namespace yaze::zelda3 {

namespace {

const gfx::TileInfo& TileInfoForQuadrant(const gfx::Tile16& tile,
                                         int quadrant) {
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

template <typename TileAt>
std::vector<Tile16PlacementRun> CollectPlacementRuns(const TileAt& tile_at,
                                                     int tile16_id) {
  constexpr int kTilesPerMapSide = 32;
  std::vector<Tile16PlacementRun> runs;
  if (tile16_id < 0) {
    return runs;
  }
  for (int y = 0; y < kTilesPerMapSide; ++y) {
    int run_start = -1;
    for (int x = 0; x <= kTilesPerMapSide; ++x) {
      const bool match = x < kTilesPerMapSide && tile_at(x, y) == tile16_id;
      if (match && run_start < 0) {
        run_start = x;
      } else if (!match && run_start >= 0) {
        runs.push_back({run_start, y, x - run_start});
        run_start = -1;
      }
    }
  }
  return runs;
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

void AccumulateTile16MapUsage(
    const std::vector<std::vector<uint16_t>>& world_tiles, int local_map,
    std::vector<int>* counts) {
  constexpr int kMapsPerRow = 8;
  constexpr int kTilesPerMapSide = 32;
  if (!counts || local_map < 0) {
    return;
  }
  const int x0 = (local_map % kMapsPerRow) * kTilesPerMapSide;
  const int y0 = (local_map / kMapsPerRow) * kTilesPerMapSide;
  const int x_end =
      std::min(x0 + kTilesPerMapSide, static_cast<int>(world_tiles.size()));
  for (int x = x0; x < x_end; ++x) {
    const auto& column = world_tiles[x];
    const int y_end =
        std::min(y0 + kTilesPerMapSide, static_cast<int>(column.size()));
    for (int y = y0; y < y_end; ++y) {
      const uint16_t tile_id = column[y];
      if (tile_id < counts->size()) {
        ++(*counts)[tile_id];
      }
    }
  }
}

std::vector<Tile16PlacementRun> FindTile16MapPlacements(
    const std::vector<std::vector<uint16_t>>& world_tiles, int local_map,
    int tile16_id) {
  constexpr int kMapsPerRow = 8;
  constexpr int kTilesPerMapSide = 32;
  if (local_map < 0) {
    return {};
  }
  const int x0 = (local_map % kMapsPerRow) * kTilesPerMapSide;
  const int y0 = (local_map / kMapsPerRow) * kTilesPerMapSide;
  return CollectPlacementRuns(
      [&](int x, int y) -> int {
        const int wx = x0 + x;
        const int wy = y0 + y;
        if (wx >= static_cast<int>(world_tiles.size()) ||
            wy >= static_cast<int>(world_tiles[wx].size())) {
          return -1;
        }
        return world_tiles[wx][wy];
      },
      tile16_id);
}

std::vector<Tile16PlacementRun> FindTile16MapPlacements(
    const std::array<uint16_t, 32 * 32>& map_tiles, int tile16_id) {
  return CollectPlacementRuns(
      [&](int x, int y) -> int { return map_tiles[y * 32 + x]; }, tile16_id);
}

std::vector<int> BuildTile16DisplayOrder(const std::vector<int>& counts,
                                         Tile16SortMode sort_mode,
                                         bool hide_unused) {
  std::vector<int> order;
  order.reserve(counts.size());
  for (int tile_id = 0; tile_id < static_cast<int>(counts.size()); ++tile_id) {
    if (!hide_unused || counts[tile_id] > 0) {
      order.push_back(tile_id);
    }
  }
  if (sort_mode == Tile16SortMode::kUsage) {
    std::stable_sort(order.begin(), order.end(),
                     [&counts](int a, int b) { return counts[a] > counts[b]; });
  }
  return order;
}

}  // namespace yaze::zelda3
