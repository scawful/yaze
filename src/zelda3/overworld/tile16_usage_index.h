#ifndef YAZE_ZELDA3_OVERWORLD_TILE16_USAGE_INDEX_H
#define YAZE_ZELDA3_OVERWORLD_TILE16_USAGE_INDEX_H

#include <array>
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
using Tile8UsageIndex = std::array<std::vector<Tile8UsageHit>, kMaxTile8UsageId>;

void ClearTile8UsageIndex(Tile8UsageIndex* usage_index);

void AddTile16ToUsageIndex(const gfx::Tile16& tile_data, int tile16_id,
                           Tile8UsageIndex* usage_index);

absl::Status BuildTile8UsageIndex(
    int total_tiles,
    const std::function<absl::StatusOr<gfx::Tile16>(int)>& tile_provider,
    Tile8UsageIndex* usage_index);

}  // namespace yaze::zelda3

#endif  // YAZE_ZELDA3_OVERWORLD_TILE16_USAGE_INDEX_H
