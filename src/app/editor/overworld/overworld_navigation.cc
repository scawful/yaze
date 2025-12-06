#include "app/editor/overworld/overworld_editor.h"

#include "app/gui/canvas/canvas.h"
#include "imgui/imgui.h"

namespace yaze::editor {

void OverworldEditor::HandleOverworldPan() {
  if (!ImGui::IsMouseDragging(ImGuiMouseButton_Middle)) {
    return;
  }

  ImVec2 delta = ImGui::GetIO().MouseDelta;
  ImVec2 new_scroll = ImVec2(ow_map_canvas_.scrolling().x + delta.x,
                             ow_map_canvas_.scrolling().y + delta.y);

  // Clamp to content bounds (scaled content vs viewport)
  float scale = ow_map_canvas_.global_scale();
  if (scale <= 0.0f) scale = 1.0f;

  ImVec2 content_px = ImVec2(kOverworldCanvasSize.x * scale,
                             kOverworldCanvasSize.y * scale);
  ImVec2 viewport_px = ImGui::GetContentRegionAvail();

  // Clamp scroll to prevent scrolling beyond map bounds
  new_scroll = gui::ClampScroll(new_scroll, content_px, viewport_px);
  ow_map_canvas_.set_scrolling(new_scroll);
}

void OverworldEditor::HandleOverworldZoom() {
  // Scroll wheel is reserved for canvas navigation/panning
  // Use toolbar buttons or context menu for zoom control
}

void OverworldEditor::ZoomIn() {
  float new_scale = std::min(kOverworldMaxZoom,
                             ow_map_canvas_.global_scale() + kOverworldZoomStep);
  ow_map_canvas_.set_global_scale(new_scale);
}

void OverworldEditor::ZoomOut() {
  float new_scale = std::max(kOverworldMinZoom,
                             ow_map_canvas_.global_scale() - kOverworldZoomStep);
  ow_map_canvas_.set_global_scale(new_scale);
}

void OverworldEditor::ResetOverworldView() {
  ow_map_canvas_.set_scrolling(ImVec2(0, 0));
  ow_map_canvas_.set_global_scale(1.0f);
}

void OverworldEditor::CenterOverworldView() {
  // Center the view on the current map
  float scale = ow_map_canvas_.global_scale();
  if (scale <= 0.0f) scale = 1.0f;

  // Calculate map position within the world
  int map_in_world = current_map_ % 0x40;
  int map_x = (map_in_world % 8) * kOverworldMapSize;
  int map_y = (map_in_world / 8) * kOverworldMapSize;

  // Get viewport size
  ImVec2 viewport_px = ImGui::GetContentRegionAvail();

  // Calculate scroll to center the current map
  float center_x = (map_x + kOverworldMapSize / 2.0f) * scale;
  float center_y = (map_y + kOverworldMapSize / 2.0f) * scale;

  ImVec2 new_scroll = ImVec2(viewport_px.x / 2.0f - center_x,
                             viewport_px.y / 2.0f - center_y);

  // Clamp scroll to valid bounds
  ImVec2 content_px = ImVec2(kOverworldCanvasSize.x * scale,
                             kOverworldCanvasSize.y * scale);
  new_scroll = gui::ClampScroll(new_scroll, content_px, viewport_px);
  ow_map_canvas_.set_scrolling(new_scroll);
}

}  // namespace yaze::editor
