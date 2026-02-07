#ifndef YAZE_ZELDA3_DUNGEON_DIMENSION_SERVICE_H
#define YAZE_ZELDA3_DUNGEON_DIMENSION_SERVICE_H

#include <tuple>
#include <utility>

#include "zelda3/dungeon/room_object.h"

namespace yaze {
namespace zelda3 {

/**
 * @brief Unified dimension lookup for dungeon room objects.
 *
 * Tries ObjectGeometry (exact buffer-replay) first, then falls back to
 * ObjectDimensionTable (hardcoded estimates). Callers no longer need to
 * maintain their own fallback chains.
 */
class DimensionService {
 public:
  static DimensionService& Get();

  struct DimensionResult {
    int offset_x_tiles = 0;
    int offset_y_tiles = 0;
    int width_tiles = 1;
    int height_tiles = 1;

    int width_pixels() const { return width_tiles * 8; }
    int height_pixels() const { return height_tiles * 8; }
  };

  // Primary lookup: returns tile-based dimensions with offset.
  DimensionResult GetDimensions(const RoomObject& obj) const;

  // Pixel-based convenience (width, height in pixels).
  std::pair<int, int> GetPixelDimensions(const RoomObject& obj) const;

  // Hit-test bounds: (x_tiles, y_tiles, width_tiles, height_tiles).
  // x/y include the object's position.
  std::tuple<int, int, int, int> GetHitTestBounds(
      const RoomObject& obj) const;

  // Selection bounds in pixels: (x_px, y_px, width_px, height_px).
  // x/y include the object's position.
  std::tuple<int, int, int, int> GetSelectionBoundsPixels(
      const RoomObject& obj) const;

 private:
  DimensionService() = default;
};

}  // namespace zelda3
}  // namespace yaze

#endif  // YAZE_ZELDA3_DUNGEON_DIMENSION_SERVICE_H
