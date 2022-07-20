#include "canvas.h"

#include <imgui/imgui.h>

#include <cmath>
#include <string>

namespace yaze {
namespace gui {

void Canvas::Update() {
  ImVec2 canvas_p0 = ImGui::GetCursorScreenPos();
  if (!custom_canvas_size_) canvas_sz_ = ImGui::GetContentRegionAvail();

  auto canvas_p1 =
      ImVec2(canvas_p0.x + canvas_sz_.x, canvas_p0.y + canvas_sz_.y);

  // Draw border and background color
  const ImGuiIO &io = ImGui::GetIO();
  ImDrawList *draw_list = ImGui::GetWindowDrawList();
  draw_list->AddRectFilled(canvas_p0, canvas_p1, IM_COL32(32, 32, 32, 255));
  draw_list->AddRect(canvas_p0, canvas_p1, IM_COL32(255, 255, 255, 255));

  // This will catch our interactions
  ImGui::InvisibleButton(
      "canvas", canvas_sz_,
      ImGuiButtonFlags_MouseButtonLeft | ImGuiButtonFlags_MouseButtonRight);
  const bool is_hovered = ImGui::IsItemHovered();  // Hovered
  const bool is_active = ImGui::IsItemActive();    // Held
  const ImVec2 origin(canvas_p0.x + scrolling_.x,
                      canvas_p0.y + scrolling_.y);  // Lock scrolled origin
  const ImVec2 mouse_pos_in_canvas(io.MousePos.x - origin.x,
                                   io.MousePos.y - origin.y);

  // Add first and second point
  if (is_hovered && !dragging_select_ &&
      ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
    points_.push_back(mouse_pos_in_canvas);
    points_.push_back(mouse_pos_in_canvas);
    dragging_select_ = true;
  }
  if (dragging_select_) {
    points_.back() = mouse_pos_in_canvas;
    if (!ImGui::IsMouseDown(ImGuiMouseButton_Left)) dragging_select_ = false;
  }

  // Pan (we use a zero mouse threshold when there's no context menu)
  const float mouse_threshold_for_pan = enable_context_menu_ ? -1.0f : 0.0f;
  if (is_active &&
      ImGui::IsMouseDragging(ImGuiMouseButton_Right, mouse_threshold_for_pan)) {
    scrolling_.x += io.MouseDelta.x;
    scrolling_.y += io.MouseDelta.y;
  }

  // Context menu (under default mouse threshold)
  ImVec2 drag_delta = ImGui::GetMouseDragDelta(ImGuiMouseButton_Right);
  if (enable_context_menu_ && drag_delta.x == 0.0f && drag_delta.y == 0.0f)
    ImGui::OpenPopupOnItemClick("context", ImGuiPopupFlags_MouseButtonRight);
  if (ImGui::BeginPopup("context")) {
    if (dragging_select_) points_.resize(points_.size() - 2);
    dragging_select_ = false;
    if (ImGui::MenuItem("Remove all", nullptr, false, points_.Size > 0)) {
      points_.clear();
    }
    ImGui::EndPopup();
  }

  // Draw grid + all lines in the canvas
  draw_list->PushClipRect(canvas_p0, canvas_p1, true);
  if (enable_grid_) {
    const float GRID_STEP = 64.0f;
    for (float x = fmodf(scrolling_.x, GRID_STEP); x < canvas_sz_.x;
         x += GRID_STEP)
      draw_list->AddLine(ImVec2(canvas_p0.x + x, canvas_p0.y),
                         ImVec2(canvas_p0.x + x, canvas_p1.y),
                         IM_COL32(200, 200, 200, 40));
    for (float y = fmodf(scrolling_.y, GRID_STEP); y < canvas_sz_.y;
         y += GRID_STEP)
      draw_list->AddLine(ImVec2(canvas_p0.x, canvas_p0.y + y),
                         ImVec2(canvas_p1.x, canvas_p0.y + y),
                         IM_COL32(200, 200, 200, 40));
  }

  for (int n = 0; n < points_.Size; n += 2) {
    draw_list->AddRect(
        ImVec2(origin.x + points_[n].x, origin.y + points_[n].y),
        ImVec2(origin.x + points_[n + 1].x, origin.y + points_[n + 1].y),
        IM_COL32(255, 255, 255, 255), 1.0f);
  }

  draw_list->PopClipRect();
}

void Canvas::DrawBackground(ImVec2 canvas_size) {
  canvas_p0_ = ImGui::GetCursorScreenPos();
  if (!custom_canvas_size_) canvas_sz_ = ImGui::GetContentRegionAvail();
  if (canvas_size.x != 0) canvas_sz_ = canvas_size;

  canvas_p1_ = ImVec2(canvas_p0_.x + canvas_sz_.x, canvas_p0_.y + canvas_sz_.y);

  // Draw border and background color
  draw_list_ = ImGui::GetWindowDrawList();
  draw_list_->AddRectFilled(canvas_p0_, canvas_p1_, IM_COL32(32, 32, 32, 255));
  draw_list_->AddRect(canvas_p0_, canvas_p1_, IM_COL32(255, 255, 255, 255));
}

void Canvas::UpdateContext() {
  // This will catch our interactions
  const ImGuiIO &io = ImGui::GetIO();
  ImGui::InvisibleButton(
      "canvas", canvas_sz_,
      ImGuiButtonFlags_MouseButtonLeft | ImGuiButtonFlags_MouseButtonRight);
  const bool is_hovered = ImGui::IsItemHovered();  // Hovered
  const bool is_active = ImGui::IsItemActive();    // Held
  const ImVec2 origin(canvas_p0_.x + scrolling_.x,
                      canvas_p0_.y + scrolling_.y);  // Lock scrolled origin
  const ImVec2 mouse_pos_in_canvas(io.MousePos.x - origin.x,
                                   io.MousePos.y - origin.y);

  // Add first and second point
  if (is_hovered && !dragging_select_ &&
      ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
    points_.push_back(mouse_pos_in_canvas);
    points_.push_back(mouse_pos_in_canvas);
    dragging_select_ = true;
  }
  if (dragging_select_) {
    points_.back() = mouse_pos_in_canvas;
    if (!ImGui::IsMouseDown(ImGuiMouseButton_Left)) dragging_select_ = false;
  }

  // Pan (we use a zero mouse threshold when there's no context menu)
  const float mouse_threshold_for_pan = enable_context_menu_ ? -1.0f : 0.0f;
  if (is_active &&
      ImGui::IsMouseDragging(ImGuiMouseButton_Right, mouse_threshold_for_pan)) {
    scrolling_.x += io.MouseDelta.x;
    scrolling_.y += io.MouseDelta.y;
  }

  // Context menu (under default mouse threshold)
  ImVec2 drag_delta = ImGui::GetMouseDragDelta(ImGuiMouseButton_Right);
  if (enable_context_menu_ && drag_delta.x == 0.0f && drag_delta.y == 0.0f)
    ImGui::OpenPopupOnItemClick("context", ImGuiPopupFlags_MouseButtonRight);
  if (ImGui::BeginPopup("context")) {
    if (dragging_select_) points_.resize(points_.size() - 2);
    dragging_select_ = false;
    if (ImGui::MenuItem("Remove all", nullptr, false, points_.Size > 0)) {
      points_.clear();
    }
    ImGui::EndPopup();
  }
}

void Canvas::DrawGrid(float grid_step) {
  // Draw grid + all lines in the canvas
  draw_list_->PushClipRect(canvas_p0_, canvas_p1_, true);
  if (enable_grid_) {
    for (float x = fmodf(scrolling_.x, grid_step); x < canvas_sz_.x;
         x += grid_step)
      draw_list_->AddLine(ImVec2(canvas_p0_.x + x, canvas_p0_.y),
                          ImVec2(canvas_p0_.x + x, canvas_p1_.y),
                          IM_COL32(200, 200, 200, 40));
    for (float y = fmodf(scrolling_.y, grid_step); y < canvas_sz_.y;
         y += grid_step)
      draw_list_->AddLine(ImVec2(canvas_p0_.x, canvas_p0_.y + y),
                          ImVec2(canvas_p1_.x, canvas_p0_.y + y),
                          IM_COL32(200, 200, 200, 40));
  }
}

void Canvas::DrawOverlay() {
  const ImVec2 origin(canvas_p0_.x + scrolling_.x,
                      canvas_p0_.y + scrolling_.y);  // Lock scrolled origin
  for (int n = 0; n < points_.Size; n += 2) {
    draw_list_->AddRect(
        ImVec2(origin.x + points_[n].x, origin.y + points_[n].y),
        ImVec2(origin.x + points_[n + 1].x, origin.y + points_[n + 1].y),
        IM_COL32(255, 255, 255, 255), 1.0f);
  }

  draw_list_->PopClipRect();
}

}  // namespace gui
}  // namespace yaze