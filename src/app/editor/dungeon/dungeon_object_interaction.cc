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
#include "zelda3/dungeon/dimension_service.h"

namespace yaze::editor {

void DungeonObjectInteraction::HandleCanvasMouseInput() {
  const ImGuiIO& io = ImGui::GetIO();

  if (!canvas_->IsMouseHovering()) {
    return;
  }

  // Handle Escape key to cancel any active placement mode
  if (ImGui::IsKeyPressed(ImGuiKey_Escape) &&
      mode_manager_.IsPlacementActive()) {
    CancelPlacement();
    return;
  }

  if (entity_coordinator_.HandleMouseWheel(io.MouseWheel)) {
    return;
  }
  HandleLayerKeyboardShortcuts();

  ImVec2 mouse_pos = io.MousePos;
  ImVec2 canvas_pos = canvas_->zero_point();
  ImVec2 canvas_mouse_pos =
      ImVec2(mouse_pos.x - canvas_pos.x, mouse_pos.y - canvas_pos.y);

  if (ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
    HandleLeftClick(canvas_mouse_pos);
  }

  // Handle continuous painting for collision
  if (mode_manager_.GetMode() == InteractionMode::PaintCollision &&
      ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
    UpdateCollisionPainting(canvas_mouse_pos);
  }

  // Handle entity drag if active
  if (mode_manager_.GetMode() == InteractionMode::DraggingEntity) {
    HandleEntityDrag();
  }

  // Handle drag in progress
  if (mode_manager_.GetMode() == InteractionMode::DraggingObjects &&
      ImGui::IsMouseDragging(ImGuiMouseButton_Left)) {
    UpdateObjectDragging(canvas_mouse_pos);
  }

  // Handle mouse release - complete drag operation
  if (ImGui::IsMouseReleased(ImGuiMouseButton_Left)) {
    HandleMouseRelease();
  }
}

void DungeonObjectInteraction::HandleLeftClick(const ImVec2& canvas_mouse_pos) {
  int canvas_x = static_cast<int>(canvas_mouse_pos.x);
  int canvas_y = static_cast<int>(canvas_mouse_pos.y);

  // Try to handle click via entity coordinator (handles placement, entity selection, and object selection)
  if (entity_coordinator_.HandleClick(canvas_x, canvas_y)) {
    // If an object selection just started, transition to dragging mode if applicable
    if (selection_.HasSelection() && !HasEntitySelection()) {
      HandleObjectSelectionStart(canvas_mouse_pos);
    }
    return;
  }

  // Not an entity click or placement; handle empty space
  HandleEmptySpaceClick(canvas_mouse_pos);
}

void DungeonObjectInteraction::UpdateCollisionPainting(const ImVec2& canvas_mouse_pos) {
  auto [room_x, room_y] = CanvasToRoomCoordinates(
      static_cast<int>(canvas_mouse_pos.x), static_cast<int>(canvas_mouse_pos.y));
  if (rooms_ && current_room_id_ >= 0 && current_room_id_ < 296) {
    auto& room = (*rooms_)[current_room_id_];
    auto& state = mode_manager_.GetModeState();
    
    // Only set for valid interior tiles (0-63)
    if (room_x >= 0 && room_x < 64 && room_y >= 0 && room_y < 64) {
      if (room.GetCollisionTile(room_x, room_y) != state.paint_collision_value) {
        room.SetCollisionTile(room_x, room_y, state.paint_collision_value);
        interaction_context_.NotifyMutation();
        interaction_context_.NotifyInvalidateCache();
      }
    }
  }
}

void DungeonObjectInteraction::HandleObjectSelectionStart(const ImVec2& canvas_mouse_pos) {
  ClearEntitySelection();
  if (selection_.HasSelection()) {
    mode_manager_.SetMode(InteractionMode::DraggingObjects);
    auto& state = mode_manager_.GetModeState();
    
    // Snapping is still here for now as tile dragging is in this class
    state.drag_start = snapping::SnapToTileGrid(canvas_mouse_pos);
    state.drag_current = state.drag_start;
    state.duplicate_on_drag = false;
    state.drag_last_tile_dx = 0;
    state.drag_last_tile_dy = 0;
    state.drag_mutation_started = false;
    state.drag_has_duplicated = false;
  }
}

void DungeonObjectInteraction::HandleEmptySpaceClick(const ImVec2& canvas_mouse_pos) {
  const ImGuiIO& io = ImGui::GetIO();
  const bool had_selection = selection_.HasSelection() || HasEntitySelection();
  
  // Clear selection unless modifier held
  if (!io.KeyShift && !io.KeyCtrl) {
    selection_.ClearSelection();
    ClearEntitySelection();
  }

  if (!had_selection) {
    // Begin rectangle selection
    mode_manager_.SetMode(InteractionMode::RectangleSelect);
    auto& state = mode_manager_.GetModeState();
    state.rect_start_x = static_cast<int>(canvas_mouse_pos.x);
    state.rect_start_y = static_cast<int>(canvas_mouse_pos.y);
    state.rect_end_x = state.rect_start_x;
    state.rect_end_y = state.rect_start_y;
    selection_.BeginRectangleSelection(state.rect_start_x, state.rect_start_y);
  }
}

void DungeonObjectInteraction::UpdateObjectDragging(const ImVec2& canvas_mouse_pos) {
  auto& state = mode_manager_.GetModeState();
  state.drag_current = snapping::SnapToTileGrid(canvas_mouse_pos);
  const bool alt_down = ImGui::GetIO().KeyAlt;
  state.duplicate_on_drag = state.duplicate_on_drag || alt_down;

  // Live-update in snapped tile increments to reduce "slide-y" feel.
  // Mutate only when the snapped delta changes; emit a single undo snapshot.
  ImVec2 drag_delta = ImVec2(state.drag_current.x - state.drag_start.x,
                             state.drag_current.y - state.drag_start.y);
  drag_delta = ApplyDragModifiers(drag_delta);

  const int tile_dx = static_cast<int>(drag_delta.x) / 8;
  const int tile_dy = static_cast<int>(drag_delta.y) / 8;

  // Option-drag (Alt) duplicates once, then moves the clones.
  if (alt_down && !state.drag_has_duplicated) {
    if (!state.drag_mutation_started) {
      interaction_context_.NotifyMutation();
      state.drag_mutation_started = true;
    }
    auto new_indices = entity_coordinator_.tile_handler().DuplicateObjects(
        current_room_id_, selection_.GetSelectedIndices(), /*delta_x=*/0,
        /*delta_y=*/0, /*notify_mutation=*/false);
    selection_.ClearSelection();
    for (size_t idx : new_indices) {
      selection_.SelectObject(idx, ObjectSelection::SelectionMode::Add);
    }
    state.drag_has_duplicated = true;
  }

  const int inc_dx = tile_dx - state.drag_last_tile_dx;
  const int inc_dy = tile_dy - state.drag_last_tile_dy;
  if (inc_dx != 0 || inc_dy != 0) {
    if (!state.drag_mutation_started) {
      interaction_context_.NotifyMutation();
      state.drag_mutation_started = true;
    }
    entity_coordinator_.tile_handler().MoveObjects(
        current_room_id_, selection_.GetSelectedIndices(), inc_dx, inc_dy,
        /*notify_mutation=*/false);
    state.drag_last_tile_dx = tile_dx;
    state.drag_last_tile_dy = tile_dy;
  }
}

void DungeonObjectInteraction::HandleMouseRelease() {
  if (mode_manager_.GetMode() == InteractionMode::DraggingObjects) {
    // Live drag already applied incremental moves; finalize by returning to
    // select mode.
    mode_manager_.SetMode(InteractionMode::Select);
  }
  
  // Rectangle selection release is handled in DrawObjectSelectRect for now 
  // to maintain consistency with existing selection logic.
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

void DungeonObjectInteraction::DrawSelectionHighlights() {
  if (!rooms_ || current_room_id_ < 0 || current_room_id_ >= 296)
    return;

  auto& room = (*rooms_)[current_room_id_];
  const auto& objects = room.GetTileObjects();

  // Use ObjectSelection's rendering (handles pulsing border, corner handles)
  selection_.DrawSelectionHighlights(
      canvas_, objects, [](const zelda3::RoomObject& obj) {
        auto result = zelda3::DimensionService::Get().GetDimensions(obj);
        return std::make_tuple(result.offset_x_tiles * 8,
                               result.offset_y_tiles * 8,
                               result.width_pixels(), result.height_pixels());
      });

  // Enhanced hover tooltip showing object info (always visible on hover)
  // Skip completely in exclusive entity mode (door/sprite/item selected)
  if (entity_coordinator_.HasEntitySelection()) {
    return;  // Entity mode active - no object tooltips or hover
  }

  if (canvas_->IsMouseHovering()) {
    // Also skip tooltip if cursor is over a door/sprite/item entity (not selected yet)
    ImGuiIO& io = ImGui::GetIO();
    ImVec2 canvas_pos = canvas_->zero_point();
    int cursor_x = static_cast<int>(io.MousePos.x - canvas_pos.x);
    int cursor_y = static_cast<int>(io.MousePos.y - canvas_pos.y);
    auto entity_at_cursor = entity_coordinator_.GetEntityAtPosition(cursor_x, cursor_y);
    if (entity_at_cursor.has_value()) {
      // Entity has priority - skip object tooltip, DrawHoverHighlight will also skip
      DrawHoverHighlight(objects);
      return;
    }

    auto hovered_index = entity_coordinator_.tile_handler().GetEntityAtPosition(cursor_x, cursor_y);
    if (hovered_index.has_value() && *hovered_index < objects.size()) {
      const auto& object = objects[*hovered_index];
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

      if (selection_.IsObjectSelected(*hovered_index)) {
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
  if (entity_coordinator_.HasEntitySelection())
    return;

  // Don't show object hover highlight if cursor is over a door/sprite/item entity
  // Entities take priority over objects for interaction
  ImGuiIO& io = ImGui::GetIO();
  ImVec2 canvas_pos = canvas_->zero_point();
  int cursor_canvas_x = static_cast<int>(io.MousePos.x - canvas_pos.x);
  int cursor_canvas_y = static_cast<int>(io.MousePos.y - canvas_pos.y);
  auto entity_at_cursor = entity_coordinator_.GetEntityAtPosition(cursor_canvas_x, cursor_canvas_y);
  if (entity_at_cursor.has_value()) {
    return;  // Entity has priority - skip object hover highlight
  }

  auto hovered_index = entity_coordinator_.tile_handler().GetEntityAtPosition(cursor_canvas_x, cursor_canvas_y);
  if (!hovered_index.has_value() || *hovered_index >= objects.size()) {
    return;
  }

  // Don't draw hover highlight if object is already selected
  if (selection_.IsObjectSelected(*hovered_index)) {
    return;
  }

  const auto& object = objects[*hovered_index];
  const auto& theme = AgentUI::GetTheme();
  ImDrawList* draw_list = ImGui::GetWindowDrawList();
  // canvas_pos already defined above for entity check
  float scale = canvas_->global_scale();

  // Calculate object position and dimensions
  auto [sel_x_px, sel_y_px, pixel_width, pixel_height] =
      zelda3::DimensionService::Get().GetSelectionBoundsPixels(object);

  // Apply scale and canvas offset
  ImVec2 obj_start(canvas_pos.x + sel_x_px * scale,
                   canvas_pos.y + sel_y_px * scale);
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
  entity_coordinator_.tile_handler().PlaceObjectAt(current_room_id_, preview_object_, room_x, room_y);

  if (object_placed_callback_) {
    object_placed_callback_(preview_object_);
  }

  interaction_context_.NotifyInvalidateCache();
  CancelPlacement();
}

void DungeonObjectInteraction::DrawSelectBox() {
  // Legacy method - rectangle selection now handled by ObjectSelection
  // Delegates to ObjectSelection's DrawRectangleSelectionBox if active
  if (selection_.IsRectangleSelectionActive()) {
    selection_.DrawRectangleSelectionBox(canvas_);
  }
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
  interaction_context_.selection = &selection_;
  entity_coordinator_.SetContext(&interaction_context_);
}

void DungeonObjectInteraction::SetPreviewObject(
    const zelda3::RoomObject& object, bool loaded) {
  preview_object_ = object;

  if (loaded && object.id_ >= 0) {
    // Cancel other placement modes (doors/sprites/items) before entering object
    // placement. We re-enable tile placement below.
    entity_coordinator_.CancelPlacement();

    // Enter object placement mode
    mode_manager_.SetMode(InteractionMode::PlaceObject);
    mode_manager_.GetModeState().preview_object = object;

    // Ensure tile placement mode is active so ghost preview can render and
    // clicks place the object.
    auto& tile_handler = entity_coordinator_.tile_handler();
    tile_handler.SetPreviewObject(preview_object_);
    if (!tile_handler.IsPlacementActive()) {
      tile_handler.BeginPlacement();
    }
  } else {
    // Exit placement mode if not loaded
    if (mode_manager_.GetMode() == InteractionMode::PlaceObject) {
      CancelPlacement();
    }
  }
}



void DungeonObjectInteraction::ClearSelection() {
  selection_.ClearSelection();
  if (mode_manager_.GetMode() == InteractionMode::DraggingObjects) {
    mode_manager_.SetMode(InteractionMode::Select);
  }
}


void DungeonObjectInteraction::HandleDeleteSelected() {
  auto indices = selection_.GetSelectedIndices();
  if (!indices.empty()) {
    entity_coordinator_.tile_handler().DeleteObjects(current_room_id_, indices);
    selection_.ClearSelection();
  }
  
  if (entity_coordinator_.HasEntitySelection()) {
    entity_coordinator_.DeleteSelectedEntity();
  }
}

void DungeonObjectInteraction::HandleDeleteAllObjects() {
  entity_coordinator_.tile_handler().DeleteAllObjects(current_room_id_);
  selection_.ClearSelection();
}

void DungeonObjectInteraction::HandleCopySelected() {
  entity_coordinator_.tile_handler().CopyObjectsToClipboard(
      current_room_id_, selection_.GetSelectedIndices());
}

void DungeonObjectInteraction::HandlePasteObjects() {
  auto& handler = entity_coordinator_.tile_handler();
  if (!handler.HasClipboardData()) return;

  const ImGuiIO& io = ImGui::GetIO();
  ImVec2 canvas_mouse_pos = ImVec2(io.MousePos.x - canvas_->zero_point().x, 
                                   io.MousePos.y - canvas_->zero_point().y);
  auto [paste_x, paste_y] = CanvasToRoomCoordinates(static_cast<int>(canvas_mouse_pos.x),
                                                    static_cast<int>(canvas_mouse_pos.y));

  auto new_indices = handler.PasteFromClipboardAt(current_room_id_, paste_x, paste_y);
  
  // Select the newly pasted objects
  if (!new_indices.empty()) {
    selection_.ClearSelection();
    for (size_t idx : new_indices) {
      selection_.SelectObject(idx, ObjectSelection::SelectionMode::Add);
    }
  }
}

void DungeonObjectInteraction::DrawGhostPreview() {
  entity_coordinator_.DrawGhostPreviews();
}

void DungeonObjectInteraction::HandleScrollWheelResize() {
  const ImGuiIO& io = ImGui::GetIO();
  entity_coordinator_.HandleMouseWheel(io.MouseWheel);
}

bool DungeonObjectInteraction::SetObjectId(size_t index, int16_t id) {
  entity_coordinator_.tile_handler().UpdateObjectsId(current_room_id_, {index}, id);
  return true;
}

bool DungeonObjectInteraction::SetObjectSize(size_t index, uint8_t size) {
  entity_coordinator_.tile_handler().UpdateObjectsSize(current_room_id_, {index}, size);
  return true;
}

bool DungeonObjectInteraction::SetObjectLayer(size_t index, zelda3::RoomObject::LayerType layer) {
  entity_coordinator_.tile_handler().UpdateObjectsLayer(current_room_id_, {index}, static_cast<int>(layer));
  return true;
}

std::pair<int, int> DungeonObjectInteraction::CalculateObjectBounds(
    const zelda3::RoomObject& object) {
  return zelda3::DimensionService::Get().GetPixelDimensions(object);
}


void DungeonObjectInteraction::SendSelectedToLayer(int target_layer) {
  entity_coordinator_.tile_handler().UpdateObjectsLayer(
      current_room_id_, selection_.GetSelectedIndices(), target_layer);
}

void DungeonObjectInteraction::SendSelectedToFront() {
  entity_coordinator_.tile_handler().SendToFront(current_room_id_, selection_.GetSelectedIndices());
}

void DungeonObjectInteraction::SendSelectedToBack() {
  entity_coordinator_.tile_handler().SendToBack(current_room_id_, selection_.GetSelectedIndices());
}

void DungeonObjectInteraction::BringSelectedForward() {
  entity_coordinator_.tile_handler().MoveForward(current_room_id_, selection_.GetSelectedIndices());
}

void DungeonObjectInteraction::SendSelectedBackward() {
  entity_coordinator_.tile_handler().MoveBackward(current_room_id_, selection_.GetSelectedIndices());
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

void DungeonObjectInteraction::SetDoorPlacementMode(bool enabled, zelda3::DoorType type) {
  if (enabled) {
    mode_manager_.SetMode(InteractionMode::PlaceDoor);
    entity_coordinator_.door_handler().SetDoorType(type);
    entity_coordinator_.door_handler().BeginPlacement();
  } else {
    entity_coordinator_.door_handler().CancelPlacement();
    if (mode_manager_.GetMode() == InteractionMode::PlaceDoor) mode_manager_.SetMode(InteractionMode::Select);
  }
}



// ============================================================================
// Sprite Placement Methods
// ============================================================================

void DungeonObjectInteraction::SetSpritePlacementMode(bool enabled, uint8_t sprite_id) {
  if (enabled) {
    mode_manager_.SetMode(InteractionMode::PlaceSprite);
    entity_coordinator_.sprite_handler().SetSpriteId(sprite_id);
    entity_coordinator_.sprite_handler().BeginPlacement();
  } else {
    entity_coordinator_.sprite_handler().CancelPlacement();
    if (mode_manager_.GetMode() == InteractionMode::PlaceSprite) mode_manager_.SetMode(InteractionMode::Select);
  }
}



// ============================================================================
// Item Placement Methods
// ============================================================================

void DungeonObjectInteraction::SetItemPlacementMode(bool enabled, uint8_t item_id) {
  if (enabled) {
    mode_manager_.SetMode(InteractionMode::PlaceItem);
    entity_coordinator_.item_handler().SetItemId(item_id);
    entity_coordinator_.item_handler().BeginPlacement();
  } else {
    entity_coordinator_.item_handler().CancelPlacement();
    if (mode_manager_.GetMode() == InteractionMode::PlaceItem) mode_manager_.SetMode(InteractionMode::Select);
  }
}



// ============================================================================
// Entity Selection Methods (Doors, Sprites, Items)
// ============================================================================

void DungeonObjectInteraction::SelectEntity(EntityType type, size_t index) {
  entity_coordinator_.SelectEntity(type, index);
}

void DungeonObjectInteraction::ClearEntitySelection() {
  entity_coordinator_.ClearEntitySelection();
}


void DungeonObjectInteraction::HandleEntityDrag() {
  const ImGuiIO& io = ImGui::GetIO();
  entity_coordinator_.HandleDrag(io.MousePos, io.MouseDelta);
}

void DungeonObjectInteraction::CancelPlacement() {
  entity_coordinator_.CancelPlacement();
  if (mode_manager_.IsPlacementActive()) {
    mode_manager_.SetMode(InteractionMode::Select);
  }
}

void DungeonObjectInteraction::DrawEntitySelectionHighlights() {
  entity_coordinator_.DrawSelectionHighlights();
}

void DungeonObjectInteraction::DrawDoorSnapIndicators() {
  // Door snap indicators are now managed by DoorInteractionHandler
  // through the entity coordinator. No-op here for backward compatibility.
}

ImVec2 DungeonObjectInteraction::ApplyDragModifiers(const ImVec2& delta) const {
  const ImGuiIO& io = ImGui::GetIO();
  if (!io.KeyShift) return delta;
  if (std::abs(delta.x) >= std::abs(delta.y)) return ImVec2(delta.x, 0.0f);
  return ImVec2(0.0f, delta.y);
}

}  // namespace yaze::editor
