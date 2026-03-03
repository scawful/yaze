#include "zelda3/overworld/tile16_stamp.h"

#include <algorithm>

#include "absl/status/status.h"
#include "zelda3/overworld/tile16_metadata.h"

namespace yaze::zelda3 {
namespace {

uint16_t ClampTile8Id(int tile8_id, int max_tile8_id) {
  return static_cast<uint16_t>(std::clamp(tile8_id, 0, max_tile8_id));
}

gfx::TileInfo MakeStampedTileInfo(const Tile16StampRequest& request,
                                  int tile8_id) {
  return gfx::TileInfo(ClampTile8Id(tile8_id, request.max_tile8_id),
                       request.palette_id, request.y_flip, request.x_flip,
                       request.priority);
}

gfx::Tile16 Make2xStampTile(const Tile16StampRequest& request, int base_tile8,
                            bool reorder_for_flip_layout) {
  const gfx::TileInfo t0 = MakeStampedTileInfo(request, base_tile8);
  const gfx::TileInfo t1 = MakeStampedTileInfo(request, base_tile8 + 1);
  const gfx::TileInfo t2 =
      MakeStampedTileInfo(request, base_tile8 + request.tile8_row_stride);
  const gfx::TileInfo t3 =
      MakeStampedTileInfo(request, base_tile8 + request.tile8_row_stride + 1);

  gfx::Tile16 stamped_tile{};
  if (reorder_for_flip_layout && request.x_flip && request.y_flip) {
    stamped_tile = gfx::Tile16(t3, t2, t1, t0);
  } else if (reorder_for_flip_layout && request.x_flip) {
    stamped_tile = gfx::Tile16(t1, t0, t3, t2);
  } else if (reorder_for_flip_layout && request.y_flip) {
    stamped_tile = gfx::Tile16(t2, t3, t0, t1);
  } else {
    stamped_tile = gfx::Tile16(t0, t1, t2, t3);
  }
  SyncTile16TilesInfo(&stamped_tile);
  return stamped_tile;
}

}  // namespace

absl::StatusOr<std::vector<Tile16StampMutation>> BuildTile16StampMutations(
    const Tile16StampRequest& request) {
  if (request.tile8_row_stride <= 0) {
    return absl::InvalidArgumentError("tile8_row_stride must be positive");
  }
  if (request.tile16_row_stride <= 0) {
    return absl::InvalidArgumentError("tile16_row_stride must be positive");
  }
  if (request.max_tile8_id < 0) {
    return absl::InvalidArgumentError("max_tile8_id must be non-negative");
  }
  if (request.max_tile16_id < 0) {
    return absl::InvalidArgumentError("max_tile16_id must be non-negative");
  }
  if (request.current_tile16_id < 0 ||
      request.current_tile16_id > request.max_tile16_id) {
    return absl::OutOfRangeError("current_tile16_id out of range");
  }

  std::vector<Tile16StampMutation> mutations;
  const bool is_1x_stamp = request.stamp_size == 1;
  const bool is_2x_stamp = request.stamp_size == 2;

  if (is_1x_stamp) {
    gfx::Tile16 staged_current = request.current_tile16;
    MutableTile16QuadrantInfo(staged_current, request.quadrant_index) =
        MakeStampedTileInfo(request, request.selected_tile8_id);
    SyncTile16TilesInfo(&staged_current);
    mutations.push_back(
        Tile16StampMutation{request.current_tile16_id, staged_current});
    return mutations;
  }

  if (is_2x_stamp) {
    mutations.push_back(Tile16StampMutation{
        request.current_tile16_id,
        Make2xStampTile(request, request.selected_tile8_id, true)});
    return mutations;
  }

  // ZScream parity: 4x paints a 2x2 tile16 patch from a 4x4 tile8 region.
  for (int patch_x = 0; patch_x < 2; ++patch_x) {
    for (int patch_y = 0; patch_y < 2; ++patch_y) {
      const int target_tile16 = request.current_tile16_id +
                                (patch_x + patch_y * request.tile16_row_stride);
      if (target_tile16 < 0 || target_tile16 > request.max_tile16_id) {
        continue;
      }

      const int base_tile8 = request.selected_tile8_id + (patch_x * 2) +
                             (patch_y * request.tile8_row_stride * 2);
      mutations.push_back(Tile16StampMutation{
          target_tile16, Make2xStampTile(request, base_tile8, false)});
    }
  }

  return mutations;
}

}  // namespace yaze::zelda3
