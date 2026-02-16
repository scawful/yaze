#include "app/editor/dungeon/dungeon_overlay_controls.h"

#include <array>
#include <stdexcept>

namespace yaze::editor {

namespace {

constexpr std::array<DungeonOverlayControlSpec, 10> kOverlaySpecs = {{
    {DungeonOverlayControlId::kGrid, "Grid (8x8)"},
    {DungeonOverlayControlId::kObjectBounds, "Object Bounds"},
    {DungeonOverlayControlId::kHoverCoordinates, "Hover Coordinates"},
    {DungeonOverlayControlId::kCameraQuadrants, "Camera Quadrants"},
    {DungeonOverlayControlId::kTrackCollision, "Track Collision"},
    {DungeonOverlayControlId::kCustomCollision, "Custom Collision"},
    {DungeonOverlayControlId::kWaterFillOracle, "Water Fill (Oracle)"},
    {DungeonOverlayControlId::kMinecartPathing, "Minecart Pathing"},
    {DungeonOverlayControlId::kTrackGaps, "Track Gaps"},
    {DungeonOverlayControlId::kTrackRoutes, "Track Routes"},
}};

}  // namespace

const std::array<DungeonOverlayControlSpec, 10>&
GetDungeonOverlayControlSpecs() {
  return kOverlaySpecs;
}

bool GetDungeonOverlayControlEnabled(const DungeonCanvasViewer& viewer,
                                     DungeonOverlayControlId id) {
  switch (id) {
    case DungeonOverlayControlId::kGrid:
      return viewer.show_grid();
    case DungeonOverlayControlId::kObjectBounds:
      return viewer.show_object_bounds();
    case DungeonOverlayControlId::kHoverCoordinates:
      return viewer.show_coordinate_overlay();
    case DungeonOverlayControlId::kCameraQuadrants:
      return viewer.show_camera_quadrant_overlay();
    case DungeonOverlayControlId::kTrackCollision:
      return viewer.show_track_collision_overlay();
    case DungeonOverlayControlId::kCustomCollision:
      return viewer.show_custom_collision_overlay();
    case DungeonOverlayControlId::kWaterFillOracle:
      return viewer.show_water_fill_overlay();
    case DungeonOverlayControlId::kMinecartPathing:
      return viewer.show_minecart_sprite_overlay();
    case DungeonOverlayControlId::kTrackGaps:
      return viewer.show_track_gap_overlay();
    case DungeonOverlayControlId::kTrackRoutes:
      return viewer.show_track_route_overlay();
  }

  throw std::invalid_argument("unknown DungeonOverlayControlId");
}

void SetDungeonOverlayControlEnabled(DungeonCanvasViewer& viewer,
                                     DungeonOverlayControlId id, bool enabled) {
  switch (id) {
    case DungeonOverlayControlId::kGrid:
      viewer.set_show_grid(enabled);
      return;
    case DungeonOverlayControlId::kObjectBounds:
      viewer.set_show_object_bounds(enabled);
      return;
    case DungeonOverlayControlId::kHoverCoordinates:
      viewer.set_show_coordinate_overlay(enabled);
      return;
    case DungeonOverlayControlId::kCameraQuadrants:
      viewer.set_show_camera_quadrant_overlay(enabled);
      return;
    case DungeonOverlayControlId::kTrackCollision:
      viewer.set_show_track_collision_overlay(enabled);
      return;
    case DungeonOverlayControlId::kCustomCollision:
      viewer.set_show_custom_collision_overlay(enabled);
      return;
    case DungeonOverlayControlId::kWaterFillOracle:
      viewer.set_show_water_fill_overlay(enabled);
      return;
    case DungeonOverlayControlId::kMinecartPathing:
      viewer.set_show_minecart_sprite_overlay(enabled);
      return;
    case DungeonOverlayControlId::kTrackGaps:
      viewer.set_show_track_gap_overlay(enabled);
      return;
    case DungeonOverlayControlId::kTrackRoutes:
      viewer.set_show_track_route_overlay(enabled);
      return;
  }

  throw std::invalid_argument("unknown DungeonOverlayControlId");
}

}  // namespace yaze::editor
