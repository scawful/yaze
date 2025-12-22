#include "app/editor/sprite/sprite_drawer.h"

#include <algorithm>

namespace yaze {
namespace editor {

SpriteDrawer::SpriteDrawer(const uint8_t* sprite_gfx_buffer)
    : sprite_gfx_(sprite_gfx_buffer) {}

void SpriteDrawer::ClearBitmap(gfx::Bitmap& bitmap) {
  if (!bitmap.is_active()) return;

  auto& data = bitmap.mutable_data();
  std::fill(data.begin(), data.end(), 0);
}

void SpriteDrawer::DrawOamTile(gfx::Bitmap& bitmap,
                               const zsprite::OamTile& tile, int origin_x,
                               int origin_y) {
  if (!sprite_gfx_) return;
  if (!bitmap.is_active()) return;

  // OAM tile positions are signed 8-bit values relative to sprite origin
  // In ZSM, x and y are uint8_t but represent signed positions
  int8_t signed_x = static_cast<int8_t>(tile.x);
  int8_t signed_y = static_cast<int8_t>(tile.y);

  int dest_x = origin_x + signed_x;
  int dest_y = origin_y + signed_y;

  if (tile.size) {
    // 16x16 mode
    DrawTile16x16(bitmap, tile.id, dest_x, dest_y, tile.mirror_x, tile.mirror_y,
                  tile.palette);
  } else {
    // 8x8 mode
    DrawTile8x8(bitmap, tile.id, dest_x, dest_y, tile.mirror_x, tile.mirror_y,
                tile.palette);
  }
}

void SpriteDrawer::DrawFrame(gfx::Bitmap& bitmap, const zsprite::Frame& frame,
                             int origin_x, int origin_y) {
  // Draw tiles in reverse order (first in list = top priority)
  // This ensures proper layering with later tiles drawn on top
  for (auto it = frame.Tiles.rbegin(); it != frame.Tiles.rend(); ++it) {
    DrawOamTile(bitmap, *it, origin_x, origin_y);
  }
}

uint8_t SpriteDrawer::GetTilePixel(uint16_t tile_id, int px, int py) const {
  if (!sprite_gfx_) return 0;
  if (tile_id > kMaxTileId) return 0;
  if (px < 0 || px >= kTileSize || py < 0 || py >= kTileSize) return 0;

  // Calculate position in 8BPP linear buffer
  // Layout: 16 tiles per row, each tile 8x8 pixels
  // Row stride: 128 bytes (16 tiles * 8 bytes)
  int tile_col = tile_id % kTilesPerRow;
  int tile_row = tile_id / kTilesPerRow;

  int base_x = tile_col * kTileSize;
  int base_y = tile_row * kTileRowSize;  // 1024 bytes per tile row

  int src_index = base_y + (py * kRowStride) + base_x + px;

  // Bounds check against typical buffer size (0x10000)
  if (src_index >= 0x10000) return 0;

  return sprite_gfx_[src_index];
}

void SpriteDrawer::DrawTile8x8(gfx::Bitmap& bitmap, uint16_t tile_id, int x,
                               int y, bool flip_x, bool flip_y,
                               uint8_t palette) {
  if (!sprite_gfx_) return;
  if (tile_id > kMaxTileId) return;

  // Sprite palettes use 16 colors each (including transparent)
  // Palette index 0-7 map to colors 0-127 in the combined palette
  uint8_t palette_offset = (palette & 0x07) * 16;

  for (int py = 0; py < kTileSize; py++) {
    int src_py = flip_y ? (kTileSize - 1 - py) : py;

    for (int px = 0; px < kTileSize; px++) {
      int src_px = flip_x ? (kTileSize - 1 - px) : px;

      uint8_t pixel = GetTilePixel(tile_id, src_px, src_py);

      // Pixel 0 is transparent
      if (pixel != 0) {
        int dest_x = x + px;
        int dest_y = y + py;

        if (dest_x >= 0 && dest_x < bitmap.width() && dest_y >= 0 &&
            dest_y < bitmap.height()) {
          int dest_index = dest_y * bitmap.width() + dest_x;
          if (dest_index >= 0 &&
              dest_index < static_cast<int>(bitmap.mutable_data().size())) {
            // Map pixel to palette: pixel value + palette offset
            // Pixel 1 -> palette_offset, pixel 2 -> palette_offset+1, etc.
            bitmap.mutable_data()[dest_index] = pixel + palette_offset;
          }
        }
      }
    }
  }
}

void SpriteDrawer::DrawTile16x16(gfx::Bitmap& bitmap, uint16_t tile_id, int x,
                                 int y, bool flip_x, bool flip_y,
                                 uint8_t palette) {
  // 16x16 tile is composed of 4 8x8 tiles in a 2x2 grid:
  //   [base + 0]  [base + 1]
  //   [base + 16] [base + 17]
  //
  // When mirrored horizontally:
  //   [base + 1]  [base + 0]
  //   [base + 17] [base + 16]
  //
  // When mirrored vertically:
  //   [base + 16] [base + 17]
  //   [base + 0]  [base + 1]
  //
  // When mirrored both:
  //   [base + 17] [base + 16]
  //   [base + 1]  [base + 0]

  uint16_t tl = tile_id;          // Top-left
  uint16_t tr = tile_id + 1;      // Top-right
  uint16_t bl = tile_id + 16;     // Bottom-left
  uint16_t br = tile_id + 17;     // Bottom-right

  // Swap tiles based on mirroring
  if (flip_x) {
    std::swap(tl, tr);
    std::swap(bl, br);
  }
  if (flip_y) {
    std::swap(tl, bl);
    std::swap(tr, br);
  }

  // Draw the 4 component tiles
  DrawTile8x8(bitmap, tl, x, y, flip_x, flip_y, palette);
  DrawTile8x8(bitmap, tr, x + 8, y, flip_x, flip_y, palette);
  DrawTile8x8(bitmap, bl, x, y + 8, flip_x, flip_y, palette);
  DrawTile8x8(bitmap, br, x + 8, y + 8, flip_x, flip_y, palette);
}

}  // namespace editor
}  // namespace yaze
