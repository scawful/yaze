#include "canvas.h"

#include <imgui/imgui.h>

#include <cmath>
#include <string>

#include "app/editor/graphics/graphics_editor.h"
#include "app/gfx/bitmap.h"
#include "app/gui/input.h"
#include "app/gui/style.h"
#include "app/rom.h"

namespace yaze {
namespace app {
namespace gui {

using ImGui::BeginMenu;
using ImGui::BeginPopup;
using ImGui::EndMenu;
using ImGui::GetContentRegionAvail;
using ImGui::GetCursorScreenPos;
using ImGui::GetIO;
using ImGui::GetMouseDragDelta;
using ImGui::GetWindowDrawList;
using ImGui::IsItemActive;
using ImGui::IsItemHovered;
using ImGui::IsMouseClicked;
using ImGui::IsMouseDragging;
using ImGui::MenuItem;
using ImGui::OpenPopupOnItemClick;
using ImGui::Selectable;
using ImGui::Separator;
using ImGui::Text;

constexpr uint32_t kRectangleColor = IM_COL32(32, 32, 32, 255);
constexpr uint32_t kRectangleBorder = IM_COL32(255, 255, 255, 255);
constexpr ImGuiButtonFlags kMouseFlags =
    ImGuiButtonFlags_MouseButtonLeft | ImGuiButtonFlags_MouseButtonRight;

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

void Canvas::UpdateInfoGrid(ImVec2 bg_size, int tile_size, float scale,
                            float grid_size) {
  enable_custom_labels_ = true;
  DrawBackground(bg_size);
  DrawGrid(grid_size);
  DrawOverlay();
}

void Canvas::DrawBackground(ImVec2 canvas_size, bool can_drag) {
  draw_list_ = GetWindowDrawList();
  canvas_p0_ = GetCursorScreenPos();
  if (!custom_canvas_size_) canvas_sz_ = GetContentRegionAvail();
  if (canvas_size.x != 0) canvas_sz_ = canvas_size;
  canvas_p1_ = ImVec2(canvas_p0_.x + (canvas_sz_.x * global_scale_),
                      canvas_p0_.y + (canvas_sz_.y * global_scale_));

  // Draw border and background color
  draw_list_->AddRectFilled(canvas_p0_, canvas_p1_, kRectangleColor);
  draw_list_->AddRect(canvas_p0_, canvas_p1_, kRectangleBorder);

  ImGui::InvisibleButton(
      canvas_id_.c_str(),
      ImVec2(canvas_sz_.x * global_scale_, canvas_sz_.y * global_scale_),
      kMouseFlags);

  if (draggable_ && IsItemHovered()) {
    const ImGuiIO &io = GetIO();
    const bool is_active = IsItemActive();  // Held
    const ImVec2 origin(canvas_p0_.x + scrolling_.x,
                        canvas_p0_.y + scrolling_.y);  // Lock scrolled origin
    const ImVec2 mouse_pos(io.MousePos.x - origin.x, io.MousePos.y - origin.y);

    // Pan (we use a zero mouse threshold when there's no context menu)
    if (const float mouse_threshold_for_pan =
            enable_context_menu_ ? -1.0f : 0.0f;
        is_active &&
        IsMouseDragging(ImGuiMouseButton_Right, mouse_threshold_for_pan)) {
      scrolling_.x += io.MouseDelta.x;
      scrolling_.y += io.MouseDelta.y;
    }
  }
}

void Canvas::DrawContextMenu(gfx::Bitmap *bitmap) {
  const ImGuiIO &io = GetIO();
  auto scaled_sz =
      ImVec2(canvas_sz_.x * global_scale_, canvas_sz_.y * global_scale_);
  const ImVec2 origin(canvas_p0_.x + scrolling_.x,
                      canvas_p0_.y + scrolling_.y);  // Lock scrolled origin
  const ImVec2 mouse_pos(io.MousePos.x - origin.x, io.MousePos.y - origin.y);

  // Context menu (under default mouse threshold)
  if (ImVec2 drag_delta = GetMouseDragDelta(ImGuiMouseButton_Right);
      enable_context_menu_ && drag_delta.x == 0.0f && drag_delta.y == 0.0f)
    OpenPopupOnItemClick(context_id_.c_str(), ImGuiPopupFlags_MouseButtonRight);

  // Contents of the Context Menu
  if (ImGui::BeginPopup(context_id_.c_str())) {
    if (MenuItem("Reset Position", nullptr, false)) {
      scrolling_.x = 0;
      scrolling_.y = 0;
    }
    MenuItem("Show Grid", nullptr, &enable_grid_);
    Selectable("Show Position Labels", &enable_hex_tile_labels_);
    if (BeginMenu("Canvas Properties")) {
      Text("Canvas Size: %.0f x %.0f", canvas_sz_.x, canvas_sz_.y);
      Text("Global Scale: %.1f", global_scale_);
      Text("Mouse Position: %.0f x %.0f", mouse_pos.x, mouse_pos.y);
      EndMenu();
    }
    if (bitmap != nullptr) {
      if (BeginMenu("Bitmap Properties")) {
        Text("Size: %.0f x %.0f", scaled_sz.x, scaled_sz.y);
        Text("Pitch: %s",
             absl::StrFormat("%d", bitmap->surface()->pitch).c_str());
        Text("BitsPerPixel: %d", bitmap->surface()->format->BitsPerPixel);
        Text("BytesPerPixel: %d", bitmap->surface()->format->BytesPerPixel);
        EndMenu();
      }
      if (BeginMenu("Bitmap Format")) {
        if (MenuItem("Indexed")) {
          bitmap->Reformat(0);
          bitmap->ApplyPalette(bitmap->palette());
          rom()->UpdateBitmap(bitmap);
        }
        if (MenuItem("2BPP")) {
          bitmap->Reformat(1);
          bitmap->ApplyPalette(bitmap->palette());
          rom()->UpdateBitmap(bitmap);
        }
        if (MenuItem("4BPP")) {
          bitmap->Reformat(2);
          bitmap->ApplyPalette(bitmap->palette());
          rom()->UpdateBitmap(bitmap);
        }
        if (MenuItem("8BPP")) {
          bitmap->Reformat(3);
          bitmap->ApplyPalette(bitmap->palette());
          rom()->UpdateBitmap(bitmap);
        }
        EndMenu();
      }
      if (BeginMenu("Bitmap Palette")) {
        if (rom()->is_loaded()) {
          gui::TextWithSeparators("ROM Palette");
          ImGui::SetNextItemWidth(100.f);
          ImGui::Combo("Palette Group", (int *)&edit_palette_group_name_index_,
                       gfx::kPaletteGroupAddressesKeys,
                       IM_ARRAYSIZE(gfx::kPaletteGroupAddressesKeys));
          ImGui::SetNextItemWidth(100.f);
          gui::InputHexWord("Palette Group Index", &edit_palette_index_);

          auto palette_group = rom()->mutable_palette_group()->get_group(
              gfx::kPaletteGroupAddressesKeys[edit_palette_group_name_index_]);
          auto palette = palette_group->mutable_palette(edit_palette_index_);

          if (ImGui::BeginChild("Palette", ImVec2(0, 300), true)) {
            gui::SelectablePalettePipeline(edit_palette_sub_index_,
                                           refresh_graphics_, *palette);

            if (refresh_graphics_) {
              auto status = bitmap->ApplyPaletteWithTransparent(
                  *palette, edit_palette_sub_index_);
              rom()->UpdateBitmap(bitmap);
              refresh_graphics_ = false;
            }
            ImGui::EndChild();
          }
        }
        EndMenu();
      }
    }
    ImGui::Separator();
    if (BeginMenu("Grid Tile Size")) {
      if (MenuItem("8x8", nullptr, custom_step_ == 8.0f)) {
        custom_step_ = 8.0f;
      }
      if (MenuItem("16x16", nullptr, custom_step_ == 16.0f)) {
        custom_step_ = 16.0f;
      }
      if (MenuItem("32x32", nullptr, custom_step_ == 32.0f)) {
        custom_step_ = 32.0f;
      }
      if (MenuItem("64x64", nullptr, custom_step_ == 64.0f)) {
        custom_step_ = 64.0f;
      }
      EndMenu();
    }

    ImGui::EndPopup();
  }
}

bool Canvas::DrawTilePainter(const Bitmap &bitmap, int size, float scale) {
  const ImGuiIO &io = GetIO();
  const bool is_hovered = IsItemHovered();
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

    mouse_pos_in_canvas_ = painter_pos;

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

    if (IsMouseClicked(ImGuiMouseButton_Left)) {
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
  const ImGuiIO &io = GetIO();
  const bool is_hovered = IsItemHovered();
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

    points_.push_back(painter_pos);
    points_.push_back(ImVec2(painter_pos.x + scaled_tile_size,
                             painter_pos.y + scaled_tile_size));

    draw_list_->AddRectFilled(
        ImVec2(origin.x + painter_pos.x + 1, origin.y + painter_pos.y + 1),
        ImVec2(origin.x + painter_pos.x + scaled_tile_size,
               origin.y + painter_pos.y + scaled_tile_size),
        IM_COL32(color.x * 255, color.y * 255, color.z * 255, 255));

    if (IsMouseClicked(ImGuiMouseButton_Left)) {
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

void Canvas::DrawTileOnBitmap(int tile_size, gfx::Bitmap *bitmap,
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
          (start_position.y + y) * bitmap->width() + (start_position.x + x);

      // Write the color to the pixel
      bitmap->WriteColor(pixel_index, color);
    }
  }
}

bool Canvas::DrawTileSelector(int size) {
  const ImGuiIO &io = GetIO();
  const bool is_hovered = IsItemHovered();
  const ImVec2 origin(canvas_p0_.x + scrolling_.x, canvas_p0_.y + scrolling_.y);
  const ImVec2 mouse_pos(io.MousePos.x - origin.x, io.MousePos.y - origin.y);

  if (is_hovered && IsMouseClicked(ImGuiMouseButton_Left)) {
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

namespace {
ImVec2 AlignPosToGrid(ImVec2 pos, float scale) {
  return ImVec2(std::floor((double)pos.x / scale) * scale,
                std::floor((double)pos.y / scale) * scale);
}
}  // namespace

void Canvas::DrawSelectRect(int current_map, int tile_size, float scale) {
  const ImGuiIO &io = GetIO();
  const ImVec2 origin(canvas_p0_.x + scrolling_.x, canvas_p0_.y + scrolling_.y);
  const ImVec2 mouse_pos(io.MousePos.x - origin.x, io.MousePos.y - origin.y);
  static ImVec2 drag_start_pos;
  const float scaled_size = tile_size * scale;
  static bool dragging = false;
  constexpr int small_map_size = 0x200;
  int superY = current_map / 8;
  int superX = current_map % 8;

  // Handle right click for single tile selection
  if (IsMouseClicked(ImGuiMouseButton_Right)) {
    ImVec2 painter_pos = AlignPosToGrid(mouse_pos, scaled_size);
    int painter_x = painter_pos.x;
    int painter_y = painter_pos.y;

    auto tile16_x = (painter_x % small_map_size) / (small_map_size / 0x20);
    auto tile16_y = (painter_y % small_map_size) / (small_map_size / 0x20);

    int index_x = superX * 0x20 + tile16_x;
    int index_y = superY * 0x20 + tile16_y;
    selected_tile_pos_ = ImVec2(index_x, index_y);
    selected_points_.clear();
    select_rect_active_ = false;

    // Start drag position for rectangle selection
    drag_start_pos = AlignPosToGrid(mouse_pos, scaled_size);
  }

  // Calculate the rectangle's top-left and bottom-right corners
  ImVec2 drag_end_pos = AlignPosToGrid(mouse_pos, scaled_size);
  if (ImGui::IsMouseDragging(ImGuiMouseButton_Right)) {
    auto start = ImVec2(canvas_p0_.x + drag_start_pos.x,
                        canvas_p0_.y + drag_start_pos.y);
    auto end = ImVec2(canvas_p0_.x + drag_end_pos.x + tile_size,
                      canvas_p0_.y + drag_end_pos.y + tile_size);
    draw_list_->AddRect(start, end, kRectangleBorder);
    dragging = true;
  }

  if (dragging) {
    // Release dragging mode
    if (!ImGui::IsMouseDown(ImGuiMouseButton_Right)) {
      dragging = false;

      // Calculate the bounds of the rectangle in terms of 16x16 tile indices
      constexpr int tile16_size = 16;
      int start_x = std::floor(drag_start_pos.x / scaled_size) * tile16_size;
      int start_y = std::floor(drag_start_pos.y / scaled_size) * tile16_size;
      int end_x = std::floor(drag_end_pos.x / scaled_size) * tile16_size;
      int end_y = std::floor(drag_end_pos.y / scaled_size) * tile16_size;

      // Swap the start and end positions if they are in the wrong order
      if (start_x > end_x) std::swap(start_x, end_x);
      if (start_y > end_y) std::swap(start_y, end_y);

      selected_tiles_.clear();
      // Number of tiles per local map (since each tile is 16x16)
      constexpr int tiles_per_local_map = small_map_size / 16;

      // Loop through the tiles in the rectangle and store their positions
      for (int y = start_y; y <= end_y; y += tile16_size) {
        for (int x = start_x; x <= end_x; x += tile16_size) {
          // Determine which local map (512x512) the tile is in
          int local_map_x = x / small_map_size;
          int local_map_y = y / small_map_size;

          // Calculate the tile's position within its local map
          int tile16_x = (x % small_map_size) / tile16_size;
          int tile16_y = (y % small_map_size) / tile16_size;

          // Calculate the index within the overall map structure
          int index_x = local_map_x * tiles_per_local_map + tile16_x;
          int index_y = local_map_y * tiles_per_local_map + tile16_y;

          selected_tiles_.push_back(ImVec2(index_x, index_y));
        }
      }
      // Clear and add the calculated rectangle points
      selected_points_.clear();
      selected_points_.push_back(drag_start_pos);
      selected_points_.push_back(drag_end_pos);
      select_rect_active_ = true;
    }
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

void Canvas::DrawBitmap(const Bitmap &bitmap, int border_offset, float scale) {
  if (!bitmap.is_active()) {
    return;
  }
  draw_list_->AddImage((void *)bitmap.texture(),
                       ImVec2(canvas_p0_.x, canvas_p0_.y),
                       ImVec2(canvas_p0_.x + (bitmap.width() * scale),
                              canvas_p0_.y + (bitmap.height() * scale)));
  draw_list_->AddRect(canvas_p0_, canvas_p1_, kRectangleBorder);
}

void Canvas::DrawBitmap(const Bitmap &bitmap, int x_offset, int y_offset,
                        float scale, int alpha) {
  if (!bitmap.is_active()) {
    return;
  }
  draw_list_->AddImage(
      (void *)bitmap.texture(),
      ImVec2(canvas_p0_.x + x_offset + scrolling_.x,
             canvas_p0_.y + y_offset + scrolling_.y),
      ImVec2(
          canvas_p0_.x + x_offset + scrolling_.x + (bitmap.width() * scale),
          canvas_p0_.y + y_offset + scrolling_.y + (bitmap.height() * scale)),
      ImVec2(0, 0), ImVec2(1, 1), IM_COL32(255, 255, 255, alpha));
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

void Canvas::DrawBitmapGroup(std::vector<int> &group,
                             std::vector<gfx::Bitmap> &tile16_individual_,
                             int tile_size, float scale) {
  if (selected_points_.size() != 2) {
    // points_ should contain exactly two points
    return;
  }
  if (group.empty()) {
    // group should not be empty
    return;
  }

  // Top-left and bottom-right corners of the rectangle
  ImVec2 rect_top_left = selected_points_[0];
  ImVec2 rect_bottom_right = selected_points_[1];

  // Calculate the start and end tiles in the grid
  int start_tile_x =
      static_cast<int>(std::floor(rect_top_left.x / (tile_size * scale)));
  int start_tile_y =
      static_cast<int>(std::floor(rect_top_left.y / (tile_size * scale)));
  int end_tile_x =
      static_cast<int>(std::floor(rect_bottom_right.x / (tile_size * scale)));
  int end_tile_y =
      static_cast<int>(std::floor(rect_bottom_right.y / (tile_size * scale)));

  if (start_tile_x > end_tile_x) std::swap(start_tile_x, end_tile_x);
  if (start_tile_y > end_tile_y) std::swap(start_tile_y, end_tile_y);

  // Calculate the size of the rectangle in 16x16 grid form
  int rect_width = (end_tile_x - start_tile_x) * tile_size;
  int rect_height = (end_tile_y - start_tile_y) * tile_size;

  int tiles_per_row = rect_width / tile_size;
  int tiles_per_col = rect_height / tile_size;

  int i = 0;
  for (int y = 0; y < tiles_per_col + 1; ++y) {
    for (int x = 0; x < tiles_per_row + 1; ++x) {
      int tile_id = group[i];

      // Check if tile_id is within the range of tile16_individual_
      if (tile_id >= 0 && tile_id < tile16_individual_.size()) {
        // Calculate the position of the tile within the rectangle
        int tile_pos_x = (x + start_tile_x) * tile_size * scale;
        int tile_pos_y = (y + start_tile_y) * tile_size * scale;

        // Draw the tile bitmap at the calculated position
        DrawBitmap(tile16_individual_[tile_id], tile_pos_x, tile_pos_y, scale,
                   150.0f);
        i++;
      }
    }
  }

  const ImGuiIO &io = GetIO();
  const ImVec2 origin(canvas_p0_.x + scrolling_.x, canvas_p0_.y + scrolling_.y);
  const ImVec2 mouse_pos(io.MousePos.x - origin.x, io.MousePos.y - origin.y);
  auto new_start_pos = AlignPosToGrid(mouse_pos, tile_size * scale);
  selected_points_.clear();
  selected_points_.push_back(new_start_pos);
  selected_points_.push_back(
      ImVec2(new_start_pos.x + rect_width, new_start_pos.y + rect_height));
  select_rect_active_ = true;
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

  if (!selected_points_.empty()) {
    for (int n = 0; n < selected_points_.size(); n += 2) {
      draw_list_->AddRect(ImVec2(origin.x + selected_points_[n].x,
                                 origin.y + selected_points_[n].y),
                          ImVec2(origin.x + selected_points_[n + 1].x + 0x10,
                                 origin.y + selected_points_[n + 1].y + 0x10),
                          IM_COL32(255, 255, 255, 255), 1.0f);
    }
  }

  draw_list_->PopClipRect();
}

void Canvas::DrawLayeredElements() {
  // Based on ImGui demo, should be adapted to use for OAM
  ImDrawList *draw_list = ImGui::GetWindowDrawList();
  {
    Text("Blue shape is drawn first: appears in back");
    Text("Red shape is drawn after: appears in front");
    ImVec2 p0 = ImGui::GetCursorScreenPos();
    draw_list->AddRectFilled(ImVec2(p0.x, p0.y), ImVec2(p0.x + 50, p0.y + 50),
                             IM_COL32(0, 0, 255, 255));  // Blue
    draw_list->AddRectFilled(ImVec2(p0.x + 25, p0.y + 25),
                             ImVec2(p0.x + 75, p0.y + 75),
                             IM_COL32(255, 0, 0, 255));  // Red
    ImGui::Dummy(ImVec2(75, 75));
  }
  ImGui::Separator();
  {
    Text("Blue shape is drawn first, into channel 1: appears in front");
    Text("Red shape is drawn after, into channel 0: appears in back");
    ImVec2 p1 = ImGui::GetCursorScreenPos();

    // Create 2 channels and draw a Blue shape THEN a Red shape.
    // You can create any number of channels. Tables API use 1 channel per
    // column in order to better batch draw calls.
    draw_list->ChannelsSplit(2);
    draw_list->ChannelsSetCurrent(1);
    draw_list->AddRectFilled(ImVec2(p1.x, p1.y), ImVec2(p1.x + 50, p1.y + 50),
                             IM_COL32(0, 0, 255, 255));  // Blue
    draw_list->ChannelsSetCurrent(0);
    draw_list->AddRectFilled(ImVec2(p1.x + 25, p1.y + 25),
                             ImVec2(p1.x + 75, p1.y + 75),
                             IM_COL32(255, 0, 0, 255));  // Red

    // Flatten/reorder channels. Red shape is in channel 0 and it appears
    // below the Blue shape in channel 1. This works by copying draw indices
    // only (vertices are not copied).
    draw_list->ChannelsMerge();
    ImGui::Dummy(ImVec2(75, 75));
    Text("After reordering, contents of channel 0 appears below channel 1.");
  }
}

}  // namespace gui
}  // namespace app
}  // namespace yaze