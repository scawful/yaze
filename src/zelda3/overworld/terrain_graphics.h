#ifndef YAZE_ZELDA3_OVERWORLD_TERRAIN_GRAPHICS_H_
#define YAZE_ZELDA3_OVERWORLD_TERRAIN_GRAPHICS_H_

#include <array>
#include <cstdint>
#include <vector>

#include "absl/status/statusor.h"

namespace yaze::zelda3 {

// Read decoded source pixels before ProcessGraphicsBuffer's palette remapping.
// A preview index of 8 alone cannot distinguish source zero from opaque color 8.
inline absl::StatusOr<std::array<uint8_t, 64>> Tile8SourceTransparency(
    const std::vector<uint8_t>& decoded, int sheet, int tile_id, bool flip_x,
    bool flip_y) {
  if (sheet <= 0 || tile_id < 0 || tile_id >= 1024 ||
      static_cast<size_t>(sheet + 1) * 4096 > decoded.size()) {
    return absl::FailedPreconditionError("Tile8 source graphics unavailable");
  }
  std::array<uint8_t, 64> mask{};
  const int local_tile = tile_id % 64;
  for (int y = 0; y < 8; ++y) {
    for (int x = 0; x < 8; ++x) {
      const int sx = flip_x ? 7 - x : x;
      const int sy = flip_y ? 7 - y : y;
      const size_t source = sheet * 4096 + (local_tile / 16) * 1024 +
                            (local_tile % 16) * 8 + sy * 128 + sx;
      if (decoded[source] > 15) {
        return absl::DataLossError("Tile8 source contains undecoded pixels");
      }
      mask[y * 8 + x] = decoded[source] == 0;
    }
  }
  return mask;
}

}  // namespace yaze::zelda3
#endif
