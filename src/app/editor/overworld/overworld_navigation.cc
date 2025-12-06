#include "app/editor/overworld/overworld_editor.h"

#include "app/gui/canvas/canvas.h"

namespace yaze::editor {

void OverworldEditor::HandleOverworldPan() {
  if (ImGui::IsMouseDragging(ImGuiMouseButton_Middle)) {
    ImVec2 delta = ImGui::GetIO().MouseDelta;
    ow_map_canvas_.set_scrolling(
        ImVec2(ow_map_canvas_.scrolling().x + delta.x,
               ow_map_canvas_.scrolling().y + delta.y));
  }
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
  // This is a placeholder implementation - actual centering logic depends on
  // canvas size and viewport size
  ow_map_canvas_.set_scrolling(ImVec2(0, 0));
}

}  // namespace yaze::editor
