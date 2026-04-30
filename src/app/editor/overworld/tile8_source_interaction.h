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
                                            float min_scale = 1.0f,
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
  const float fit_scale =
      usable_width / static_cast<float>(source_bitmap_width_px);
  if (fit_scale < min_scale) {
    // Fit wins over nominal readability. The Tile8 source is still vertically
    // scrollable, but it should not force a horizontal clip/scroll at default
    // or narrow dock widths.
    constexpr float kAbsoluteMinimumReadableScale = 0.35f;
    return std::clamp(fit_scale, kAbsoluteMinimumReadableScale, max_scale);
  }
  return std::clamp(fit_scale, min_scale, max_scale);
}

inline float ComputeTile8SourcePanelHeight(float available_height_px,
                                           int source_bitmap_height_px,
                                           float display_scale,
                                           float preferred_height_px = 0.0f,
                                           float min_height_px = 220.0f,
                                           float max_height_px = 520.0f) {
  if (min_height_px <= 0.0f) {
    min_height_px = 1.0f;
  }
  max_height_px = std::max(min_height_px, max_height_px);

  constexpr float kVerticalChromeAllowance = 18.0f;
  const float sheet_height =
      source_bitmap_height_px > 0 && display_scale > 0.0f
          ? (static_cast<float>(source_bitmap_height_px) * display_scale) +
                kVerticalChromeAllowance
          : max_height_px;

  float target_height =
      preferred_height_px > 0.0f ? preferred_height_px : sheet_height;
  target_height = std::clamp(target_height, min_height_px, max_height_px);

  if (available_height_px <= 0.0f) {
    return target_height;
  }

  const float clamped_max_height =
      std::min(max_height_px, std::max(1.0f, available_height_px));
  const float clamped_min_height = std::min(min_height_px, clamped_max_height);
  return std::clamp(target_height, clamped_min_height, clamped_max_height);
}

}  // namespace editor
}  // namespace yaze

#endif  // YAZE_APP_EDITOR_OVERWORLD_TILE8_SOURCE_INTERACTION_H
