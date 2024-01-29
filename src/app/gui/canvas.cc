#include "canvas.h"

#include <imgui/imgui.h>

#include <cmath>
#include <string>

#include "app/editor/graphics_editor.h"
#include "app/gfx/bitmap.h"
#include "app/rom.h"

namespace yaze {
namespace app {
namespace gui {

constexpr uint32_t kRectangleColor = IM_COL32(32, 32, 32, 255);
constexpr uint32_t kRectangleBorder = IM_COL32(255, 255, 255, 255);
constexpr ImGuiButtonFlags kMouseFlags =
    ImGuiButtonFlags_MouseButtonLeft | ImGuiButtonFlags_MouseButtonRight;

void Canvas::Update(const gfx::Bitmap &bitmap, ImVec2 bg_size, int tile_size,
                    float scale, float grid_size) {
  if (scale != 1.0f) {
    bg_size.x *= scale / 2;
    bg_size.y *= scale / 2;
  }
  DrawBackground(bg_size);
  DrawContextMenu();
  DrawTileSelector(tile_size);
  DrawBitmap(bitmap, 2, scale);
  DrawGrid(grid_size);
  DrawOverlay();
}

void Canvas::UpdateColorPainter(gfx::Bitmap &bitmap, const ImVec4 &color,
                                const std::function<void()> &event,
                                int tile_size, float scale) {
  global_scale_ = scale;
  DrawBackground();
  DrawContextMenu();
  DrawBitmap(bitmap, 2, scale);
  if (DrawSolidTilePainter(color, tile_size)) {
    event();
  }
  DrawGrid();
  DrawOverlay();
}

void Canvas::UpdateEvent(const std::function<void()> &event, ImVec2 bg_size,
                         int tile_size, float scale, float grid_size) {
  DrawBackground(bg_size);
  DrawContextMenu();
  event();
  DrawGrid(grid_size);
  DrawOverlay();
}

void Canvas::UpdateInfoGrid(ImVec2 bg_size, int tile_size, float scale,
                            float grid_size) {
  enable_custom_labels_ = true;
  DrawBackground(bg_size);
  DrawGrid(grid_size);
  DrawOverlay();
}

void Canvas::DrawBackground(ImVec2 canvas_size, bool can_drag) {
  canvas_p0_ = ImGui::GetCursorScreenPos();
  if (!custom_canvas_size_) canvas_sz_ = ImGui::GetContentRegionAvail();
  if (canvas_size.x != 0) canvas_sz_ = canvas_size;
  canvas_p1_ = ImVec2(canvas_p0_.x + (canvas_sz_.x * global_scale_),
                      canvas_p0_.y + (canvas_sz_.y * global_scale_));
  draw_list_ = ImGui::GetWindowDrawList();  // Draw border and background color
  draw_list_->AddRectFilled(canvas_p0_, canvas_p1_, kRectangleColor);
  draw_list_->AddRect(canvas_p0_, canvas_p1_, kRectangleBorder);

  const ImGuiIO &io = ImGui::GetIO();
  auto scaled_sz =
      ImVec2(canvas_sz_.x * global_scale_, canvas_sz_.y * global_scale_);
  ImGui::InvisibleButton("canvas", scaled_sz, kMouseFlags);

  if (draggable_ && ImGui::IsItemHovered()) {
    const bool is_active = ImGui::IsItemActive();  // Held
    const ImVec2 origin(canvas_p0_.x + scrolling_.x,
                        canvas_p0_.y + scrolling_.y);  // Lock scrolled origin
    const ImVec2 mouse_pos(io.MousePos.x - origin.x, io.MousePos.y - origin.y);

    // Pan (we use a zero mouse threshold when there's no context menu)
    if (const float mouse_threshold_for_pan =
            enable_context_menu_ ? -1.0f : 0.0f;
        is_active && ImGui::IsMouseDragging(ImGuiMouseButton_Right,
                                            mouse_threshold_for_pan)) {
      scrolling_.x += io.MouseDelta.x;
      scrolling_.y += io.MouseDelta.y;
    }
  }
}

void Canvas::DrawContextMenu() {
  const ImGuiIO &io = ImGui::GetIO();
  auto scaled_sz =
      ImVec2(canvas_sz_.x * global_scale_, canvas_sz_.y * global_scale_);
  const ImVec2 origin(canvas_p0_.x + scrolling_.x,
                      canvas_p0_.y + scrolling_.y);  // Lock scrolled origin
  const ImVec2 mouse_pos(io.MousePos.x - origin.x, io.MousePos.y - origin.y);

  // Context menu (under default mouse threshold)
  if (ImVec2 drag_delta = ImGui::GetMouseDragDelta(ImGuiMouseButton_Right);
      enable_context_menu_ && drag_delta.x == 0.0f && drag_delta.y == 0.0f)
    ImGui::OpenPopupOnItemClick("context", ImGuiPopupFlags_MouseButtonRight);

  // Contents of the Context Menu
  if (ImGui::BeginPopup("context")) {
    if (ImGui::MenuItem("Reset Position", nullptr, false)) {
      scrolling_.x = 0;
      scrolling_.y = 0;
    }
    ImGui::MenuItem("Show Grid", nullptr, &enable_grid_);
    ImGui::Selectable("Show Labels", &enable_hex_tile_labels_);
    if (ImGui::BeginMenu("Canvas Properties")) {
      ImGui::Text("Canvas Size: %.0f x %.0f", canvas_sz_.x, canvas_sz_.y);
      ImGui::Text("Global Scale: %.1f", global_scale_);
      ImGui::Text("Mouse Position: %.0f x %.0f", mouse_pos.x, mouse_pos.y);
      ImGui::EndMenu();
    }
    if (ImGui::BeginMenu("Bitmap Properties")) {
      ImGui::Text("Bitmap Size: %.0f x %.0f", scaled_sz.x, scaled_sz.y);
      ImGui::Text("Bitmap Position: %.0f x %.0f", origin.x, origin.y);
      ImGui::EndMenu();
    }
    ImGui::Separator();
    if (ImGui::BeginMenu("Grid Tile Size")) {
      if (ImGui::MenuItem("8x8", nullptr, custom_step_ == 8.0f)) {
        custom_step_ = 8.0f;
      }
      if (ImGui::MenuItem("16x16", nullptr, custom_step_ == 16.0f)) {
        custom_step_ = 16.0f;
      }
      if (ImGui::MenuItem("32x32", nullptr, custom_step_ == 32.0f)) {
        custom_step_ = 32.0f;
      }
      if (ImGui::MenuItem("64x64", nullptr, custom_step_ == 64.0f)) {
        custom_step_ = 64.0f;
      }
      ImGui::EndMenu();
    }
    // TODO: Add a menu item for selecting the palette

    ImGui::EndPopup();
  }
}

bool Canvas::DrawTilePainter(const Bitmap &bitmap, int size, float scale) {
  const ImGuiIO &io = ImGui::GetIO();
  const bool is_hovered = ImGui::IsItemHovered();
  is_hovered_ = is_hovered;
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
    painter_pos.x =
        std::floor((double)mouse_pos.x / (size * scale)) * (size * scale);
    painter_pos.y =
        std::floor((double)mouse_pos.y / (size * scale)) * (size * scale);

    auto painter_pos_end =
        ImVec2(painter_pos.x + (size * scale), painter_pos.y + (size * scale));
    points_.push_back(painter_pos);
    points_.push_back(painter_pos_end);

    if (bitmap.is_active()) {
      draw_list_->AddImage(
          (void *)bitmap.texture(),
          ImVec2(origin.x + painter_pos.x, origin.y + painter_pos.y),
          ImVec2(origin.x + painter_pos.x + (size)*scale,
                 origin.y + painter_pos.y + size * scale));
    }

    if (ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
      // Draw the currently selected tile on the overworld here
      // Save the coordinates of the selected tile.
      drawn_tile_pos_ = painter_pos;
      return true;
    } else if (ImGui::IsMouseDragging(ImGuiMouseButton_Left)) {
      // Draw the currently selected tile on the overworld here
      // Save the coordinates of the selected tile.
      drawn_tile_pos_ = painter_pos;
      return true;
    }
  } else {
    // Erase the hover when the mouse is not in the canvas window.
    points_.clear();
  }
  return false;
}

bool Canvas::DrawSolidTilePainter(const ImVec4 &color, int tile_size) {
  const ImGuiIO &io = ImGui::GetIO();
  const bool is_hovered = ImGui::IsItemHovered();
  is_hovered_ = is_hovered;
  // Lock scrolled origin
  const ImVec2 origin(canvas_p0_.x + scrolling_.x, canvas_p0_.y + scrolling_.y);
  const ImVec2 mouse_pos(io.MousePos.x - origin.x, io.MousePos.y - origin.y);
  auto scaled_tile_size = tile_size * global_scale_;

  static bool is_dragging = false;
  static ImVec2 start_drag_pos;

  if (is_hovered) {
    // Reset the previous tile hover
    if (!points_.empty()) {
      points_.clear();
    }

    // Calculate the coordinates of the mouse
    ImVec2 painter_pos;
    painter_pos.x =
        std::floor((double)mouse_pos.x / scaled_tile_size) * scaled_tile_size;
    painter_pos.y =
        std::floor((double)mouse_pos.y / scaled_tile_size) * scaled_tile_size;

    // Clamp the size to a grid
    painter_pos.x =
        std::clamp(painter_pos.x, 0.0f, canvas_sz_.x * global_scale_);
    painter_pos.y =
        std::clamp(painter_pos.y, 0.0f, canvas_sz_.y * global_scale_);

    auto painter_pos_end = ImVec2(painter_pos.x + scaled_tile_size,
                                  painter_pos.y + scaled_tile_size);
    points_.push_back(painter_pos);
    points_.push_back(painter_pos_end);

    draw_list_->AddRectFilled(
        ImVec2(origin.x + painter_pos.x + 1, origin.y + painter_pos.y + 1),
        ImVec2(origin.x + painter_pos.x + scaled_tile_size,
               origin.y + painter_pos.y + scaled_tile_size),
        IM_COL32(color.x * 255, color.y * 255, color.z * 255, 255));

    if (ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
      is_dragging = true;
      start_drag_pos = painter_pos;
    }

    if (is_dragging && ImGui::IsMouseReleased(ImGuiMouseButton_Left)) {
      is_dragging = false;
      drawn_tile_pos_ = start_drag_pos;
      return true;
    }

  } else {
    // Erase the hover when the mouse is not in the canvas window.
    points_.clear();
  }
  return false;
}

void Canvas::DrawTileOnBitmap(int tile_size, gfx::Bitmap &bitmap,
                              ImVec4 color) {
  const ImVec2 position = drawn_tile_pos_;
  int tile_index_x = static_cast<int>(position.x / global_scale_) / tile_size;
  int tile_index_y = static_cast<int>(position.y / global_scale_) / tile_size;

  ImVec2 start_position(tile_index_x * tile_size, tile_index_y * tile_size);

  // Update the bitmap's pixel data based on the start_position and color
  for (int y = 0; y < tile_size; ++y) {
    for (int x = 0; x < tile_size; ++x) {
      // Calculate the actual pixel index in the bitmap
      int pixel_index =
          (start_position.y + y) * bitmap.width() + (start_position.x + x);

      // Write the color to the pixel
      bitmap.WriteColor(pixel_index, color);
    }
  }
}

bool Canvas::DrawTileSelector(int size) {
  const ImGuiIO &io = ImGui::GetIO();
  const bool is_hovered = ImGui::IsItemHovered();
  const ImVec2 origin(canvas_p0_.x + scrolling_.x, canvas_p0_.y + scrolling_.y);
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
    mouse_pos_in_canvas_ = painter_pos;
  }

  if (is_hovered && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
    return true;
  }

  return false;
}

bool Canvas::HandleTileEdits(Canvas &blockset_canvas,
                             std::vector<gfx::Bitmap> &source_blockset,
                             int &current_tile, float scale,
                             int tile_painter_size, int tiles_per_row) {
  if (!blockset_canvas.points().empty()) {
    uint16_t x = blockset_canvas.points().front().x / 32;
    uint16_t y = blockset_canvas.points().front().y / 32;
    current_tile = x + (y * tiles_per_row);
    if (DrawTilePainter(source_blockset[current_tile], tile_painter_size,
                        scale)) {
      return true;
    }
  }
  return false;
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

void Canvas::DrawBitmap(const Bitmap &bitmap, int border_offset, float scale) {
  draw_list_->AddImage((void *)bitmap.texture(),
                       ImVec2(canvas_p0_.x, canvas_p0_.y),
                       ImVec2(canvas_p0_.x + (bitmap.width() * scale),
                              canvas_p0_.y + (bitmap.height() * scale)));
  draw_list_->AddRect(canvas_p0_, canvas_p1_, kRectangleBorder);
}

void Canvas::DrawBitmap(const Bitmap &bitmap, int x_offset, int y_offset,
                        float scale) {
  draw_list_->AddImage(
      (void *)bitmap.texture(),
      ImVec2(canvas_p0_.x + x_offset + scrolling_.x,
             canvas_p0_.y + y_offset + scrolling_.y),
      ImVec2(
          canvas_p0_.x + x_offset + scrolling_.x + (bitmap.width() * scale),
          canvas_p0_.y + y_offset + scrolling_.y + (bitmap.height() * scale)));
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
  draw_list_->AddRect(origin, size, IM_COL32(255, 255, 255, 200), 0, 0, 1.5f);
}

void Canvas::DrawOutlineWithColor(int x, int y, int w, int h, ImVec4 color) {
  ImVec2 origin(canvas_p0_.x + scrolling_.x + x,
                canvas_p0_.y + scrolling_.y + y);
  ImVec2 size(canvas_p0_.x + scrolling_.x + x + w,
              canvas_p0_.y + scrolling_.y + y + h);
  draw_list_->AddRect(origin, size,
                      IM_COL32(color.x, color.y, color.z, color.w));
}


void Canvas::DrawOutlineWithColor(int x, int y, int w, int h, uint32_t color) {
  ImVec2 origin(canvas_p0_.x + scrolling_.x + x,
                canvas_p0_.y + scrolling_.y + y);
  ImVec2 size(canvas_p0_.x + scrolling_.x + x + w,
              canvas_p0_.y + scrolling_.y + y + h);
  draw_list_->AddRect(origin, size, color);
}

namespace {
std::vector<int> GetTileIDsInGrid(int start_x, int start_y, int width,
                                  int height, int tile_size) {
  std::vector<int> tile_ids;

  int num_tiles_x = width / tile_size;
  int num_tiles_y = height / tile_size;

  for (int y = 0; y < num_tiles_y; y++) {
    for (int x = 0; x < num_tiles_x; x++) {
      int tile_id = (start_y / tile_size + y) * (width / tile_size) +
                    (start_x / tile_size + x);
      tile_ids.push_back(tile_id);
    }
  }

  return tile_ids;
}
}  // namespace

void Canvas::DrawSelectRect(int tile_size, float scale) {
  const ImGuiIO &io = ImGui::GetIO();
  static ImVec2 drag_start_pos;
  static bool dragging = false;

  if (ImGui::IsMouseClicked(ImGuiMouseButton_Right)) {
    // Start dragging and snap the start position to the nearest grid point
    drag_start_pos.x =
        std::floor(io.MousePos.x / (tile_size * scale)) * tile_size * scale;
    drag_start_pos.y =
        std::floor(io.MousePos.y / (tile_size * scale)) * tile_size * scale;
    dragging = true;
  }

  ImVec2 current_pos = io.MousePos;
  // Snap current position to the nearest grid point
  ImVec2 snapped_current_pos;
  snapped_current_pos.x =
      std::floor(current_pos.x / (tile_size * scale)) * tile_size * scale;
  snapped_current_pos.y =
      std::floor(current_pos.y / (tile_size * scale)) * tile_size * scale;
  // Calculate rect_min and rect_max considering the drag direction
  ImVec2 rect_min = ImVec2(std::min(drag_start_pos.x, snapped_current_pos.x),
                           std::min(drag_start_pos.y, snapped_current_pos.y));
  ImVec2 rect_max = ImVec2(
      std::max(drag_start_pos.x, snapped_current_pos.x) + tile_size * scale,
      std::max(drag_start_pos.y, snapped_current_pos.y) + tile_size * scale);

  if (dragging && ImGui::IsMouseDragging(ImGuiMouseButton_Right)) {
    draw_list_->AddRect(rect_min, rect_max, kRectangleBorder);
  }

  if (dragging && !ImGui::IsMouseDown(ImGuiMouseButton_Right)) {
    dragging = false;
    ImVec2 scaled_rect_min =
        ImVec2(drag_start_pos.x / scale, drag_start_pos.y / scale);
    ImVec2 scaled_rect_max = ImVec2(rect_max.x / scale, rect_max.y / scale);

    // Here, calculate and store the tile16 IDs within the rectangle
    selected_tiles_ =
        GetTileIDsInGrid(scaled_rect_min.x, scaled_rect_min.y,
                         scaled_rect_max.x - scaled_rect_min.x,
                         scaled_rect_max.y - scaled_rect_min.y, tile_size);

    // Clear and add the calculated rectangle points
    points_.clear();
    points_.push_back(scaled_rect_min);
    points_.push_back(scaled_rect_max);
  }
}

void Canvas::DrawBitmapGroup(std::vector<int> &group,
                             std::vector<gfx::Bitmap> &tile16_individual_,
                             int tile_size, float scale) {
  if (points_.size() != 2) {
    // Handle error: points_ should contain exactly two points
    return;
  }

  // Top-left and bottom-right corners of the rectangle
  ImVec2 rect_top_left = points_[0];
  ImVec2 rect_bottom_right = points_[1];

  // Calculate the start and end tiles in the grid
  int start_tile_x = static_cast<int>(rect_top_left.x / (tile_size * scale));
  int start_tile_y = static_cast<int>(rect_top_left.y / (tile_size * scale));
  int end_tile_x = static_cast<int>(rect_bottom_right.x / (tile_size * scale));
  int end_tile_y = static_cast<int>(rect_bottom_right.y / (tile_size * scale));

  // Calculate the size of the rectangle in 16x16 grid form
  int rect_width = (end_tile_x - start_tile_x + 1) * tile_size;
  int rect_height = (end_tile_y - start_tile_y + 1) * tile_size;

  const int tiles_per_row = rect_width / tile_size;

  for (int i = 0; i < group.size(); ++i) {
    int tile_id = group[i];

    // Check if tile_id is within the range of tile16_individual_
    if (tile_id >= 0 && tile_id < tile16_individual_.size()) {
      int x = i % tiles_per_row;
      int y = i / tiles_per_row;

      // Calculate the position of the tile within the rectangle
      int tile_pos_x = x * tile_size * scale + start_tile_x * tile_size * scale;
      int tile_pos_y = y * tile_size * scale + start_tile_y * tile_size * scale;

      // Draw the tile bitmap at the calculated position
      DrawBitmap(tile16_individual_[tile_id], tile_pos_x, tile_pos_y, scale);
    }
  }
}

void Canvas::DrawRect(int x, int y, int w, int h, ImVec4 color) {
  ImVec2 origin(canvas_p0_.x + scrolling_.x + x,
                canvas_p0_.y + scrolling_.y + y);
  ImVec2 size(canvas_p0_.x + scrolling_.x + x + w,
              canvas_p0_.y + scrolling_.y + y + h);
  draw_list_->AddRectFilled(origin, size,
                            IM_COL32(color.x, color.y, color.z, color.w));
  // Add a black outline
  ImVec2 outline_origin(origin.x - 1, origin.y - 1);
  ImVec2 outline_size(size.x + 1, size.y + 1);
  draw_list_->AddRect(outline_origin, outline_size, IM_COL32(0, 0, 0, 255));
}

void Canvas::DrawText(std::string text, int x, int y) {
  draw_list_->AddText(ImVec2(canvas_p0_.x + scrolling_.x + x + 1,
                             canvas_p0_.y + scrolling_.y + y + 1),
                      IM_COL32(0, 0, 0, 255), text.data());
  draw_list_->AddText(
      ImVec2(canvas_p0_.x + scrolling_.x + x, canvas_p0_.y + scrolling_.y + y),
      IM_COL32(255, 255, 255, 255), text.data());
}

void Canvas::DrawGridLines(float grid_step) {
  for (float x = fmodf(scrolling_.x, grid_step);
       x < canvas_sz_.x * global_scale_; x += grid_step)
    draw_list_->AddLine(ImVec2(canvas_p0_.x + x, canvas_p0_.y),
                        ImVec2(canvas_p0_.x + x, canvas_p1_.y),
                        IM_COL32(200, 200, 200, 50), 0.5f);
  for (float y = fmodf(scrolling_.y, grid_step);
       y < canvas_sz_.y * global_scale_; y += grid_step)
    draw_list_->AddLine(ImVec2(canvas_p0_.x, canvas_p0_.y + y),
                        ImVec2(canvas_p1_.x, canvas_p0_.y + y),
                        IM_COL32(200, 200, 200, 50), 0.5f);
}

void Canvas::DrawGrid(float grid_step, int tile_id_offset) {
  // Draw grid + all lines in the canvas
  draw_list_->PushClipRect(canvas_p0_, canvas_p1_, true);
  if (enable_grid_) {
    if (custom_step_ != 0.f) grid_step = custom_step_;
    grid_step *= global_scale_;  // Apply global scale to grid step

    DrawGridLines(grid_step);

    if (highlight_tile_id != -1) {
      int tile_x = highlight_tile_id % 8;
      int tile_y = highlight_tile_id / 8;
      ImVec2 tile_pos(canvas_p0_.x + scrolling_.x + tile_x * grid_step,
                      canvas_p0_.y + scrolling_.y + tile_y * grid_step);
      ImVec2 tile_pos_end(tile_pos.x + grid_step, tile_pos.y + grid_step);

      draw_list_->AddRectFilled(tile_pos, tile_pos_end,
                                IM_COL32(255, 0, 255, 255));
    }

    if (enable_hex_tile_labels_) {
      // Draw the hex ID of the tile in the center of the tile square
      for (float x = fmodf(scrolling_.x, grid_step);
           x < canvas_sz_.x * global_scale_; x += grid_step) {
        for (float y = fmodf(scrolling_.y, grid_step);
             y < canvas_sz_.y * global_scale_; y += grid_step) {
          int tile_x = (x - scrolling_.x) / grid_step;
          int tile_y = (y - scrolling_.y) / grid_step;
          int tile_id = tile_x + (tile_y * 16);
          std::string hex_id = absl::StrFormat("%02X", tile_id);
          draw_list_->AddText(ImVec2(canvas_p0_.x + x + (grid_step / 2) - 4,
                                     canvas_p0_.y + y + (grid_step / 2) - 4),
                              IM_COL32(255, 255, 255, 255), hex_id.data());
        }
      }
    }

    if (enable_custom_labels_) {
      // Draw the contents of labels on the grid
      for (float x = fmodf(scrolling_.x, grid_step);
           x < canvas_sz_.x * global_scale_; x += grid_step) {
        for (float y = fmodf(scrolling_.y, grid_step);
             y < canvas_sz_.y * global_scale_; y += grid_step) {
          int tile_x = (x - scrolling_.x) / grid_step;
          int tile_y = (y - scrolling_.y) / grid_step;
          int tile_id = tile_x + (tile_y * tile_id_offset);

          if (tile_id >= labels_[current_labels_].size()) {
            break;
          }
          std::string label = labels_[current_labels_][tile_id];
          draw_list_->AddText(
              ImVec2(canvas_p0_.x + x + (grid_step / 2) - tile_id_offset,
                     canvas_p0_.y + y + (grid_step / 2) - tile_id_offset),
              IM_COL32(255, 255, 255, 255), label.data());
        }
      }
    }
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