#include "canvas.h"

#include <imgui/imgui.h>

#include <cmath>
#include <string>

#include "app/gfx/bitmap.h"
#include "app/rom.h"

namespace yaze {
namespace gui {

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

void Canvas::DrawContextMenu() {
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
  if (is_hovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
    ImVec2 draw_tile_outline_pos;
    draw_tile_outline_pos.x =
        std::round((double)mouse_pos_in_canvas.x / 32) * 32;
    draw_tile_outline_pos.y =
        std::round((double)mouse_pos_in_canvas.y / 32) * 32;

    points_.push_back(draw_tile_outline_pos);
    points_.push_back(
        ImVec2(draw_tile_outline_pos.x + 32, draw_tile_outline_pos.y + 32));
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
    if (ImGui::MenuItem("Reset Position", nullptr, false)) {
      scrolling_.x = 0;
      scrolling_.y = 0;
    }
    if (ImGui::MenuItem("Remove all", nullptr, false, points_.Size > 0)) {
      points_.clear();
    }
    ImGui::EndPopup();
  }
}

void Canvas::DrawTilesFromUser(app::ROM &rom, Bytes &tile,
                               app::gfx::SNESPalette &pal) {
  ImVec2 draw_tile_outline_pos;

  // Add rectangle
  if (is_hovered_ && ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
    draw_tile_outline_pos.x =
        std::round((double)mouse_pos_in_canvas_.x / 16) * 16;
    draw_tile_outline_pos.y =
        std::round((double)mouse_pos_in_canvas_.y / 16) * 16;

    points_.push_back(draw_tile_outline_pos);
    points_.push_back(
        ImVec2(draw_tile_outline_pos.x + 16, draw_tile_outline_pos.y + 16));

    changed_tiles_.emplace_back(app::gfx::Bitmap(16, 16, 64, tile.data()));
    changed_tiles_.back().ApplyPalette(pal);
    rom.RenderBitmap(&(changed_tiles_.back()));
  }
}

void Canvas::DrawBitmap(const Bitmap &bitmap, int border_offset, bool ready) {
  if (ready) {
    draw_list_->AddImage(
        (void *)bitmap.GetTexture(),
        ImVec2(canvas_p0_.x + border_offset, canvas_p0_.y + border_offset),
        ImVec2(canvas_p0_.x + (bitmap.GetWidth() * 2),
               canvas_p0_.y + (bitmap.GetHeight() * 2)));
  }
}

void Canvas::DrawBitmap(const Bitmap &bitmap, int x_offset, int y_offset) {
  draw_list_->AddImage(
      (void *)bitmap.GetTexture(),
      ImVec2(canvas_p0_.x + x_offset + scrolling_.x,
             canvas_p0_.y + y_offset + scrolling_.y),
      ImVec2(canvas_p0_.x + x_offset + scrolling_.x + (bitmap.GetWidth()),
             canvas_p0_.y + y_offset + scrolling_.y + (bitmap.GetHeight())));
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
}  // namespace yaze