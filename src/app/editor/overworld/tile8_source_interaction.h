#ifndef YAZE_APP_EDITOR_OVERWORLD_TILE8_SOURCE_INTERACTION_H
#define YAZE_APP_EDITOR_OVERWORLD_TILE8_SOURCE_INTERACTION_H

#include <algorithm>

namespace yaze {
namespace editor {

inline bool ComputeTile8UsageHighlight(bool source_hovered,
                                       bool right_mouse_down) {
  return source_hovered && right_mouse_down;
}

inline int ComputeTile8IndexFromCanvasMouse(float mouse_x, float mouse_y,
                                            int source_bitmap_width_px,
                                            int max_tile_count,
                                            float display_scale) {
  if (source_bitmap_width_px <= 0 || max_tile_count <= 0 ||
      display_scale <= 0) {
    return -1;
  }
  if (mouse_x < 0.0f || mouse_y < 0.0f) {
    return -1;
  }

  const int tile_x = static_cast<int>(mouse_x / (8.0f * display_scale));
  const int tile_y = static_cast<int>(mouse_y / (8.0f * display_scale));
  if (tile_x < 0 || tile_y < 0) {
    return -1;
  }

  const int tiles_per_row = source_bitmap_width_px / 8;
  if (tiles_per_row <= 0) {
    return -1;
  }

  const int tile8_id = tile_x + (tile_y * tiles_per_row);
  if (tile8_id < 0 || tile8_id >= max_tile_count) {
    return -1;
  }
  return tile8_id;
}

inline float ComputeTile8SourceDisplayScale(float available_width_px,
                                            int source_bitmap_width_px,
                                            float min_scale = 1.5f,
                                            float max_scale = 4.0f) {
  if (source_bitmap_width_px <= 0 || available_width_px <= 0.0f ||
      min_scale <= 0.0f || max_scale < min_scale) {
    return min_scale;
  }

  // Leave room for child-window padding and the vertical scrollbar. The Tile8
  // sheet should normally fit horizontally; height is handled by scrolling.
  constexpr float kHorizontalChromeAllowance = 24.0f;
  const float usable_width =
      std::max(1.0f, available_width_px - kHorizontalChromeAllowance);
  return std::clamp(usable_width / static_cast<float>(source_bitmap_width_px),
                    min_scale, max_scale);
}

}  // namespace editor
}  // namespace yaze

#endif  // YAZE_APP_EDITOR_OVERWORLD_TILE8_SOURCE_INTERACTION_H
