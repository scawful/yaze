#ifndef YAZE_ZELDA3_DUNGEON_GEOMETRY_OBJECT_GEOMETRY_H
#define YAZE_ZELDA3_DUNGEON_GEOMETRY_OBJECT_GEOMETRY_H

#include <cstdint>
#include <optional>
#include <tuple>
#include <unordered_map>
#include <vector>

#include "absl/status/statusor.h"
#include "zelda3/dungeon/draw_routines/draw_routine_types.h"
#include "zelda3/dungeon/room_object.h"

namespace yaze {
namespace zelda3 {

/**
 * @brief Simple rectangle for selection bounds
 */
struct SelectionRect {
  int x_tiles = 0;
  int y_tiles = 0;
  int width_tiles = 0;
  int height_tiles = 0;
  
  int width_pixels() const { return width_tiles * 8; }
  int height_pixels() const { return height_tiles * 8; }
};

/**
 * @brief Bounding box result for a draw routine execution.
 *
 * All values are in tile units. Convenience helpers expose pixel sizes for
 * callers that need screen-space values.
 *
 * For BG2 overlay objects (Layer 1 objects), the bounds also serve as the
 * BG1 mask rectangle - the area where BG1 pixels should be transparent
 * to allow BG2 content to show through during compositing.
 *
 * For triangular shapes (diagonal ceilings 0xA0-0xA3), the selection_bounds
 * provides a tighter hitbox than the full render_bounds bounding box.
 */
struct GeometryBounds {
  // Full rendered area (bounding box of all drawn tiles)
  int min_x_tiles = 0;
  int min_y_tiles = 0;
  int width_tiles = 0;
  int height_tiles = 0;

  // Layer information for BG2 masking
  bool is_bg2_overlay = false;  // True if this object draws to BG2 (Layer 1)

  // Optional tighter selection hitbox for non-rectangular shapes
  // If not set, selection uses the full render bounds
  std::optional<SelectionRect> selection_bounds;

  int max_x_tiles() const { return min_x_tiles + width_tiles; }
  int max_y_tiles() const { return min_y_tiles + height_tiles; }
  int width_pixels() const { return width_tiles * 8; }
  int height_pixels() const { return height_tiles * 8; }

  // Pixel-based accessors for the mask rectangle
  int min_x_pixels() const { return min_x_tiles * 8; }
  int min_y_pixels() const { return min_y_tiles * 8; }

  /**
   * @brief Check if this object requires BG1 masking.
   *
   * BG2 overlay objects need corresponding pixels in BG1 marked as transparent
   * so the BG2 content shows through during layer compositing.
   * 94 rooms in vanilla ALTTP have such objects.
   */
  bool RequiresBG1Mask() const { return is_bg2_overlay && width_tiles > 0; }

  /**
   * @brief Check if this object has a tighter selection hitbox.
   *
   * For non-rectangular shapes like diagonal ceilings, the selection_bounds
   * provides a more accurate hitbox for mouse interaction.
   */
  bool HasTighterSelectionBounds() const { return selection_bounds.has_value(); }

  /**
   * @brief Get the selection bounds for hit testing.
   *
   * Returns selection_bounds if set, otherwise returns the full render bounds.
   */
  SelectionRect GetSelectionBounds() const {
    if (selection_bounds.has_value()) {
      return *selection_bounds;
    }
    return SelectionRect{min_x_tiles, min_y_tiles, width_tiles, height_tiles};
  }

  /**
   * @brief Get the BG1 mask rectangle in pixel coordinates.
   *
   * @param obj_x Object's base X position in tiles
   * @param obj_y Object's base Y position in tiles
   * @return Tuple of (start_x, start_y, width, height) in pixels
   */
  std::tuple<int, int, int, int> GetBG1MaskRect(int obj_x, int obj_y) const {
    int start_x = (obj_x + min_x_tiles) * 8;
    int start_y = (obj_y + min_y_tiles) * 8;
    return {start_x, start_y, width_pixels(), height_pixels()};
  }
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

  // Measure bounds by object ID (resolves object_id -> routine_id internally).
  // Results are cached by (routine_id, object_id, size).
  absl::StatusOr<GeometryBounds> MeasureByObjectId(
      const RoomObject& object) const;

  // Clear the measurement cache (e.g., after routine registry changes).
  void ClearCache();

  // Measure bounds for a specific routine metadata entry.
  absl::StatusOr<GeometryBounds> MeasureRoutine(
      const DrawRoutineInfo& routine, const RoomObject& object) const;

  /**
   * @brief Measure bounds for a BG2 overlay object and mark it for masking.
   *
   * This is a convenience method that measures the object and sets the
   * is_bg2_overlay flag if the object's layer indicates it draws to BG2.
   * Used by the dungeon editor to determine BG1 mask regions.
   *
   * @param routine_id Draw routine ID
   * @param object Object with layer information
   * @return GeometryBounds with is_bg2_overlay set appropriately
   */
  absl::StatusOr<GeometryBounds> MeasureForLayerCompositing(
      int routine_id, const RoomObject& object) const;

  /**
   * @brief Get list of routine IDs that draw to BG2 layer.
   *
   * These routines write to the lower tilemap (BG2) and require
   * corresponding BG1 transparency for proper compositing.
   */
  static bool IsLayerOneRoutine(int routine_id);

  /**
   * @brief Check if a routine ID corresponds to a diagonal ceiling.
   *
   * Diagonal ceilings (routines 75-78 for objects 0xA0-0xA3) have triangular
   * fill patterns that require tighter selection bounds than their bounding box.
   */
  static bool IsDiagonalCeilingRoutine(int routine_id);

  /**
   * @brief Compute tighter selection bounds for diagonal shapes.
   *
   * For triangular diagonal ceilings, this computes a selection rectangle
   * that's roughly 70% of the bounding box, centered on the visual content.
   *
   * @param render_bounds Full render bounds from MeasureRoutine
   * @param routine_id The routine ID to check for diagonal shapes
   * @return GeometryBounds with selection_bounds set if applicable
   */
  static GeometryBounds ApplySelectionBounds(GeometryBounds render_bounds,
                                              int routine_id);

 private:
  ObjectGeometry();
  void BuildRegistry();

  // Cache key: combines routine_id, object_id, and size into a single uint64.
  struct CacheKey {
    int routine_id;
    int16_t object_id;
    uint8_t size;
    bool operator==(const CacheKey& o) const {
      return routine_id == o.routine_id && object_id == o.object_id &&
             size == o.size;
    }
  };
  struct CacheKeyHash {
    size_t operator()(const CacheKey& k) const {
      return std::hash<uint64_t>()(
          (static_cast<uint64_t>(k.routine_id) << 32) |
          (static_cast<uint64_t>(static_cast<uint16_t>(k.object_id)) << 8) |
          k.size);
    }
  };

  std::vector<DrawRoutineInfo> routines_;
  std::unordered_map<int, DrawRoutineInfo> routine_map_;
  mutable std::unordered_map<CacheKey, GeometryBounds, CacheKeyHash> cache_;
};

}  // namespace zelda3
}  // namespace yaze

#endif  // YAZE_ZELDA3_DUNGEON_GEOMETRY_OBJECT_GEOMETRY_H
