#ifndef YAZE_ZELDA3_DUNGEON_GEOMETRY_OBJECT_GEOMETRY_H
#define YAZE_ZELDA3_DUNGEON_GEOMETRY_OBJECT_GEOMETRY_H

#include <unordered_map>
#include <vector>

#include "absl/status/statusor.h"
#include "zelda3/dungeon/draw_routines/draw_routine_types.h"
#include "zelda3/dungeon/room_object.h"

namespace yaze {
namespace zelda3 {

/**
 * @brief Bounding box result for a draw routine execution.
 *
 * All values are in tile units. Convenience helpers expose pixel sizes for
 * callers that need screen-space values.
 */
struct GeometryBounds {
  int min_x_tiles = 0;
  int min_y_tiles = 0;
  int width_tiles = 0;
  int height_tiles = 0;

  int max_x_tiles() const { return min_x_tiles + width_tiles; }
  int max_y_tiles() const { return min_y_tiles + height_tiles; }
  int width_pixels() const { return width_tiles * 8; }
  int height_pixels() const { return height_tiles * 8; }
};

/**
 * @brief Side-car geometry engine that replays draw routines against an
 *        off-screen buffer to calculate real extents.
 *
 * This is intentionally self-contained (no ROM or editor coupling) so it can
 * be tested in isolation and later dropped into ObjectDrawer/editor hit-testing
 * paths.
 */
class ObjectGeometry {
 public:
  static ObjectGeometry& Get();

  // Look up routine metadata by routine ID. Returns nullptr on unknown IDs.
  const DrawRoutineInfo* LookupRoutine(int routine_id) const;

  // Measure bounds by routine id using the object's size/id bits.
  absl::StatusOr<GeometryBounds> MeasureByRoutineId(
      int routine_id, const RoomObject& object) const;

  // Measure bounds for a specific routine metadata entry.
  absl::StatusOr<GeometryBounds> MeasureRoutine(
      const DrawRoutineInfo& routine, const RoomObject& object) const;

 private:
  ObjectGeometry();
  void BuildRegistry();

  std::vector<DrawRoutineInfo> routines_;
  std::unordered_map<int, DrawRoutineInfo> routine_map_;
};

}  // namespace zelda3
}  // namespace yaze

#endif  // YAZE_ZELDA3_DUNGEON_GEOMETRY_OBJECT_GEOMETRY_H
