#include "app/editor/overworld/overworld_editor.h"

#include "app/gui/canvas/canvas.h"
#include "imgui/imgui.h"

namespace yaze::editor {

void OverworldEditor::HandleOverworldPan() {
  // Determine if panning should occur:
  // 1. Middle-click drag always pans (all modes)
  // 2. Left-click drag pans in mouse mode when not hovering over an entity
  bool should_pan = false;

  if (ImGui::IsMouseDragging(ImGuiMouseButton_Middle)) {
    should_pan = true;
  } else if (ImGui::IsMouseDragging(ImGuiMouseButton_Left) &&
             current_mode == EditingMode::MOUSE) {
    // In mouse mode, left-click pans unless hovering over an entity
    bool over_entity = entity_renderer_ &&
                       entity_renderer_->hovered_entity() != nullptr;
    // Also don't pan if we're currently dragging an entity
    if (!over_entity && !is_dragging_entity_) {
      should_pan = true;
    }
  }

  if (!should_pan) {
    return;
  }

  // Pan by adjusting ImGui's scroll position (scrollbars handle actual scroll)
  ImVec2 delta = ImGui::GetIO().MouseDelta;
  float new_scroll_x = ImGui::GetScrollX() - delta.x;
  float new_scroll_y = ImGui::GetScrollY() - delta.y;

  // Get scroll limits from ImGui
  float max_scroll_x = ImGui::GetScrollMaxX();
  float max_scroll_y = ImGui::GetScrollMaxY();

  // Clamp to valid scroll range
  new_scroll_x = std::clamp(new_scroll_x, 0.0f, max_scroll_x);
  new_scroll_y = std::clamp(new_scroll_y, 0.0f, max_scroll_y);

  ImGui::SetScrollX(new_scroll_x);
  ImGui::SetScrollY(new_scroll_y);
}

void OverworldEditor::HandleOverworldZoom() {
  // Scroll wheel is reserved for canvas navigation/panning
  // Use toolbar buttons or context menu for zoom control
}

void OverworldEditor::ZoomIn() {
  float new_scale = std::min(kOverworldMaxZoom,
                             ow_map_canvas_.global_scale() + kOverworldZoomStep);
  ow_map_canvas_.set_global_scale(new_scale);
  // Scroll will be clamped automatically by ImGui on next frame
}

void OverworldEditor::ZoomOut() {
  float new_scale = std::max(kOverworldMinZoom,
                             ow_map_canvas_.global_scale() - kOverworldZoomStep);
  ow_map_canvas_.set_global_scale(new_scale);
  // Scroll will be clamped automatically by ImGui on next frame
}

void OverworldEditor::ClampOverworldScroll() {
  // ImGui handles scroll clamping automatically via GetScrollMaxX/Y
  // This function is now a no-op but kept for API compatibility
}

void OverworldEditor::ResetOverworldView() {
  // Reset ImGui scroll to top-left
  ImGui::SetScrollX(0);
  ImGui::SetScrollY(0);
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

  // Calculate scroll to center the current map (in ImGui's positive scroll space)
  float center_x = (map_x + kOverworldMapSize / 2.0f) * scale;
  float center_y = (map_y + kOverworldMapSize / 2.0f) * scale;

  float scroll_x = center_x - viewport_px.x / 2.0f;
  float scroll_y = center_y - viewport_px.y / 2.0f;

  // Clamp to valid scroll range
  scroll_x = std::clamp(scroll_x, 0.0f, ImGui::GetScrollMaxX());
  scroll_y = std::clamp(scroll_y, 0.0f, ImGui::GetScrollMaxY());

  ImGui::SetScrollX(scroll_x);
  ImGui::SetScrollY(scroll_y);
}

}  // namespace yaze::editor
