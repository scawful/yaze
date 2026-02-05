// Related header
#include "dungeon_object_interaction.h"
#include "absl/strings/str_format.h"

// C++ standard library headers
#include <algorithm>
#include <cmath>

// Third-party library headers
#include "imgui/imgui.h"

// Project headers
#include "app/editor/agent/agent_ui_theme.h"
#include "app/editor/dungeon/dungeon_coordinates.h"
#include "app/gfx/resource/arena.h"
#include "app/gui/core/icons.h"

namespace yaze::editor {

void DungeonObjectInteraction::HandleCanvasMouseInput() {
  const ImGuiIO& io = ImGui::GetIO();

  // Check if mouse is over the canvas
  if (!canvas_->IsMouseHovering()) {
    return;
  }

  // Handle Escape key to cancel any active placement mode
  if (ImGui::IsKeyPressed(ImGuiKey_Escape) &&
      mode_manager_.IsPlacementActive()) {
    CancelPlacement();
    return;
  }

  // Handle scroll wheel for resizing selected objects
  HandleScrollWheelResize();

  // Handle layer assignment keyboard shortcuts (1, 2, 3 keys)
  HandleLayerKeyboardShortcuts();

  // Get mouse position relative to canvas
  ImVec2 mouse_pos = io.MousePos;
  ImVec2 canvas_pos = canvas_->zero_point();

  // Convert to canvas coordinates
  ImVec2 canvas_mouse_pos =
      ImVec2(mouse_pos.x - canvas_pos.x, mouse_pos.y - canvas_pos.y);

  // Handle left mouse click based on current mode
  if (ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
    switch (mode_manager_.GetMode()) {
      case InteractionMode::PlaceDoor:
        PlaceDoorAtPosition(static_cast<int>(canvas_mouse_pos.x),
                            static_cast<int>(canvas_mouse_pos.y));
        break;

      case InteractionMode::PlaceSprite:
        PlaceSpriteAtPosition(static_cast<int>(canvas_mouse_pos.x),
                              static_cast<int>(canvas_mouse_pos.y));
        break;

      case InteractionMode::PlaceItem:
        PlaceItemAtPosition(static_cast<int>(canvas_mouse_pos.x),
                            static_cast<int>(canvas_mouse_pos.y));
        break;

      case InteractionMode::PlaceObject: {
        auto [room_x, room_y] =
            CanvasToRoomCoordinates(static_cast<int>(canvas_mouse_pos.x),
                                    static_cast<int>(canvas_mouse_pos.y));
        PlaceObjectAtPosition(room_x, room_y);
        break;
      }

      case InteractionMode::Select:
      default:
        // Selection mode: try to select entity (door/sprite/item) first, then objects
        if (!TrySelectEntityAtCursor()) {
          // No entity - try to select object at cursor
          if (!TrySelectObjectAtCursor()) {
            const bool had_selection =
                selection_.HasSelection() || HasEntitySelection();
            // Clicked empty space - start rectangle selection
            if (!io.KeyShift && !io.KeyCtrl) {
              // Clear selection unless modifier held
              selection_.ClearSelection();
              ClearEntitySelection();
            }
            if (!had_selection) {
              // Begin rectangle selection for multi-select only when no prior
              // selection exists (matches ZScream behavior).
              mode_manager_.SetMode(InteractionMode::RectangleSelect);
              auto& state = mode_manager_.GetModeState();
              state.rect_start_x = static_cast<int>(canvas_mouse_pos.x);
              state.rect_start_y = static_cast<int>(canvas_mouse_pos.y);
              state.rect_end_x = state.rect_start_x;
              state.rect_end_y = state.rect_start_y;
              selection_.BeginRectangleSelection(state.rect_start_x,
                                                 state.rect_start_y);
            }
          } else {
            // Clicked on an object - start drag if we have selected objects
            ClearEntitySelection();  // Clear entity selection when selecting object
            if (selection_.HasSelection()) {
              mode_manager_.SetMode(InteractionMode::DraggingObjects);
              auto& state = mode_manager_.GetModeState();
              state.drag_start = canvas_mouse_pos;
              state.drag_current = canvas_mouse_pos;
              state.duplicate_on_drag = false;
            }
          }
        }
        break;
    }
  }

  // Handle entity drag if active
  if (mode_manager_.GetMode() == InteractionMode::DraggingEntity) {
    HandleEntityDrag();
  }

  // Handle drag in progress
  if (mode_manager_.GetMode() == InteractionMode::DraggingObjects &&
      ImGui::IsMouseDragging(ImGuiMouseButton_Left)) {
    auto& state = mode_manager_.GetModeState();
    state.drag_current = canvas_mouse_pos;
    state.duplicate_on_drag = state.duplicate_on_drag || ImGui::GetIO().KeyAlt;
    DrawDragPreview();
  }

  // Handle mouse release - complete drag operation
  if (ImGui::IsMouseReleased(ImGuiMouseButton_Left) &&
      mode_manager_.GetMode() == InteractionMode::DraggingObjects) {
    auto& state = mode_manager_.GetModeState();

    // Apply drag transformation to selected objects
    auto selected_indices = selection_.GetSelectedIndices();
    if (!selected_indices.empty() && rooms_ && current_room_id_ >= 0 &&
        current_room_id_ < 296) {
      interaction_context_.NotifyMutation();

      auto& room = (*rooms_)[current_room_id_];
      ImVec2 drag_delta = ImVec2(state.drag_current.x - state.drag_start.x,
                                 state.drag_current.y - state.drag_start.y);
      drag_delta = ApplyDragModifiers(drag_delta);

      // Convert pixel delta to tile delta
      int tile_delta_x = static_cast<int>(drag_delta.x) / 8;
      int tile_delta_y = static_cast<int>(drag_delta.y) / 8;

      // Only apply if there's meaningful movement
      if (tile_delta_x != 0 || tile_delta_y != 0) {
        auto& objects = room.GetTileObjects();
        auto target_indices = selected_indices;
        if (state.duplicate_on_drag) {
          std::vector<zelda3::RoomObject> clones;
          clones.reserve(selected_indices.size());
          for (size_t index : selected_indices) {
            if (index < objects.size()) {
              clones.push_back(objects[index]);
            }
          }
          const size_t base_index = objects.size();
          for (auto& clone : clones) {
            objects.push_back(clone);
          }

          selection_.ClearSelection();
          target_indices.clear();
          for (size_t i = 0; i < clones.size(); ++i) {
            const size_t new_index = base_index + i;
            target_indices.push_back(new_index);
            selection_.SelectObject(new_index,
                                    ObjectSelection::SelectionMode::Add);
          }
        }

        for (size_t index : target_indices) {
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

        // Ensure renderers refresh after positional change
        room.MarkObjectsDirty();

        // Trigger cache invalidation and re-render
        interaction_context_.NotifyInvalidateCache();
      }
    }

    // Return to select mode
    mode_manager_.SetMode(InteractionMode::Select);
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

    constexpr int kMinRectPixels = 6;
    if (io.KeyAlt || !selection_.IsRectangleLargeEnough(kMinRectPixels)) {
      selection_.CancelRectangleSelection();
      if (!io.KeyShift && !io.KeyCtrl) {
        selection_.ClearSelection();
        ClearEntitySelection();
      }
    } else {
      // Determine selection mode based on modifiers
      ObjectSelection::SelectionMode mode =
          ObjectSelection::SelectionMode::Single;
      if (io.KeyShift) {
        mode = ObjectSelection::SelectionMode::Add;
      } else if (io.KeyCtrl) {
        mode = ObjectSelection::SelectionMode::Toggle;
      }

      selection_.EndRectangleSelection(room.GetTileObjects(), mode);
    }

    mode_manager_.SetMode(InteractionMode::Select);
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
        // Use GetSelectionBounds for accurate visual bounds + offsets
        // (doesn't inflate size=0 to 32 like the game's GetSize_1to15or32)
        auto& dim_table = zelda3::ObjectDimensionTable::Get();
        if (dim_table.IsLoaded()) {
          auto bounds = dim_table.GetSelectionBounds(obj.id_, obj.size_);
          return std::make_tuple(bounds.offset_x * 8, bounds.offset_y * 8,
                                 bounds.width * 8, bounds.height * 8);
        }
        // Fallback to drawer (aligns with render) if table not loaded
        if (object_drawer_) {
          auto dims = object_drawer_->CalculateObjectDimensions(obj);
          return std::make_tuple(0, 0, dims.first, dims.second);
        }
        return std::make_tuple(0, 0, 16, 16);  // Safe fallback
      });

  // Enhanced hover tooltip showing object info (always visible on hover)
  // Skip completely in exclusive entity mode (door/sprite/item selected)
  if (is_entity_mode_) {
    return;  // Entity mode active - no object tooltips or hover
  }

  if (canvas_->IsMouseHovering()) {
    // Also skip tooltip if cursor is over a door/sprite/item entity (not selected yet)
    ImGuiIO& io = ImGui::GetIO();
    ImVec2 canvas_pos = canvas_->zero_point();
    int cursor_x = static_cast<int>(io.MousePos.x - canvas_pos.x);
    int cursor_y = static_cast<int>(io.MousePos.y - canvas_pos.y);
    auto entity_at_cursor = GetEntityAtPosition(cursor_x, cursor_y);
    if (entity_at_cursor.has_value()) {
      // Entity has priority - skip object tooltip, DrawHoverHighlight will also skip
      DrawHoverHighlight(objects);
      return;
    }

    size_t hovered_index = GetHoveredObjectIndex();
    if (hovered_index != static_cast<size_t>(-1) &&
        hovered_index < objects.size()) {
      const auto& object = objects[hovered_index];
      std::string object_name = zelda3::GetObjectName(object.id_);
      int subtype = zelda3::GetObjectSubtype(object.id_);
      int layer = object.GetLayerValue();

      // Get subtype name
      const char* subtype_names[] = {"Unknown", "Type 1", "Type 2", "Type 3"};
      const char* subtype_name =
          (subtype >= 0 && subtype <= 3) ? subtype_names[subtype] : "Unknown";

      // Build informative tooltip
      std::string tooltip;
      tooltip += object_name;
      tooltip += " (" + std::string(subtype_name) + ")";
      tooltip += "\n";
      tooltip += "ID: 0x" + absl::StrFormat("%03X", object.id_);
      tooltip += " | Layer: " + std::to_string(layer + 1);
      tooltip += " | Pos: (" + std::to_string(object.x_) + ", " +
                 std::to_string(object.y_) + ")";
      tooltip += "\nSize: " + std::to_string(object.size_) + " (0x" +
                 absl::StrFormat("%02X", object.size_) + ")";

      if (selection_.IsObjectSelected(hovered_index)) {
        tooltip += "\n" ICON_MD_MOUSE " Scroll wheel to resize";
        tooltip += "\n" ICON_MD_DRAG_INDICATOR " Drag to move";
      } else {
        tooltip += "\n" ICON_MD_TOUCH_APP " Click to select";
      }

      ImGui::SetTooltip("%s", tooltip.c_str());
    }
  }

  // Draw hover highlight for non-selected objects
  DrawHoverHighlight(objects);
}

void DungeonObjectInteraction::DrawHoverHighlight(
    const std::vector<zelda3::RoomObject>& objects) {
  if (!canvas_->IsMouseHovering())
    return;

  // Skip all object hover in exclusive entity mode (door/sprite/item selected)
  if (is_entity_mode_)
    return;

  // Don't show object hover highlight if cursor is over a door/sprite/item entity
  // Entities take priority over objects for interaction
  ImGuiIO& io = ImGui::GetIO();
  ImVec2 canvas_pos = canvas_->zero_point();
  int cursor_canvas_x = static_cast<int>(io.MousePos.x - canvas_pos.x);
  int cursor_canvas_y = static_cast<int>(io.MousePos.y - canvas_pos.y);
  auto entity_at_cursor = GetEntityAtPosition(cursor_canvas_x, cursor_canvas_y);
  if (entity_at_cursor.has_value()) {
    return;  // Entity has priority - skip object hover highlight
  }

  size_t hovered_index = GetHoveredObjectIndex();
  if (hovered_index == static_cast<size_t>(-1) ||
      hovered_index >= objects.size()) {
    return;
  }

  // Don't draw hover highlight if object is already selected
  if (selection_.IsObjectSelected(hovered_index)) {
    return;
  }

  const auto& object = objects[hovered_index];
  const auto& theme = AgentUI::GetTheme();
  ImDrawList* draw_list = ImGui::GetWindowDrawList();
  // canvas_pos already defined above for entity check
  float scale = canvas_->global_scale();

  // Calculate object position and dimensions
  auto [obj_x, obj_y] =
      selection_.RoomToCanvasCoordinates(object.x_, object.y_);

  int pixel_width, pixel_height;
  auto& dim_table = zelda3::ObjectDimensionTable::Get();
  if (dim_table.IsLoaded()) {
    auto [w_tiles, h_tiles] =
        dim_table.GetSelectionDimensions(object.id_, object.size_);
    pixel_width = w_tiles * 8;
    pixel_height = h_tiles * 8;
  } else if (object_drawer_) {
    auto dims = object_drawer_->CalculateObjectDimensions(object);
    pixel_width = dims.first;
    pixel_height = dims.second;
  } else {
    pixel_width = 16;
    pixel_height = 16;
  }

  // Apply scale and canvas offset
  ImVec2 obj_start(canvas_pos.x + obj_x * scale, canvas_pos.y + obj_y * scale);
  ImVec2 obj_end(obj_start.x + pixel_width * scale,
                 obj_start.y + pixel_height * scale);

  // Expand slightly for visibility
  constexpr float margin = 2.0f;
  obj_start.x -= margin;
  obj_start.y -= margin;
  obj_end.x += margin;
  obj_end.y += margin;

  // Get layer-based color for consistent highlighting
  ImVec4 layer_color = selection_.GetLayerTypeColor(object);

  // Draw subtle hover highlight with layer-based color
  ImVec4 hover_fill = ImVec4(layer_color.x, layer_color.y, layer_color.z,
                             0.15f  // Very subtle fill
  );
  ImVec4 hover_border =
      ImVec4(layer_color.x, layer_color.y, layer_color.z,
             0.6f  // Visible but not as prominent as selection
      );

  // Draw filled background for better visibility
  draw_list->AddRectFilled(obj_start, obj_end, ImGui::GetColorU32(hover_fill));

  // Draw dashed-style border (simulated with thinner line)
  draw_list->AddRect(obj_start, obj_end, ImGui::GetColorU32(hover_border), 0.0f,
                     0, 1.5f);
}

void DungeonObjectInteraction::PlaceObjectAtPosition(int room_x, int room_y) {
  if (!mode_manager_.IsObjectPlacementActive() || preview_object_.id_ < 0 ||
      !rooms_)
    return;

  if (current_room_id_ < 0 || current_room_id_ >= 296)
    return;

  interaction_context_.NotifyMutation();

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
  interaction_context_.NotifyInvalidateCache();

  // Exit placement mode after placing a single object
  CancelPlacement();
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
  if (mode_manager_.GetMode() != InteractionMode::DraggingObjects ||
      selected_indices.empty() || !rooms_)
    return;
  if (current_room_id_ < 0 || current_room_id_ >= 296)
    return;

  // Draw drag preview for selected objects
  ImDrawList* draw_list = ImGui::GetWindowDrawList();
  ImVec2 canvas_pos = canvas_->zero_point();
  const auto& state = mode_manager_.GetModeState();
  ImVec2 drag_delta = ImVec2(state.drag_current.x - state.drag_start.x,
                             state.drag_current.y - state.drag_start.y);
  drag_delta = ApplyDragModifiers(drag_delta);

  auto& room = (*rooms_)[current_room_id_];
  const auto& objects = room.GetTileObjects();

  // Draw preview of where objects would be moved
  for (size_t index : selected_indices) {
    if (index < objects.size()) {
      const auto& object = objects[index];
      auto [canvas_x, canvas_y] = RoomToCanvasCoordinates(object.x_, object.y_);

      // Calculate object size using shared dimension logic
      auto [obj_width, obj_height] = CalculateObjectBounds(object);

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
    std::array<zelda3::Room, dungeon_coords::kRoomCount>* rooms, int room_id) {
  rooms_ = rooms;
  current_room_id_ = room_id;
  interaction_context_.rooms = rooms;
  interaction_context_.current_room_id = room_id;
  entity_coordinator_.SetContext(&interaction_context_);
}

void DungeonObjectInteraction::SetPreviewObject(
    const zelda3::RoomObject& object, bool loaded) {
  preview_object_ = object;

  if (loaded && object.id_ >= 0) {
    // Enter object placement mode
    mode_manager_.SetMode(InteractionMode::PlaceObject);
    mode_manager_.GetModeState().preview_object = object;
    RenderGhostPreviewBitmap();
  } else {
    // Exit placement mode if not loaded
    if (mode_manager_.GetMode() == InteractionMode::PlaceObject) {
      mode_manager_.SetMode(InteractionMode::Select);
    }
    ghost_preview_buffer_.reset();
  }
}

void DungeonObjectInteraction::RenderGhostPreviewBitmap() {
  if (!rom_ || !rom_->is_loaded()) {
    ghost_preview_buffer_.reset();
    return;
  }

  // Need room graphics to render the object
  if (!rooms_ || current_room_id_ < 0 ||
      current_room_id_ >= static_cast<int>(rooms_->size())) {
    ghost_preview_buffer_.reset();
    return;
  }

  auto& room = (*rooms_)[current_room_id_];
  if (!room.IsLoaded()) {
    ghost_preview_buffer_.reset();
    return;
  }

  // Calculate object dimensions
  auto [width, height] = CalculateObjectBounds(preview_object_);
  width = std::max(width, 16);
  height = std::max(height, 16);

  // Create or resize the buffer
  ghost_preview_buffer_ =
      std::make_unique<gfx::BackgroundBuffer>(width, height);

  // Get graphics data from the room
  const uint8_t* gfx_data = room.get_gfx_buffer().data();

  // Render the preview object
  zelda3::ObjectDrawer drawer(rom_, current_room_id_, gfx_data);
  drawer.InitializeDrawRoutines();

  auto status =
      drawer.DrawObject(preview_object_, *ghost_preview_buffer_,
                        *ghost_preview_buffer_, current_palette_group_);
  if (!status.ok()) {
    ghost_preview_buffer_.reset();
    return;
  }

  // Create texture for the preview
  auto& bitmap = ghost_preview_buffer_->bitmap();
  if (bitmap.size() > 0) {
    gfx::Arena::Get().QueueTextureCommand(
        gfx::Arena::TextureCommandType::CREATE, &bitmap);
    gfx::Arena::Get().ProcessTextureQueue(nullptr);
  }
}

void DungeonObjectInteraction::ClearSelection() {
  selection_.ClearSelection();
  if (mode_manager_.GetMode() == InteractionMode::DraggingObjects) {
    mode_manager_.SetMode(InteractionMode::Select);
  }
}

bool DungeonObjectInteraction::TrySelectObjectAtCursor() {
  // Don't attempt object selection in exclusive entity mode
  if (is_entity_mode_)
    return false;

  size_t hovered = GetHoveredObjectIndex();
  if (hovered == static_cast<size_t>(-1)) {
    return false;
  }

  const ImGuiIO& io = ImGui::GetIO();
  if (io.KeyAlt) {
    if (selection_.IsObjectSelected(hovered)) {
      return true;
    }
    selection_.ClearSelection();
    return false;
  }
  ObjectSelection::SelectionMode mode = ObjectSelection::SelectionMode::Single;

  if (io.KeyShift) {
    mode = ObjectSelection::SelectionMode::Add;
  } else if (io.KeyCtrl) {
    mode = ObjectSelection::SelectionMode::Toggle;
  }

  selection_.SelectObject(hovered, mode);
  return true;
}

void DungeonObjectInteraction::HandleDeleteSelected() {
  auto indices = selection_.GetSelectedIndices();
  if (indices.empty() || !rooms_)
    return;
  if (current_room_id_ < 0 || current_room_id_ >= 296)
    return;

  interaction_context_.NotifyMutation();

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
  interaction_context_.NotifyInvalidateCache();
}

void DungeonObjectInteraction::HandleDeleteAllObjects() {
  if (!rooms_)
    return;
  if (current_room_id_ < 0 || current_room_id_ >= 296)
    return;

  auto& room = (*rooms_)[current_room_id_];
  if (room.GetTileObjects().empty()) {
    return;
  }

  interaction_context_.NotifyMutation();

  room.ClearTileObjects();
  selection_.ClearSelection();

  interaction_context_.NotifyInvalidateCache();
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

  interaction_context_.NotifyMutation();

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
    interaction_context_.NotifyInvalidateCache();
  }
}

void DungeonObjectInteraction::DrawGhostPreview() {
  // Draw entity-specific ghost previews based on current mode
  switch (mode_manager_.GetMode()) {
    case InteractionMode::PlaceDoor:
      DrawDoorGhostPreview();
      return;
    case InteractionMode::PlaceSprite:
      DrawSpriteGhostPreview();
      return;
    case InteractionMode::PlaceItem:
      DrawItemGhostPreview();
      return;
    case InteractionMode::PlaceObject:
      // Continue below to draw object ghost preview
      break;
    default:
      return;  // No ghost preview in other modes
  }

  // Only draw object ghost preview when in object placement mode
  if (preview_object_.id_ < 0)
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
  // Size is a single 4-bit value (0-15), NOT two separate nibbles
  int size = preview_object_.size_ & 0x0F;
  int obj_width, obj_height;
  if (preview_object_.id_ >= 0x60 && preview_object_.id_ <= 0x7F) {
    // Vertical objects
    obj_width = 16;
    obj_height = 16 + size * 16;
  } else {
    // Horizontal objects (default)
    obj_width = 16 + size * 16;
    obj_height = 16;
  }
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
  bool drew_bitmap = false;

  // Try to draw the rendered object preview bitmap
  if (ghost_preview_buffer_) {
    auto& bitmap = ghost_preview_buffer_->bitmap();
    if (bitmap.texture()) {
      // Draw the actual object graphics with transparency
      ImVec2 bitmap_end(preview_start.x + bitmap.width() * scale,
                        preview_start.y + bitmap.height() * scale);

      // Draw with semi-transparency (ghost effect)
      draw_list->AddImage((ImTextureID)(intptr_t)bitmap.texture(),
                          preview_start, bitmap_end, ImVec2(0, 0), ImVec2(1, 1),
                          IM_COL32(255, 255, 255, 180));  // Semi-transparent

      // Draw outline around the bitmap
      ImVec4 preview_outline = ImVec4(theme.dungeon_selection_primary.x,
                                      theme.dungeon_selection_primary.y,
                                      theme.dungeon_selection_primary.z,
                                      0.78f);  // More visible
      draw_list->AddRect(preview_start, bitmap_end,
                         ImGui::GetColorU32(preview_outline), 0.0f, 0, 2.0f);
      drew_bitmap = true;
    }
  }

  // Fallback: draw colored rectangle if no bitmap available
  if (!drew_bitmap) {
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
  }

  // Draw object ID text at corner
  std::string id_text = absl::StrFormat("0x%02X", preview_object_.id_);
  ImVec2 text_pos(preview_start.x + 2, preview_start.y + 2);

  // Draw text background for readability
  ImVec2 text_size = ImGui::CalcTextSize(id_text.c_str());
  draw_list->AddRectFilled(
      text_pos,
      ImVec2(text_pos.x + text_size.x + 4, text_pos.y + text_size.y + 2),
      IM_COL32(0, 0, 0, 180));
  draw_list->AddText(ImVec2(text_pos.x + 2, text_pos.y + 1),
                     ImGui::GetColorU32(theme.text_primary), id_text.c_str());

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

  // Don't resize if in any placement mode
  if (mode_manager_.IsPlacementActive())
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
  interaction_context_.NotifyMutation();

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

  room.MarkObjectsDirty();

  // Trigger cache invalidation and re-render
  interaction_context_.NotifyInvalidateCache();
}

std::pair<int, int> DungeonObjectInteraction::CalculateObjectBounds(
    const zelda3::RoomObject& object) {
  // Try dimension table first for consistency with selection/highlights
  auto& dim_table = zelda3::ObjectDimensionTable::Get();
  if (dim_table.IsLoaded()) {
    auto [w_tiles, h_tiles] = dim_table.GetDimensions(object.id_, object.size_);
    return {w_tiles * 8, h_tiles * 8};
  }

  // If we have a ROM, use ObjectDrawer to calculate accurate dimensions
  if (rom_) {
    if (!object_drawer_) {
      object_drawer_ =
          std::make_unique<zelda3::ObjectDrawer>(rom_, current_room_id_);
    }
    return object_drawer_->CalculateObjectDimensions(object);
  }

  // Fallback to simplified calculation if no ROM available
  // Size is a single 4-bit value (0-15), NOT two separate nibbles
  int size = object.size_ & 0x0F;
  int width, height;
  if (object.id_ >= 0x60 && object.id_ <= 0x7F) {
    // Vertical objects
    width = 16;
    height = 16 + size * 16;
  } else {
    // Horizontal objects (default)
    width = 16 + size * 16;
    height = 16;
  }
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

    if (!selection_.PassesLayerFilterForObject(object)) {
      continue;
    }

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

void DungeonObjectInteraction::SendSelectedToLayer(int target_layer) {
  auto indices = selection_.GetSelectedIndices();
  if (indices.empty() || !rooms_)
    return;
  if (current_room_id_ < 0 || current_room_id_ >= 296)
    return;

  // Validate target layer
  if (target_layer < 0 || target_layer > 2) {
    return;
  }

  interaction_context_.NotifyMutation();

  auto& room = (*rooms_)[current_room_id_];
  auto& objects = room.GetTileObjects();

  // Update layer for all selected objects
  for (size_t index : indices) {
    if (index < objects.size()) {
      objects[index].layer_ =
          static_cast<zelda3::RoomObject::LayerType>(target_layer);
    }
  }

  room.MarkObjectsDirty();

  // Trigger cache invalidation and re-render
  interaction_context_.NotifyInvalidateCache();
}

void DungeonObjectInteraction::SendSelectedToFront() {
  auto indices = selection_.GetSelectedIndices();
  if (indices.empty() || !rooms_)
    return;
  if (current_room_id_ < 0 || current_room_id_ >= 296)
    return;

  interaction_context_.NotifyMutation();

  auto& room = (*rooms_)[current_room_id_];
  auto& objects = room.GetTileObjects();

  // Move selected objects to the end of the list (drawn last = appears on top)
  // Process in reverse order to maintain relative order of selected objects
  std::vector<zelda3::RoomObject> selected_objects;
  std::vector<zelda3::RoomObject> other_objects;

  for (size_t i = 0; i < objects.size(); ++i) {
    if (std::find(indices.begin(), indices.end(), i) != indices.end()) {
      selected_objects.push_back(objects[i]);
    } else {
      other_objects.push_back(objects[i]);
    }
  }

  // Rebuild: other objects first, then selected objects at end
  objects.clear();
  objects.insert(objects.end(), other_objects.begin(), other_objects.end());
  objects.insert(objects.end(), selected_objects.begin(),
                 selected_objects.end());

  // Update selection to new indices (at end of list)
  selection_.ClearSelection();
  for (size_t i = 0; i < selected_objects.size(); ++i) {
    selection_.SelectObject(other_objects.size() + i,
                            ObjectSelection::SelectionMode::Add);
  }

  room.MarkObjectsDirty();

  interaction_context_.NotifyInvalidateCache();
}

void DungeonObjectInteraction::SendSelectedToBack() {
  auto indices = selection_.GetSelectedIndices();
  if (indices.empty() || !rooms_)
    return;
  if (current_room_id_ < 0 || current_room_id_ >= 296)
    return;

  interaction_context_.NotifyMutation();

  auto& room = (*rooms_)[current_room_id_];
  auto& objects = room.GetTileObjects();

  // Move selected objects to the beginning of the list (drawn first = appears behind)
  std::vector<zelda3::RoomObject> selected_objects;
  std::vector<zelda3::RoomObject> other_objects;

  for (size_t i = 0; i < objects.size(); ++i) {
    if (std::find(indices.begin(), indices.end(), i) != indices.end()) {
      selected_objects.push_back(objects[i]);
    } else {
      other_objects.push_back(objects[i]);
    }
  }

  // Rebuild: selected objects first, then other objects
  objects.clear();
  objects.insert(objects.end(), selected_objects.begin(),
                 selected_objects.end());
  objects.insert(objects.end(), other_objects.begin(), other_objects.end());

  // Update selection to new indices (at start of list)
  selection_.ClearSelection();
  for (size_t i = 0; i < selected_objects.size(); ++i) {
    selection_.SelectObject(i, ObjectSelection::SelectionMode::Add);
  }

  room.MarkObjectsDirty();

  interaction_context_.NotifyInvalidateCache();
}

void DungeonObjectInteraction::BringSelectedForward() {
  auto indices = selection_.GetSelectedIndices();
  if (indices.empty() || !rooms_)
    return;
  if (current_room_id_ < 0 || current_room_id_ >= 296)
    return;

  auto& room = (*rooms_)[current_room_id_];
  auto& objects = room.GetTileObjects();

  // Move each selected object up one position (towards end of list)
  // Process from end to start to avoid shifting issues
  std::sort(indices.begin(), indices.end());

  // Check if any selected object is already at the end
  bool all_at_end = true;
  for (size_t idx : indices) {
    if (idx < objects.size() - 1) {
      all_at_end = false;
      break;
    }
  }
  if (all_at_end)
    return;

  interaction_context_.NotifyMutation();

  // Track new indices after moves
  std::vector<size_t> new_indices;

  // Process from end to avoid index shifting issues
  for (auto it = indices.rbegin(); it != indices.rend(); ++it) {
    size_t idx = *it;
    if (idx < objects.size() - 1) {
      // Swap with next object
      std::swap(objects[idx], objects[idx + 1]);
      new_indices.push_back(idx + 1);
    } else {
      new_indices.push_back(idx);
    }
  }

  // Update selection
  selection_.ClearSelection();
  for (size_t idx : new_indices) {
    selection_.SelectObject(idx, ObjectSelection::SelectionMode::Add);
  }

  room.MarkObjectsDirty();

  interaction_context_.NotifyInvalidateCache();
}

void DungeonObjectInteraction::SendSelectedBackward() {
  auto indices = selection_.GetSelectedIndices();
  if (indices.empty() || !rooms_)
    return;
  if (current_room_id_ < 0 || current_room_id_ >= 296)
    return;

  auto& room = (*rooms_)[current_room_id_];
  auto& objects = room.GetTileObjects();

  // Move each selected object down one position (towards start of list)
  // Process from start to end to avoid shifting issues
  std::sort(indices.begin(), indices.end());

  // Check if any selected object is already at the start
  bool all_at_start = true;
  for (size_t idx : indices) {
    if (idx > 0) {
      all_at_start = false;
      break;
    }
  }
  if (all_at_start)
    return;

  interaction_context_.NotifyMutation();

  // Track new indices after moves
  std::vector<size_t> new_indices;

  // Process from start to avoid index shifting issues
  for (size_t idx : indices) {
    if (idx > 0) {
      // Swap with previous object
      std::swap(objects[idx], objects[idx - 1]);
      new_indices.push_back(idx - 1);
    } else {
      new_indices.push_back(idx);
    }
  }

  // Update selection
  selection_.ClearSelection();
  for (size_t idx : new_indices) {
    selection_.SelectObject(idx, ObjectSelection::SelectionMode::Add);
  }

  room.MarkObjectsDirty();

  interaction_context_.NotifyInvalidateCache();
}

void DungeonObjectInteraction::HandleLayerKeyboardShortcuts() {
  // Only process if we have selected objects
  if (!selection_.HasSelection())
    return;

  // Only when not typing in a text field
  if (ImGui::IsAnyItemActive())
    return;

  // Check for layer assignment shortcuts (1, 2, 3 keys)
  if (ImGui::IsKeyPressed(ImGuiKey_1)) {
    SendSelectedToLayer(0);  // Layer 1 (BG1)
  } else if (ImGui::IsKeyPressed(ImGuiKey_2)) {
    SendSelectedToLayer(1);  // Layer 2 (BG2)
  } else if (ImGui::IsKeyPressed(ImGuiKey_3)) {
    SendSelectedToLayer(2);  // Layer 3 (BG3)
  }

  // Object ordering shortcuts
  // Ctrl+Shift+] = Bring to Front, Ctrl+Shift+[ = Send to Back
  // Ctrl+] = Bring Forward, Ctrl+[ = Send Backward
  auto& io = ImGui::GetIO();
  if (io.KeyCtrl && io.KeyShift) {
    if (ImGui::IsKeyPressed(ImGuiKey_RightBracket)) {
      SendSelectedToFront();
    } else if (ImGui::IsKeyPressed(ImGuiKey_LeftBracket)) {
      SendSelectedToBack();
    }
  } else if (io.KeyCtrl) {
    if (ImGui::IsKeyPressed(ImGuiKey_RightBracket)) {
      BringSelectedForward();
    } else if (ImGui::IsKeyPressed(ImGuiKey_LeftBracket)) {
      SendSelectedBackward();
    }
  }
}

// ============================================================================
// Door Placement Methods
// ============================================================================

void DungeonObjectInteraction::SetDoorPlacementMode(bool enabled,
                                                    zelda3::DoorType type) {
  if (enabled) {
    mode_manager_.SetMode(InteractionMode::PlaceDoor);
    mode_manager_.GetModeState().preview_door_type = type;
    ghost_preview_buffer_.reset();  // Clear object ghost preview
  } else {
    if (mode_manager_.GetMode() == InteractionMode::PlaceDoor) {
      mode_manager_.SetMode(InteractionMode::Select);
    }
  }
}

void DungeonObjectInteraction::DrawDoorGhostPreview() {
  // Only draw if door placement mode is active
  if (mode_manager_.GetMode() != InteractionMode::PlaceDoor)
    return;

  // Check if mouse is over the canvas
  if (!canvas_->IsMouseHovering())
    return;

  const ImGuiIO& io = ImGui::GetIO();
  ImVec2 canvas_pos = canvas_->zero_point();
  ImVec2 mouse_pos = io.MousePos;

  // Convert mouse position to canvas coordinates (in pixels)
  int canvas_x = static_cast<int>(mouse_pos.x - canvas_pos.x);
  int canvas_y = static_cast<int>(mouse_pos.y - canvas_pos.y);

  // Detect which wall the cursor is near
  zelda3::DoorDirection direction;
  if (!zelda3::DoorPositionManager::DetectWallFromPosition(canvas_x, canvas_y,
                                                           direction)) {
    // Not near a wall - don't show preview
    return;
  }

  // Snap to nearest valid door position
  uint8_t position = zelda3::DoorPositionManager::SnapToNearestPosition(
      canvas_x, canvas_y, direction);

  // Store detected values for placement
  auto& state = mode_manager_.GetModeState();
  state.detected_door_direction = direction;
  state.snapped_door_position = position;

  // Get door position in tile coordinates
  auto [tile_x, tile_y] =
      zelda3::DoorPositionManager::PositionToTileCoords(position, direction);

  // Get door dimensions
  auto dims = zelda3::GetDoorDimensions(direction);
  int door_width_px = dims.width_tiles * 8;
  int door_height_px = dims.height_tiles * 8;

  // Convert to canvas pixel coordinates
  auto [snap_canvas_x, snap_canvas_y] = RoomToCanvasCoordinates(tile_x, tile_y);

  // Draw ghost preview
  ImDrawList* draw_list = ImGui::GetWindowDrawList();
  float scale = canvas_->global_scale();

  ImVec2 preview_start(canvas_pos.x + snap_canvas_x * scale,
                       canvas_pos.y + snap_canvas_y * scale);
  ImVec2 preview_end(preview_start.x + door_width_px * scale,
                     preview_start.y + door_height_px * scale);

  const auto& theme = AgentUI::GetTheme();

  // Draw semi-transparent filled rectangle
  ImU32 fill_color = IM_COL32(theme.dungeon_selection_primary.x * 255,
                              theme.dungeon_selection_primary.y * 255,
                              theme.dungeon_selection_primary.z * 255,
                              80);  // Semi-transparent
  draw_list->AddRectFilled(preview_start, preview_end, fill_color);

  // Draw outline
  ImVec4 outline_color = ImVec4(theme.dungeon_selection_primary.x,
                                theme.dungeon_selection_primary.y,
                                theme.dungeon_selection_primary.z, 0.9f);
  draw_list->AddRect(preview_start, preview_end,
                     ImGui::GetColorU32(outline_color), 0.0f, 0, 2.0f);

  // Draw door type label
  const char* type_name =
      std::string(zelda3::GetDoorTypeName(GetPreviewDoorType())).c_str();
  const char* dir_name =
      std::string(zelda3::GetDoorDirectionName(direction)).c_str();
  char label[64];
  snprintf(label, sizeof(label), "%s (%s)", type_name, dir_name);

  ImVec2 text_pos(preview_start.x, preview_start.y - 16 * scale);
  draw_list->AddText(text_pos, IM_COL32(255, 255, 255, 200), label);
}

void DungeonObjectInteraction::PlaceDoorAtPosition(int canvas_x, int canvas_y) {
  if (mode_manager_.GetMode() != InteractionMode::PlaceDoor || !rooms_)
    return;

  if (current_room_id_ < 0 || current_room_id_ >= 296)
    return;

  // Detect wall from position
  zelda3::DoorDirection direction;
  if (!zelda3::DoorPositionManager::DetectWallFromPosition(canvas_x, canvas_y,
                                                           direction)) {
    // Not near a wall - can't place door
    return;
  }

  // Snap to nearest valid door position
  uint8_t position = zelda3::DoorPositionManager::SnapToNearestPosition(
      canvas_x, canvas_y, direction);

  // Validate position
  if (!zelda3::DoorPositionManager::IsValidPosition(position, direction)) {
    return;
  }

  interaction_context_.NotifyMutation();

  // Create the door
  zelda3::Room::Door new_door;
  new_door.position = position;
  new_door.type = GetPreviewDoorType();
  new_door.direction = direction;
  // Encode bytes for ROM storage
  auto [byte1, byte2] = new_door.EncodeBytes();
  new_door.byte1 = byte1;
  new_door.byte2 = byte2;

  // Add door to room
  auto& room = (*rooms_)[current_room_id_];
  room.AddDoor(new_door);

  // Trigger cache invalidation
  interaction_context_.NotifyInvalidateCache();
}

// ============================================================================
// Sprite Placement Methods
// ============================================================================

void DungeonObjectInteraction::SetSpritePlacementMode(bool enabled,
                                                      uint8_t sprite_id) {
  if (enabled) {
    mode_manager_.SetMode(InteractionMode::PlaceSprite);
    mode_manager_.GetModeState().preview_sprite_id = sprite_id;
    ghost_preview_buffer_.reset();  // Clear object ghost preview
  } else {
    if (mode_manager_.GetMode() == InteractionMode::PlaceSprite) {
      mode_manager_.SetMode(InteractionMode::Select);
    }
  }
}

void DungeonObjectInteraction::DrawSpriteGhostPreview() {
  if (mode_manager_.GetMode() != InteractionMode::PlaceSprite)
    return;

  if (!canvas_->IsMouseHovering())
    return;

  const ImGuiIO& io = ImGui::GetIO();
  ImVec2 canvas_pos = canvas_->zero_point();
  ImVec2 mouse_pos = io.MousePos;
  float scale = canvas_->global_scale();

  // Convert to room coordinates (sprites use 16-pixel grid)
  int canvas_x = static_cast<int>((mouse_pos.x - canvas_pos.x) / scale);
  int canvas_y = static_cast<int>((mouse_pos.y - canvas_pos.y) / scale);

  // Snap to 16-pixel grid (sprite coordinate system)
  int snapped_x = (canvas_x / 16) * 16;
  int snapped_y = (canvas_y / 16) * 16;

  // Draw ghost rectangle for sprite preview
  ImVec2 rect_min(canvas_pos.x + snapped_x * scale,
                  canvas_pos.y + snapped_y * scale);
  ImVec2 rect_max(rect_min.x + 16 * scale, rect_min.y + 16 * scale);

  // Semi-transparent green for sprites
  ImU32 fill_color = IM_COL32(50, 200, 50, 100);
  ImU32 outline_color = IM_COL32(50, 255, 50, 200);

  canvas_->draw_list()->AddRectFilled(rect_min, rect_max, fill_color);
  canvas_->draw_list()->AddRect(rect_min, rect_max, outline_color, 0.0f, 0,
                                2.0f);

  // Draw sprite ID label
  std::string label = absl::StrFormat("%02X", GetPreviewSpriteId());
  canvas_->draw_list()->AddText(rect_min, IM_COL32(255, 255, 255, 255),
                                label.c_str());
}

void DungeonObjectInteraction::PlaceSpriteAtPosition(int canvas_x,
                                                     int canvas_y) {
  if (mode_manager_.GetMode() != InteractionMode::PlaceSprite || !rooms_)
    return;

  if (current_room_id_ < 0 || current_room_id_ >= 296)
    return;

  float scale = canvas_->global_scale();
  if (scale <= 0.0f)
    scale = 1.0f;

  // Convert to sprite coordinates (16-pixel units)
  int sprite_x = canvas_x / static_cast<int>(16 * scale);
  int sprite_y = canvas_y / static_cast<int>(16 * scale);

  // Clamp to valid range (0-31 for each axis in a 512x512 room)
  sprite_x = std::clamp(sprite_x, 0, 31);
  sprite_y = std::clamp(sprite_y, 0, 31);

  interaction_context_.NotifyMutation();

  // Create the sprite
  zelda3::Sprite new_sprite(GetPreviewSpriteId(),
                            static_cast<uint8_t>(sprite_x),
                            static_cast<uint8_t>(sprite_y), 0, 0);

  // Add sprite to room
  auto& room = (*rooms_)[current_room_id_];
  room.GetSprites().push_back(new_sprite);

  // Trigger cache invalidation
  interaction_context_.NotifyInvalidateCache();
}

// ============================================================================
// Item Placement Methods
// ============================================================================

void DungeonObjectInteraction::SetItemPlacementMode(bool enabled,
                                                    uint8_t item_id) {
  if (enabled) {
    mode_manager_.SetMode(InteractionMode::PlaceItem);
    mode_manager_.GetModeState().preview_item_id = item_id;
    ghost_preview_buffer_.reset();  // Clear object ghost preview
  } else {
    if (mode_manager_.GetMode() == InteractionMode::PlaceItem) {
      mode_manager_.SetMode(InteractionMode::Select);
    }
  }
}

void DungeonObjectInteraction::DrawItemGhostPreview() {
  if (mode_manager_.GetMode() != InteractionMode::PlaceItem)
    return;

  if (!canvas_->IsMouseHovering())
    return;

  const ImGuiIO& io = ImGui::GetIO();
  ImVec2 canvas_pos = canvas_->zero_point();
  ImVec2 mouse_pos = io.MousePos;
  float scale = canvas_->global_scale();

  // Convert to room coordinates (items use 8-pixel grid for fine positioning)
  int canvas_x = static_cast<int>((mouse_pos.x - canvas_pos.x) / scale);
  int canvas_y = static_cast<int>((mouse_pos.y - canvas_pos.y) / scale);

  // Snap to 8-pixel grid
  int snapped_x = (canvas_x / 8) * 8;
  int snapped_y = (canvas_y / 8) * 8;

  // Draw ghost rectangle for item preview
  ImVec2 rect_min(canvas_pos.x + snapped_x * scale,
                  canvas_pos.y + snapped_y * scale);
  ImVec2 rect_max(rect_min.x + 16 * scale, rect_min.y + 16 * scale);

  // Semi-transparent yellow for items
  ImU32 fill_color = IM_COL32(200, 200, 50, 100);
  ImU32 outline_color = IM_COL32(255, 255, 50, 200);

  canvas_->draw_list()->AddRectFilled(rect_min, rect_max, fill_color);
  canvas_->draw_list()->AddRect(rect_min, rect_max, outline_color, 0.0f, 0,
                                2.0f);

  // Draw item ID label
  std::string label = absl::StrFormat("%02X", GetPreviewItemId());
  canvas_->draw_list()->AddText(rect_min, IM_COL32(255, 255, 255, 255),
                                label.c_str());
}

void DungeonObjectInteraction::PlaceItemAtPosition(int canvas_x, int canvas_y) {
  if (mode_manager_.GetMode() != InteractionMode::PlaceItem || !rooms_)
    return;

  if (current_room_id_ < 0 || current_room_id_ >= 296)
    return;

  float scale = canvas_->global_scale();
  if (scale <= 0.0f)
    scale = 1.0f;

  // Convert to pixel coordinates
  int pixel_x = canvas_x / static_cast<int>(scale);
  int pixel_y = canvas_y / static_cast<int>(scale);

  // PotItem position encoding:
  // high byte * 16 = Y, low byte * 4 = X
  // So: X = pixel_x / 4, Y = pixel_y / 16
  int encoded_x = pixel_x / 4;
  int encoded_y = pixel_y / 16;

  // Clamp to valid range
  encoded_x = std::clamp(encoded_x, 0, 255);
  encoded_y = std::clamp(encoded_y, 0, 255);

  interaction_context_.NotifyMutation();

  // Create the pot item
  zelda3::PotItem new_item;
  new_item.position = static_cast<uint16_t>((encoded_y << 8) | encoded_x);
  new_item.item = GetPreviewItemId();

  // Add item to room
  auto& room = (*rooms_)[current_room_id_];
  room.GetPotItems().push_back(new_item);

  // Trigger cache invalidation
  interaction_context_.NotifyInvalidateCache();
}

// ============================================================================
// Entity Selection Methods (Doors, Sprites, Items)
// ============================================================================

void DungeonObjectInteraction::SelectEntity(EntityType type, size_t index) {
  // Clear object selection when selecting an entity
  if (type != EntityType::Object) {
    selection_.ClearSelection();
  }

  selected_entity_.type = type;
  selected_entity_.index = index;

  // Enter exclusive entity mode - suppresses all object interactions
  is_entity_mode_ = (type != EntityType::None && type != EntityType::Object);

  interaction_context_.NotifyEntityChanged();
}

void DungeonObjectInteraction::ClearEntitySelection() {
  selected_entity_.type = EntityType::None;
  selected_entity_.index = 0;
  if (mode_manager_.GetMode() == InteractionMode::DraggingEntity) {
    mode_manager_.SetMode(InteractionMode::Select);
  }
  is_entity_mode_ = false;  // Exit exclusive entity mode
}

std::optional<SelectedEntity> DungeonObjectInteraction::GetEntityAtPosition(
    int canvas_x, int canvas_y) const {
  if (!rooms_ || current_room_id_ < 0 || current_room_id_ >= 296)
    return std::nullopt;

  const auto& room = (*rooms_)[current_room_id_];

  // Convert screen coordinates to room coordinates by accounting for canvas scale
  float scale = canvas_->global_scale();
  if (scale <= 0.0f)
    scale = 1.0f;
  int room_x = static_cast<int>(canvas_x / scale);
  int room_y = static_cast<int>(canvas_y / scale);

  // Check doors first (they have higher priority for selection)
  const auto& doors = room.GetDoors();
  for (size_t i = 0; i < doors.size(); ++i) {
    const auto& door = doors[i];

    // Get door position in tile coordinates
    auto [tile_x, tile_y] = door.GetTileCoords();

    // Get door dimensions
    auto dims = zelda3::GetDoorDimensions(door.direction);

    // Convert to pixel coordinates
    int door_x = tile_x * 8;
    int door_y = tile_y * 8;
    int door_w = dims.width_tiles * 8;
    int door_h = dims.height_tiles * 8;

    // Check if point is inside door bounds (using room coordinates)
    if (room_x >= door_x && room_x < door_x + door_w && room_y >= door_y &&
        room_y < door_y + door_h) {
      return SelectedEntity{EntityType::Door, i};
    }
  }

  // Check sprites (16x16 hitbox)
  // NOTE: Sprite coordinates are in 16-pixel units (0-31 range = 512 pixels)
  const auto& sprites = room.GetSprites();
  for (size_t i = 0; i < sprites.size(); ++i) {
    const auto& sprite = sprites[i];

    // Sprites use 16-pixel coordinate system
    int sprite_x = sprite.x() * 16;
    int sprite_y = sprite.y() * 16;

    // 16x16 hitbox (using room coordinates)
    if (room_x >= sprite_x && room_x < sprite_x + 16 && room_y >= sprite_y &&
        room_y < sprite_y + 16) {
      return SelectedEntity{EntityType::Sprite, i};
    }
  }

  // Check pot items - they have their own position data from ROM
  const auto& pot_items = room.GetPotItems();

  for (size_t i = 0; i < pot_items.size(); ++i) {
    const auto& pot_item = pot_items[i];

    // Get pixel coordinates from PotItem
    int item_x = pot_item.GetPixelX();
    int item_y = pot_item.GetPixelY();

    // 16x16 hitbox (using room coordinates)
    if (room_x >= item_x && room_x < item_x + 16 && room_y >= item_y &&
        room_y < item_y + 16) {
      return SelectedEntity{EntityType::Item, i};
    }
  }

  return std::nullopt;
}

bool DungeonObjectInteraction::TrySelectEntityAtCursor() {
  if (!canvas_->IsMouseHovering())
    return false;

  const ImGuiIO& io = ImGui::GetIO();
  if (io.KeyAlt) {
    selection_.ClearSelection();
    ClearEntitySelection();
    return false;
  }
  ImVec2 canvas_pos = canvas_->zero_point();
  int canvas_x = static_cast<int>(io.MousePos.x - canvas_pos.x);
  int canvas_y = static_cast<int>(io.MousePos.y - canvas_pos.y);

  auto entity = GetEntityAtPosition(canvas_x, canvas_y);
  if (entity.has_value()) {
    // Clear previous object selection
    selection_.ClearSelection();

    SelectEntity(entity->type, entity->index);

    // Start drag
    mode_manager_.SetMode(InteractionMode::DraggingEntity);
    auto& state = mode_manager_.GetModeState();
    state.entity_drag_start =
        ImVec2(static_cast<float>(canvas_x), static_cast<float>(canvas_y));
    state.entity_drag_current = state.entity_drag_start;

    return true;
  }

  // No entity at cursor - clear entity selection
  ClearEntitySelection();
  return false;
}

void DungeonObjectInteraction::HandleEntityDrag() {
  if (mode_manager_.GetMode() != InteractionMode::DraggingEntity ||
      selected_entity_.type == EntityType::None)
    return;

  const ImGuiIO& io = ImGui::GetIO();
  auto& state = mode_manager_.GetModeState();

  if (!ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
    // Mouse released - complete the drag
    if (selected_entity_.type == EntityType::Door) {
      // For doors, snap to valid wall position
      ImVec2 canvas_pos = canvas_->zero_point();
      int canvas_x = static_cast<int>(io.MousePos.x - canvas_pos.x);
      int canvas_y = static_cast<int>(io.MousePos.y - canvas_pos.y);

      // Detect wall
      zelda3::DoorDirection direction;
      if (zelda3::DoorPositionManager::DetectWallFromPosition(
              canvas_x, canvas_y, direction)) {
        // Snap to nearest valid position
        uint8_t position = zelda3::DoorPositionManager::SnapToNearestPosition(
            canvas_x, canvas_y, direction);

        if (zelda3::DoorPositionManager::IsValidPosition(position, direction)) {
          // Update door position
          if (rooms_ && current_room_id_ >= 0 && current_room_id_ < 296) {
            auto& room = (*rooms_)[current_room_id_];
            auto& doors = room.GetDoors();
            if (selected_entity_.index < doors.size()) {
              interaction_context_.NotifyMutation();

              doors[selected_entity_.index].position = position;
              doors[selected_entity_.index].direction = direction;

              // Re-encode bytes
              auto [b1, b2] = doors[selected_entity_.index].EncodeBytes();
              doors[selected_entity_.index].byte1 = b1;
              doors[selected_entity_.index].byte2 = b2;

              // Mark room dirty
              room.MarkObjectsDirty();

              interaction_context_.NotifyInvalidateCache();
            }
          }
        }
      }
    } else if (selected_entity_.type == EntityType::Sprite) {
      // Move sprite to new position
      ImVec2 canvas_pos = canvas_->zero_point();
      int canvas_x = static_cast<int>(io.MousePos.x - canvas_pos.x);
      int canvas_y = static_cast<int>(io.MousePos.y - canvas_pos.y);

      // Convert to sprite coordinates (16-pixel units)
      int tile_x = canvas_x / 16;
      int tile_y = canvas_y / 16;

      // Clamp to room bounds (sprites use 0-31 range)
      tile_x = std::clamp(tile_x, 0, 31);
      tile_y = std::clamp(tile_y, 0, 31);

      if (rooms_ && current_room_id_ >= 0 && current_room_id_ < 296) {
        auto& room = (*rooms_)[current_room_id_];
        auto& sprites = room.GetSprites();
        if (selected_entity_.index < sprites.size()) {
          interaction_context_.NotifyMutation();

          sprites[selected_entity_.index].set_x(tile_x);
          sprites[selected_entity_.index].set_y(tile_y);

          interaction_context_.NotifyEntityChanged();
        }
      }
    }

    // Return to select mode
    mode_manager_.SetMode(InteractionMode::Select);
    return;
  }

  // Update drag position
  ImVec2 canvas_pos = canvas_->zero_point();
  state.entity_drag_current =
      ImVec2(io.MousePos.x - canvas_pos.x, io.MousePos.y - canvas_pos.y);
}

void DungeonObjectInteraction::DrawEntitySelectionHighlights() {
  if (selected_entity_.type == EntityType::None)
    return;

  if (!rooms_ || current_room_id_ < 0 || current_room_id_ >= 296)
    return;

  const auto& room = (*rooms_)[current_room_id_];
  const auto& theme = AgentUI::GetTheme();
  ImDrawList* draw_list = ImGui::GetWindowDrawList();
  ImVec2 canvas_pos = canvas_->zero_point();
  float scale = canvas_->global_scale();

  ImVec2 pos, size;
  ImU32 color;
  const char* label = "";

  switch (selected_entity_.type) {
    case EntityType::Door: {
      const auto& doors = room.GetDoors();
      if (selected_entity_.index >= doors.size())
        return;

      const auto& door = doors[selected_entity_.index];
      auto [tile_x, tile_y] = door.GetTileCoords();
      auto dims = zelda3::GetDoorDimensions(door.direction);

      // If dragging, use current drag position for door preview
      if (mode_manager_.GetMode() == InteractionMode::DraggingEntity) {
        const auto& state = mode_manager_.GetModeState();
        int drag_x = static_cast<int>(state.entity_drag_current.x);
        int drag_y = static_cast<int>(state.entity_drag_current.y);

        zelda3::DoorDirection dir;
        bool is_inner = false;
        if (zelda3::DoorPositionManager::DetectWallSection(drag_x, drag_y, dir,
                                                           is_inner)) {
          uint8_t snap_pos = zelda3::DoorPositionManager::SnapToNearestPosition(
              drag_x, drag_y, dir);
          auto [snap_x, snap_y] =
              zelda3::DoorPositionManager::PositionToTileCoords(snap_pos, dir);
          tile_x = snap_x;
          tile_y = snap_y;
          dims = zelda3::GetDoorDimensions(dir);
        }
      }

      pos = ImVec2(canvas_pos.x + tile_x * 8 * scale,
                   canvas_pos.y + tile_y * 8 * scale);
      size =
          ImVec2(dims.width_tiles * 8 * scale, dims.height_tiles * 8 * scale);
      color = IM_COL32(255, 165, 0, 180);  // Orange
      label = "Door";
      break;
    }

    case EntityType::Sprite: {
      const auto& sprites = room.GetSprites();
      if (selected_entity_.index >= sprites.size())
        return;

      const auto& sprite = sprites[selected_entity_.index];
      // Sprites use 16-pixel coordinate system
      int pixel_x = sprite.x() * 16;
      int pixel_y = sprite.y() * 16;

      // If dragging, use current drag position (snapped to 16-pixel grid)
      if (mode_manager_.GetMode() == InteractionMode::DraggingEntity) {
        const auto& state = mode_manager_.GetModeState();
        int tile_x = static_cast<int>(state.entity_drag_current.x) / 16;
        int tile_y = static_cast<int>(state.entity_drag_current.y) / 16;
        tile_x = std::clamp(tile_x, 0, 31);
        tile_y = std::clamp(tile_y, 0, 31);
        pixel_x = tile_x * 16;
        pixel_y = tile_y * 16;
      }

      pos = ImVec2(canvas_pos.x + pixel_x * scale,
                   canvas_pos.y + pixel_y * scale);
      size = ImVec2(16 * scale, 16 * scale);
      color = IM_COL32(0, 255, 0, 180);  // Green
      label = "Sprite";
      break;
    }

    case EntityType::Item: {
      // Pot items have their own position data from ROM
      const auto& pot_items = room.GetPotItems();

      if (selected_entity_.index >= pot_items.size())
        return;

      const auto& pot_item = pot_items[selected_entity_.index];
      int pixel_x = pot_item.GetPixelX();
      int pixel_y = pot_item.GetPixelY();

      pos = ImVec2(canvas_pos.x + pixel_x * scale,
                   canvas_pos.y + pixel_y * scale);
      size = ImVec2(16 * scale, 16 * scale);
      color = IM_COL32(255, 255, 0, 180);  // Yellow
      label = "Item";
      break;
    }

    default:
      return;
  }

  // Draw selection rectangle with animated border
  static float pulse = 0.0f;
  pulse += ImGui::GetIO().DeltaTime * 3.0f;
  float alpha = 0.5f + 0.3f * sinf(pulse);

  ImU32 fill_color =
      (color & 0x00FFFFFF) | (static_cast<ImU32>(alpha * 100) << 24);
  draw_list->AddRectFilled(pos, ImVec2(pos.x + size.x, pos.y + size.y),
                           fill_color);
  draw_list->AddRect(pos, ImVec2(pos.x + size.x, pos.y + size.y), color, 0.0f,
                     0, 2.0f);

  // Draw label
  ImVec2 text_pos(pos.x, pos.y - 14 * scale);
  draw_list->AddText(text_pos, IM_COL32(255, 255, 255, 220), label);

  // Draw snap position indicators when dragging a door
  DrawDoorSnapIndicators();
}

void DungeonObjectInteraction::DrawDoorSnapIndicators() {
  // Only show snap indicators when dragging a door entity
  if (mode_manager_.GetMode() != InteractionMode::DraggingEntity ||
      selected_entity_.type != EntityType::Door)
    return;

  // Detect wall direction and section (outer wall vs inner seam) from drag position
  const auto& state = mode_manager_.GetModeState();
  zelda3::DoorDirection direction;
  bool is_inner = false;
  int drag_x = static_cast<int>(state.entity_drag_current.x);
  int drag_y = static_cast<int>(state.entity_drag_current.y);
  if (!zelda3::DoorPositionManager::DetectWallSection(drag_x, drag_y, direction,
                                                      is_inner))
    return;

  // Get the starting position index for this section
  uint8_t start_pos =
      zelda3::DoorPositionManager::GetSectionStartPosition(direction, is_inner);

  // Get the nearest snap position
  uint8_t nearest_snap = zelda3::DoorPositionManager::SnapToNearestPosition(
      drag_x, drag_y, direction);

  ImDrawList* draw_list = ImGui::GetWindowDrawList();
  ImVec2 canvas_pos = canvas_->zero_point();
  float scale = canvas_->global_scale();
  const auto& theme = AgentUI::GetTheme();
  auto dims = zelda3::GetDoorDimensions(direction);

  // Draw indicators for 6 positions in this section (3 X positions × 2 Y offsets)
  // Positions are: start_pos+0,1,2 (one Y offset) and start_pos+3,4,5 (other Y offset)
  for (uint8_t i = 0; i < 6; ++i) {
    uint8_t pos = start_pos + i;
    auto [tile_x, tile_y] =
        zelda3::DoorPositionManager::PositionToTileCoords(pos, direction);
    float pixel_x = tile_x * 8.0f;
    float pixel_y = tile_y * 8.0f;

    ImVec2 snap_start(canvas_pos.x + pixel_x * scale,
                      canvas_pos.y + pixel_y * scale);
    ImVec2 snap_end(snap_start.x + dims.width_pixels() * scale,
                    snap_start.y + dims.height_pixels() * scale);

    if (pos == nearest_snap) {
      // Highlighted nearest position - brighter with thicker border
      ImVec4 highlight = ImVec4(theme.dungeon_selection_primary.x,
                                theme.dungeon_selection_primary.y,
                                theme.dungeon_selection_primary.z, 0.75f);
      draw_list->AddRect(snap_start, snap_end, ImGui::GetColorU32(highlight),
                         0.0f, 0, 2.5f);
    } else {
      // Ghosted other positions - semi-transparent thin border
      ImVec4 ghost = ImVec4(1.0f, 1.0f, 1.0f, 0.25f);
      draw_list->AddRect(snap_start, snap_end, ImGui::GetColorU32(ghost), 0.0f,
                         0, 1.0f);
    }
  }
}

ImVec2 DungeonObjectInteraction::ApplyDragModifiers(const ImVec2& delta) const {
  const ImGuiIO& io = ImGui::GetIO();
  if (!io.KeyShift) {
    return delta;
  }

  if (std::abs(delta.x) >= std::abs(delta.y)) {
    return ImVec2(delta.x, 0.0f);
  }
  return ImVec2(0.0f, delta.y);
}

}  // namespace yaze::editor
