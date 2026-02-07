#include "zelda3/dungeon/dimension_service.h"

#include <algorithm>

#include "zelda3/dungeon/geometry/object_geometry.h"
#include "zelda3/dungeon/object_dimensions.h"

namespace yaze {
namespace zelda3 {

DimensionService& DimensionService::Get() {
  static DimensionService instance;
  return instance;
}

DimensionService::DimensionResult DimensionService::GetDimensions(
    const RoomObject& obj) const {
  // Try ObjectGeometry first (exact buffer-replay).
  auto geo_result = ObjectGeometry::Get().MeasureByObjectId(obj);
  if (geo_result.ok()) {
    const auto& bounds = *geo_result;
    return DimensionResult{
        .offset_x_tiles = bounds.min_x_tiles,
        .offset_y_tiles = bounds.min_y_tiles,
        .width_tiles = std::max(1, bounds.width_tiles),
        .height_tiles = std::max(1, bounds.height_tiles),
    };
  }

  // Fall back to ObjectDimensionTable.
  auto& dim_table = ObjectDimensionTable::Get();
  if (dim_table.IsLoaded()) {
    auto sel = dim_table.GetSelectionBounds(obj.id_, obj.size_);
    return DimensionResult{
        .offset_x_tiles = sel.offset_x,
        .offset_y_tiles = sel.offset_y,
        .width_tiles = std::max(1, sel.width),
        .height_tiles = std::max(1, sel.height),
    };
  }

  // Last resort: naive estimate.
  int size = obj.size_ & 0x0F;
  return DimensionResult{
      .offset_x_tiles = 0,
      .offset_y_tiles = 0,
      .width_tiles = std::max(1, 2 + size * 2),
      .height_tiles = 2,
  };
}

std::pair<int, int> DimensionService::GetPixelDimensions(
    const RoomObject& obj) const {
  auto result = GetDimensions(obj);
  return {result.width_pixels(), result.height_pixels()};
}

std::tuple<int, int, int, int> DimensionService::GetHitTestBounds(
    const RoomObject& obj) const {
  auto result = GetDimensions(obj);
  return {obj.x_ + result.offset_x_tiles, obj.y_ + result.offset_y_tiles,
          result.width_tiles, result.height_tiles};
}

std::tuple<int, int, int, int> DimensionService::GetSelectionBoundsPixels(
    const RoomObject& obj) const {
  auto result = GetDimensions(obj);
  int x_px = (obj.x_ + result.offset_x_tiles) * 8;
  int y_px = (obj.y_ + result.offset_y_tiles) * 8;
  return {x_px, y_px, result.width_pixels(), result.height_pixels()};
}

}  // namespace zelda3
}  // namespace yaze
