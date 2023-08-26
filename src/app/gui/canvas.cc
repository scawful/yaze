#include "canvas.h"

#include <imgui/imgui.h>

#include <cmath>
#include <string>

#include "app/gfx/bitmap.h"
#include "app/rom.h"

namespace yaze {
namespace app {

namespace gui {

constexpr uint32_t kRectangleColor = IM_COL32(32, 32, 32, 255);
constexpr uint32_t kRectangleBorder = IM_COL32(255, 255, 255, 255);
constexpr ImGuiButtonFlags kMouseFlags =
    ImGuiButtonFlags_MouseButtonLeft | ImGuiButtonFlags_MouseButtonRight;

void Canvas::DrawBackground(ImVec2 canvas_size) {
  canvas_p0_ = ImGui::GetCursorScreenPos();
  if (!custom_canvas_size_) canvas_sz_ = ImGui::GetContentRegionAvail();
  if (canvas_size.x != 0) canvas_sz_ = canvas_size;
  canvas_p1_ = ImVec2(canvas_p0_.x + canvas_sz_.x, canvas_p0_.y + canvas_sz_.y);
  draw_list_ = ImGui::GetWindowDrawList();  // Draw border and background color
  draw_list_->AddRectFilled(canvas_p0_, canvas_p1_, kRectangleColor);
  draw_list_->AddRect(canvas_p0_, canvas_p1_, kRectangleBorder);
}

void Canvas::DrawContextMenu() {
  const ImGuiIO &io = ImGui::GetIO();
  ImGui::InvisibleButton("canvas", canvas_sz_, kMouseFlags);
  const bool is_active = ImGui::IsItemActive();  // Held
  const ImVec2 origin(canvas_p0_.x + scrolling_.x,
                      canvas_p0_.y + scrolling_.y);  // Lock scrolled origin
  const ImVec2 mouse_pos(io.MousePos.x - origin.x, io.MousePos.y - origin.y);

  // Pan (we use a zero mouse threshold when there's no context menu)
  if (const float mouse_threshold_for_pan = enable_context_menu_ ? -1.0f : 0.0f;
      is_active &&
      ImGui::IsMouseDragging(ImGuiMouseButton_Right, mouse_threshold_for_pan)) {
    scrolling_.x += io.MouseDelta.x;
    scrolling_.y += io.MouseDelta.y;
  }

  // Context menu (under default mouse threshold)
  if (ImVec2 drag_delta = ImGui::GetMouseDragDelta(ImGuiMouseButton_Right);
      enable_context_menu_ && drag_delta.x == 0.0f && drag_delta.y == 0.0f)
    ImGui::OpenPopupOnItemClick("context", ImGuiPopupFlags_MouseButtonRight);

  // Contents of the Context Menu
  if (ImGui::BeginPopup("context")) {
    ImGui::MenuItem("Show Grid", nullptr, &enable_grid_);
    if (ImGui::MenuItem("Reset Position", nullptr, false)) {
      scrolling_.x = 0;
      scrolling_.y = 0;
    }
    ImGui::EndPopup();
  }
}

bool Canvas::DrawTilePainter(const Bitmap &bitmap, int size) {
  const ImGuiIO &io = ImGui::GetIO();
  const bool is_hovered = ImGui::IsItemHovered();
  // Lock scrolled origin
  const ImVec2 origin(canvas_p0_.x + scrolling_.x, canvas_p0_.y + scrolling_.y);
  const ImVec2 mouse_pos(io.MousePos.x - origin.x, io.MousePos.y - origin.y);

  if (is_hovered) {
    // Reset the previous tile hover
    if (!points_.empty()) {
      points_.clear();
    }

    // Calculate the coordinates of the mouse
    ImVec2 painter_pos;
    painter_pos.x = std::floor((double)mouse_pos.x / size) * size;
    painter_pos.y = std::floor((double)mouse_pos.y / size) * size;

    auto painter_pos_end = ImVec2(painter_pos.x + size, painter_pos.y + size);
    points_.push_back(painter_pos);
    points_.push_back(painter_pos_end);

    if (bitmap.IsActive()) {
      draw_list_->AddImage(
          (void *)bitmap.texture(),
          ImVec2(origin.x + painter_pos.x, origin.y + painter_pos.y),
          ImVec2(origin.x + painter_pos.x + bitmap.width(),
                 origin.y + painter_pos.y + bitmap.height()));
    }

    if (ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
      // Draw the currently selected tile on the overworld here
      // Save the coordinates of the selected tile.
      drawn_tile_pos_ = mouse_pos;
      return true;
    }

  } else {
    // Erase the hover when the mouse is not in the canvas window.
    points_.clear();
  }
  return false;
}

void Canvas::DrawTileSelector(int size) {
  const ImGuiIO &io = ImGui::GetIO();
  const bool is_hovered = ImGui::IsItemHovered();  // Hovered
  const ImVec2 origin(canvas_p0_.x + scrolling_.x,
                      canvas_p0_.y + scrolling_.y);  // Lock scrolled origin
  const ImVec2 mouse_pos(io.MousePos.x - origin.x, io.MousePos.y - origin.y);

  if (is_hovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
    if (!points_.empty()) {
      points_.clear();
    }
    ImVec2 painter_pos;
    painter_pos.x = std::floor((double)mouse_pos.x / size) * size;
    painter_pos.y = std::floor((double)mouse_pos.y / size) * size;

    points_.push_back(painter_pos);
    points_.push_back(ImVec2(painter_pos.x + size, painter_pos.y + size));
  }
}

void Canvas::DrawBitmap(const Bitmap &bitmap, int border_offset, bool ready) {
  if (ready) {
    draw_list_->AddImage(
        (void *)bitmap.texture(),
        ImVec2(canvas_p0_.x + border_offset, canvas_p0_.y + border_offset),
        ImVec2(canvas_p0_.x + (bitmap.width() * 2),
               canvas_p0_.y + (bitmap.height() * 2)));
  }
}

void Canvas::DrawBitmap(const Bitmap &bitmap, int x_offset, int y_offset) {
  draw_list_->AddImage(
      (void *)bitmap.texture(),
      ImVec2(canvas_p0_.x + x_offset + scrolling_.x,
             canvas_p0_.y + y_offset + scrolling_.y),
      ImVec2(canvas_p0_.x + x_offset + scrolling_.x + (bitmap.width()),
             canvas_p0_.y + y_offset + scrolling_.y + (bitmap.height())));
}

// TODO: Add parameters for sizing and positioning
void Canvas::DrawBitmapTable(const BitmapTable &gfx_bin) {
  for (const auto &[key, value] : gfx_bin) {
    int offset = 0x40 * (key + 1);
    int top_left_y = canvas_p0_.y + 2;
    if (key >= 1) {
      top_left_y = canvas_p0_.y + 0x40 * key;
    }
    draw_list_->AddImage((void *)value.texture(),
                         ImVec2(canvas_p0_.x + 2, top_left_y),
                         ImVec2(canvas_p0_.x + 0x100, canvas_p0_.y + offset));
  }
}

void Canvas::DrawOutline(int x, int y, int w, int h) {
  ImVec2 origin(canvas_p0_.x + scrolling_.x + x,
                canvas_p0_.y + scrolling_.y + y);
  ImVec2 size(canvas_p0_.x + scrolling_.x + x + w,
              canvas_p0_.y + scrolling_.y + y + h);
  draw_list_->AddRect(origin, size, IM_COL32(255, 255, 255, 255));
}

void Canvas::DrawRect(int x, int y, int w, int h, ImVec4 color) {
  ImVec2 origin(canvas_p0_.x + scrolling_.x + x,
                canvas_p0_.y + scrolling_.y + y);
  ImVec2 size(canvas_p0_.x + scrolling_.x + x + w,
              canvas_p0_.y + scrolling_.y + y + h);
  draw_list_->AddRectFilled(origin, size,
                            IM_COL32(color.x, color.y, color.z, color.w));
}

void Canvas::DrawText(std::string text, int x, int y) {
  draw_list_->AddText(
      ImVec2(canvas_p0_.x + scrolling_.x + x, canvas_p0_.y + scrolling_.y + y),
      IM_COL32(255, 255, 255, 255), text.data());
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
}  // namespace app
}  // namespace yaze