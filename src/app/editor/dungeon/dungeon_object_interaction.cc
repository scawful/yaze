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
#include "app/editor/dungeon/interaction/paint_util.h"
#include "app/gfx/resource/arena.h"
#include "app/gui/core/icons.h"
#include "zelda3/dungeon/dimension_service.h"

namespace yaze::editor {

void DungeonObjectInteraction::HandleCanvasMouseInput() {
  const ImGuiIO& io = ImGui::GetIO();
  const bool hovered = canvas_->IsMouseHovering();
  const bool mouse_left_down = ImGui::IsMouseDown(ImGuiMouseButton_Left);
  const bool mouse_left_released = ImGui::IsMouseReleased(ImGuiMouseButton_Left);

  // Keep processing drag/release if an interaction started on the canvas but
  // the cursor left the bounds before the mouse button was released.
  const bool has_active_marquee = selection_.IsRectangleSelectionActive();
  const bool should_process_without_hover =
      has_active_marquee ||
      mode_manager_.GetMode() == InteractionMode::DraggingObjects ||
      (entity_coordinator_.HasEntitySelection() &&
       (mouse_left_down || mouse_left_released)) ||
      mouse_left_released;

  if (!hovered && !should_process_without_hover) {
    return;
  }

  // Handle Escape key to cancel any active placement mode
  if (ImGui::IsKeyPressed(ImGuiKey_Escape) &&
      entity_coordinator_.IsPlacementActive()) {
    CancelPlacement();
    return;
  }

  if (hovered) {
    if (entity_coordinator_.HandleMouseWheel(io.MouseWheel)) {
      return;
    }
    HandleLayerKeyboardShortcuts();
  }

  ImVec2 mouse_pos = io.MousePos;
  ImVec2 canvas_pos = canvas_->zero_point();
  ImVec2 canvas_mouse_pos =
      ImVec2(mouse_pos.x - canvas_pos.x, mouse_pos.y - canvas_pos.y);

  // Painting modes are exclusive; don't also select/drag/mutate entities.
  if (hovered && mouse_left_down) {
    const auto mode = mode_manager_.GetMode();
    if (mode == InteractionMode::PaintCollision) {
      UpdateCollisionPainting(canvas_mouse_pos);
      return;
    }
    if (mode == InteractionMode::PaintWaterFill) {
      UpdateWaterFillPainting(canvas_mouse_pos);
      return;
    }
  }

  if (hovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
    HandleLeftClick(canvas_mouse_pos);
  }

  // Dispatch drag to coordinator (handlers gate internally via drag state).
  if (ImGui::IsMouseDragging(ImGuiMouseButton_Left)) {
    entity_coordinator_.HandleDrag(canvas_mouse_pos, io.MouseDelta);
  }

  // Handle mouse release - complete drag operation
  if (mouse_left_released) {
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
      // Start a paint stroke (single undo snapshot per stroke).
      if (!state.is_painting) {
        state.is_painting = true;
        state.paint_mutation_started = false;
        state.paint_last_tile_x = room_x;
        state.paint_last_tile_y = room_y;
      }

      const int x0 =
          (state.paint_last_tile_x >= 0) ? state.paint_last_tile_x : room_x;
      const int y0 =
          (state.paint_last_tile_y >= 0) ? state.paint_last_tile_y : room_y;

      bool changed = false;
      auto ensure_mutation = [&]() {
        if (!state.paint_mutation_started) {
          interaction_context_.NotifyMutation(MutationDomain::kCustomCollision);
          state.paint_mutation_started = true;
        }
      };

      paint_util::ForEachPointOnLine(x0, y0, room_x, room_y,
                                    [&](int lx, int ly) {
        paint_util::ForEachPointInSquareBrush(
            lx, ly, state.paint_brush_radius,
            /*min_x=*/0, /*min_y=*/0, /*max_x=*/63, /*max_y=*/63,
            [&](int bx, int by) {
              if (room.GetCollisionTile(bx, by) == state.paint_collision_value) {
                return;
              }
              ensure_mutation();
              room.SetCollisionTile(bx, by, state.paint_collision_value);
              changed = true;
            });
      });

      if (changed) {
        interaction_context_.NotifyInvalidateCache(MutationDomain::kCustomCollision);
      }

      state.paint_last_tile_x = room_x;
      state.paint_last_tile_y = room_y;
    }
  }
}

void DungeonObjectInteraction::UpdateWaterFillPainting(
    const ImVec2& canvas_mouse_pos) {
  const ImGuiIO& io = ImGui::GetIO();
  const bool erase = io.KeyAlt;

  auto [room_x, room_y] = CanvasToRoomCoordinates(
      static_cast<int>(canvas_mouse_pos.x),
      static_cast<int>(canvas_mouse_pos.y));
  if (rooms_ && current_room_id_ >= 0 && current_room_id_ < 296) {
    auto& room = (*rooms_)[current_room_id_];
    auto& state = mode_manager_.GetModeState();

    // Only set for valid interior tiles (0-63)
    if (room_x >= 0 && room_x < 64 && room_y >= 0 && room_y < 64) {
      const bool new_val = !erase;
      // Start a paint stroke (single undo snapshot per stroke).
      if (!state.is_painting) {
        state.is_painting = true;
        state.paint_mutation_started = false;
        state.paint_last_tile_x = room_x;
        state.paint_last_tile_y = room_y;
      }

      const int x0 =
          (state.paint_last_tile_x >= 0) ? state.paint_last_tile_x : room_x;
      const int y0 =
          (state.paint_last_tile_y >= 0) ? state.paint_last_tile_y : room_y;

      bool changed = false;
      auto ensure_mutation = [&]() {
        if (!state.paint_mutation_started) {
          interaction_context_.NotifyMutation(MutationDomain::kWaterFill);
          state.paint_mutation_started = true;
        }
      };

      paint_util::ForEachPointOnLine(x0, y0, room_x, room_y,
                                    [&](int lx, int ly) {
        paint_util::ForEachPointInSquareBrush(
            lx, ly, state.paint_brush_radius,
            /*min_x=*/0, /*min_y=*/0, /*max_x=*/63, /*max_y=*/63,
            [&](int bx, int by) {
              if (room.GetWaterFillTile(bx, by) == new_val) {
                return;
              }
              ensure_mutation();
              room.SetWaterFillTile(bx, by, new_val);
              changed = true;
            });
      });

      if (changed) {
        interaction_context_.NotifyInvalidateCache(MutationDomain::kWaterFill);
      }

      state.paint_last_tile_x = room_x;
      state.paint_last_tile_y = room_y;
    }
  }
}

void DungeonObjectInteraction::HandleObjectSelectionStart(const ImVec2& canvas_mouse_pos) {
  ClearEntitySelection();
  if (selection_.HasSelection()) {
    mode_manager_.SetMode(InteractionMode::DraggingObjects);
    entity_coordinator_.tile_handler().InitDrag(canvas_mouse_pos);
  }
}

void DungeonObjectInteraction::HandleEmptySpaceClick(const ImVec2& canvas_mouse_pos) {
  const ImGuiIO& io = ImGui::GetIO();
  const bool additive = io.KeyShift || io.KeyCtrl || io.KeySuper;

  // Tile-object marquee selection is exclusive of door/sprite/item selection.
  ClearEntitySelection();

  // Clear existing object selection unless modifier held (Shift/Ctrl/Cmd).
  if (!additive) {
    selection_.ClearSelection();
  }

  // Always start a marquee selection drag on empty space; click-release without
  // dragging behaves like a normal "clear selection" click.
  entity_coordinator_.tile_handler().BeginMarqueeSelection(canvas_mouse_pos);
}


void DungeonObjectInteraction::HandleMouseRelease() {
  {
    // End paint strokes on mouse release so a new left-drag creates a new undo
    // snapshot. Keep the paint mode active (tool stays selected).
    const auto mode = mode_manager_.GetMode();
    if (mode == InteractionMode::PaintCollision ||
        mode == InteractionMode::PaintWaterFill) {
      auto& state = mode_manager_.GetModeState();
      state.is_painting = false;
      state.paint_mutation_started = false;
      state.paint_last_tile_x = -1;
      state.paint_last_tile_y = -1;
    }
  }

  if (mode_manager_.GetMode() == InteractionMode::DraggingObjects) {
    mode_manager_.SetMode(InteractionMode::Select);
  }
  entity_coordinator_.HandleRelease();
  // Marquee selection finalization is handled by TileObjectHandler via
  // CheckForObjectSelection().
}

void DungeonObjectInteraction::CheckForObjectSelection() {
  // Draw/update active marquee selection for tile objects (delegated).
  const ImGuiIO& io = ImGui::GetIO();
  const ImVec2 canvas_pos = canvas_->zero_point();
  const ImVec2 mouse_pos =
      ImVec2(io.MousePos.x - canvas_pos.x, io.MousePos.y - canvas_pos.y);

  entity_coordinator_.tile_handler().HandleMarqueeSelection(
      mouse_pos,
      /*mouse_left_down=*/ImGui::IsMouseDown(ImGuiMouseButton_Left),
      /*mouse_left_released=*/ImGui::IsMouseReleased(ImGuiMouseButton_Left),
      /*shift_down=*/io.KeyShift,
      /*toggle_down=*/io.KeyCtrl || io.KeySuper,
      /*alt_down=*/io.KeyAlt);
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

  interaction_context_.NotifyInvalidateCache(MutationDomain::kTileObjects);
  CancelPlacement();
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


}  // namespace yaze::editor
