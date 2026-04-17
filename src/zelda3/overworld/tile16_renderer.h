#ifndef YAZE_ZELDA3_OVERWORLD_TILE16_RENDERER_H
#define YAZE_ZELDA3_OVERWORLD_TILE16_RENDERER_H

#include <array>
#include <cstdint>
#include <vector>

#include "absl/status/status.h"
#include "app/gfx/core/bitmap.h"
#include "app/gfx/render/tilemap.h"
#include "app/gfx/types/snes_tile.h"

namespace yaze::zelda3 {

using Tile8PixelData = std::array<uint8_t, 64>;

// Compose a tile16 pixel buffer from ALTTP TileInfo metadata.
//
// Rules:
// - Each quadrant resolves tile8 ID + h/v flip from TileInfo.
// - Pixel index is low nibble from source tile8.
// - Palette row is encoded per quadrant: (pixel & 0x0F) + (palette * 0x10).
std::vector<uint8_t> RenderTile16PixelsFromMetadata(
    const gfx::Tile16& tile_data,
    const std::vector<Tile8PixelData>& tile8_pixels);
std::vector<uint8_t> RenderTile16PixelsFromMetadata(
    const gfx::Tile16& tile_data,
    const std::vector<gfx::Bitmap>& tile8_bitmaps);

// Compose a tile16 bitmap (16x16, 8bpp) from metadata using the same transform
// as RenderTile16PixelsFromMetadata.
absl::Status RenderTile16BitmapFromMetadata(
    const gfx::Tile16& tile_data,
    const std::vector<Tile8PixelData>& tile8_pixels,
    gfx::Bitmap* output_bitmap);
absl::Status RenderTile16BitmapFromMetadata(
    const gfx::Tile16& tile_data, const std::vector<gfx::Bitmap>& tile8_bitmaps,
    gfx::Bitmap* output_bitmap);

// Compute tile count from an active tile16 atlas using 16x16 tiles.
// Falls back to kNumTile16Individual when atlas metadata is unavailable.
int ComputeTile16Count(const gfx::Tilemap* tile16_blockset);

// Blit a 16x16 tile bitmap into a destination atlas at tile_id location.
void BlitTile16BitmapToAtlas(gfx::Bitmap* destination, int tile_id,
                             const gfx::Bitmap& source_bitmap);

}  // namespace yaze::zelda3

#endif  // YAZE_ZELDA3_OVERWORLD_TILE16_RENDERER_H
