#ifndef YAZE_APP_EDITOR_DUNGEON_INTERACTION_PAINT_UTIL_H_
#define YAZE_APP_EDITOR_DUNGEON_INTERACTION_PAINT_UTIL_H_

#include <algorithm>
#include <cstdlib>
#include <utility>

namespace yaze::editor::paint_util {

// Iterates integer grid points on a line segment (inclusive endpoints).
// Uses a classic Bresenham formulation; order is from (x0,y0) -> (x1,y1).
template <typename Fn>
inline void ForEachPointOnLine(int x0, int y0, int x1, int y1, Fn&& fn) {
  int dx = std::abs(x1 - x0);
  int sx = x0 < x1 ? 1 : -1;
  int dy = -std::abs(y1 - y0);
  int sy = y0 < y1 ? 1 : -1;
  int err = dx + dy;  // error value e_xy

  while (true) {
    fn(x0, y0);
    if (x0 == x1 && y0 == y1) {
      break;
    }
    const int e2 = 2 * err;
    if (e2 >= dy) {
      err += dy;
      x0 += sx;
    }
    if (e2 <= dx) {
      err += dx;
      y0 += sy;
    }
  }
}

// Iterates integer grid points in a square brush centered at (cx,cy).
// Bounds are inclusive.
template <typename Fn>
inline void ForEachPointInSquareBrush(int cx, int cy, int radius, int min_x,
                                      int min_y, int max_x, int max_y, Fn&& fn) {
  radius = std::max(0, radius);
  const int x0 = std::max(min_x, cx - radius);
  const int x1 = std::min(max_x, cx + radius);
  const int y0 = std::max(min_y, cy - radius);
  const int y1 = std::min(max_y, cy + radius);

  for (int y = y0; y <= y1; ++y) {
    for (int x = x0; x <= x1; ++x) {
      fn(x, y);
    }
  }
}

}  // namespace yaze::editor::paint_util

#endif  // YAZE_APP_EDITOR_DUNGEON_INTERACTION_PAINT_UTIL_H_

