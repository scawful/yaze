#include "canvas_interaction_handler.h"

#include <cmath>
#include "app/gui/canvas/canvas_interaction.h"
#include "imgui/imgui.h"

namespace yaze {
namespace gui {

namespace {

// Helper function to align position to grid (local version)
ImVec2 AlignToGridLocal(ImVec2 pos, float grid_step) {
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
  ImVec2 paint_pos = AlignToGridLocal(mouse_pos, scaled_size);
  mouse_pos_in_canvas_ = paint_pos;
  auto paint_pos_end = ImVec2(paint_pos.x + scaled_size, paint_pos.y + scaled_size);
  
  hover_points_.push_back(paint_pos);
  hover_points_.push_back(paint_pos_end);

  // Draw preview of tile at hover position
  if (bitmap.is_active() && draw_list) {
    draw_list->AddImage(
        reinterpret_cast<ImTextureID>(bitmap.texture()),
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
            reinterpret_cast<ImTextureID>(tilemap.atlas.texture()),
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
    ImVec2 painter_pos = AlignToGridLocal(mouse_pos, tile_size);

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
  
  if (!is_hovered) {
    return false;
  }
  
  // Create CanvasGeometry from parameters
  CanvasGeometry geometry;
  geometry.canvas_p0 = canvas_p0;
  geometry.scrolling = scrolling;
  geometry.scaled_size = ImVec2(tile_size * global_scale, tile_size * global_scale);
  geometry.canvas_sz = ImVec2(tile_size, tile_size);  // Will be updated if needed
  
  // Call new event-based function
  RectSelectionEvent event = HandleRectangleSelection(
      geometry, current_map, tile_size, draw_list, ImGuiMouseButton_Right);
  
  // Update internal state for backward compatibility
  if (event.is_complete) {
    selected_tiles_ = event.selected_tiles;
    selected_points_.clear();
    selected_points_.push_back(event.start_pos);
    selected_points_.push_back(event.end_pos);
    rect_select_active_ = true;
    return true;
  }
  
  if (!event.selected_tiles.empty() && !event.is_complete) {
    // Single tile selection
    selected_tile_pos_ = event.selected_tiles[0];
    selected_points_.clear();
    rect_select_active_ = false;
  }
  
  rect_select_active_ = event.is_active;
  
  return false;
}

// Helper methods - these are thin wrappers that could be static but kept as instance
// methods for potential future state access
ImVec2 CanvasInteractionHandler::AlignPosToGrid(ImVec2 pos, float grid_step) {
  return AlignToGridLocal(pos, grid_step);
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
