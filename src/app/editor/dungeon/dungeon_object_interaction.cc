#include "dungeon_object_interaction.h"

#include "app/gui/color.h"
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
      // TODO: Apply drag transformation to selected objects
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
  if (!canvas_->IsMouseHovering()) return;
  
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
    
    // Draw selection rectangle
    ImVec2 start =
        ImVec2(canvas_pos.x + std::min(drag_start_pos.x, mouse_pos.x),
               canvas_pos.y + std::min(drag_start_pos.y, mouse_pos.y));
    ImVec2 end = ImVec2(canvas_pos.x + std::max(drag_start_pos.x, mouse_pos.x),
                        canvas_pos.y + std::max(drag_start_pos.y, mouse_pos.y));
    
    ImDrawList* draw_list = ImGui::GetWindowDrawList();
    draw_list->AddRect(start, end, IM_COL32(255, 255, 0, 255), 0.0f, 0, 2.0f);
    draw_list->AddRectFilled(start, end, IM_COL32(255, 255, 0, 32));
  }
  
  // Complete selection on mouse release
  if (dragging && !ImGui::IsMouseDown(ImGuiMouseButton_Right)) {
    dragging = false;
    object_select_active_ = true;
    SelectObjectsInRect();
  }
}

void DungeonObjectInteraction::SelectObjectsInRect() {
  if (!rooms_ || current_room_id_ < 0 || current_room_id_ >= 296) return;
  
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
  
  // Highlight selected objects
  if (!selected_object_indices_.empty()) {
    for (size_t index : selected_object_indices_) {
      if (index < objects.size()) {
        const auto& object = objects[index];
        auto [canvas_x, canvas_y] =
            RoomToCanvasCoordinates(object.x_, object.y_);
        
        // Draw selection highlight
        ImDrawList* draw_list = ImGui::GetWindowDrawList();
        ImVec2 canvas_pos = canvas_->zero_point();
        ImVec2 obj_start(canvas_pos.x + canvas_x - 2,
                         canvas_pos.y + canvas_y - 2);
        ImVec2 obj_end(canvas_pos.x + canvas_x + 18,
                       canvas_pos.y + canvas_y + 18);
        draw_list->AddRect(obj_start, obj_end, IM_COL32(0, 255, 255, 255), 0.0f,
                           0, 2.0f);
      }
    }
  }
}

void DungeonObjectInteraction::PlaceObjectAtPosition(int room_x, int room_y) {
  if (!object_loaded_ || preview_object_.id_ < 0 || !rooms_) return;
  
  if (current_room_id_ < 0 || current_room_id_ >= 296) return;
  
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
  if (!is_selecting_) return;
  
  ImDrawList* draw_list = ImGui::GetWindowDrawList();
  ImVec2 canvas_pos = canvas_->zero_point();
  
  // Calculate select box bounds
  ImVec2 start = ImVec2(
      canvas_pos.x + std::min(select_start_pos_.x, select_current_pos_.x),
      canvas_pos.y + std::min(select_start_pos_.y, select_current_pos_.y));
  ImVec2 end = ImVec2(
      canvas_pos.x + std::max(select_start_pos_.x, select_current_pos_.x),
      canvas_pos.y + std::max(select_start_pos_.y, select_current_pos_.y));
  
  // Draw selection box
  draw_list->AddRect(start, end, IM_COL32(255, 255, 0, 255), 0.0f, 0, 2.0f);
  draw_list->AddRectFilled(start, end, IM_COL32(255, 255, 0, 32));
}

void DungeonObjectInteraction::DrawDragPreview() {
  if (!is_dragging_) return;
  
  // Draw drag preview for selected objects
  ImDrawList* draw_list = ImGui::GetWindowDrawList();
  ImVec2 canvas_pos = canvas_->zero_point();
  ImVec2 drag_delta = ImVec2(drag_current_pos_.x - drag_start_pos_.x,
                             drag_current_pos_.y - drag_start_pos_.y);
  
  // Draw preview of where objects would be moved
  for (int obj_id : selected_objects_) {
    // TODO: Draw preview of object at new position
    // This would require getting the object's current position and drawing it
    // offset by drag_delta
  }
}

void DungeonObjectInteraction::UpdateSelectedObjects() {
  if (!is_selecting_ || !rooms_) return;
  
  selected_objects_.clear();
  
  if (current_room_id_ < 0 || current_room_id_ >= 296) return;
  
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
  if (!is_selecting_) return false;
  
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

std::pair<int, int> DungeonObjectInteraction::RoomToCanvasCoordinates(int room_x, int room_y) const {
  return {room_x * 16, room_y * 16};
}

std::pair<int, int> DungeonObjectInteraction::CanvasToRoomCoordinates(int canvas_x, int canvas_y) const {
  return {canvas_x / 16, canvas_y / 16};
}

bool DungeonObjectInteraction::IsWithinCanvasBounds(int canvas_x, int canvas_y, int margin) const {
  auto canvas_size = canvas_->canvas_size();
  auto global_scale = canvas_->global_scale();
  int scaled_width = static_cast<int>(canvas_size.x * global_scale);
  int scaled_height = static_cast<int>(canvas_size.y * global_scale);
  
  return (canvas_x >= -margin && canvas_y >= -margin &&
          canvas_x <= scaled_width + margin &&
          canvas_y <= scaled_height + margin);
}

void DungeonObjectInteraction::SetCurrentRoom(std::array<zelda3::Room, 0x128>* rooms, int room_id) {
  rooms_ = rooms;
  current_room_id_ = room_id;
}

void DungeonObjectInteraction::SetPreviewObject(const zelda3::RoomObject& object, bool loaded) {
  preview_object_ = object;
  object_loaded_ = loaded;
}

void DungeonObjectInteraction::ClearSelection() {
  selected_object_indices_.clear();
  object_select_active_ = false;
  is_selecting_ = false;
  is_dragging_ = false;
}

}  // namespace yaze::editor
