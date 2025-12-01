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
  if (ImGui::GetIO().MouseWheel != 0.0f && ow_map_canvas_.IsMouseHovering()) {
    float zoom_delta = ImGui::GetIO().MouseWheel * 0.1f;
    float new_scale = ow_map_canvas_.global_scale() + zoom_delta;
    new_scale = std::clamp(new_scale, 0.1f, 5.0f);
    ow_map_canvas_.set_global_scale(new_scale);
  }
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
