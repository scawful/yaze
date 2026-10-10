#include "app/editor/dungeon/dungeon_overlay_controls.h"

#include <array>
#include <stdexcept>

#include "app/gui/automation/widget_auto_register.h"
#include "app/gui/core/ui_helpers.h"
#include "imgui/imgui.h"
#include "util/i18n/tr.h"

namespace yaze::editor {

namespace {

constexpr std::array<DungeonOverlayControlSpec, 13> kOverlaySpecs = {{
    {DungeonOverlayControlId::kChestContents, "Chest contents"},
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
    {DungeonOverlayControlId::kCustomObjects, "Custom Objects (Oracle)"},
    {DungeonOverlayControlId::kCrumbleFloor, "Crumble Floor (Oracle)"},
}};

}  // namespace

const std::array<DungeonOverlayControlSpec, 13>&
GetDungeonOverlayControlSpecs() {
  return kOverlaySpecs;
}

float DungeonLayerControlsWidth() {
  return ImGui::CalcTextSize("BG1").x + ImGui::CalcTextSize("BG2").x +
         ImGui::CalcTextSize(tr("Sprites")).x +
         ImGui::GetStyle().FramePadding.x * 6.0f +
         ImGui::GetStyle().ItemSpacing.x * 2.0f;
}

void DrawDungeonLayerControls(DungeonCanvasViewer& viewer, int room_id,
                              float height) {
  if (room_id < 0 || room_id >= zelda3::kNumberOfRooms)
    return;
  ImGui::PushID("DungeonLayers");
  gui::AutoWidgetScope scope("Dungeon/Layers");
  const auto toggle = [&](const char* label, bool enabled,
                          const char* tooltip) {
    const float width =
        ImGui::CalcTextSize(label).x + ImGui::GetStyle().FramePadding.x * 2.0f;
    const bool changed =
        gui::ToggleButton(label, enabled, ImVec2(width, height));
    gui::AutoRegisterLastItem("button", label);
    if (ImGui::IsItemHovered())
      ImGui::SetTooltip("%s", tooltip);
    return changed;
  };
  if (toggle("BG1", viewer.IsBG1Visible(room_id), "Show main background layer"))
    viewer.SetBG1Visible(room_id, !viewer.IsBG1Visible(room_id));
  ImGui::SameLine();
  if (toggle("BG2", viewer.IsBG2Visible(room_id),
             "Show lower background layer"))
    viewer.SetBG2Visible(room_id, !viewer.IsBG2Visible(room_id));
  ImGui::SameLine();
  if (toggle(tr("Sprites"), viewer.AreSpritesVisible(), "Show room sprites"))
    viewer.SetSpritesVisible(!viewer.AreSpritesVisible());
  ImGui::PopID();
}

void DrawDungeonOverlayControls(DungeonCanvasViewer& viewer) {
  gui::AutoWidgetScope scope("Dungeon/ViewOptions");
  for (const auto& spec : GetDungeonOverlayControlSpecs()) {
    if (spec.id == DungeonOverlayControlId::kTrackCollision)
      ImGui::SeparatorText(tr("Authoring"));
    bool enabled = GetDungeonOverlayControlEnabled(viewer, spec.id);
    if (ImGui::Checkbox(tr(spec.label), &enabled))
      SetDungeonOverlayControlEnabled(viewer, spec.id, enabled);
    gui::AutoRegisterLastItem("checkbox", spec.label);
  }
  bool pots = viewer.ArePotItemsVisible();
  if (ImGui::Checkbox(tr("Pot items"), &pots))
    viewer.SetPotItemsVisible(pots);
}

bool GetDungeonOverlayControlEnabled(const DungeonCanvasViewer& viewer,
                                     DungeonOverlayControlId id) {
  switch (id) {
    case DungeonOverlayControlId::kChestContents:
      return viewer.AreChestContentsVisible();
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
    case DungeonOverlayControlId::kCrumbleFloor:
      return viewer.show_crumble_floor();
    case DungeonOverlayControlId::kCustomObjects:
      return viewer.show_custom_objects_overlay();
  }

  throw std::invalid_argument("unknown DungeonOverlayControlId");
}

void SetDungeonOverlayControlEnabled(DungeonCanvasViewer& viewer,
                                     DungeonOverlayControlId id, bool enabled) {
  switch (id) {
    case DungeonOverlayControlId::kChestContents:
      viewer.SetChestContentsVisible(enabled);
      return;
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
    case DungeonOverlayControlId::kCrumbleFloor:
      viewer.set_show_crumble_floor(enabled);
      return;
    case DungeonOverlayControlId::kCustomObjects:
      viewer.set_show_custom_objects_overlay(enabled);
      return;
  }

  throw std::invalid_argument("unknown DungeonOverlayControlId");
}

}  // namespace yaze::editor
