// Related header
#include "dungeon_object_interaction.h"

// C++ standard library headers
#include <algorithm>

// Third-party library headers
#include "imgui/imgui.h"

// Project headers
#include "app/editor/agent/agent_ui_theme.h"
#include "app/gui/core/icons.h"

namespace yaze::editor {

void DungeonObjectInteraction::HandleCanvasMouseInput() {
  const ImGuiIO& io = ImGui::GetIO();

  // Check if mouse is over the canvas
  if (!canvas_->IsMouseHovering()) {
    return;
  }

  // Handle scroll wheel for resizing selected objects
  HandleScrollWheelResize();

  // Get mouse position relative to canvas
  ImVec2 mouse_pos = io.MousePos;
  ImVec2 canvas_pos = canvas_->zero_point();

  // Convert to canvas coordinates
  ImVec2 canvas_mouse_pos =
      ImVec2(mouse_pos.x - canvas_pos.x, mouse_pos.y - canvas_pos.y);

  // Handle left mouse click
  if (ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
    if (object_loaded_) {
      // Place mode: add object at clicked position
      auto [room_x, room_y] =
          CanvasToRoomCoordinates(static_cast<int>(canvas_mouse_pos.x),
                                  static_cast<int>(canvas_mouse_pos.y));
      PlaceObjectAtPosition(room_x, room_y);
    } else {
      // Selection mode: try to select object at cursor
      if (!TrySelectObjectAtCursor()) {
        // Clicked empty space - start rectangle selection
        if (!io.KeyShift && !io.KeyCtrl) {
          // Clear selection unless modifier held
          selection_.ClearSelection();
        }
        // Begin rectangle selection for multi-select
        selection_.BeginRectangleSelection(static_cast<int>(canvas_mouse_pos.x),
                                           static_cast<int>(canvas_mouse_pos.y));
      } else {
        // Clicked on an object - start drag if we have selected objects
        if (selection_.HasSelection()) {
          is_dragging_ = true;
          drag_start_pos_ = canvas_mouse_pos;
          drag_current_pos_ = canvas_mouse_pos;
        }
      }
    }
  }

  // Handle drag in progress
  if (is_dragging_ && ImGui::IsMouseDragging(ImGuiMouseButton_Left)) {
    drag_current_pos_ = canvas_mouse_pos;
    DrawDragPreview();
  }

  // Handle mouse release - complete drag operation
  if (ImGui::IsMouseReleased(ImGuiMouseButton_Left) && is_dragging_) {
    is_dragging_ = false;

    // Apply drag transformation to selected objects
    auto selected_indices = selection_.GetSelectedIndices();
    if (!selected_indices.empty() && rooms_ && current_room_id_ >= 0 &&
        current_room_id_ < 296) {
      if (mutation_hook_) {
        mutation_hook_();
      }

      auto& room = (*rooms_)[current_room_id_];
      ImVec2 drag_delta = ImVec2(drag_current_pos_.x - drag_start_pos_.x,
                                 drag_current_pos_.y - drag_start_pos_.y);

      // Convert pixel delta to tile delta
      int tile_delta_x = static_cast<int>(drag_delta.x) / 8;
      int tile_delta_y = static_cast<int>(drag_delta.y) / 8;

      // Only apply if there's meaningful movement
      if (tile_delta_x != 0 || tile_delta_y != 0) {
        auto& objects = room.GetTileObjects();
        for (size_t index : selected_indices) {
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
  // Draw and handle object selection rectangle
  DrawObjectSelectRect();
}

void DungeonObjectInteraction::DrawObjectSelectRect() {
  if (!canvas_->IsMouseHovering())
    return;
  if (!rooms_ || current_room_id_ < 0 || current_room_id_ >= 296)
    return;

  const ImGuiIO& io = ImGui::GetIO();
  const ImVec2 canvas_pos = canvas_->zero_point();
  const ImVec2 mouse_pos =
      ImVec2(io.MousePos.x - canvas_pos.x, io.MousePos.y - canvas_pos.y);

  // Rectangle selection is started in HandleCanvasMouseInput on left-click
  // Here we just update and draw during drag

  // Update rectangle during left-click drag
  if (selection_.IsRectangleSelectionActive() &&
      ImGui::IsMouseDragging(ImGuiMouseButton_Left)) {
    selection_.UpdateRectangleSelection(static_cast<int>(mouse_pos.x),
                                        static_cast<int>(mouse_pos.y));
    // Use ObjectSelection's drawing (themed, consistent)
    selection_.DrawRectangleSelectionBox(canvas_);
  }

  // Complete selection on left mouse release
  if (selection_.IsRectangleSelectionActive() &&
      !ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
    auto& room = (*rooms_)[current_room_id_];

    // Determine selection mode based on modifiers
    ObjectSelection::SelectionMode mode = ObjectSelection::SelectionMode::Single;
    if (io.KeyShift) {
      mode = ObjectSelection::SelectionMode::Add;
    } else if (io.KeyCtrl) {
      mode = ObjectSelection::SelectionMode::Toggle;
    }

    selection_.EndRectangleSelection(room.GetTileObjects(), mode);
  }
}

void DungeonObjectInteraction::SelectObjectsInRect() {
  // Legacy method - rectangle selection is now handled by ObjectSelection
  // in DrawObjectSelectRect() / EndRectangleSelection()
  // This method is kept for API compatibility but does nothing
}

void DungeonObjectInteraction::DrawSelectionHighlights() {
  if (!rooms_ || current_room_id_ < 0 || current_room_id_ >= 296)
    return;

  auto& room = (*rooms_)[current_room_id_];
  const auto& objects = room.GetTileObjects();

  // Use ObjectSelection's rendering (handles pulsing border, corner handles)
  selection_.DrawSelectionHighlights(
      canvas_, objects, [this](const zelda3::RoomObject& obj) {
        if (object_drawer_) {
          return object_drawer_->CalculateObjectDimensions(obj);
        }
        // Fallback if no drawer available
        return std::make_pair(16, 16);
      });

  // Interaction-specific: size tooltip when hovering over selected object
  if (canvas_->IsMouseHovering()) {
    size_t hovered_index = GetHoveredObjectIndex();
    if (selection_.IsObjectSelected(hovered_index) &&
        hovered_index < objects.size()) {
      const auto& object = objects[hovered_index];
      ImGui::SetTooltip("Size: %d (0x%02X)\nScroll wheel to resize",
                        object.size_, object.size_);
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
    object_placed_callback_(preview_object_);
  }

  // Trigger cache invalidation
  if (cache_invalidation_callback_) {
    cache_invalidation_callback_();
  }
}

void DungeonObjectInteraction::DrawSelectBox() {
  // Legacy method - rectangle selection now handled by ObjectSelection
  // Delegates to ObjectSelection's DrawRectangleSelectionBox if active
  if (selection_.IsRectangleSelectionActive()) {
    selection_.DrawRectangleSelectionBox(canvas_);
  }
}

void DungeonObjectInteraction::DrawDragPreview() {
  const auto& theme = AgentUI::GetTheme();
  auto selected_indices = selection_.GetSelectedIndices();
  if (!is_dragging_ || selected_indices.empty() || !rooms_)
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
  for (size_t index : selected_indices) {
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
      draw_list->AddRect(preview_start, preview_end,
                         ImGui::GetColorU32(theme.dungeon_selection_secondary),
                         0.0f, 0, 1.5f);
    }
  }
}

void DungeonObjectInteraction::UpdateSelectedObjects() {
  // Legacy method - selection now handled by ObjectSelection class
  // Kept for API compatibility
}

bool DungeonObjectInteraction::IsObjectInSelectBox(
    const zelda3::RoomObject& object) const {
  // Legacy method - selection now handled by ObjectSelection class
  // Kept for API compatibility
  return false;
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
  selection_.ClearSelection();
  is_dragging_ = false;
}

bool DungeonObjectInteraction::TrySelectObjectAtCursor() {
  size_t hovered = GetHoveredObjectIndex();
  if (hovered == static_cast<size_t>(-1)) {
    return false;
  }

  const ImGuiIO& io = ImGui::GetIO();
  ObjectSelection::SelectionMode mode = ObjectSelection::SelectionMode::Single;

  if (io.KeyShift) {
    mode = ObjectSelection::SelectionMode::Add;
  } else if (io.KeyCtrl) {
    mode = ObjectSelection::SelectionMode::Toggle;
  }

  selection_.SelectObject(hovered, mode);
  return true;
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
    if (selection_.HasSelection()) {
      const size_t selection_count = selection_.GetSelectionCount();

      // Show object details for single selection
      if (selection_count == 1) {
        size_t index = *selection_.GetSelectedIndices().begin();
        if (rooms_ && current_room_id_ >= 0 && current_room_id_ < 296) {
          auto& room = (*rooms_)[current_room_id_];
          const auto& objects = room.GetTileObjects();
          if (index < objects.size()) {
            const auto& object = objects[index];
            // Layer 1 is BG2, others (0, 2) are BG1
            const char* layer_name =
                (object.GetLayerValue() == 1) ? "BG2" : "BG1";
            ImGui::Text("Object ID: 0x%02X (%s)", object.id_, layer_name);
            ImGui::Separator();
          }
        }
      }

      if (ImGui::MenuItem("Cut", "Ctrl+X")) {
        HandleCopySelected();
        HandleDeleteSelected();
      }
      if (ImGui::MenuItem("Copy", "Ctrl+C")) {
        HandleCopySelected();
      }
      if (ImGui::MenuItem("Duplicate", "Ctrl+D")) {
        // Copy then paste at offset location
        HandleCopySelected();
        HandlePasteObjects();
      }
      if (ImGui::MenuItem("Delete", "Del")) {
        HandleDeleteSelected();
      }

      ImGui::Separator();

      // Properties dialog stub - only show for single selection
      if (selection_count == 1 && ImGui::MenuItem("Properties...", nullptr, false, false)) {
        // TODO: Implement properties dialog
        // This would open a modal with:
        // - Object ID (editable)
        // - Position X, Y (editable)
        // - Size value (editable with visual preview)
        // - Layer selection (BG1, BG2, BG3)
      }

      ImGui::Separator();
      ImGui::TextDisabled("%zu object%s selected", selection_count,
                          selection_count == 1 ? "" : "s");
    }

    if (has_clipboard_data_) {
      if (ImGui::MenuItem("Paste", "Ctrl+V")) {
        HandlePasteObjects();
      }
      ImGui::Separator();
    }

    if (object_loaded_) {
      ImGui::Text("Placing: Object 0x%02X", preview_object_.id_);
      if (ImGui::MenuItem("Cancel Placement", "Esc")) {
        object_loaded_ = false;
      }
    } else if (!selection_.HasSelection()) {
      ImGui::TextDisabled("Left-click + drag to multi-select");
      ImGui::TextDisabled("Click object, then drag to move");
      ImGui::TextDisabled("Scroll wheel to resize selected");
    }

    ImGui::EndPopup();
  }
}

void DungeonObjectInteraction::HandleDeleteSelected() {
  auto indices = selection_.GetSelectedIndices();
  if (indices.empty() || !rooms_)
    return;
  if (current_room_id_ < 0 || current_room_id_ >= 296)
    return;

  if (mutation_hook_) {
    mutation_hook_();
  }

  auto& room = (*rooms_)[current_room_id_];

  // Sort indices in descending order to avoid index shifts during deletion
  std::sort(indices.rbegin(), indices.rend());

  // Delete selected objects using Room's RemoveTileObject method
  for (size_t index : indices) {
    room.RemoveTileObject(index);
  }

  // Clear selection
  selection_.ClearSelection();

  // Trigger cache invalidation and re-render
  if (cache_invalidation_callback_) {
    cache_invalidation_callback_();
  }
}

void DungeonObjectInteraction::HandleCopySelected() {
  auto indices = selection_.GetSelectedIndices();
  if (indices.empty() || !rooms_)
    return;
  if (current_room_id_ < 0 || current_room_id_ >= 296)
    return;

  auto& room = (*rooms_)[current_room_id_];
  const auto& objects = room.GetTileObjects();

  // Copy selected objects to clipboard
  clipboard_.clear();
  for (size_t index : indices) {
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
  ImVec4 preview_fill = ImVec4(theme.dungeon_selection_primary.x,
                               theme.dungeon_selection_primary.y,
                               theme.dungeon_selection_primary.z,
                               0.25f);  // Semi-transparent
  draw_list->AddRectFilled(preview_start, preview_end,
                           ImGui::GetColorU32(preview_fill));

  // Draw solid outline for visibility
  ImVec4 preview_outline = ImVec4(theme.dungeon_selection_primary.x,
                                  theme.dungeon_selection_primary.y,
                                  theme.dungeon_selection_primary.z,
                                  0.78f);  // More visible
  draw_list->AddRect(preview_start, preview_end,
                     ImGui::GetColorU32(preview_outline), 0.0f, 0, 2.0f);

  // Draw object ID text at corner
  std::string id_text = absl::StrFormat("0x%02X", preview_object_.id_);
  ImVec2 text_pos(preview_start.x + 2, preview_start.y + 2);
  draw_list->AddText(text_pos, ImGui::GetColorU32(theme.text_primary),
                     id_text.c_str());

  // Draw crosshair at placement position
  constexpr float crosshair_size = 8.0f;
  ImVec2 center(preview_start.x + (obj_width * scale) / 2,
                preview_start.y + (obj_height * scale) / 2);
  ImVec4 crosshair_color =
      ImVec4(theme.text_primary.x, theme.text_primary.y, theme.text_primary.z,
             0.78f);  // Slightly transparent
  ImU32 crosshair = ImGui::GetColorU32(crosshair_color);
  draw_list->AddLine(ImVec2(center.x - crosshair_size, center.y),
                     ImVec2(center.x + crosshair_size, center.y), crosshair,
                     1.5f);
  draw_list->AddLine(ImVec2(center.x, center.y - crosshair_size),
                     ImVec2(center.x, center.y + crosshair_size), crosshair,
                     1.5f);
}

void DungeonObjectInteraction::HandleScrollWheelResize() {
  const ImGuiIO& io = ImGui::GetIO();

  // Only resize if mouse wheel is being used
  if (io.MouseWheel == 0.0f)
    return;

  // Don't resize if placing an object
  if (object_loaded_)
    return;

  // Check if cursor is over a selected object
  if (!selection_.HasSelection())
    return;

  size_t hovered = GetHoveredObjectIndex();
  if (hovered == static_cast<size_t>(-1))
    return;

  // Only resize if hovering over a selected object
  if (!selection_.IsObjectSelected(hovered))
    return;

  if (!rooms_ || current_room_id_ < 0 || current_room_id_ >= 296)
    return;

  // Call mutation hook before changes
  if (mutation_hook_) {
    mutation_hook_();
  }

  auto& room = (*rooms_)[current_room_id_];
  auto& objects = room.GetTileObjects();

  // Determine resize delta (1 for scroll up, -1 for scroll down)
  int resize_delta = (io.MouseWheel > 0.0f) ? 1 : -1;

  // Resize all selected objects uniformly
  auto selected_indices = selection_.GetSelectedIndices();
  for (size_t index : selected_indices) {
    if (index >= objects.size())
      continue;

    auto& object = objects[index];

    // Current size value (0-15)
    int current_size = static_cast<int>(object.size_);
    int new_size = current_size + resize_delta;

    // Clamp to valid range (0-15)
    new_size = std::clamp(new_size, 0, 15);

    // Update object size
    object.size_ = static_cast<uint8_t>(new_size);
  }

  // Trigger cache invalidation and re-render
  if (cache_invalidation_callback_) {
    cache_invalidation_callback_();
  }
}

std::pair<int, int> DungeonObjectInteraction::CalculateObjectBounds(
    const zelda3::RoomObject& object) {
  // If we have a ROM, use ObjectDrawer to calculate accurate dimensions
  if (rom_) {
    if (!object_drawer_) {
      object_drawer_ = std::make_unique<zelda3::ObjectDrawer>(rom_, current_room_id_);
    }
    return object_drawer_->CalculateObjectDimensions(object);
  }

  // Fallback to naive calculation if no ROM available
  // Matches DungeonCanvasViewer::DrawRoomObjects logic
  int size_h = (object.size_ & 0x0F);
  int size_v = (object.size_ >> 4) & 0x0F;
  int width = (size_h + 1) * 8;
  int height = (size_v + 1) * 8;
  return {width, height};
}

size_t DungeonObjectInteraction::GetHoveredObjectIndex() const {
  if (!rooms_ || current_room_id_ < 0 || current_room_id_ >= 296)
    return static_cast<size_t>(-1);

  // Get mouse position
  const ImGuiIO& io = ImGui::GetIO();
  ImVec2 canvas_pos = canvas_->zero_point();
  ImVec2 mouse_pos = io.MousePos;
  ImVec2 canvas_mouse_pos =
      ImVec2(mouse_pos.x - canvas_pos.x, mouse_pos.y - canvas_pos.y);

  // Convert to room coordinates
  auto [room_x, room_y] =
      CanvasToRoomCoordinates(static_cast<int>(canvas_mouse_pos.x),
                              static_cast<int>(canvas_mouse_pos.y));

  // Check all objects in reverse order (top to bottom, prioritize recent)
  auto& room = (*rooms_)[current_room_id_];
  const auto& objects = room.GetTileObjects();

  // We need to cast away constness to call CalculateObjectBounds which might
  // initialize the drawer. This is safe as it doesn't modify logical state.
  auto* mutable_this = const_cast<DungeonObjectInteraction*>(this);

  for (size_t i = objects.size(); i > 0; --i) {
    size_t index = i - 1;
    const auto& object = objects[index];

    // Calculate object bounds using accurate logic
    auto [width, height] = mutable_this->CalculateObjectBounds(object);
    
    // Convert width/height (pixels) to tiles for comparison with room_x/room_y
    // room_x/room_y are in tiles (8x8 pixels)
    // object.x_/y_ are in tiles
    
    int obj_x = object.x_;
    int obj_y = object.y_;
    
    // Check if mouse is within object bounds
    // Note: room_x/y are tile coordinates. width/height are pixels.
    // We need to check pixel coordinates or convert width/height to tiles.
    // Let's check pixel coordinates for better precision if needed, 
    // but room_x/y are integers (tiles).
    
    // Convert mouse to pixels relative to room origin
    int mouse_pixel_x = static_cast<int>(canvas_mouse_pos.x);
    int mouse_pixel_y = static_cast<int>(canvas_mouse_pos.y);
    
    int obj_pixel_x = obj_x * 8;
    int obj_pixel_y = obj_y * 8;
    
    if (mouse_pixel_x >= obj_pixel_x && mouse_pixel_x < obj_pixel_x + width &&
        mouse_pixel_y >= obj_pixel_y && mouse_pixel_y < obj_pixel_y + height) {
      return index;
    }
  }

  return static_cast<size_t>(-1);
}

}  // namespace yaze::editor
