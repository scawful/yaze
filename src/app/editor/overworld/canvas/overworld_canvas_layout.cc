#include "app/editor/overworld/canvas/overworld_canvas_layout.h"

namespace yaze::editor {

ImVec2 OverworldCanvasLayout::BlockOrigin(int world) const {
  if (IsSingleWorld()) {
    return ImVec2(0.0f, 0.0f);
  }
  constexpr float kStep =
      static_cast<float>(kOverworldWorldBlockPx + kUnifiedOverworldGutterPx);
  switch (world) {
    case 0:
      return ImVec2(0.0f, 0.0f);
    case 1:
      return ImVec2(0.0f, kStep);
    case 2:
      return ImVec2(kStep, 0.0f);
    default:
      return ImVec2(0.0f, 0.0f);
  }
}

ImVec2 OverworldCanvasLayout::LyraTailOrigin() const {
  const ImVec2 special_origin = BlockOrigin(2);
  return ImVec2(special_origin.x,
                special_origin.y + static_cast<float>(kOverworldMapSizePx * 4));
}

ImVec2 OverworldCanvasLayout::MapOrigin(int map_id) const {
  if (map_id < 0 || map_id >= zelda3::kExpandedMapCount) {
    return ImVec2(0.0f, 0.0f);
  }
  const ImVec2 block = BlockOrigin(WorldForMap(map_id));
  const ImVec2 local = SingleMapOrigin(map_id);
  return ImVec2(block.x + local.x, block.y + local.y);
}

ImVec2 OverworldCanvasLayout::CanvasSize() const {
  if (IsSingleWorld()) {
    const float size = static_cast<float>(kOverworldWorldBlockPx);
    return ImVec2(size, size);
  }
  const float size = static_cast<float>(kUnifiedCanvasExtentPx);
  return ImVec2(size, size);
}

ImVec2 OverworldCanvasLayout::FitExtent(int current_world,
                                        int displayed_map_count) const {
  if (IsUnified()) {
    return CanvasSize();
  }
  const int world = std::clamp(current_world, 0, kOverworldWorldCount - 1);
  const int world_maps = std::clamp(displayed_map_count - WorldStartMap(world),
                                    1, kOverworldMapsPerWorld);
  const float world_height = static_cast<float>(
      ((world_maps + kOverworldGridCols - 1) / kOverworldGridCols) *
      kOverworldMapSizePx);
  return ImVec2(static_cast<float>(kOverworldWorldBlockPx), world_height);
}

std::optional<OverworldCanvasWorldPoint>
OverworldCanvasLayout::WorldLocalFromCanvas(ImVec2 canvas_point,
                                            int current_world) const {
  if (!std::isfinite(canvas_point.x) || !std::isfinite(canvas_point.y) ||
      canvas_point.x < 0.0f || canvas_point.y < 0.0f) {
    return std::nullopt;
  }

  constexpr float kBlockSize = static_cast<float>(kOverworldWorldBlockPx);
  if (IsSingleWorld()) {
    if (current_world < 0 || current_world >= kOverworldWorldCount ||
        canvas_point.x >= kBlockSize || canvas_point.y >= kBlockSize) {
      return std::nullopt;
    }
    return OverworldCanvasWorldPoint{current_world, canvas_point};
  }

  constexpr float kSecondBlockStart =
      static_cast<float>(kOverworldWorldBlockPx + kUnifiedOverworldGutterPx);
  constexpr float kMaxExtent = static_cast<float>(kUnifiedCanvasExtentPx);

  if (canvas_point.x < kBlockSize && canvas_point.y < kBlockSize) {
    return OverworldCanvasWorldPoint{0, canvas_point};
  }
  if (canvas_point.x < kBlockSize && canvas_point.y >= kSecondBlockStart &&
      canvas_point.y < kMaxExtent) {
    return OverworldCanvasWorldPoint{
        1, ImVec2(canvas_point.x, canvas_point.y - kSecondBlockStart)};
  }
  if (canvas_point.x >= kSecondBlockStart && canvas_point.x < kMaxExtent &&
      canvas_point.y < kBlockSize) {
    return OverworldCanvasWorldPoint{
        2, ImVec2(canvas_point.x - kSecondBlockStart, canvas_point.y)};
  }

  // Gutter strips, south-east empty quadrant, or outside Unified canvas.
  return std::nullopt;
}

std::optional<int> OverworldCanvasLayout::MapAt(ImVec2 canvas_point,
                                                int current_world,
                                                int map_count) const {
  const auto world_point = WorldLocalFromCanvas(canvas_point, current_world);
  if (!world_point) {
    return std::nullopt;
  }
  const int map_x =
      static_cast<int>(world_point->local_position.x) / kOverworldMapSizePx;
  const int map_y =
      static_cast<int>(world_point->local_position.y) / kOverworldMapSizePx;
  if (map_x < 0 || map_x >= kOverworldGridCols || map_y < 0 ||
      map_y >= kOverworldGridRows) {
    return std::nullopt;
  }
  const int map_id = MapIdFromLocal(world_point->world, map_x, map_y);
  if (map_id < 0 || map_id >= map_count) {
    return std::nullopt;
  }
  return map_id;
}

float OverworldCanvasLayout::MinZoom(ImVec2 viewport_size, int current_world,
                                     int displayed_map_count,
                                     float single_min_zoom) const {
  if (IsSingleWorld()) {
    return single_min_zoom;
  }
  if (!std::isfinite(viewport_size.x) || !std::isfinite(viewport_size.y) ||
      viewport_size.x <= 0.0f || viewport_size.y <= 0.0f) {
    return kUnifiedOverworldMinZoomFloor;
  }
  const ImVec2 extent = FitExtent(current_world, displayed_map_count);
  const float raw_fit =
      std::min(viewport_size.x / extent.x, viewport_size.y / extent.y);
  return std::clamp(std::min(single_min_zoom, raw_fit),
                    kUnifiedOverworldMinZoomFloor, single_min_zoom);
}

float OverworldCanvasLayout::FitScale(ImVec2 viewport_size, int current_world,
                                      int displayed_map_count,
                                      float max_zoom) const {
  const float min_zoom = MinZoom(viewport_size, current_world,
                                 displayed_map_count, kOverworldMinZoom);
  if (!std::isfinite(viewport_size.x) || !std::isfinite(viewport_size.y) ||
      viewport_size.x <= 0.0f || viewport_size.y <= 0.0f) {
    return min_zoom;
  }
  const ImVec2 extent = FitExtent(current_world, displayed_map_count);
  const float raw_scale =
      std::min(viewport_size.x / extent.x, viewport_size.y / extent.y);
  return std::clamp(raw_scale, min_zoom, max_zoom);
}

}  // namespace yaze::editor
