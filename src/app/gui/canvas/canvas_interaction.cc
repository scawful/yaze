#include "canvas_interaction.h"

#include <algorithm>
#include <cmath>

#include "imgui/imgui.h"

namespace yaze {
namespace gui {

namespace {

// Static state for rectangle selection (temporary until we have proper state
// management)
struct SelectRectState {
  ImVec2 drag_start_pos = ImVec2(-1, -1);
  bool is_dragging = false;
};

// Per-canvas state (keyed by canvas geometry pointer for simplicity)
// TODO(scawful): Replace with proper state management in Phase 2.5
thread_local SelectRectState g_select_rect_state;

}  // namespace

// ============================================================================
// Helper Functions
// ============================================================================

ImVec2 AlignToGrid(ImVec2 pos, float grid_step) {
  return ImVec2(std::floor(pos.x / grid_step) * grid_step,
                std::floor(pos.y / grid_step) * grid_step);
}

ImVec2 GetMouseInCanvasSpace(const CanvasGeometry& geometry) {
  const ImGuiIO& imgui_io = ImGui::GetIO();
  const ImVec2 origin(geometry.canvas_p0.x + geometry.scrolling.x,
                      geometry.canvas_p0.y + geometry.scrolling.y);
  return ImVec2(imgui_io.MousePos.x - origin.x, imgui_io.MousePos.y - origin.y);
}

bool IsMouseInCanvas(const CanvasGeometry& geometry) {
  const ImGuiIO& imgui_io = ImGui::GetIO();
  return imgui_io.MousePos.x >= geometry.canvas_p0.x &&
         imgui_io.MousePos.x <= geometry.canvas_p1.x &&
         imgui_io.MousePos.y >= geometry.canvas_p0.y &&
         imgui_io.MousePos.y <= geometry.canvas_p1.y;
}

ImVec2 CanvasToTileGrid(ImVec2 canvas_pos, float tile_size,
                        float global_scale) {
  const float scaled_size = tile_size * global_scale;
  return ImVec2(std::floor(canvas_pos.x / scaled_size),
                std::floor(canvas_pos.y / scaled_size));
}

// ============================================================================
// Rectangle Selection Implementation
// ============================================================================

RectSelectionEvent HandleRectangleSelection(const CanvasGeometry& geometry,
                                            int current_map, float tile_size,
                                            ImDrawList* draw_list,
                                            ImGuiMouseButton mouse_button) {
  RectSelectionEvent event;
  event.current_map = current_map;

  if (!IsMouseInCanvas(geometry)) {
    return event;
  }

  const ImVec2 mouse_pos = GetMouseInCanvasSpace(geometry);
  const float scaled_size =
      tile_size * geometry.scaled_size.x / geometry.canvas_sz.x;
  constexpr int kSmallMapSize = 0x200;  // 512 pixels

  // Calculate super X/Y accounting for world offset
  int super_y = 0;
  int super_x = 0;
  if (current_map < 0x40) {
    // Light World (0x00-0x3F)
    super_y = current_map / 8;
    super_x = current_map % 8;
  } else if (current_map < 0x80) {
    // Dark World (0x40-0x7F)
    super_y = (current_map - 0x40) / 8;
    super_x = (current_map - 0x40) % 8;
  } else {
    // Special World (0x80+)
    super_y = (current_map - 0x80) / 8;
    super_x = (current_map - 0x80) % 8;
  }

  // Handle mouse button press - start selection
  if (ImGui::IsMouseClicked(mouse_button)) {
    g_select_rect_state.drag_start_pos = AlignToGrid(mouse_pos, scaled_size);
    g_select_rect_state.is_dragging = false;

    // Single tile selection on click
    ImVec2 painter_pos = AlignToGrid(mouse_pos, scaled_size);
    int painter_x = static_cast<int>(painter_pos.x);
    int painter_y = static_cast<int>(painter_pos.y);

    auto tile16_x = (painter_x % kSmallMapSize) / (kSmallMapSize / 0x20);
    auto tile16_y = (painter_y % kSmallMapSize) / (kSmallMapSize / 0x20);

    int index_x = super_x * 0x20 + tile16_x;
    int index_y = super_y * 0x20 + tile16_y;

    event.start_pos = painter_pos;
    event.selected_tiles.push_back(
        ImVec2(static_cast<float>(index_x), static_cast<float>(index_y)));
  }

  // Handle dragging - draw preview rectangle
  ImVec2 drag_end_pos = AlignToGrid(mouse_pos, scaled_size);
  if (ImGui::IsMouseDragging(mouse_button) && draw_list) {
    g_select_rect_state.is_dragging = true;
    event.is_active = true;
    event.start_pos = g_select_rect_state.drag_start_pos;
    event.end_pos = drag_end_pos;

    // Draw preview rectangle
    auto start =
        ImVec2(geometry.canvas_p0.x + g_select_rect_state.drag_start_pos.x,
               geometry.canvas_p0.y + g_select_rect_state.drag_start_pos.y);
    auto end = ImVec2(geometry.canvas_p0.x + drag_end_pos.x + tile_size,
                      geometry.canvas_p0.y + drag_end_pos.y + tile_size);
    draw_list->AddRect(start, end, IM_COL32(255, 255, 255, 255));
  }

  // Handle mouse release - complete selection
  if (g_select_rect_state.is_dragging && !ImGui::IsMouseDown(mouse_button)) {
    g_select_rect_state.is_dragging = false;
    event.is_complete = true;
    event.is_active = false;
    event.start_pos = g_select_rect_state.drag_start_pos;
    event.end_pos = drag_end_pos;

    // Calculate selected tiles
    constexpr int kTile16Size = 16;
    int start_x = static_cast<int>(std::floor(
                      g_select_rect_state.drag_start_pos.x / scaled_size)) *
                  kTile16Size;
    int start_y = static_cast<int>(std::floor(
                      g_select_rect_state.drag_start_pos.y / scaled_size)) *
                  kTile16Size;
    int end_x = static_cast<int>(std::floor(drag_end_pos.x / scaled_size)) *
                kTile16Size;
    int end_y = static_cast<int>(std::floor(drag_end_pos.y / scaled_size)) *
                kTile16Size;

    if (start_x > end_x)
      std::swap(start_x, end_x);
    if (start_y > end_y)
      std::swap(start_y, end_y);

    constexpr int kTilesPerLocalMap = kSmallMapSize / 16;

    for (int tile_y = start_y; tile_y <= end_y; tile_y += kTile16Size) {
      for (int tile_x = start_x; tile_x <= end_x; tile_x += kTile16Size) {
        int local_map_x = tile_x / kSmallMapSize;
        int local_map_y = tile_y / kSmallMapSize;

        int tile16_x = (tile_x % kSmallMapSize) / kTile16Size;
        int tile16_y = (tile_y % kSmallMapSize) / kTile16Size;

        int index_x = local_map_x * kTilesPerLocalMap + tile16_x;
        int index_y = local_map_y * kTilesPerLocalMap + tile16_y;

        event.selected_tiles.emplace_back(static_cast<float>(index_x),
                                          static_cast<float>(index_y));
      }
    }
  }

  return event;
}

TileSelectionEvent HandleTileSelection(const CanvasGeometry& geometry,
                                       int current_map, float tile_size,
                                       ImGuiMouseButton mouse_button) {
  TileSelectionEvent event;

  if (!IsMouseInCanvas(geometry) || !ImGui::IsMouseClicked(mouse_button)) {
    return event;
  }

  const ImVec2 mouse_pos = GetMouseInCanvasSpace(geometry);
  const float scaled_size =
      tile_size * geometry.scaled_size.x / geometry.canvas_sz.x;
  constexpr int kSmallMapSize = 0x200;

  // Calculate super X/Y
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

  ImVec2 painter_pos = AlignToGrid(mouse_pos, scaled_size);
  int painter_x = static_cast<int>(painter_pos.x);
  int painter_y = static_cast<int>(painter_pos.y);

  auto tile16_x = (painter_x % kSmallMapSize) / (kSmallMapSize / 0x20);
  auto tile16_y = (painter_y % kSmallMapSize) / (kSmallMapSize / 0x20);

  int index_x = super_x * 0x20 + tile16_x;
  int index_y = super_y * 0x20 + tile16_y;

  event.tile_position =
      ImVec2(static_cast<float>(index_x), static_cast<float>(index_y));
  event.is_valid = true;

  return event;
}

// ============================================================================
// Tile Painting Implementation
// ============================================================================

TilePaintEvent HandleTilePaint(const CanvasGeometry& geometry, int tile_id,
                               float tile_size, ImGuiMouseButton mouse_button) {
  TilePaintEvent event;
  event.tile_id = tile_id;

  if (!IsMouseInCanvas(geometry)) {
    return event;
  }

  const ImVec2 mouse_pos = GetMouseInCanvasSpace(geometry);
  const float scaled_size =
      tile_size * geometry.scaled_size.x / geometry.canvas_sz.x;

  ImVec2 paint_pos = AlignToGrid(mouse_pos, scaled_size);
  event.position = mouse_pos;
  event.grid_position = paint_pos;

  // Check for paint action
  if (ImGui::IsMouseClicked(mouse_button)) {
    event.is_complete = true;
    event.is_drag = false;
  } else if (ImGui::IsMouseDragging(mouse_button)) {
    event.is_complete = true;
    event.is_drag = true;
  }

  return event;
}

TilePaintEvent HandleTilePaintWithPreview(const CanvasGeometry& geometry,
                                          const gfx::Bitmap& bitmap,
                                          float tile_size,
                                          ImDrawList* draw_list,
                                          ImGuiMouseButton mouse_button) {
  TilePaintEvent event;

  if (!IsMouseInCanvas(geometry)) {
    return event;
  }

  const ImVec2 mouse_pos = GetMouseInCanvasSpace(geometry);
  const float scaled_size =
      tile_size * geometry.scaled_size.x / geometry.canvas_sz.x;

  // Calculate grid-aligned paint position
  ImVec2 paint_pos = AlignToGrid(mouse_pos, scaled_size);
  event.position = mouse_pos;
  event.grid_position = paint_pos;

  auto paint_pos_end =
      ImVec2(paint_pos.x + scaled_size, paint_pos.y + scaled_size);

  // Draw preview of tile at hover position
  if (bitmap.is_active() && draw_list) {
    const ImVec2 origin(geometry.canvas_p0.x + geometry.scrolling.x,
                        geometry.canvas_p0.y + geometry.scrolling.y);
    draw_list->AddImage(
        reinterpret_cast<ImTextureID>(bitmap.texture()),
        ImVec2(origin.x + paint_pos.x, origin.y + paint_pos.y),
        ImVec2(origin.x + paint_pos_end.x, origin.y + paint_pos_end.y));
  }

  // Check for paint action
  if (ImGui::IsMouseClicked(mouse_button) &&
      ImGui::IsMouseDragging(mouse_button)) {
    event.is_complete = true;
    event.is_drag = true;
  }

  return event;
}

TilePaintEvent HandleTilemapPaint(const CanvasGeometry& geometry,
                                  const gfx::Tilemap& tilemap, int current_tile,
                                  ImDrawList* draw_list,
                                  ImGuiMouseButton mouse_button) {
  TilePaintEvent event;
  event.tile_id = current_tile;

  if (!IsMouseInCanvas(geometry)) {
    return event;
  }

  const ImVec2 mouse_pos = GetMouseInCanvasSpace(geometry);
  const float scaled_size =
      16.0f * geometry.scaled_size.x / geometry.canvas_sz.x;

  ImVec2 paint_pos = AlignToGrid(mouse_pos, scaled_size);
  event.position = mouse_pos;
  event.grid_position = paint_pos;

  // Draw preview if tilemap has texture
  if (tilemap.atlas.is_active() && draw_list) {
    const ImVec2 origin(geometry.canvas_p0.x + geometry.scrolling.x,
                        geometry.canvas_p0.y + geometry.scrolling.y);
    // TODO(scawful): Render tilemap preview
    (void)origin;  // Suppress unused warning
  }

  // Check for paint action
  if (ImGui::IsMouseDown(mouse_button)) {
    event.is_complete = true;
    event.is_drag = ImGui::IsMouseDragging(mouse_button);
  }

  return event;
}

// ============================================================================
// Hover and Preview Implementation
// ============================================================================

HoverEvent HandleHover(const CanvasGeometry& geometry, float tile_size) {
  HoverEvent event;

  if (!IsMouseInCanvas(geometry)) {
    return event;
  }

  const ImVec2 mouse_pos = GetMouseInCanvasSpace(geometry);
  const float scaled_size =
      tile_size * geometry.scaled_size.x / geometry.canvas_sz.x;

  event.position = mouse_pos;
  event.grid_position = AlignToGrid(mouse_pos, scaled_size);
  event.is_valid = true;

  return event;
}

void RenderHoverPreview(const CanvasGeometry& geometry, const HoverEvent& hover,
                        float tile_size, ImDrawList* draw_list, ImU32 color) {
  if (!hover.is_valid || !draw_list) {
    return;
  }

  const float scaled_size =
      tile_size * geometry.scaled_size.x / geometry.canvas_sz.x;
  const ImVec2 origin(geometry.canvas_p0.x + geometry.scrolling.x,
                      geometry.canvas_p0.y + geometry.scrolling.y);

  ImVec2 preview_start = ImVec2(origin.x + hover.grid_position.x,
                                origin.y + hover.grid_position.y);
  ImVec2 preview_end =
      ImVec2(preview_start.x + scaled_size, preview_start.y + scaled_size);

  draw_list->AddRectFilled(preview_start, preview_end, color);
}

// ============================================================================
// Entity Interaction Implementation (Stub for Phase 2.4)
// ============================================================================

EntityInteractionEvent HandleEntityInteraction(const CanvasGeometry& geometry,
                                               int entity_id,
                                               ImVec2 entity_position) {
  EntityInteractionEvent event;
  event.entity_id = entity_id;
  event.position = entity_position;

  if (!IsMouseInCanvas(geometry)) {
    return event;
  }

  // TODO(scawful): Implement entity interaction logic in Phase 2.4
  // For now, just return empty event

  return event;
}

}  // namespace gui
}  // namespace yaze
