// Related header
#include "door_interaction_handler.h"

// Third-party library headers
#include "imgui/imgui.h"

// Project headers
#include "app/editor/agent/agent_ui_theme.h"
#include "zelda3/dungeon/dungeon_limits.h"

namespace yaze::editor {

void DoorInteractionHandler::BeginPlacement() {
  door_placement_mode_ = true;
  ClearSelection();
}

void DoorInteractionHandler::CancelPlacement() {
  door_placement_mode_ = false;
  detected_door_direction_ = zelda3::DoorDirection::North;
  snapped_door_position_ = 0;
}

bool DoorInteractionHandler::HandleClick(int canvas_x, int canvas_y) {
  if (!HasValidContext()) return false;

  if (door_placement_mode_) {
    PlaceDoorAtSnappedPosition(canvas_x, canvas_y);
    return true;
  }

  // Try to select door at position
  auto door_index = GetEntityAtPosition(canvas_x, canvas_y);
  if (door_index.has_value()) {
    SelectDoor(*door_index);
    is_dragging_ = true;
    drag_start_pos_ = ImVec2(static_cast<float>(canvas_x),
                              static_cast<float>(canvas_y));
    drag_current_pos_ = drag_start_pos_;
    return true;
  }

  ClearSelection();
  return false;
}

void DoorInteractionHandler::HandleDrag(ImVec2 current_pos, ImVec2 delta) {
  if (!is_dragging_ || !selected_door_index_.has_value()) return;

  drag_current_pos_ = current_pos;
}

void DoorInteractionHandler::HandleRelease() {
  if (!is_dragging_ || !selected_door_index_.has_value()) {
    is_dragging_ = false;
    return;
  }

  auto* room = GetCurrentRoom();
  if (!room) {
    is_dragging_ = false;
    return;
  }

  int drag_x = static_cast<int>(drag_current_pos_.x);
  int drag_y = static_cast<int>(drag_current_pos_.y);

  // Detect wall from final position
  zelda3::DoorDirection direction;
  if (zelda3::DoorPositionManager::DetectWallFromPosition(drag_x, drag_y,
                                                           direction)) {
    uint8_t position = zelda3::DoorPositionManager::SnapToNearestPosition(
        drag_x, drag_y, direction);

    if (zelda3::DoorPositionManager::IsValidPosition(position, direction)) {
      ctx_->NotifyMutation(MutationDomain::kDoors);

      auto& doors = room->GetDoors();
      if (*selected_door_index_ < doors.size()) {
        doors[*selected_door_index_].position = position;
        doors[*selected_door_index_].direction = direction;

        // Re-encode bytes for ROM storage
        auto [b1, b2] = doors[*selected_door_index_].EncodeBytes();
        doors[*selected_door_index_].byte1 = b1;
        doors[*selected_door_index_].byte2 = b2;

        room->MarkObjectsDirty();
        ctx_->NotifyInvalidateCache(MutationDomain::kDoors);
      }
    }
  }

  is_dragging_ = false;
}

void DoorInteractionHandler::DrawGhostPreview() {
  if (!door_placement_mode_ || !HasValidContext()) return;

  auto* canvas = ctx_->canvas;
  if (!canvas->IsMouseHovering()) return;

  const ImGuiIO& io = ImGui::GetIO();
  ImVec2 canvas_pos = canvas->zero_point();
  int canvas_x = static_cast<int>(io.MousePos.x - canvas_pos.x);
  int canvas_y = static_cast<int>(io.MousePos.y - canvas_pos.y);

  // Try to update snapped position
  if (!UpdateSnappedPosition(canvas_x, canvas_y)) {
    // Placement guidance: make invalid hover state explicit instead of showing
    // no preview.
    if (canvas->IsMouseHovering()) {
      const auto& theme = AgentUI::GetTheme();
      ImDrawList* draw_list = ImGui::GetWindowDrawList();
      ImVec2 hint_pos(io.MousePos.x + 14.0f, io.MousePos.y + 10.0f);
      draw_list->AddText(hint_pos, ImGui::GetColorU32(theme.status_warning),
                         "Move cursor near a wall to place door");
      ImGui::SetTooltip("Door placement requires a wall-adjacent position.");
    }
    return;  // Not near a wall
  }

  // Get door position in tile coordinates
  auto [tile_x, tile_y] = zelda3::DoorPositionManager::PositionToTileCoords(
      snapped_door_position_, detected_door_direction_);

  // Get door dimensions
  auto dims = zelda3::GetDoorDimensions(detected_door_direction_);
  int door_width_px = dims.width_tiles * 8;
  int door_height_px = dims.height_tiles * 8;

  // Convert to canvas pixel coordinates
  auto [snap_canvas_x, snap_canvas_y] = RoomToCanvas(tile_x, tile_y);

  // Draw ghost preview
  ImDrawList* draw_list = ImGui::GetWindowDrawList();
  float scale = GetCanvasScale();

  ImVec2 preview_start(canvas_pos.x + snap_canvas_x * scale,
                       canvas_pos.y + snap_canvas_y * scale);
  ImVec2 preview_end(preview_start.x + door_width_px * scale,
                     preview_start.y + door_height_px * scale);

  const auto& theme = AgentUI::GetTheme();

  // Capacity-aware colors: normal / near-limit (>=14/16) / blocked (>=16/16).
  
  auto* room = GetCurrentRoom();
  size_t current_door_count = room ? room->GetDoors().size() : 0;
  const bool at_door_limit = (current_door_count >= zelda3::kMaxDoors);
  const bool near_door_limit = (current_door_count >= zelda3::kMaxDoors - 2);

  ImVec4 base_color = theme.dungeon_selection_primary;
  if (at_door_limit) {
    base_color = theme.status_error;
  } else if (near_door_limit) {
    base_color = theme.status_warning;
  }

  // Draw semi-transparent filled rectangle
  ImVec4 fill_vec(base_color.x, base_color.y, base_color.z, 0.31f);
  draw_list->AddRectFilled(preview_start, preview_end,
                           ImGui::GetColorU32(fill_vec));

  // Draw outline
  ImVec4 outline_color(base_color.x, base_color.y, base_color.z, 0.9f);
  draw_list->AddRect(preview_start, preview_end,
                     ImGui::GetColorU32(outline_color), 0.0f, 0, 2.0f);

  // Draw door type label
  std::string type_name(zelda3::GetDoorTypeName(preview_door_type_));
  std::string dir_name(zelda3::GetDoorDirectionName(detected_door_direction_));
  std::string label = type_name + " (" + dir_name + ")";

  ImVec2 text_pos(preview_start.x, preview_start.y - 16 * scale);
  draw_list->AddText(text_pos, IM_COL32(255, 255, 255, 200), label.c_str());

  // Capacity tooltip when at/near limit
  if ((at_door_limit || near_door_limit) &&
      ImGui::IsMouseHoveringRect(preview_start, preview_end)) {
    ImGui::SetTooltip("Doors: %zu/%zu%s", current_door_count, zelda3::kMaxDoors,
                      at_door_limit ? "\nPlacement blocked" : "\nNear limit");
  }

}

void DoorInteractionHandler::DrawSelectionHighlight() {
  if (!selected_door_index_.has_value() || !HasValidContext()) return;

  auto* room = GetCurrentRoom();
  if (!room) return;

  const auto& doors = room->GetDoors();
  if (*selected_door_index_ >= doors.size()) return;

  const auto& door = doors[*selected_door_index_];
  auto [tile_x, tile_y] = door.GetTileCoords();
  auto dims = zelda3::GetDoorDimensions(door.direction);

  // If dragging, use current drag position for door preview
  if (is_dragging_) {
    int drag_x = static_cast<int>(drag_current_pos_.x);
    int drag_y = static_cast<int>(drag_current_pos_.y);

    zelda3::DoorDirection dir;
    bool is_inner = false;
    if (zelda3::DoorPositionManager::DetectWallSection(drag_x, drag_y, dir,
                                                        is_inner)) {
      uint8_t snap_pos =
          zelda3::DoorPositionManager::SnapToNearestPosition(drag_x, drag_y, dir);
      auto [snap_x, snap_y] =
          zelda3::DoorPositionManager::PositionToTileCoords(snap_pos, dir);
      tile_x = snap_x;
      tile_y = snap_y;
      dims = zelda3::GetDoorDimensions(dir);
    }
  }

  ImDrawList* draw_list = ImGui::GetWindowDrawList();
  ImVec2 canvas_pos = GetCanvasZeroPoint();
  float scale = GetCanvasScale();

  ImVec2 pos(canvas_pos.x + tile_x * 8 * scale,
             canvas_pos.y + tile_y * 8 * scale);
  ImVec2 size(dims.width_tiles * 8 * scale, dims.height_tiles * 8 * scale);

  // Animated selection
  static float pulse = 0.0f;
  pulse += ImGui::GetIO().DeltaTime * 3.0f;
  float alpha = 0.5f + 0.3f * sinf(pulse);

  ImU32 color = IM_COL32(255, 165, 0, 180);  // Orange
  ImU32 fill_color = (color & 0x00FFFFFF) | (static_cast<ImU32>(alpha * 100) << 24);

  draw_list->AddRectFilled(pos, ImVec2(pos.x + size.x, pos.y + size.y),
                            fill_color);
  draw_list->AddRect(pos, ImVec2(pos.x + size.x, pos.y + size.y), color, 0.0f,
                     0, 2.0f);

  // Draw label
  ImVec2 text_pos(pos.x, pos.y - 14 * scale);
  draw_list->AddText(text_pos, IM_COL32(255, 255, 255, 220), "Door");

  // Draw snap indicators when dragging
  if (is_dragging_) {
    DrawSnapIndicators();
  }
}

std::optional<size_t> DoorInteractionHandler::GetEntityAtPosition(
    int canvas_x, int canvas_y) const {
  if (!HasValidContext()) return std::nullopt;

  auto* room = ctx_->GetCurrentRoomConst();
  if (!room) return std::nullopt;

  // Convert screen coordinates to room coordinates
  float scale = GetCanvasScale();
  int room_x = static_cast<int>(canvas_x / scale);
  int room_y = static_cast<int>(canvas_y / scale);

  const auto& doors = room->GetDoors();
  for (size_t i = 0; i < doors.size(); ++i) {
    const auto& door = doors[i];

    auto [tile_x, tile_y] = door.GetTileCoords();
    auto dims = zelda3::GetDoorDimensions(door.direction);

    int door_x = tile_x * 8;
    int door_y = tile_y * 8;
    int door_w = dims.width_tiles * 8;
    int door_h = dims.height_tiles * 8;

    if (room_x >= door_x && room_x < door_x + door_w && room_y >= door_y &&
        room_y < door_y + door_h) {
      return i;
    }
  }

  return std::nullopt;
}

void DoorInteractionHandler::SelectDoor(size_t index) {
  selected_door_index_ = index;
  ctx_->NotifyEntityChanged();
}

void DoorInteractionHandler::ClearSelection() {
  selected_door_index_ = std::nullopt;
  is_dragging_ = false;
}

void DoorInteractionHandler::DeleteSelected() {
  if (!selected_door_index_.has_value() || !HasValidContext()) return;

  auto* room = GetCurrentRoom();
  if (!room) return;

  auto& doors = room->GetDoors();
  if (*selected_door_index_ >= doors.size()) return;

  ctx_->NotifyMutation(MutationDomain::kDoors);
  doors.erase(doors.begin() + static_cast<ptrdiff_t>(*selected_door_index_));
  room->MarkObjectsDirty();
  ctx_->NotifyInvalidateCache(MutationDomain::kDoors);
  ClearSelection();
}

void DoorInteractionHandler::PlaceDoorAtSnappedPosition(int canvas_x,
                                                         int canvas_y) {
  if (!HasValidContext()) {
    placement_block_reason_ = PlacementBlockReason::kInvalidRoom;
    return;
  }

  auto* room = GetCurrentRoom();
  if (!room) {
    placement_block_reason_ = PlacementBlockReason::kInvalidRoom;
    return;
  }

  // Enforce door limit at placement time (matches DungeonValidator::zelda3::kMaxDoors)
  
  if (room->GetDoors().size() >= zelda3::kMaxDoors) {
    placement_block_reason_ = PlacementBlockReason::kDoorLimit;
    return;
  }

  // Detect wall from position
  zelda3::DoorDirection direction;
  if (!zelda3::DoorPositionManager::DetectWallFromPosition(canvas_x, canvas_y,
                                                            direction)) {
    placement_block_reason_ = PlacementBlockReason::kInvalidPosition;
    return;
  }

  // Snap to nearest valid position
  uint8_t position =
      zelda3::DoorPositionManager::SnapToNearestPosition(canvas_x, canvas_y, direction);

  // Validate position
  if (!zelda3::DoorPositionManager::IsValidPosition(position, direction)) {
    placement_block_reason_ = PlacementBlockReason::kInvalidPosition;
    return;
  }

  placement_block_reason_ = PlacementBlockReason::kNone;

  ctx_->NotifyMutation(MutationDomain::kDoors);

  // Create the door
  zelda3::Room::Door new_door;
  new_door.position = position;
  new_door.type = preview_door_type_;
  new_door.direction = direction;

  // Encode bytes for ROM storage
  auto [byte1, byte2] = new_door.EncodeBytes();
  new_door.byte1 = byte1;
  new_door.byte2 = byte2;

  // Add door to room
  room->AddDoor(new_door);
  TriggerSuccessToast();

  ctx_->NotifyInvalidateCache(MutationDomain::kDoors);
}

bool DoorInteractionHandler::UpdateSnappedPosition(int canvas_x, int canvas_y) {
  zelda3::DoorDirection direction;
  if (!zelda3::DoorPositionManager::DetectWallFromPosition(canvas_x, canvas_y,
                                                            direction)) {
    return false;
  }

  detected_door_direction_ = direction;
  snapped_door_position_ =
      zelda3::DoorPositionManager::SnapToNearestPosition(canvas_x, canvas_y, direction);
  return true;
}

void DoorInteractionHandler::DrawSnapIndicators() {
  if (!is_dragging_ || !HasValidContext()) return;

  int drag_x = static_cast<int>(drag_current_pos_.x);
  int drag_y = static_cast<int>(drag_current_pos_.y);

  zelda3::DoorDirection direction;
  bool is_inner = false;
  if (!zelda3::DoorPositionManager::DetectWallSection(drag_x, drag_y, direction,
                                                       is_inner)) {
    return;
  }

  uint8_t start_pos =
      zelda3::DoorPositionManager::GetSectionStartPosition(direction, is_inner);
  uint8_t nearest_snap =
      zelda3::DoorPositionManager::SnapToNearestPosition(drag_x, drag_y, direction);

  ImDrawList* draw_list = ImGui::GetWindowDrawList();
  ImVec2 canvas_pos = GetCanvasZeroPoint();
  float scale = GetCanvasScale();
  const auto& theme = AgentUI::GetTheme();
  auto dims = zelda3::GetDoorDimensions(direction);

  // Capacity-aware indicator colors: inherit the same thresholds used in
  // DrawGhostPreview so dragging mirrors the placement ghost feedback.
  
  auto* snap_room = GetCurrentRoom();
  size_t door_count = snap_room ? snap_room->GetDoors().size() : 0;
  const bool snap_at_limit = (door_count >= zelda3::kMaxDoors);
  const bool snap_near_limit = (door_count >= zelda3::kMaxDoors - 2);

  // Draw indicators for 6 positions in this section
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
      // Highlighted nearest snap position — capacity-aware color.
      ImVec4 base;
      if (snap_at_limit) {
        base = theme.status_error;
      } else if (snap_near_limit) {
        base = theme.status_warning;
      } else {
        base = theme.dungeon_selection_primary;
      }
      ImVec4 highlight(base.x, base.y, base.z, 0.75f);
      draw_list->AddRect(snap_start, snap_end, ImGui::GetColorU32(highlight),
                         0.0f, 0, 2.5f);
    } else {
      // Ghosted other positions — tint red when at limit.
      ImVec4 ghost = snap_at_limit
                         ? ImVec4(theme.status_error.x, theme.status_error.y,
                                  theme.status_error.z, 0.20f)
                         : ImVec4(1.0f, 1.0f, 1.0f, 0.25f);
      draw_list->AddRect(snap_start, snap_end, ImGui::GetColorU32(ghost), 0.0f,
                         0, 1.0f);
    }
  }
}

}  // namespace yaze::editor
