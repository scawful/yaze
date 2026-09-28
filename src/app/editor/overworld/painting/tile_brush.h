#ifndef YAZE_APP_EDITOR_OVERWORLD_PAINTING_TILE_BRUSH_H_
#define YAZE_APP_EDITOR_OVERWORLD_PAINTING_TILE_BRUSH_H_

#include <algorithm>
#include <cstddef>
#include <vector>

namespace yaze::editor {

// Captured Tile16 IDs in row-major order. Source coordinates and the moving
// destination preview are deliberately not part of the brush value.
struct TileBrush {
  int width = 0;
  int height = 0;
  std::vector<int> tile_ids;

  bool valid() const {
    return width > 0 && width <= 256 && height > 0 && height <= 256 &&
           tile_ids.size() == static_cast<size_t>(width) * height &&
           std::all_of(tile_ids.begin(), tile_ids.end(),
                       [](int id) { return id >= 0 && id <= 0xFFFF; });
  }

  int at(int x, int y) const { return tile_ids[y * width + x]; }
};

}  // namespace yaze::editor

#endif  // YAZE_APP_EDITOR_OVERWORLD_PAINTING_TILE_BRUSH_H_
