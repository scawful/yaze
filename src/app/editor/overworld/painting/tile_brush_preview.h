#ifndef YAZE_APP_EDITOR_OVERWORLD_PAINTING_TILE_BRUSH_PREVIEW_H_
#define YAZE_APP_EDITOR_OVERWORLD_PAINTING_TILE_BRUSH_PREVIEW_H_

#include <algorithm>
#include <cstdint>
#include <span>
#include <vector>

#include "app/editor/overworld/painting/tile_brush.h"
#include "app/gfx/core/bitmap.h"

namespace yaze::editor {

// A compact piece of a brush, clipped to one 32x32-tile destination map.
struct MapBrushPreview {
  int tile_x = 0;
  int tile_y = 0;
  int width = 0;
  int height = 0;
  std::vector<uint8_t> pixels;
};

inline MapBrushPreview BuildMapBrushPreview(const TileBrush& brush,
                                            int anchor_x, int anchor_y,
                                            int map_x, int map_y,
                                            std::span<const uint8_t> atlas,
                                            int atlas_width = 128) {
  MapBrushPreview result;
  if (!brush.valid() || atlas_width < 16 || atlas_width % 16 != 0)
    return result;
  const int left = std::max(anchor_x, map_x * 32);
  const int top = std::max(anchor_y, map_y * 32);
  const int right = std::min(anchor_x + brush.width, (map_x + 1) * 32);
  const int bottom = std::min(anchor_y + brush.height, (map_y + 1) * 32);
  if (right <= left || bottom <= top)
    return result;
  result.tile_x = left;
  result.tile_y = top;
  result.width = (right - left) * 16;
  result.height = (bottom - top) * 16;
  result.pixels.resize(result.width * result.height);
  const int columns = atlas_width / 16;
  for (int y = top; y < bottom; ++y) {
    for (int x = left; x < right; ++x) {
      const int id = brush.at(x - anchor_x, y - anchor_y);
      const size_t source =
          (id / columns * 16) * atlas_width + (id % columns * 16);
      if (source + 15 * atlas_width + 16 > atlas.size())
        continue;
      for (int row = 0; row < 16; ++row) {
        std::copy_n(atlas.begin() + source + row * atlas_width, 16,
                    result.pixels.begin() +
                        ((y - top) * 16 + row) * result.width +
                        (x - left) * 16);
      }
    }
  }
  return result;
}

// A GPU texture cannot be resized with UpdateTexture. Keep one quadrant-sized
// allocation per destination map while the clipped piece grows/shrinks at its
// edges. This is a storage-quadrant extent, not the parent area's dimensions.
inline void SyncMapBrushPreview(const MapBrushPreview& piece,
                                const gfx::SnesPalette& palette,
                                gfx::Bitmap& preview) {
  constexpr int kTextureExtent = 32 * 16;
  if (piece.width <= 0 || piece.height <= 0 || piece.width > kTextureExtent ||
      piece.height > kTextureExtent ||
      piece.pixels.size() != static_cast<size_t>(piece.width) * piece.height) {
    return;
  }
  std::vector<uint8_t> pixels(kTextureExtent * kTextureExtent, 0);
  for (int y = 0; y < piece.height; ++y) {
    std::copy_n(piece.pixels.begin() + y * piece.width, piece.width,
                pixels.begin() + y * kTextureExtent);
  }
  const bool pixels_changed =
      !preview.is_active() || preview.vector() != pixels;
  const bool palette_changed = !(preview.palette() == palette);
  if (pixels_changed) {
    preview.Create(kTextureExtent, kTextureExtent, 8, pixels);
  }
  if (pixels_changed || palette_changed)
    preview.SetPalette(palette);
  if (!preview.texture())
    preview.CreateTexture();
  else if (pixels_changed || palette_changed)
    preview.UpdateTexture();
}

// The 16x16 pixels (palette indices) of `tile_id` in a tile16 blockset atlas
// `atlas_width` pixels wide; empty when the tile lies outside the atlas.
inline std::vector<uint8_t> Tile16PixelsFromBlockset(
    std::span<const uint8_t> atlas, int tile_id, int atlas_width = 128) {
  if (tile_id < 0 || atlas_width < 16 || atlas_width % 16 != 0)
    return {};
  const int columns = atlas_width / 16;
  const size_t source =
      static_cast<size_t>(tile_id / columns * 16) * atlas_width +
      (tile_id % columns * 16);
  if (source + 15 * static_cast<size_t>(atlas_width) + 16 > atlas.size())
    return {};
  std::vector<uint8_t> pixels(16 * 16);
  for (int row = 0; row < 16; ++row) {
    std::copy_n(atlas.begin() + source + row * atlas_width, 16,
                pixels.begin() + row * 16);
  }
  return pixels;
}

}  // namespace yaze::editor

#endif  // YAZE_APP_EDITOR_OVERWORLD_PAINTING_TILE_BRUSH_PREVIEW_H_
