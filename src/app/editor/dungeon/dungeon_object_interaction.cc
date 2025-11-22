#include "dungeon_object_interaction.h"

#include <algorithm>

#include "app/editor/agent/agent_ui_theme.h"
#include "imgui/imgui.h"

namespace yaze::editor {

void DungeonObjectInteraction::HandleCanvasMouseInput() {
  const ImGuiIO& io = ImGui::GetIO();

  // Check if mouse is over the canvas
  if (!canvas_->IsMouseHovering()) {
    return;
  }

  // Get mouse position relative to canvas
  ImVec2 mouse_pos = io.MousePos;
  ImVec2 canvas_pos = canvas_->zero_point();
  ImVec2 canvas_size = canvas_->canvas_size();

  // Convert to canvas coordinates
  ImVec2 canvas_mouse_pos =
      ImVec2(mouse_pos.x - canvas_pos.x, mouse_pos.y - canvas_pos.y);

  // Handle mouse clicks
  if (ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
    if (ImGui::IsKeyDown(ImGuiKey_LeftCtrl) ||
        ImGui::IsKeyDown(ImGuiKey_RightCtrl)) {
      // Start selection box
      is_selecting_ = true;
      select_start_pos_ = canvas_mouse_pos;
      select_current_pos_ = canvas_mouse_pos;
      selected_objects_.clear();
    } else {
      // Start dragging or place object
      if (object_loaded_) {
        // Convert canvas coordinates to room coordinates
        auto [room_x, room_y] =
            CanvasToRoomCoordinates(static_cast<int>(canvas_mouse_pos.x),
                                    static_cast<int>(canvas_mouse_pos.y));
        PlaceObjectAtPosition(room_x, room_y);
      } else {
        // Start dragging existing objects
        is_dragging_ = true;
        drag_start_pos_ = canvas_mouse_pos;
        drag_current_pos_ = canvas_mouse_pos;
      }
    }
  }

  // Handle mouse drag
  if (is_selecting_ && ImGui::IsMouseDragging(ImGuiMouseButton_Left)) {
    select_current_pos_ = canvas_mouse_pos;
    UpdateSelectedObjects();
  }

  if (is_dragging_ && ImGui::IsMouseDragging(ImGuiMouseButton_Left)) {
    drag_current_pos_ = canvas_mouse_pos;
    DrawDragPreview();
  }

  // Handle mouse release
  if (ImGui::IsMouseReleased(ImGuiMouseButton_Left)) {
    if (is_selecting_) {
      is_selecting_ = false;
      UpdateSelectedObjects();
    }
    if (is_dragging_) {
      is_dragging_ = false;
      // Apply drag transformation to selected objects
      if (!selected_object_indices_.empty() && rooms_ &&
          current_room_id_ >= 0 && current_room_id_ < 296) {
        if (mutation_hook_) {
          mutation_hook_();
        }
        auto& room = (*rooms_)[current_room_id_];
        ImVec2 drag_delta = ImVec2(drag_current_pos_.x - drag_start_pos_.x,
                                   drag_current_pos_.y - drag_start_pos_.y);

        // Convert pixel delta to tile delta
        int tile_delta_x = static_cast<int>(drag_delta.x) / 8;
        int tile_delta_y = static_cast<int>(drag_delta.y) / 8;

        // Move all selected objects
        auto& objects = room.GetTileObjects();
        for (size_t index : selected_object_indices_) {
          if (index < objects.size()) {
            objects[index].x_ += tile_delta_x;
            objects[index].y_ += tile_delta_y;

            // Clamp to room bounds (64x64 tiles)
            objects[index].x_ =
                std::clamp(static_cast<int>(objects[index].x_), 0, 63);
            objects[index].y_ =
                std::clamp(static_cast<int>(objects[index].y_), 0, 63);
          }
        }

        // Trigger cache invalidation and re-render
        if (cache_invalidation_callback_) {
          cache_invalidation_callback_();
        }
      }
    }
  }
}

void DungeonObjectInteraction::CheckForObjectSelection() {
  // Draw object selection rectangle similar to OverworldEditor
  DrawObjectSelectRect();

  // Handle object selection when rectangle is active
  if (object_select_active_) {
    SelectObjectsInRect();
  }
}

void DungeonObjectInteraction::DrawObjectSelectRect() {
  if (!canvas_->IsMouseHovering())
    return;

  const ImGuiIO& io = ImGui::GetIO();
  const ImVec2 canvas_pos = canvas_->zero_point();
  const ImVec2 mouse_pos =
      ImVec2(io.MousePos.x - canvas_pos.x, io.MousePos.y - canvas_pos.y);

  static bool dragging = false;
  static ImVec2 drag_start_pos;

  // Right click to start object selection
  if (ImGui::IsMouseClicked(ImGuiMouseButton_Right) && !object_loaded_) {
    drag_start_pos = mouse_pos;
    object_select_start_ = mouse_pos;
    selected_object_indices_.clear();
    object_select_active_ = false;
    dragging = false;
  }

  // Right drag to create selection rectangle
  if (ImGui::IsMouseDragging(ImGuiMouseButton_Right) && !object_loaded_) {
    object_select_end_ = mouse_pos;
    dragging = true;

    // Draw selection rectangle with theme colors
    const auto& theme = AgentUI::GetTheme();
    ImVec2 start =
        ImVec2(canvas_pos.x + std::min(drag_start_pos.x, mouse_pos.x),
               canvas_pos.y + std::min(drag_start_pos.y, mouse_pos.y));
    ImVec2 end = ImVec2(canvas_pos.x + std::max(drag_start_pos.x, mouse_pos.x),
                        canvas_pos.y + std::max(drag_start_pos.y, mouse_pos.y));

    ImDrawList* draw_list = ImGui::GetWindowDrawList();
    // Use accent color for selection box (high visibility at 0.85f alpha)
    ImU32 selection_color = ImGui::ColorConvertFloat4ToU32(
        ImVec4(theme.accent_color.x, theme.accent_color.y, theme.accent_color.z, 0.85f));
    ImU32 selection_fill = ImGui::ColorConvertFloat4ToU32(
        ImVec4(theme.accent_color.x, theme.accent_color.y, theme.accent_color.z, 0.15f));
    draw_list->AddRect(start, end, selection_color, 0.0f, 0, 2.0f);
    draw_list->AddRectFilled(start, end, selection_fill);
  }

  // Complete selection on mouse release
  if (dragging && !ImGui::IsMouseDown(ImGuiMouseButton_Right)) {
    dragging = false;
    object_select_active_ = true;
    SelectObjectsInRect();
  }
}

void DungeonObjectInteraction::SelectObjectsInRect() {
  if (!rooms_ || current_room_id_ < 0 || current_room_id_ >= 296)
    return;

  auto& room = (*rooms_)[current_room_id_];
  selected_object_indices_.clear();

  // Calculate selection bounds in room coordinates
  auto [start_room_x, start_room_y] = CanvasToRoomCoordinates(
      static_cast<int>(std::min(object_select_start_.x, object_select_end_.x)),
      static_cast<int>(std::min(object_select_start_.y, object_select_end_.y)));
  auto [end_room_x, end_room_y] = CanvasToRoomCoordinates(
      static_cast<int>(std::max(object_select_start_.x, object_select_end_.x)),
      static_cast<int>(std::max(object_select_start_.y, object_select_end_.y)));

  // Find objects within selection rectangle
  const auto& objects = room.GetTileObjects();
  for (size_t i = 0; i < objects.size(); ++i) {
    const auto& object = objects[i];
    if (object.x_ >= start_room_x && object.x_ <= end_room_x &&
        object.y_ >= start_room_y && object.y_ <= end_room_y) {
      selected_object_indices_.push_back(i);
    }
  }
}

void DungeonObjectInteraction::DrawSelectionHighlights() {
  if (!rooms_ || current_room_id_ < 0 || current_room_id_ >= 296)
    return;

  auto& room = (*rooms_)[current_room_id_];
  const auto& objects = room.GetTileObjects();

  // Draw highlights for all selected objects with theme colors
  const auto& theme = AgentUI::GetTheme();
  ImDrawList* draw_list = ImGui::GetWindowDrawList();
  ImVec2 canvas_pos = canvas_->zero_point();

  for (size_t index : selected_object_indices_) {
    if (index < objects.size()) {
      const auto& object = objects[index];
      auto [canvas_x, canvas_y] = RoomToCanvasCoordinates(object.x_, object.y_);

      // Calculate object size for highlight
      int obj_width = 8 + (object.size_ & 0x0F) * 4;
      int obj_height = 8 + ((object.size_ >> 4) & 0x0F) * 4;
      obj_width = std::min(obj_width, 64);
      obj_height = std::min(obj_height, 64);

      // Draw selection highlight using accent color
      ImVec2 obj_start(canvas_pos.x + canvas_x - 2,
                       canvas_pos.y + canvas_y - 2);
      ImVec2 obj_end(canvas_pos.x + canvas_x + obj_width + 2,
                     canvas_pos.y + canvas_y + obj_height + 2);

      // Animated selection (pulsing effect) with theme accent color
      float pulse =
          0.7f + 0.3f * std::sin(static_cast<float>(ImGui::GetTime()) * 4.0f);
      ImU32 selection_color = ImGui::ColorConvertFloat4ToU32(
          ImVec4(theme.accent_color.x * pulse, theme.accent_color.y * pulse,
                 theme.accent_color.z * pulse, 0.85f));
      draw_list->AddRect(obj_start, obj_end, selection_color, 0.0f, 0, 2.5f);

      // Draw corner handles for selected objects (high-contrast cyan-white)
      constexpr float handle_size = 4.0f;
      // Entity visibility standard: Cyan-white at 0.85f alpha for high contrast
      ImU32 handle_color = ImGui::GetColorU32(theme.dungeon_selection_handle);
      draw_list->AddRectFilled(
          ImVec2(obj_start.x - handle_size / 2, obj_start.y - handle_size / 2),
          ImVec2(obj_start.x + handle_size / 2, obj_start.y + handle_size / 2),
          handle_color);
      draw_list->AddRectFilled(
          ImVec2(obj_end.x - handle_size / 2, obj_start.y - handle_size / 2),
          ImVec2(obj_end.x + handle_size / 2, obj_start.y + handle_size / 2),
          handle_color);
      draw_list->AddRectFilled(
          ImVec2(obj_start.x - handle_size / 2, obj_end.y - handle_size / 2),
          ImVec2(obj_start.x + handle_size / 2, obj_end.y + handle_size / 2),
          handle_color);
      draw_list->AddRectFilled(
          ImVec2(obj_end.x - handle_size / 2, obj_end.y - handle_size / 2),
          ImVec2(obj_end.x + handle_size / 2, obj_end.y + handle_size / 2),
          handle_color);
    }
  }
}

void DungeonObjectInteraction::PlaceObjectAtPosition(int room_x, int room_y) {
  if (!object_loaded_ || preview_object_.id_ < 0 || !rooms_)
    return;

  if (current_room_id_ < 0 || current_room_id_ >= 296)
    return;

  if (mutation_hook_) {
    mutation_hook_();
  }

  // Create new object at the specified position
  auto new_object = preview_object_;
  new_object.x_ = room_x;
  new_object.y_ = room_y;

  // Add object to room
  auto& room = (*rooms_)[current_room_id_];
  room.AddTileObject(new_object);

  // Notify callback if set
  if (object_placed_callback_) {
    object_placed_callback_(new_object);
  }

  // Trigger cache invalidation
  if (cache_invalidation_callback_) {
    cache_invalidation_callback_();
  }
}

void DungeonObjectInteraction::DrawSelectBox() {
  if (!is_selecting_)
    return;

  const auto& theme = AgentUI::GetTheme();
  ImDrawList* draw_list = ImGui::GetWindowDrawList();
  ImVec2 canvas_pos = canvas_->zero_point();

  // Calculate select box bounds
  ImVec2 start = ImVec2(
      canvas_pos.x + std::min(select_start_pos_.x, select_current_pos_.x),
      canvas_pos.y + std::min(select_start_pos_.y, select_current_pos_.y));
  ImVec2 end = ImVec2(
      canvas_pos.x + std::max(select_start_pos_.x, select_current_pos_.x),
      canvas_pos.y + std::max(select_start_pos_.y, select_current_pos_.y));

  // Draw selection box with theme colors
  ImU32 selection_color = ImGui::ColorConvertFloat4ToU32(
      ImVec4(theme.accent_color.x, theme.accent_color.y, theme.accent_color.z, 0.85f));
  ImU32 selection_fill = ImGui::ColorConvertFloat4ToU32(
      ImVec4(theme.accent_color.x, theme.accent_color.y, theme.accent_color.z, 0.15f));
  draw_list->AddRect(start, end, selection_color, 0.0f, 0, 2.0f);
  draw_list->AddRectFilled(start, end, selection_fill);
}

void DungeonObjectInteraction::DrawDragPreview() {
  const auto& theme = AgentUI::GetTheme();
  if (!is_dragging_ || selected_object_indices_.empty() || !rooms_)
    return;
  if (current_room_id_ < 0 || current_room_id_ >= 296)
    return;

  // Draw drag preview for selected objects
  ImDrawList* draw_list = ImGui::GetWindowDrawList();
  ImVec2 canvas_pos = canvas_->zero_point();
  ImVec2 drag_delta = ImVec2(drag_current_pos_.x - drag_start_pos_.x,
                             drag_current_pos_.y - drag_start_pos_.y);

  auto& room = (*rooms_)[current_room_id_];
  const auto& objects = room.GetTileObjects();

  // Draw preview of where objects would be moved
  for (size_t index : selected_object_indices_) {
    if (index < objects.size()) {
      const auto& object = objects[index];
      auto [canvas_x, canvas_y] = RoomToCanvasCoordinates(object.x_, object.y_);

      // Calculate object size
      int obj_width = 8 + (object.size_ & 0x0F) * 4;
      int obj_height = 8 + ((object.size_ >> 4) & 0x0F) * 4;
      obj_width = std::min(obj_width, 64);
      obj_height = std::min(obj_height, 64);

      // Draw semi-transparent preview at new position
      ImVec2 preview_start(canvas_pos.x + canvas_x + drag_delta.x,
                           canvas_pos.y + canvas_y + drag_delta.y);
      ImVec2 preview_end(preview_start.x + obj_width,
                         preview_start.y + obj_height);

      // Draw ghosted object
      draw_list->AddRectFilled(preview_start, preview_end,
                               ImGui::GetColorU32(theme.dungeon_drag_preview));
      draw_list->AddRect(preview_start, preview_end, ImGui::GetColorU32(theme.dungeon_selection_secondary),
                         0.0f, 0, 1.5f);
    }
  }
}

void DungeonObjectInteraction::UpdateSelectedObjects() {
  if (!is_selecting_ || !rooms_)
    return;

  selected_objects_.clear();

  if (current_room_id_ < 0 || current_room_id_ >= 296)
    return;

  auto& room = (*rooms_)[current_room_id_];

  // Check each object in the room
  for (const auto& object : room.GetTileObjects()) {
    if (IsObjectInSelectBox(object)) {
      selected_objects_.push_back(object.id_);
    }
  }
}

bool DungeonObjectInteraction::IsObjectInSelectBox(
    const zelda3::RoomObject& object) const {
  if (!is_selecting_)
    return false;

  // Convert object position to canvas coordinates
  auto [canvas_x, canvas_y] = RoomToCanvasCoordinates(object.x_, object.y_);

  // Calculate select box bounds
  float min_x = std::min(select_start_pos_.x, select_current_pos_.x);
  float max_x = std::max(select_start_pos_.x, select_current_pos_.x);
  float min_y = std::min(select_start_pos_.y, select_current_pos_.y);
  float max_y = std::max(select_start_pos_.y, select_current_pos_.y);

  // Check if object is within select box
  return (canvas_x >= min_x && canvas_x <= max_x && canvas_y >= min_y &&
          canvas_y <= max_y);
}

std::pair<int, int> DungeonObjectInteraction::RoomToCanvasCoordinates(
    int room_x, int room_y) const {
  // Dungeon tiles are 8x8 pixels, convert room coordinates (tiles) to pixels
  return {room_x * 8, room_y * 8};
}

std::pair<int, int> DungeonObjectInteraction::CanvasToRoomCoordinates(
    int canvas_x, int canvas_y) const {
  // Convert canvas pixels back to room coordinates (tiles)
  return {canvas_x / 8, canvas_y / 8};
}

bool DungeonObjectInteraction::IsWithinCanvasBounds(int canvas_x, int canvas_y,
                                                    int margin) const {
  auto canvas_size = canvas_->canvas_size();
  auto global_scale = canvas_->global_scale();
  int scaled_width = static_cast<int>(canvas_size.x * global_scale);
  int scaled_height = static_cast<int>(canvas_size.y * global_scale);

  return (canvas_x >= -margin && canvas_y >= -margin &&
          canvas_x <= scaled_width + margin &&
          canvas_y <= scaled_height + margin);
}

void DungeonObjectInteraction::SetCurrentRoom(
    std::array<zelda3::Room, 0x128>* rooms, int room_id) {
  rooms_ = rooms;
  current_room_id_ = room_id;
}

void DungeonObjectInteraction::SetPreviewObject(
    const zelda3::RoomObject& object, bool loaded) {
  preview_object_ = object;
  object_loaded_ = loaded;
}

void DungeonObjectInteraction::ClearSelection() {
  selected_object_indices_.clear();
  object_select_active_ = false;
  is_selecting_ = false;
  is_dragging_ = false;
}

void DungeonObjectInteraction::ShowContextMenu() {
  if (!canvas_->IsMouseHovering())
    return;

  // Show context menu on right-click when not dragging
  if (ImGui::IsMouseClicked(ImGuiMouseButton_Right) && !is_dragging_) {
    ImGui::OpenPopup("DungeonObjectContextMenu");
  }

  if (ImGui::BeginPopup("DungeonObjectContextMenu")) {
    // Show different options based on current state
    if (!selected_object_indices_.empty()) {
      if (ImGui::MenuItem("Delete Selected", "Del")) {
        HandleDeleteSelected();
      }
      if (ImGui::MenuItem("Copy Selected", "Ctrl+C")) {
        HandleCopySelected();
      }
      ImGui::Separator();
    }

    if (has_clipboard_data_) {
      if (ImGui::MenuItem("Paste Objects", "Ctrl+V")) {
        HandlePasteObjects();
      }
      ImGui::Separator();
    }

    if (object_loaded_) {
      ImGui::Text("Placing: Object 0x%02X", preview_object_.id_);
      if (ImGui::MenuItem("Cancel Placement", "Esc")) {
        object_loaded_ = false;
      }
    } else {
      ImGui::Text("Right-click + drag to select");
      ImGui::Text("Left-click + drag to move");
    }

    ImGui::EndPopup();
  }
}

void DungeonObjectInteraction::HandleDeleteSelected() {
  if (selected_object_indices_.empty() || !rooms_)
    return;
  if (current_room_id_ < 0 || current_room_id_ >= 296)
    return;

  if (mutation_hook_) {
    mutation_hook_();
  }

  auto& room = (*rooms_)[current_room_id_];

  // Sort indices in descending order to avoid index shifts during deletion
  std::vector<size_t> sorted_indices = selected_object_indices_;
  std::sort(sorted_indices.rbegin(), sorted_indices.rend());

  // Delete selected objects using Room's RemoveTileObject method
  for (size_t index : sorted_indices) {
    room.RemoveTileObject(index);
  }

  // Clear selection
  ClearSelection();

  // Trigger cache invalidation and re-render
  if (cache_invalidation_callback_) {
    cache_invalidation_callback_();
  }
}

void DungeonObjectInteraction::HandleCopySelected() {
  if (selected_object_indices_.empty() || !rooms_)
    return;
  if (current_room_id_ < 0 || current_room_id_ >= 296)
    return;

  auto& room = (*rooms_)[current_room_id_];
  const auto& objects = room.GetTileObjects();

  // Copy selected objects to clipboard
  clipboard_.clear();
  for (size_t index : selected_object_indices_) {
    if (index < objects.size()) {
      clipboard_.push_back(objects[index]);
    }
  }

  has_clipboard_data_ = !clipboard_.empty();
}

void DungeonObjectInteraction::HandlePasteObjects() {
  if (!has_clipboard_data_ || !rooms_)
    return;
  if (current_room_id_ < 0 || current_room_id_ >= 296)
    return;

  if (mutation_hook_) {
    mutation_hook_();
  }

  auto& room = (*rooms_)[current_room_id_];

  // Get mouse position for paste location
  const ImGuiIO& io = ImGui::GetIO();
  ImVec2 mouse_pos = io.MousePos;
  ImVec2 canvas_pos = canvas_->zero_point();
  ImVec2 canvas_mouse_pos =
      ImVec2(mouse_pos.x - canvas_pos.x, mouse_pos.y - canvas_pos.y);
  auto [paste_x, paste_y] =
      CanvasToRoomCoordinates(static_cast<int>(canvas_mouse_pos.x),
                              static_cast<int>(canvas_mouse_pos.y));

  // Calculate offset from first object in clipboard
  if (!clipboard_.empty()) {
    int offset_x = paste_x - clipboard_[0].x_;
    int offset_y = paste_y - clipboard_[0].y_;

    // Paste all objects with offset
    for (const auto& obj : clipboard_) {
      auto new_obj = obj;
      new_obj.x_ = obj.x_ + offset_x;
      new_obj.y_ = obj.y_ + offset_y;

      // Clamp to room bounds
      new_obj.x_ = std::clamp(static_cast<int>(new_obj.x_), 0, 63);
      new_obj.y_ = std::clamp(static_cast<int>(new_obj.y_), 0, 63);

      room.AddTileObject(new_obj);
    }

    // Trigger cache invalidation and re-render
    if (cache_invalidation_callback_) {
      cache_invalidation_callback_();
    }
  }
}

void DungeonObjectInteraction::DrawGhostPreview() {
  // Only draw ghost preview when an object is loaded for placement
  if (!object_loaded_ || preview_object_.id_ < 0)
    return;

  // Check if mouse is over the canvas
  if (!canvas_->IsMouseHovering())
    return;

  const ImGuiIO& io = ImGui::GetIO();
  ImVec2 canvas_pos = canvas_->zero_point();
  ImVec2 mouse_pos = io.MousePos;

  // Convert mouse position to canvas coordinates
  ImVec2 canvas_mouse_pos =
      ImVec2(mouse_pos.x - canvas_pos.x, mouse_pos.y - canvas_pos.y);

  // Convert to room tile coordinates
  auto [room_x, room_y] =
      CanvasToRoomCoordinates(static_cast<int>(canvas_mouse_pos.x),
                              static_cast<int>(canvas_mouse_pos.y));

  // Validate position is within room bounds (64x64 tiles)
  if (room_x < 0 || room_x >= 64 || room_y < 0 || room_y >= 64)
    return;

  // Convert back to canvas pixel coordinates (for snapped position)
  auto [snap_canvas_x, snap_canvas_y] = RoomToCanvasCoordinates(room_x, room_y);

  // Calculate object dimensions
  int obj_width = 8 + (preview_object_.size_ & 0x0F) * 4;
  int obj_height = 8 + ((preview_object_.size_ >> 4) & 0x0F) * 4;
  obj_width = std::min(obj_width, 256);
  obj_height = std::min(obj_height, 256);

  // Draw ghost preview at snapped position
  ImDrawList* draw_list = ImGui::GetWindowDrawList();
  float scale = canvas_->global_scale();

  // Apply canvas scale and offset
  ImVec2 preview_start(canvas_pos.x + snap_canvas_x * scale,
                       canvas_pos.y + snap_canvas_y * scale);
  ImVec2 preview_end(preview_start.x + obj_width * scale,
                     preview_start.y + obj_height * scale);

  const auto& theme = AgentUI::GetTheme();

  // Draw semi-transparent filled rectangle (ghost effect)
  ImVec4 preview_fill = ImVec4(
      theme.dungeon_selection_primary.x,
      theme.dungeon_selection_primary.y,
      theme.dungeon_selection_primary.z,
      0.25f);  // Semi-transparent
  draw_list->AddRectFilled(preview_start, preview_end,
                           ImGui::GetColorU32(preview_fill));

  // Draw solid outline for visibility
  ImVec4 preview_outline = ImVec4(
      theme.dungeon_selection_primary.x,
      theme.dungeon_selection_primary.y,
      theme.dungeon_selection_primary.z,
      0.78f);  // More visible
  draw_list->AddRect(preview_start, preview_end,
                     ImGui::GetColorU32(preview_outline),
                     0.0f, 0, 2.0f);

  // Draw object ID text at corner
  std::string id_text = absl::StrFormat("0x%02X", preview_object_.id_);
  ImVec2 text_pos(preview_start.x + 2, preview_start.y + 2);
  draw_list->AddText(text_pos, ImGui::GetColorU32(theme.text_primary), id_text.c_str());

  // Draw crosshair at placement position
  constexpr float crosshair_size = 8.0f;
  ImVec2 center(preview_start.x + (obj_width * scale) / 2,
                preview_start.y + (obj_height * scale) / 2);
  ImVec4 crosshair_color = ImVec4(
      theme.text_primary.x,
      theme.text_primary.y,
      theme.text_primary.z,
      0.78f);  // Slightly transparent
  ImU32 crosshair = ImGui::GetColorU32(crosshair_color);
  draw_list->AddLine(ImVec2(center.x - crosshair_size, center.y),
                     ImVec2(center.x + crosshair_size, center.y),
                     crosshair, 1.5f);
  draw_list->AddLine(ImVec2(center.x, center.y - crosshair_size),
                     ImVec2(center.x, center.y + crosshair_size),
                     crosshair, 1.5f);
}

}  // namespace yaze::editor
