// Related header
#include "door_interaction_handler.h"

// Third-party library headers
#include "imgui/imgui.h"

#include <algorithm>
#include <cmath>
#include <string>

// Project headers
#include "absl/strings/str_format.h"
#include "app/editor/agent/agent_ui_theme.h"
#include "app/editor/dungeon/dungeon_canvas_viewer.h"
#include "app/editor/dungeon/interaction/ghost_preview_feedback.h"
#include "zelda3/dungeon/dungeon_limits.h"

namespace yaze::editor {

namespace {

PlacementCapacityState ToPlacementCapacityState(
    DoorInteractionHandler::GhostCapacityState state) {
  return static_cast<PlacementCapacityState>(state);
}

}  // namespace

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
  if (!HasValidContext())
    return false;

  if (door_placement_mode_) {
    PlaceDoorAtSnappedPosition(canvas_x, canvas_y);
    return true;
  }

  // Try to select door at position
  auto door_index = GetEntityAtPosition(canvas_x, canvas_y);
  if (door_index.has_value()) {
    SelectDoor(*door_index);
    is_dragging_ = true;
    drag_start_pos_ =
        ImVec2(static_cast<float>(canvas_x), static_cast<float>(canvas_y));
    drag_current_pos_ = drag_start_pos_;
    return true;
  }

  ClearSelection();
  return false;
}

void DoorInteractionHandler::HandleDrag(ImVec2 current_pos, ImVec2 delta) {
  if (!is_dragging_ || !selected_door_index_.has_value())
    return;

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

DoorInteractionHandler::GhostCapacityState
DoorInteractionHandler::GetPlacementGhostCapacityState() const {
  auto* room = GetCurrentRoom();
  const size_t current_door_count = room ? room->GetDoors().size() : 0;
  return static_cast<GhostCapacityState>(
      GetPlacementCapacityState(current_door_count, zelda3::kMaxDoors));
}

void DoorInteractionHandler::DrawGhostPreview() {
  if (!door_placement_mode_ || !HasValidContext())
    return;

  auto* canvas = ctx_->canvas;
  if (!canvas->IsMouseHovering())
    return;

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
  auto dims = zelda3::GetEditorDoorDimensions(detected_door_direction_,
                                              preview_door_type_);
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

  const auto capacity_state = GetPlacementGhostCapacityState();
  const auto placement_state = ToPlacementCapacityState(capacity_state);
  auto* room = GetCurrentRoom();
  const size_t current_door_count = room ? room->GetDoors().size() : 0;

  const ImVec4 base_color = GetPlacementAccentColor(
      theme, placement_state, theme.dungeon_selection_primary);

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

  const std::string badge_text =
      absl::StrFormat("Doors %zu/%zu", current_door_count, zelda3::kMaxDoors);
  ImVec2 badge_min;
  switch (detected_door_direction_) {
    case zelda3::DoorDirection::North:
      badge_min = ImVec2(preview_start.x, preview_end.y + 6.0f);
      break;
    case zelda3::DoorDirection::South: {
      const ImVec2 badge_size = ImGui::CalcTextSize(badge_text.c_str());
      const ImVec2 status_size = ImGui::CalcTextSize(
          GetPlacementCapacityStatusText(placement_state).data());
      const float badge_height = badge_size.y + status_size.y + 14.0f;
      badge_min =
          ImVec2(preview_start.x, preview_start.y - badge_height - 6.0f);
      break;
    }
    case zelda3::DoorDirection::West:
      badge_min = ImVec2(preview_end.x + 6.0f, preview_start.y);
      break;
    case zelda3::DoorDirection::East: {
      const ImVec2 badge_size = ImGui::CalcTextSize(badge_text.c_str());
      const ImVec2 status_size = ImGui::CalcTextSize(
          GetPlacementCapacityStatusText(placement_state).data());
      const float badge_width = std::max(badge_size.x, status_size.x) + 14.0f;
      badge_min = ImVec2(preview_start.x - badge_width - 6.0f, preview_start.y);
      break;
    }
  }
  DrawPlacementCapacityBadge(draw_list, badge_min, theme, placement_state,
                             badge_text);

  // Capacity tooltip when at/near limit
  if (capacity_state != GhostCapacityState::kNormal &&
      ImGui::IsMouseHoveringRect(preview_start, preview_end)) {
    ImGui::SetTooltip(
        "Doors: %zu/%zu\n%s", current_door_count, zelda3::kMaxDoors,
        GetPlacementCapacityTooltipSuffix(placement_state).data());
  }
}

void DoorInteractionHandler::DrawSelectionHighlight() {
  if (!selected_door_index_.has_value() || !HasValidContext())
    return;

  auto* room = GetCurrentRoom();
  if (!room)
    return;

  const auto& doors = room->GetDoors();
  if (*selected_door_index_ >= doors.size())
    return;

  const auto& door = doors[*selected_door_index_];
  auto [tile_x, tile_y] = door.GetTileCoords();
  auto dims = door.GetEditorDimensions();

  // If dragging, use current drag position for door preview
  if (is_dragging_) {
    int drag_x = static_cast<int>(drag_current_pos_.x);
    int drag_y = static_cast<int>(drag_current_pos_.y);

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
      dims = zelda3::GetEditorDoorDimensions(dir, door.type);
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
  ImU32 fill_color =
      (color & 0x00FFFFFF) | (static_cast<ImU32>(alpha * 100) << 24);

  draw_list->AddRectFilled(pos, ImVec2(pos.x + size.x, pos.y + size.y),
                           fill_color);
  draw_list->AddRect(pos, ImVec2(pos.x + size.x, pos.y + size.y), color, 0.0f,
                     0, 2.0f);

  // Draw label + reciprocal-door cue (D3). Badge hangs off the wall-facing
  // edge of the selection box so the cue visually points at the neighbor room.
  std::string pair_badge;
  ImU32 pair_color = IM_COL32(255, 255, 255, 220);
  if (!is_dragging_ && ctx_ && ctx_->rooms) {
    const int neighbor = NeighborRoomId(ctx_->current_room_id, door.direction);
    if (neighbor < 0) {
      pair_badge = "edge";
      pair_color = IM_COL32(170, 170, 170, 220);
    } else {
      const auto opposite = OppositeDir(door.direction);
      const auto& neighbor_doors = (*ctx_->rooms)[neighbor].GetDoors();
      bool exact_pair = false;
      bool any_on_opposite = false;
      for (const auto& nd : neighbor_doors) {
        if (nd.direction == opposite) {
          any_on_opposite = true;
          if (nd.position == door.position) {
            exact_pair = true;
            break;
          }
        }
      }
      if (exact_pair) {
        pair_badge = absl::StrFormat("pair 0x%03X", neighbor);
        pair_color = IM_COL32(120, 220, 150, 235);  // green
      } else if (any_on_opposite) {
        pair_badge = absl::StrFormat("~0x%03X", neighbor);
        pair_color = IM_COL32(255, 200, 90, 235);  // amber — approximate pair
      } else {
        pair_badge = absl::StrFormat("no pair 0x%03X", neighbor);
        pair_color = IM_COL32(255, 130, 90, 235);  // red-orange
      }
    }
  }

  ImVec2 label_pos(pos.x, pos.y - 14 * scale);
  draw_list->AddText(label_pos, IM_COL32(255, 255, 255, 220), "Door");

  if (!pair_badge.empty()) {
    // Anchor the badge on the wall-facing side so it cues the neighbor room.
    ImVec2 badge_pos;
    switch (door.direction) {
      case zelda3::DoorDirection::North:
        badge_pos = ImVec2(pos.x + 40.0f, pos.y - 14 * scale);
        break;
      case zelda3::DoorDirection::South:
        badge_pos = ImVec2(pos.x, pos.y + size.y + 2.0f);
        break;
      case zelda3::DoorDirection::West:
        badge_pos = ImVec2(pos.x - 88.0f, pos.y + size.y * 0.5f - 7.0f);
        break;
      case zelda3::DoorDirection::East:
        badge_pos = ImVec2(pos.x + size.x + 6.0f, pos.y + size.y * 0.5f - 7.0f);
        break;
    }
    draw_list->AddText(badge_pos, pair_color, pair_badge.c_str());
  }

  // Draw snap indicators when dragging
  if (is_dragging_) {
    DrawSnapIndicators();
  }
}

std::optional<size_t> DoorInteractionHandler::GetEntityAtPosition(
    int canvas_x, int canvas_y) const {
  if (!HasValidContext())
    return std::nullopt;

  auto* room = ctx_->GetCurrentRoomConst();
  if (!room)
    return std::nullopt;

  // Convert screen coordinates to room coordinates
  float scale = GetCanvasScale();
  int room_x = static_cast<int>(canvas_x / scale);
  int room_y = static_cast<int>(canvas_y / scale);

  const auto& doors = room->GetDoors();
  for (size_t i = 0; i < doors.size(); ++i) {
    const auto& door = doors[i];

    auto [door_x, door_y, door_w, door_h] = door.GetEditorBounds();

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
  if (!selected_door_index_.has_value() || !HasValidContext())
    return;

  auto* room = GetCurrentRoom();
  if (!room)
    return;

  auto& doors = room->GetDoors();
  if (*selected_door_index_ >= doors.size())
    return;

  ctx_->NotifyMutation(MutationDomain::kDoors);
  doors.erase(doors.begin() + static_cast<ptrdiff_t>(*selected_door_index_));
  room->MarkObjectStreamDirty();
  ctx_->NotifyInvalidateCache(MutationDomain::kDoors);
  ClearSelection();
  ctx_->NotifyEntityChanged();
}

void DoorInteractionHandler::DeleteAll() {
  if (!HasValidContext()) {
    return;
  }

  auto* room = GetCurrentRoom();
  if (!room || room->GetDoors().empty()) {
    return;
  }

  ctx_->NotifyMutation(MutationDomain::kDoors);
  room->GetDoors().clear();
  room->MarkObjectStreamDirty();
  ctx_->NotifyInvalidateCache(MutationDomain::kDoors);
  ClearSelection();
  ctx_->NotifyEntityChanged();
}

bool DoorInteractionHandler::MutateDoorType(size_t index,
                                            zelda3::DoorType new_type) {
  if (!HasValidContext())
    return false;

  auto* room = GetCurrentRoom();
  if (!room)
    return false;

  auto& doors = room->GetDoors();
  if (index >= doors.size())
    return false;

  auto& door = doors[index];
  if (door.type == new_type)
    return false;

  ctx_->NotifyMutation(MutationDomain::kDoors);

  door.type = new_type;
  auto [b1, b2] = door.EncodeBytes();
  door.byte1 = b1;
  door.byte2 = b2;

  room->MarkObjectsDirty();
  ctx_->NotifyInvalidateCache(MutationDomain::kDoors);
  ctx_->NotifyEntityChanged();
  return true;
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
  uint8_t position = zelda3::DoorPositionManager::SnapToNearestPosition(
      canvas_x, canvas_y, direction);

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
  snapped_door_position_ = zelda3::DoorPositionManager::SnapToNearestPosition(
      canvas_x, canvas_y, direction);
  return true;
}

void DoorInteractionHandler::DrawSnapIndicators() {
  if (!is_dragging_ || !HasValidContext())
    return;

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
  uint8_t nearest_snap = zelda3::DoorPositionManager::SnapToNearestPosition(
      drag_x, drag_y, direction);

  ImDrawList* draw_list = ImGui::GetWindowDrawList();
  ImVec2 canvas_pos = GetCanvasZeroPoint();
  float scale = GetCanvasScale();
  const auto& theme = AgentUI::GetTheme();
  zelda3::DoorType indicator_type = preview_door_type_;
  auto* room = GetCurrentRoom();
  if (room && selected_door_index_.has_value() &&
      *selected_door_index_ < room->GetDoors().size()) {
    indicator_type = room->GetDoors()[*selected_door_index_].type;
  }
  auto dims = zelda3::GetEditorDoorDimensions(direction, indicator_type);

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
      ImVec4 highlight(theme.dungeon_selection_primary.x,
                       theme.dungeon_selection_primary.y,
                       theme.dungeon_selection_primary.z, 0.75f);
      draw_list->AddRect(snap_start, snap_end, ImGui::GetColorU32(highlight),
                         0.0f, 0, 2.5f);
    } else {
      ImVec4 ghost(1.0f, 1.0f, 1.0f, 0.25f);
      draw_list->AddRect(snap_start, snap_end, ImGui::GetColorU32(ghost), 0.0f,
                         0, 1.0f);
    }
  }
}

}  // namespace yaze::editor
