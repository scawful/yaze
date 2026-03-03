#include "zelda3/overworld/tile16_renderer.h"

#include <algorithm>

#include "zelda3/overworld/tile16_metadata.h"

namespace yaze::zelda3 {

namespace {

constexpr int kTile8Size = 8;
constexpr int kTile16Size = 16;
constexpr int kTile16PixelCount = 256;

}  // namespace

std::vector<uint8_t> RenderTile16PixelsFromMetadata(
    const gfx::Tile16& tile_data,
    const std::vector<gfx::Bitmap>& tile8_bitmaps) {
  std::vector<uint8_t> tile16_pixels(kTile16PixelCount, 0);

  for (int quadrant = 0; quadrant < 4; ++quadrant) {
    const gfx::TileInfo& tile_info = Tile16QuadrantInfo(tile_data, quadrant);
    const int quadrant_x = quadrant % 2;
    const int quadrant_y = quadrant / 2;
    const int tile8_id = tile_info.id_;

    if (tile8_id < 0 || tile8_id >= static_cast<int>(tile8_bitmaps.size())) {
      continue;
    }

    const gfx::Bitmap& source_tile8 = tile8_bitmaps[tile8_id];
    if (!source_tile8.is_active()) {
      continue;
    }

    const bool x_flip = tile_info.horizontal_mirror_;
    const bool y_flip = tile_info.vertical_mirror_;
    const uint8_t palette_index =
        static_cast<uint8_t>(tile_info.palette_ & 0x07);

    for (int ty = 0; ty < kTile8Size; ++ty) {
      for (int tx = 0; tx < kTile8Size; ++tx) {
        const int src_x = x_flip ? (kTile8Size - 1 - tx) : tx;
        const int src_y = y_flip ? (kTile8Size - 1 - ty) : ty;
        const int src_index = src_y * kTile8Size + src_x;

        const int dst_x = (quadrant_x * kTile8Size) + tx;
        const int dst_y = (quadrant_y * kTile8Size) + ty;
        const int dst_index = dst_y * kTile16Size + dst_x;

        if (src_index < 0 ||
            src_index >= static_cast<int>(source_tile8.size()) ||
            dst_index < 0 || dst_index >= kTile16PixelCount) {
          continue;
        }

        const uint8_t pixel = source_tile8.data()[src_index];
        tile16_pixels[dst_index] = (pixel & 0x0F) + (palette_index * 0x10);
      }
    }
  }

  return tile16_pixels;
}

absl::Status RenderTile16BitmapFromMetadata(
    const gfx::Tile16& tile_data, const std::vector<gfx::Bitmap>& tile8_bitmaps,
    gfx::Bitmap* output_bitmap) {
  if (!output_bitmap) {
    return absl::InvalidArgumentError("Output bitmap pointer is null");
  }
  if (tile8_bitmaps.empty()) {
    return absl::FailedPreconditionError("Tile8 bitmap source is empty");
  }

  const auto pixels = RenderTile16PixelsFromMetadata(tile_data, tile8_bitmaps);
  output_bitmap->Create(kTile16Size, kTile16Size, 8, pixels);
  return absl::OkStatus();
}

}  // namespace yaze::zelda3
