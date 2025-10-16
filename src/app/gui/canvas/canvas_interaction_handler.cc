#include "canvas_interaction_handler.h"

#include <algorithm>
#include <cmath>
#include "imgui/imgui.h"

namespace yaze {
namespace gui {

namespace {

// Helper function to align position to grid
ImVec2 AlignToGrid(ImVec2 pos, float grid_step) {
  return ImVec2(std::floor(pos.x / grid_step) * grid_step,
                std::floor(pos.y / grid_step) * grid_step);
}

}  // namespace

void CanvasInteractionHandler::Initialize(const std::string& canvas_id) {
  canvas_id_ = canvas_id;
  ClearState();
}

void CanvasInteractionHandler::ClearState() {
  hover_points_.clear();
  selected_points_.clear();
  selected_tiles_.clear();
  drawn_tile_pos_ = ImVec2(-1, -1);
  mouse_pos_in_canvas_ = ImVec2(0, 0);
  selected_tile_pos_ = ImVec2(-1, -1);
  rect_select_active_ = false;
}

TileInteractionResult CanvasInteractionHandler::Update(
    ImVec2 canvas_p0, ImVec2 scrolling, float /*global_scale*/, float /*tile_size*/,
    ImVec2 /*canvas_size*/, bool is_hovered) {
  
  TileInteractionResult result;
  
  if (!is_hovered) {
    hover_points_.clear();
    return result;
  }
  
  const ImGuiIO& imgui_io = ImGui::GetIO();
  const ImVec2 origin(canvas_p0.x + scrolling.x, canvas_p0.y + scrolling.y);
  mouse_pos_in_canvas_ = ImVec2(imgui_io.MousePos.x - origin.x, imgui_io.MousePos.y - origin.y);
  
  // Update based on current mode - each mode is handled by its specific Draw method
  // This method exists for future state updates if needed
  (void)current_mode_;  // Suppress unused warning
  
  return result;
}

bool CanvasInteractionHandler::DrawTilePainter(
    const gfx::Bitmap& bitmap, ImDrawList* draw_list, ImVec2 canvas_p0,
    ImVec2 scrolling, float global_scale, float tile_size, bool is_hovered) {
  
  const ImGuiIO& imgui_io = ImGui::GetIO();
  const ImVec2 origin(canvas_p0.x + scrolling.x, canvas_p0.y + scrolling.y);
  const ImVec2 mouse_pos(imgui_io.MousePos.x - origin.x, imgui_io.MousePos.y - origin.y);
  const auto scaled_size = tile_size * global_scale;

  // Clear hover when not hovering
  if (!is_hovered) {
    hover_points_.clear();
    return false;
  }

  // Reset previous hover
  hover_points_.clear();

  // Calculate grid-aligned paint position
  ImVec2 paint_pos = AlignToGrid(mouse_pos, scaled_size);
  mouse_pos_in_canvas_ = paint_pos;
  auto paint_pos_end = ImVec2(paint_pos.x + scaled_size, paint_pos.y + scaled_size);
  
  hover_points_.push_back(paint_pos);
  hover_points_.push_back(paint_pos_end);

  // Draw preview of tile at hover position
  if (bitmap.is_active() && draw_list) {
    draw_list->AddImage(
        (ImTextureID)(intptr_t)bitmap.texture(),
        ImVec2(origin.x + paint_pos.x, origin.y + paint_pos.y),
        ImVec2(origin.x + paint_pos.x + scaled_size, origin.y + paint_pos.y + scaled_size));
  }

  // Check for paint action
  if (ImGui::IsMouseClicked(ImGuiMouseButton_Left) &&
      ImGui::IsMouseDragging(ImGuiMouseButton_Left)) {
    drawn_tile_pos_ = paint_pos;
    return true;
  }

  return false;
}

bool CanvasInteractionHandler::DrawTilemapPainter(
    gfx::Tilemap& tilemap, int current_tile, ImDrawList* draw_list,
    ImVec2 canvas_p0, ImVec2 scrolling, float global_scale, bool is_hovered) {
  
  const ImGuiIO& imgui_io = ImGui::GetIO();
  const ImVec2 origin(canvas_p0.x + scrolling.x, canvas_p0.y + scrolling.y);
  const ImVec2 mouse_pos(imgui_io.MousePos.x - origin.x, imgui_io.MousePos.y - origin.y);
  
  // Safety check
  if (!tilemap.atlas.is_active() || tilemap.tile_size.x <= 0) {
    return false;
  }
  
  const auto scaled_size = tilemap.tile_size.x * global_scale;

  if (!is_hovered) {
    hover_points_.clear();
    return false;
  }

  hover_points_.clear();
  
  ImVec2 paint_pos = AlignToGrid(mouse_pos, scaled_size);
  mouse_pos_in_canvas_ = paint_pos;

  hover_points_.push_back(paint_pos);
  hover_points_.push_back(ImVec2(paint_pos.x + scaled_size, paint_pos.y + scaled_size));

  // Draw tile preview from atlas
  if (tilemap.atlas.is_active() && tilemap.atlas.texture() && draw_list) {
    int tiles_per_row = tilemap.atlas.width() / tilemap.tile_size.x;
    if (tiles_per_row > 0) {
      int tile_x = (current_tile % tiles_per_row) * tilemap.tile_size.x;
      int tile_y = (current_tile / tiles_per_row) * tilemap.tile_size.y;
      
      if (tile_x >= 0 && tile_x < tilemap.atlas.width() && 
          tile_y >= 0 && tile_y < tilemap.atlas.height()) {
        
        ImVec2 uv0 = ImVec2(static_cast<float>(tile_x) / tilemap.atlas.width(), 
                           static_cast<float>(tile_y) / tilemap.atlas.height());
        ImVec2 uv1 = ImVec2(static_cast<float>(tile_x + tilemap.tile_size.x) / tilemap.atlas.width(),
                           static_cast<float>(tile_y + tilemap.tile_size.y) / tilemap.atlas.height());
        
        draw_list->AddImage(
            (ImTextureID)(intptr_t)tilemap.atlas.texture(),
            ImVec2(origin.x + paint_pos.x, origin.y + paint_pos.y),
            ImVec2(origin.x + paint_pos.x + scaled_size, origin.y + paint_pos.y + scaled_size),
            uv0, uv1);
      }
    }
  }

  if (ImGui::IsMouseClicked(ImGuiMouseButton_Left) ||
      ImGui::IsMouseDragging(ImGuiMouseButton_Left)) {
    drawn_tile_pos_ = paint_pos;
    return true;
  }

  return false;
}

bool CanvasInteractionHandler::DrawSolidTilePainter(
    const ImVec4& color, ImDrawList* draw_list, ImVec2 canvas_p0,
    ImVec2 scrolling, float global_scale, float tile_size, bool is_hovered) {
  
  const ImGuiIO& imgui_io = ImGui::GetIO();
  const ImVec2 origin(canvas_p0.x + scrolling.x, canvas_p0.y + scrolling.y);
  const ImVec2 mouse_pos(imgui_io.MousePos.x - origin.x, imgui_io.MousePos.y - origin.y);
  auto scaled_tile_size = tile_size * global_scale;
  static bool is_dragging = false;
  static ImVec2 start_drag_pos;

  if (!is_hovered) {
    hover_points_.clear();
    return false;
  }

  hover_points_.clear();

  ImVec2 paint_pos = AlignToGrid(mouse_pos, scaled_tile_size);
  mouse_pos_in_canvas_ = paint_pos;

  // Clamp to canvas bounds (assuming canvas_size from Update)
  // For now, skip clamping as we don't have canvas_size here

  hover_points_.push_back(paint_pos);
  hover_points_.push_back(ImVec2(paint_pos.x + scaled_tile_size, paint_pos.y + scaled_tile_size));

  if (draw_list) {
    draw_list->AddRectFilled(
        ImVec2(origin.x + paint_pos.x + 1, origin.y + paint_pos.y + 1),
        ImVec2(origin.x + paint_pos.x + scaled_tile_size,
               origin.y + paint_pos.y + scaled_tile_size),
        IM_COL32(color.x * 255, color.y * 255, color.z * 255, 255));
  }

  if (ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
    is_dragging = true;
    start_drag_pos = paint_pos;
  }

  if (is_dragging && ImGui::IsMouseReleased(ImGuiMouseButton_Left)) {
    is_dragging = false;
    drawn_tile_pos_ = start_drag_pos;
    return true;
  }

  return false;
}

bool CanvasInteractionHandler::DrawTileSelector(
    ImDrawList* /*draw_list*/, ImVec2 canvas_p0, ImVec2 scrolling, float tile_size,
    bool is_hovered) {
  
  const ImGuiIO& imgui_io = ImGui::GetIO();
  const ImVec2 origin(canvas_p0.x + scrolling.x, canvas_p0.y + scrolling.y);
  const ImVec2 mouse_pos(imgui_io.MousePos.x - origin.x, imgui_io.MousePos.y - origin.y);

  if (is_hovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
    hover_points_.clear();
    ImVec2 painter_pos = AlignToGrid(mouse_pos, tile_size);

    hover_points_.push_back(painter_pos);
    hover_points_.push_back(ImVec2(painter_pos.x + tile_size, painter_pos.y + tile_size));
    mouse_pos_in_canvas_ = painter_pos;
  }

  if (is_hovered && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
    return true;
  }

  return false;
}

bool CanvasInteractionHandler::DrawSelectRect(
    int current_map, ImDrawList* draw_list, ImVec2 canvas_p0, ImVec2 scrolling,
    float global_scale, float tile_size, bool is_hovered) {
  
  const ImGuiIO& imgui_io = ImGui::GetIO();
  const ImVec2 origin(canvas_p0.x + scrolling.x, canvas_p0.y + scrolling.y);
  const ImVec2 mouse_pos(imgui_io.MousePos.x - origin.x, imgui_io.MousePos.y - origin.y);
  const float scaled_size = tile_size * global_scale;
  static ImVec2 drag_start_pos;
  static bool dragging = false;
  constexpr int small_map_size = 0x200;
  
  if (!is_hovered) {
    return false;
  }
  
  // Calculate superX and superY accounting for world offset
  int super_y = 0;
  int super_x = 0;
  if (current_map < 0x40) {
    super_y = current_map / 8;
    super_x = current_map % 8;
  } else if (current_map < 0x80) {
    super_y = (current_map - 0x40) / 8;
    super_x = (current_map - 0x40) % 8;
  } else {
    super_y = (current_map - 0x80) / 8;
    super_x = (current_map - 0x80) % 8;
  }

  // Handle right click for single tile selection
  if (ImGui::IsMouseClicked(ImGuiMouseButton_Right)) {
    ImVec2 painter_pos = AlignToGrid(mouse_pos, scaled_size);
    int painter_x = painter_pos.x;
    int painter_y = painter_pos.y;

    auto tile16_x = (painter_x % small_map_size) / (small_map_size / 0x20);
    auto tile16_y = (painter_y % small_map_size) / (small_map_size / 0x20);

    int index_x = super_x * 0x20 + tile16_x;
    int index_y = super_y * 0x20 + tile16_y;
    selected_tile_pos_ = ImVec2(index_x, index_y);
    selected_points_.clear();
    rect_select_active_ = false;

    drag_start_pos = AlignToGrid(mouse_pos, scaled_size);
  }

  // Draw rectangle while dragging
  ImVec2 drag_end_pos = AlignToGrid(mouse_pos, scaled_size);
  if (ImGui::IsMouseDragging(ImGuiMouseButton_Right) && draw_list) {
    auto start = ImVec2(canvas_p0.x + drag_start_pos.x,
                        canvas_p0.y + drag_start_pos.y);
    auto end = ImVec2(canvas_p0.x + drag_end_pos.x + tile_size,
                      canvas_p0.y + drag_end_pos.y + tile_size);
    draw_list->AddRect(start, end, IM_COL32(255, 255, 255, 255));
    dragging = true;
  }

  // Complete selection on release
  if (dragging && !ImGui::IsMouseDown(ImGuiMouseButton_Right)) {
    dragging = false;

    constexpr int tile16_size = 16;
    int start_x = std::floor(drag_start_pos.x / scaled_size) * tile16_size;
    int start_y = std::floor(drag_start_pos.y / scaled_size) * tile16_size;
    int end_x = std::floor(drag_end_pos.x / scaled_size) * tile16_size;
    int end_y = std::floor(drag_end_pos.y / scaled_size) * tile16_size;

    if (start_x > end_x) std::swap(start_x, end_x);
    if (start_y > end_y) std::swap(start_y, end_y);

    selected_tiles_.clear();
    selected_tiles_.reserve(((end_x - start_x) / tile16_size + 1) * 
                           ((end_y - start_y) / tile16_size + 1));
    
    constexpr int tiles_per_local_map = small_map_size / 16;

    for (int tile_y = start_y; tile_y <= end_y; tile_y += tile16_size) {
      for (int tile_x = start_x; tile_x <= end_x; tile_x += tile16_size) {
        int local_map_x = tile_x / small_map_size;
        int local_map_y = tile_y / small_map_size;

        int tile16_x = (tile_x % small_map_size) / tile16_size;
        int tile16_y = (tile_y % small_map_size) / tile16_size;

        int index_x = local_map_x * tiles_per_local_map + tile16_x;
        int index_y = local_map_y * tiles_per_local_map + tile16_y;

        selected_tiles_.emplace_back(index_x, index_y);
      }
    }
    
    selected_points_.clear();
    selected_points_.push_back(drag_start_pos);
    selected_points_.push_back(drag_end_pos);
    rect_select_active_ = true;
    return true;
  }

  return false;
}

// Helper methods - these are thin wrappers that could be static but kept as instance
// methods for potential future state access
ImVec2 CanvasInteractionHandler::AlignPosToGrid(ImVec2 pos, float grid_step) {
  return AlignToGrid(pos, grid_step);
}

ImVec2 CanvasInteractionHandler::GetMousePosition(ImVec2 canvas_p0, ImVec2 scrolling) {
  const ImGuiIO& imgui_io = ImGui::GetIO();
  const ImVec2 origin(canvas_p0.x + scrolling.x, canvas_p0.y + scrolling.y);
  return ImVec2(imgui_io.MousePos.x - origin.x, imgui_io.MousePos.y - origin.y);
}

bool CanvasInteractionHandler::IsMouseClicked(ImGuiMouseButton button) {
  return ImGui::IsMouseClicked(button);
}

bool CanvasInteractionHandler::IsMouseDoubleClicked(ImGuiMouseButton button) {
  return ImGui::IsMouseDoubleClicked(button);
}

bool CanvasInteractionHandler::IsMouseDragging(ImGuiMouseButton button) {
  return ImGui::IsMouseDragging(button);
}

bool CanvasInteractionHandler::IsMouseReleased(ImGuiMouseButton button) {
  return ImGui::IsMouseReleased(button);
}

}  // namespace gui
}  // namespace yaze
