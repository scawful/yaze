#ifndef YAZE_APP_EDITOR_OVERWORLD_PAINTING_TILE_BRUSH_PREVIEW_H_
#define YAZE_APP_EDITOR_OVERWORLD_PAINTING_TILE_BRUSH_PREVIEW_H_

#include <algorithm>
#include <cstdint>
#include <span>
#include <vector>

#include "app/editor/overworld/painting/tile_brush.h"

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

}  // namespace yaze::editor

#endif  // YAZE_APP_EDITOR_OVERWORLD_PAINTING_TILE_BRUSH_PREVIEW_H_
