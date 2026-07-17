// Related header
#include "dungeon_object_interaction.h"
#include "absl/strings/str_format.h"
#include "app/editor/dungeon/dungeon_room_store.h"

// C++ standard library headers
#include <algorithm>
#include <cmath>

// Third-party library headers
#include "imgui/imgui.h"

// Project headers
#include "app/editor/dungeon/dungeon_coordinates.h"
#include "app/editor/dungeon/interaction/paint_util.h"
#include "app/gfx/resource/arena.h"
#include "app/gui/core/agent_theme.h"
#include "app/gui/core/icons.h"
#include "app/gui/core/theme_manager.h"
#include "zelda3/dungeon/dimension_service.h"
#include "zelda3/dungeon/object_layer_semantics.h"

namespace yaze::editor {

namespace {

constexpr int kRoomPixelMax = 511;

uint16_t EncodePotItemPosition(int pixel_x, int pixel_y) {
  const int clamped_x = std::clamp(pixel_x, 0, kRoomPixelMax);
  const int clamped_y = std::clamp(pixel_y, 0, kRoomPixelMax);
  const int encoded_x = std::clamp(clamped_x / 4, 0, 255);
  const int encoded_y = std::clamp(clamped_y / 16, 0, 255);
  return static_cast<uint16_t>((encoded_y << 8) | encoded_x);
}

}  // namespace

void DungeonObjectInteraction::HandleCanvasMouseInput() {
  const ImGuiIO& io = ImGui::GetIO();
  const bool hovered = canvas_->IsMouseHovering();
  const bool mouse_left_down = ImGui::IsMouseDown(ImGuiMouseButton_Left);
  const bool mouse_left_released =
      ImGui::IsMouseReleased(ImGuiMouseButton_Left);

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

  const ImVec2 canvas_mouse_pos =
      GetCanvasTransform().ScreenToRoomPixels(io.MousePos);
  const int canvas_mouse_x = static_cast<int>(std::floor(canvas_mouse_pos.x));
  const int canvas_mouse_y = static_cast<int>(std::floor(canvas_mouse_pos.y));
  const bool pointer_within_room =
      dungeon_coords::IsWithinBounds(canvas_mouse_x, canvas_mouse_y);

  // Handle Escape key to cancel any active placement mode
  if (ImGui::IsKeyPressed(ImGuiKey_Escape) &&
      entity_coordinator_.IsPlacementActive()) {
    CancelPlacement();
    return;
  }

  if (hovered) {
    if (pointer_within_room &&
        entity_coordinator_.HandleMouseWheel(io.MouseWheel)) {
      return;
    }
    HandleLayerKeyboardShortcuts();
    if (HandleKeyboardNudge()) {
      return;
    }
  }

  // Painting modes are exclusive; don't also select/drag/mutate entities.
  if (hovered && mouse_left_down) {
    const auto mode = mode_manager_.GetMode();
    if (mode == InteractionMode::PaintCollision) {
      if (pointer_within_room) {
        UpdateCollisionPainting(canvas_mouse_pos);
      }
      return;
    }
    if (mode == InteractionMode::PaintWaterFill) {
      if (pointer_within_room) {
        UpdateWaterFillPainting(canvas_mouse_pos);
      }
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
  int canvas_x = static_cast<int>(std::floor(canvas_mouse_pos.x));
  int canvas_y = static_cast<int>(std::floor(canvas_mouse_pos.y));

  // Try to handle click via entity coordinator (handles placement, entity selection, and object selection)
  if (entity_coordinator_.HandleClick(canvas_x, canvas_y)) {
    // If a selected room element was clicked, prime drag state. Plain clicks on
    // selected mixed members preserve the whole selection; movement begins only
    // if the mouse actually drags.
    if (!entity_coordinator_.IsPlacementActive() &&
        (selection_.HasSelection() || HasEntitySelection())) {
      HandleObjectSelectionStart(canvas_mouse_pos);
    }
    return;
  }

  // The canvas viewport can contain blank space after panning. Overlay clicks
  // are dispatched above, but blank space outside the translated room must not
  // clear selection or start a marquee.
  if (!dungeon_coords::IsWithinBounds(canvas_x, canvas_y)) {
    return;
  }

  // Not an entity click or placement; handle empty space
  HandleEmptySpaceClick(canvas_mouse_pos);
}

void DungeonObjectInteraction::UpdateCollisionPainting(
    const ImVec2& canvas_mouse_pos) {
  auto [room_x, room_y] =
      CanvasToRoomCoordinates(static_cast<int>(canvas_mouse_pos.x),
                              static_cast<int>(canvas_mouse_pos.y));
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

      paint_util::ForEachPointOnLine(
          x0, y0, room_x, room_y, [&](int lx, int ly) {
            paint_util::ForEachPointInSquareBrush(
                lx, ly, state.paint_brush_radius,
                /*min_x=*/0, /*min_y=*/0, /*max_x=*/63, /*max_y=*/63,
                [&](int bx, int by) {
                  if (room.GetCollisionTile(bx, by) ==
                      state.paint_collision_value) {
                    return;
                  }
                  ensure_mutation();
                  room.SetCollisionTile(bx, by, state.paint_collision_value);
                  changed = true;
                });
          });

      if (changed) {
        interaction_context_.NotifyInvalidateCache(
            MutationDomain::kCustomCollision);
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

  auto [room_x, room_y] =
      CanvasToRoomCoordinates(static_cast<int>(canvas_mouse_pos.x),
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

      paint_util::ForEachPointOnLine(
          x0, y0, room_x, room_y, [&](int lx, int ly) {
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

void DungeonObjectInteraction::HandleObjectSelectionStart(
    const ImVec2& canvas_mouse_pos) {
  const bool has_object_selection = selection_.HasSelection();
  const bool has_entity_selection = HasEntitySelection();
  if (!has_object_selection && !has_entity_selection) {
    return;
  }

  mode_manager_.SetMode(InteractionMode::DraggingObjects);
  if (has_object_selection) {
    entity_coordinator_.tile_handler().InitDrag(canvas_mouse_pos);
  }
  if (has_entity_selection) {
    entity_coordinator_.BeginSelectionDrag(canvas_mouse_pos);
  }
}

void DungeonObjectInteraction::HandleEmptySpaceClick(
    const ImVec2& canvas_mouse_pos) {
  const ImGuiIO& io = ImGui::GetIO();
  const bool additive = io.KeyShift || io.KeyCtrl || io.KeySuper;
  const bool had_selection =
      selection_.HasSelection() || entity_coordinator_.HasEntitySelection();

  // ZScream treats an empty click against an existing selection as a clear
  // action. Rectangle selection starts from a clean canvas gesture.
  if (!additive) {
    ClearEntitySelection();
    selection_.ClearSelection();
  }

  if (!had_selection) {
    entity_coordinator_.tile_handler().BeginMarqueeSelection(canvas_mouse_pos);
  }
}

void DungeonObjectInteraction::HandleMouseRelease() {
  {
    // End paint strokes on mouse release so a new left-drag creates a new undo
    // snapshot. Keep the paint mode active (tool stays selected).
    const auto mode = mode_manager_.GetMode();
    if (mode == InteractionMode::PaintCollision ||
        mode == InteractionMode::PaintWaterFill) {
      auto& state = mode_manager_.GetModeState();
      const bool had_mutation = state.paint_mutation_started;
      state.is_painting = false;
      state.paint_mutation_started = false;
      state.paint_last_tile_x = -1;
      state.paint_last_tile_y = -1;
      // Emit a final invalidation after the stroke ends so domain-specific undo
      // capture can finalize the action once we're no longer "painting".
      if (had_mutation) {
        interaction_context_.NotifyInvalidateCache(
            (mode == InteractionMode::PaintCollision)
                ? MutationDomain::kCustomCollision
                : MutationDomain::kWaterFill);
      }
    }
  }

  if (mode_manager_.GetMode() == InteractionMode::DraggingObjects) {
    mode_manager_.SetMode(InteractionMode::Select);
  }
  entity_coordinator_.HandleRelease();
  // Marquee selection finalization is handled by TileObjectHandler via
  // CheckForObjectSelection().
}

bool DungeonObjectInteraction::HandleKeyboardNudge() {
  if (!rooms_ || current_room_id_ < 0 ||
      current_room_id_ >= static_cast<int>(rooms_->size())) {
    return false;
  }

  const ImGuiIO& io = ImGui::GetIO();
  if (ImGui::IsAnyItemActive() || io.KeyCtrl || io.KeySuper || io.KeyAlt ||
      ImGui::IsMouseDown(ImGuiMouseButton_Left) ||
      entity_coordinator_.IsPlacementActive()) {
    return false;
  }

  const auto mode = mode_manager_.GetMode();
  if (mode == InteractionMode::DraggingObjects ||
      mode == InteractionMode::PaintCollision ||
      mode == InteractionMode::PaintWaterFill) {
    return false;
  }

  int delta_x = 0;
  int delta_y = 0;
  if (ImGui::IsKeyPressed(ImGuiKey_LeftArrow, true)) {
    delta_x = -1;
  } else if (ImGui::IsKeyPressed(ImGuiKey_RightArrow, true)) {
    delta_x = 1;
  }
  if (ImGui::IsKeyPressed(ImGuiKey_UpArrow, true)) {
    delta_y = -1;
  } else if (ImGui::IsKeyPressed(ImGuiKey_DownArrow, true)) {
    delta_y = 1;
  }

  if (delta_x == 0 && delta_y == 0) {
    return false;
  }

  return NudgeSelected(delta_x, delta_y);
}

bool DungeonObjectInteraction::NudgeSelected(int delta_x, int delta_y) {
  if (!rooms_ || current_room_id_ < 0 ||
      current_room_id_ >= static_cast<int>(rooms_->size())) {
    return false;
  }

  bool handled = false;
  if (selection_.HasSelection()) {
    entity_coordinator_.tile_handler().MoveObjects(
        current_room_id_, selection_.GetSelectedIndices(), delta_x, delta_y);
    handled = true;
  }

  if (entity_coordinator_.HasEntitySelection()) {
    handled = entity_coordinator_.NudgeSelected(delta_x, delta_y) || handled;
  }

  return handled;
}

void DungeonObjectInteraction::CheckForObjectSelection() {
  // Draw/update active marquee selection for tile objects (delegated).
  const ImGuiIO& io = ImGui::GetIO();
  const ImVec2 mouse_pos = GetCanvasTransform().ScreenToRoomPixels(io.MousePos);
  const bool mouse_left_released =
      ImGui::IsMouseReleased(ImGuiMouseButton_Left);

  if (mouse_left_released && selection_.IsRectangleSelectionActive()) {
    selection_.UpdateRectangleSelection(static_cast<int>(mouse_pos.x),
                                        static_cast<int>(mouse_pos.y));
    constexpr int kMinRectPixels = 6;
    if (!io.KeyAlt && selection_.IsRectangleLargeEnough(kMinRectPixels)) {
      entity_coordinator_.SelectEntitiesInRect(
          selection_.GetRectangleSelectionBounds(),
          /*additive=*/false,
          /*toggle=*/false);
    }
  }

  entity_coordinator_.tile_handler().HandleMarqueeSelection(
      mouse_pos,
      /*mouse_left_down=*/ImGui::IsMouseDown(ImGuiMouseButton_Left),
      /*mouse_left_released=*/mouse_left_released,
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
                               result.offset_y_tiles * 8, result.width_pixels(),
                               result.height_pixels());
      });

  // Enhanced hover tooltip showing object info (always visible on hover)
  // Skip completely in exclusive entity mode (door/sprite/item selected)
  if (entity_coordinator_.HasEntitySelection()) {
    return;  // Entity mode active - no object tooltips or hover
  }

  if (canvas_->IsMouseHovering()) {
    // Also skip tooltip if cursor is over a door/sprite/item entity (not selected yet)
    ImGuiIO& io = ImGui::GetIO();
    const auto [cursor_x, cursor_y] =
        GetCanvasTransform().ScreenToRoomPixelCoordinates(io.MousePos);
    auto entity_at_cursor =
        entity_coordinator_.GetEntityAtPosition(cursor_x, cursor_y);
    if (entity_at_cursor.has_value()) {
      // Entity has priority - skip object tooltip, DrawHoverHighlight will also skip
      DrawHoverHighlight(objects);
      return;
    }

    auto hovered_index = entity_coordinator_.tile_handler().GetEntityAtPosition(
        cursor_x, cursor_y);
    if (hovered_index.has_value() && *hovered_index < objects.size()) {
      const auto& object = objects[*hovered_index];
      std::string object_name = zelda3::GetObjectName(object.id_);
      int subtype = zelda3::GetObjectSubtype(object.id_);
      const int layer = object.GetLayerValue();

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
      if (zelda3::UsesRoomObjectStream(object)) {
        static constexpr const char* kStreamNames[] = {"Primary", "BG2 overlay",
                                                       "BG1 overlay"};
        const char* stream_name =
            (layer >= 0 && layer < 3) ? kStreamNames[layer] : "Unknown";
        tooltip += " | Object stream: " + std::string(stream_name);
      } else {
        const char* layer_name =
            layer == 0 ? "Upper layer (BG1)" : "Lower layer (BG2)";
        tooltip += " | Special layer: " + std::string(layer_name);
      }
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
  const DungeonCanvasTransform transform = GetCanvasTransform();
  const auto [cursor_canvas_x, cursor_canvas_y] =
      transform.ScreenToRoomPixelCoordinates(io.MousePos);
  auto entity_at_cursor =
      entity_coordinator_.GetEntityAtPosition(cursor_canvas_x, cursor_canvas_y);
  if (entity_at_cursor.has_value()) {
    return;  // Entity has priority - skip object hover highlight
  }

  auto hovered_index = entity_coordinator_.tile_handler().GetEntityAtPosition(
      cursor_canvas_x, cursor_canvas_y);
  if (!hovered_index.has_value() || *hovered_index >= objects.size()) {
    return;
  }
  const auto& object = objects[*hovered_index];

  // Don't draw hover highlight if object is already selected
  if (selection_.IsObjectSelected(*hovered_index)) {
    return;
  }

  const auto& theme = AgentUI::GetTheme();
  ImDrawList* draw_list = ImGui::GetWindowDrawList();
  // Calculate object position and dimensions
  auto [sel_x_px, sel_y_px, pixel_width, pixel_height] =
      zelda3::DimensionService::Get().GetSelectionBoundsPixels(object);

  ImVec2 obj_start = transform.RoomPixelsToScreen(
      ImVec2(static_cast<float>(sel_x_px), static_cast<float>(sel_y_px)));
  const ImVec2 obj_size = transform.RoomSizeToScreen(ImVec2(
      static_cast<float>(pixel_width), static_cast<float>(pixel_height)));
  ImVec2 obj_end(obj_start.x + obj_size.x, obj_start.y + obj_size.y);

  // Expand slightly for visibility
  constexpr float margin = 2.0f;
  obj_start.x -= margin;
  obj_start.y -= margin;
  obj_end.x += margin;
  obj_end.y += margin;

  // Draw subtle hover highlight with unified theme color
  ImVec4 hover_fill = theme.selection_hover;
  hover_fill.w *= 0.5f;  // Make it more subtle for hover fill

  ImVec4 hover_border = theme.selection_hover;

  // Draw filled background for better visibility
  draw_list->AddRectFilled(obj_start, obj_end, ImGui::GetColorU32(hover_fill));

  // Draw dashed-style border (simulated with thinner line)
  draw_list->AddRect(obj_start, obj_end, ImGui::GetColorU32(hover_border), 0.0f,
                     0, 1.5f);
}

void DungeonObjectInteraction::PlaceObjectAtPosition(int room_x, int room_y) {
  entity_coordinator_.tile_handler().PlaceObjectAt(
      current_room_id_, preview_object_, room_x, room_y);

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
  return dungeon_coords::IsWithinBounds(canvas_x, canvas_y, margin);
}

void DungeonObjectInteraction::SetCurrentRoom(DungeonRoomStore* rooms,
                                              int room_id) {
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
  if (!rooms_ || current_room_id_ < 0 ||
      current_room_id_ >= static_cast<int>(rooms_->size())) {
    return;
  }

  const auto selected_objects = selection_.GetSelectedIndices();
  const bool has_object_selection = !selected_objects.empty();
  const bool has_entity_selection = entity_coordinator_.HasEntitySelection();
  if (!has_object_selection && !has_entity_selection) {
    return;
  }

  entity_clipboard_.Clear();
  bool clipboard_origin_set = false;

  if (has_object_selection) {
    entity_coordinator_.tile_handler().CopyObjectsToClipboard(current_room_id_,
                                                              selected_objects);
    const auto& objects = (*rooms_)[current_room_id_].GetTileObjects();
    for (size_t index : selected_objects) {
      if (index >= objects.size()) {
        continue;
      }
      entity_clipboard_.origin_tile_x = objects[index].x_;
      entity_clipboard_.origin_tile_y = objects[index].y_;
      entity_clipboard_.origin_pixel_x =
          entity_clipboard_.origin_tile_x * dungeon_coords::kTileSize;
      entity_clipboard_.origin_pixel_y =
          entity_clipboard_.origin_tile_y * dungeon_coords::kTileSize;
      clipboard_origin_set = true;
      break;
    }
  } else {
    entity_coordinator_.tile_handler().ClearClipboard();
  }

  CopySelectedEntitiesToClipboard(clipboard_origin_set);
}

void DungeonObjectInteraction::CopySelectedEntitiesToClipboard(
    bool clipboard_origin_set) {
  if (!rooms_ || current_room_id_ < 0 ||
      current_room_id_ >= static_cast<int>(rooms_->size())) {
    return;
  }

  const auto& room = (*rooms_)[current_room_id_];
  auto selected_entities = entity_coordinator_.GetSelectedEntities();
  if (selected_entities.empty() && entity_coordinator_.HasEntitySelection()) {
    const SelectedEntity selected = entity_coordinator_.GetSelectedEntity();
    if (selected.type != EntityType::None) {
      selected_entities.push_back(selected);
    }
  }

  for (const auto entity : selected_entities) {
    switch (entity.type) {
      case EntityType::Sprite: {
        const auto& sprites = room.GetSprites();
        if (entity.index >= sprites.size()) {
          break;
        }
        const auto& sprite = sprites[entity.index];
        if (!clipboard_origin_set && !entity_clipboard_.HasData()) {
          entity_clipboard_.origin_pixel_x =
              sprite.x() * dungeon_coords::kSpriteTileSize;
          entity_clipboard_.origin_pixel_y =
              sprite.y() * dungeon_coords::kSpriteTileSize;
          entity_clipboard_.origin_tile_x =
              entity_clipboard_.origin_pixel_x / dungeon_coords::kTileSize;
          entity_clipboard_.origin_tile_y =
              entity_clipboard_.origin_pixel_y / dungeon_coords::kTileSize;
        }
        entity_clipboard_.sprites.push_back(sprite);
        break;
      }
      case EntityType::Item: {
        const auto& items = room.GetPotItems();
        if (entity.index >= items.size()) {
          break;
        }
        const auto& item = items[entity.index];
        if (!clipboard_origin_set && !entity_clipboard_.HasData()) {
          entity_clipboard_.origin_pixel_x = item.GetPixelX();
          entity_clipboard_.origin_pixel_y = item.GetPixelY();
          entity_clipboard_.origin_tile_x =
              entity_clipboard_.origin_pixel_x / dungeon_coords::kTileSize;
          entity_clipboard_.origin_tile_y =
              entity_clipboard_.origin_pixel_y / dungeon_coords::kTileSize;
        }
        entity_clipboard_.items.push_back(item);
        break;
      }
      case EntityType::Door:
      case EntityType::Object:
      case EntityType::None:
      default:
        break;
    }
  }
}

std::vector<SelectedEntity> DungeonObjectInteraction::PasteEntityClipboardAt(
    int target_pixel_x, int target_pixel_y) {
  std::vector<SelectedEntity> pasted_entities;
  if (!entity_clipboard_.HasData() || !rooms_ || current_room_id_ < 0 ||
      current_room_id_ >= static_cast<int>(rooms_->size())) {
    return pasted_entities;
  }

  auto& room = (*rooms_)[current_room_id_];
  const int delta_pixel_x = target_pixel_x - entity_clipboard_.origin_pixel_x;
  const int delta_pixel_y = target_pixel_y - entity_clipboard_.origin_pixel_y;

  if (!entity_clipboard_.sprites.empty()) {
    interaction_context_.NotifyMutation(MutationDomain::kSprites);
    auto& sprites = room.GetSprites();
    for (auto sprite : entity_clipboard_.sprites) {
      const int next_x = std::clamp(
          (sprite.x() * dungeon_coords::kSpriteTileSize + delta_pixel_x) /
              dungeon_coords::kSpriteTileSize,
          0, dungeon_coords::kSpriteGridMax);
      const int next_y = std::clamp(
          (sprite.y() * dungeon_coords::kSpriteTileSize + delta_pixel_y) /
              dungeon_coords::kSpriteTileSize,
          0, dungeon_coords::kSpriteGridMax);
      sprite.set_x(next_x);
      sprite.set_y(next_y);
      sprites.push_back(sprite);
      pasted_entities.push_back(
          SelectedEntity{EntityType::Sprite, sprites.size() - 1});
    }
    room.MarkSpritesDirty();
    interaction_context_.NotifyInvalidateCache(MutationDomain::kSprites);
  }

  if (!entity_clipboard_.items.empty()) {
    interaction_context_.NotifyMutation(MutationDomain::kItems);
    auto& items = room.GetPotItems();
    for (auto item : entity_clipboard_.items) {
      item.position = EncodePotItemPosition(item.GetPixelX() + delta_pixel_x,
                                            item.GetPixelY() + delta_pixel_y);
      items.push_back(item);
      pasted_entities.push_back(
          SelectedEntity{EntityType::Item, items.size() - 1});
    }
    room.MarkPotItemsDirty();
    interaction_context_.NotifyInvalidateCache(MutationDomain::kItems);
  }

  if (!pasted_entities.empty()) {
    interaction_context_.NotifyEntityChanged();
  }
  return pasted_entities;
}

void DungeonObjectInteraction::HandlePasteObjects() {
  if (!HasClipboardData()) {
    return;
  }

  if (!rooms_ || current_room_id_ < 0 ||
      current_room_id_ >= static_cast<int>(rooms_->size())) {
    return;
  }

  auto& handler = entity_coordinator_.tile_handler();
  const ImGuiIO& io = ImGui::GetIO();
  const auto [canvas_mouse_x, canvas_mouse_y] =
      GetCanvasTransform().ScreenToRoomPixelCoordinates(io.MousePos);
  auto [paste_x, paste_y] =
      CanvasToRoomCoordinates(canvas_mouse_x, canvas_mouse_y);
  int paste_pixel_x = paste_x * dungeon_coords::kTileSize;
  int paste_pixel_y = paste_y * dungeon_coords::kTileSize;

  if (!IsWithinCanvasBounds(canvas_mouse_x, canvas_mouse_y, 0)) {
    const int fallback_delta = entity_clipboard_.sprites.empty()
                                   ? dungeon_coords::kTileSize
                                   : dungeon_coords::kSpriteTileSize;
    paste_pixel_x =
        std::clamp(entity_clipboard_.origin_pixel_x + fallback_delta, 0,
                   dungeon_coords::kRoomPixelWidth - dungeon_coords::kTileSize);
    paste_pixel_y = std::clamp(
        entity_clipboard_.origin_pixel_y + fallback_delta, 0,
        dungeon_coords::kRoomPixelHeight - dungeon_coords::kTileSize);
    paste_x = paste_pixel_x / dungeon_coords::kTileSize;
    paste_y = paste_pixel_y / dungeon_coords::kTileSize;
  }

  std::vector<size_t> new_indices;
  if (handler.HasClipboardData()) {
    new_indices = handler.PasteFromClipboard(
        current_room_id_, paste_x - entity_clipboard_.origin_tile_x,
        paste_y - entity_clipboard_.origin_tile_y);
  }
  auto new_entities = PasteEntityClipboardAt(paste_pixel_x, paste_pixel_y);

  if (!new_indices.empty() || !new_entities.empty()) {
    selection_.ClearSelection();
    for (size_t idx : new_indices) {
      selection_.SelectObject(idx, ObjectSelection::SelectionMode::Add);
    }
    entity_coordinator_.SetSelectedEntities(std::move(new_entities));
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
  entity_coordinator_.tile_handler().UpdateObjectsId(current_room_id_, {index},
                                                     id);
  return true;
}

bool DungeonObjectInteraction::SetObjectSize(size_t index, uint8_t size) {
  entity_coordinator_.tile_handler().UpdateObjectsSize(current_room_id_,
                                                       {index}, size);
  return true;
}

bool DungeonObjectInteraction::SetObjectLayer(
    size_t index, zelda3::RoomObject::LayerType layer) {
  return entity_coordinator_.tile_handler().UpdateObjectsLayer(
      current_room_id_, {index}, static_cast<int>(layer));
}

std::pair<int, int> DungeonObjectInteraction::CalculateObjectBounds(
    const zelda3::RoomObject& object) {
  return zelda3::DimensionService::Get().GetPixelDimensions(object);
}

bool DungeonObjectInteraction::CanAssignSelectedObjectsToLayer(
    int target_layer) const {
  if (!rooms_ || current_room_id_ < 0 ||
      current_room_id_ >= static_cast<int>(rooms_->size()) ||
      target_layer < 0 || target_layer > 2) {
    return false;
  }

  const auto selected = selection_.GetSelectedIndices();
  if (selected.empty()) {
    return false;
  }

  const auto& objects = (*rooms_)[current_room_id_].GetTileObjects();
  for (const size_t index : selected) {
    if (index >= objects.size() ||
        (target_layer == 2 && !zelda3::UsesRoomObjectStream(objects[index]))) {
      return false;
    }
  }
  return true;
}

bool DungeonObjectInteraction::SendSelectedToLayer(int target_layer) {
  if (!CanAssignSelectedObjectsToLayer(target_layer)) {
    return false;
  }
  return entity_coordinator_.tile_handler().UpdateObjectsLayer(
      current_room_id_, selection_.GetSelectedIndices(), target_layer);
}

void DungeonObjectInteraction::SendSelectedToFront() {
  entity_coordinator_.tile_handler().SendToFront(
      current_room_id_, selection_.GetSelectedIndices());
}

void DungeonObjectInteraction::SendSelectedToBack() {
  entity_coordinator_.tile_handler().SendToBack(
      current_room_id_, selection_.GetSelectedIndices());
}

void DungeonObjectInteraction::BringSelectedForward() {
  entity_coordinator_.tile_handler().MoveForward(
      current_room_id_, selection_.GetSelectedIndices());
}

void DungeonObjectInteraction::SendSelectedBackward() {
  entity_coordinator_.tile_handler().MoveBackward(
      current_room_id_, selection_.GetSelectedIndices());
}

void DungeonObjectInteraction::HandleLayerKeyboardShortcuts() {
  // Only process if we have selected objects
  if (!selection_.HasSelection())
    return;

  // Only when not typing in a text field
  if (ImGui::IsAnyItemActive())
    return;

  // Check for stored placement shortcuts (1, 2, 3 keys). The third room
  // object stream is intentionally unavailable to torches/pushable blocks.
  if (ImGui::IsKeyPressed(ImGuiKey_1)) {
    SendSelectedToLayer(0);  // Primary stream / upper layer (BG1)
  } else if (ImGui::IsKeyPressed(ImGuiKey_2)) {
    SendSelectedToLayer(1);  // BG2 overlay / lower layer (BG2)
  } else if (ImGui::IsKeyPressed(ImGuiKey_3)) {
    SendSelectedToLayer(2);  // BG1 overlay stream
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
    entity_coordinator_.door_handler().SetDoorType(type);
    entity_coordinator_.door_handler().BeginPlacement();
  } else {
    entity_coordinator_.door_handler().CancelPlacement();
    if (mode_manager_.GetMode() == InteractionMode::PlaceDoor)
      mode_manager_.SetMode(InteractionMode::Select);
  }
}

// ============================================================================
// Sprite Placement Methods
// ============================================================================

void DungeonObjectInteraction::SetSpritePlacementMode(bool enabled,
                                                      uint8_t sprite_id) {
  if (enabled) {
    mode_manager_.SetMode(InteractionMode::PlaceSprite);
    entity_coordinator_.sprite_handler().SetSpriteId(sprite_id);
    entity_coordinator_.sprite_handler().BeginPlacement();
  } else {
    entity_coordinator_.sprite_handler().CancelPlacement();
    if (mode_manager_.GetMode() == InteractionMode::PlaceSprite)
      mode_manager_.SetMode(InteractionMode::Select);
  }
}

// ============================================================================
// Item Placement Methods
// ============================================================================

void DungeonObjectInteraction::SetItemPlacementMode(bool enabled,
                                                    uint8_t item_id) {
  if (enabled) {
    mode_manager_.SetMode(InteractionMode::PlaceItem);
    entity_coordinator_.item_handler().SetItemId(item_id);
    entity_coordinator_.item_handler().BeginPlacement();
  } else {
    entity_coordinator_.item_handler().CancelPlacement();
    if (mode_manager_.GetMode() == InteractionMode::PlaceItem)
      mode_manager_.SetMode(InteractionMode::Select);
  }
}

// ============================================================================
// Entity Selection Methods (Doors, Sprites, Items)
// ============================================================================

void DungeonObjectInteraction::SelectEntity(EntityType type, size_t index) {
  selection_.ClearSelection();
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
  entity_coordinator_.DrawPostPlacementOverlays();
}

void DungeonObjectInteraction::DrawDoorSnapIndicators() {
  // Door snap indicators are now managed by DoorInteractionHandler
  // through the entity coordinator. No-op here for backward compatibility.
}

}  // namespace yaze::editor
