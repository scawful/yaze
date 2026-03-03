#ifndef YAZE_ZELDA3_OVERWORLD_TILE16_STAMP_H
#define YAZE_ZELDA3_OVERWORLD_TILE16_STAMP_H

#include <cstdint>
#include <vector>

#include "absl/status/statusor.h"
#include "app/gfx/types/snes_tile.h"

namespace yaze::zelda3 {

struct Tile16StampMutation {
  int tile16_id = 0;
  gfx::Tile16 tile_data;
};

struct Tile16StampRequest {
  gfx::Tile16 current_tile16;
  int current_tile16_id = 0;
  int selected_tile8_id = 0;
  int stamp_size = 1;  // 1x, 2x, or 4x (non-1/2 values use 4x behavior)
  int quadrant_index = 0;

  uint8_t palette_id = 0;
  bool x_flip = false;
  bool y_flip = false;
  bool priority = false;

  int tile8_row_stride = 16;
  int tile16_row_stride = 8;
  int max_tile8_id = 1023;
  int max_tile16_id = 0x0FFF;
};

absl::StatusOr<std::vector<Tile16StampMutation>> BuildTile16StampMutations(
    const Tile16StampRequest& request);

}  // namespace yaze::zelda3

#endif  // YAZE_ZELDA3_OVERWORLD_TILE16_STAMP_H
