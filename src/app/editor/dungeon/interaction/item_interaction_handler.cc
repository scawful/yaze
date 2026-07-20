// Related header
#include "item_interaction_handler.h"

// C++ standard library
#include <algorithm>

// Third-party library headers
#include "absl/strings/str_format.h"
#include "imgui/imgui.h"

// Project headers
#include "app/editor/agent/agent_ui_theme.h"
#include "app/editor/dungeon/dungeon_coordinates.h"

namespace yaze::editor {

void ItemInteractionHandler::BeginPlacement() {
  item_placement_mode_ = true;
  ClearSelection();
}

bool ItemInteractionHandler::HandleClick(int canvas_x, int canvas_y) {
  if (!HasValidContext())
    return false;

  if (item_placement_mode_) {
    if (IsWithinBounds(canvas_x, canvas_y)) {
      PlaceItemAtPosition(canvas_x, canvas_y);
    }
    return true;
  }

  // Try to select item at position
  auto item_index = GetEntityAtPosition(canvas_x, canvas_y);
  if (item_index.has_value()) {
    SelectItem(*item_index);
    is_dragging_ = true;
    drag_start_pos_ =
        ImVec2(static_cast<float>(canvas_x), static_cast<float>(canvas_y));
    drag_current_pos_ = drag_start_pos_;
    return true;
  }

  ClearSelection();
  return false;
}

void ItemInteractionHandler::HandleDrag(ImVec2 current_pos, ImVec2 delta) {
  if (!is_dragging_ || !selected_item_index_.has_value())
    return;
  drag_current_pos_ = current_pos;
}

void ItemInteractionHandler::HandleRelease() {
  if (!is_dragging_ || !selected_item_index_.has_value()) {
    is_dragging_ = false;
    return;
  }

  auto* room = GetCurrentRoom();
  if (!room) {
    is_dragging_ = false;
    return;
  }

  const int pixel_x = std::clamp(static_cast<int>(drag_current_pos_.x), 0,
                                 dungeon_coords::kRoomPixelWidth - 1);
  const int pixel_y = std::clamp(static_cast<int>(drag_current_pos_.y), 0,
                                 dungeon_coords::kRoomPixelHeight - 1);

  // PotItem position encoding:
  // high byte * 16 = Y, low byte * 4 = X
  const int encoded_x = pixel_x / 4;
  const int encoded_y = pixel_y / 16;
  const uint16_t next_position =
      static_cast<uint16_t>((encoded_y << 8) | encoded_x);

  auto& pot_items = room->GetPotItems();
  if (*selected_item_index_ < pot_items.size()) {
    if (pot_items[*selected_item_index_].position == next_position) {
      is_dragging_ = false;
      return;
    }

    ctx_->NotifyMutation(MutationDomain::kItems);
    pot_items[*selected_item_index_].position = next_position;
    room->MarkPotItemsDirty();
    ctx_->NotifyInvalidateCache(MutationDomain::kItems);
    ctx_->NotifyEntityChanged();
  }

  is_dragging_ = false;
}

void ItemInteractionHandler::DrawGhostPreview() {
  if (!item_placement_mode_ || !HasValidContext())
    return;

  auto* canvas = ctx_->canvas;
  const auto pointer_screen_pos = GetPointerScreenPosition();
  if (!pointer_screen_pos.has_value())
    return;

  const DungeonCanvasTransform transform = GetCanvasTransform();
  const auto [canvas_x, canvas_y] =
      transform.ScreenToRoomPixelCoordinates(*pointer_screen_pos);

  // Snap to 8-pixel grid
  int snapped_x =
      (canvas_x / dungeon_coords::kTileSize) * dungeon_coords::kTileSize;
  int snapped_y =
      (canvas_y / dungeon_coords::kTileSize) * dungeon_coords::kTileSize;

  // Draw ghost rectangle for item preview
  const ImVec2 rect_min = transform.RoomPixelsToScreen(
      ImVec2(static_cast<float>(snapped_x), static_cast<float>(snapped_y)));
  const ImVec2 rect_size = transform.RoomSizeToScreen(ImVec2(16, 16));
  const ImVec2 rect_max(rect_min.x + rect_size.x, rect_min.y + rect_size.y);

  const auto& theme = AgentUI::GetTheme();
  ImVec4 fill_color = theme.dungeon_selection_primary;
  fill_color.w = 0.35f;
  ImVec4 outline_color = theme.dungeon_selection_primary;
  outline_color.w = 0.85f;

  canvas->draw_list()->AddRectFilled(rect_min, rect_max,
                                     ImGui::GetColorU32(fill_color));
  canvas->draw_list()->AddRect(
      rect_min, rect_max, ImGui::GetColorU32(outline_color), 0.0f, 0, 2.0f);

  // Draw item ID label
  std::string label = absl::StrFormat("%02X", preview_item_id_);
  canvas->draw_list()->AddText(rect_min, ImGui::GetColorU32(theme.text_primary),
                               label.c_str());
}

void ItemInteractionHandler::DrawSelectionHighlight() {
  if (!selected_item_index_.has_value() || !HasValidContext())
    return;

  auto* room = GetCurrentRoom();
  if (!room)
    return;

  const auto& pot_items = room->GetPotItems();
  if (*selected_item_index_ >= pot_items.size())
    return;

  const auto& pot_item = pot_items[*selected_item_index_];
  int pixel_x = pot_item.GetPixelX();
  int pixel_y = pot_item.GetPixelY();

  // If dragging, use current drag position
  if (is_dragging_) {
    pixel_x = static_cast<int>(drag_current_pos_.x);
    pixel_y = static_cast<int>(drag_current_pos_.y);
    // Snap to 8-pixel grid
    pixel_x = (pixel_x / dungeon_coords::kTileSize) * dungeon_coords::kTileSize;
    pixel_y = (pixel_y / dungeon_coords::kTileSize) * dungeon_coords::kTileSize;
  }

  ImDrawList* draw_list = ImGui::GetWindowDrawList();
  const DungeonCanvasTransform transform = GetCanvasTransform();
  const float scale = transform.scale();
  const ImVec2 pos = transform.RoomPixelsToScreen(
      ImVec2(static_cast<float>(pixel_x), static_cast<float>(pixel_y)));
  const ImVec2 size = transform.RoomSizeToScreen(ImVec2(16, 16));

  // Animated selection
  static float pulse = 0.0f;
  pulse += ImGui::GetIO().DeltaTime * 3.0f;
  float alpha = 0.5f + 0.3f * sinf(pulse);

  ImU32 color = IM_COL32(255, 255, 0, 180);  // Yellow
  ImU32 fill_color =
      (color & 0x00FFFFFF) | (static_cast<ImU32>(alpha * 100) << 24);

  draw_list->AddRectFilled(pos, ImVec2(pos.x + size.x, pos.y + size.y),
                           fill_color);
  draw_list->AddRect(pos, ImVec2(pos.x + size.x, pos.y + size.y), color, 0.0f,
                     0, 2.0f);

  // Draw label
  ImVec2 text_pos(pos.x, pos.y - 14 * scale);
  draw_list->AddText(text_pos, IM_COL32(255, 255, 255, 220), "Item");
}

std::optional<size_t> ItemInteractionHandler::GetEntityAtPosition(
    int canvas_x, int canvas_y) const {
  if (!HasValidContext() || !IsWithinBounds(canvas_x, canvas_y))
    return std::nullopt;

  auto* room = ctx_->GetCurrentRoomConst();
  if (!room)
    return std::nullopt;

  // Check pot items
  const auto& pot_items = room->GetPotItems();
  for (size_t i = 0; i < pot_items.size(); ++i) {
    const auto& pot_item = pot_items[i];

    int item_x = pot_item.GetPixelX();
    int item_y = pot_item.GetPixelY();

    // 16x16 hitbox
    if (canvas_x >= item_x && canvas_x < item_x + 16 && canvas_y >= item_y &&
        canvas_y < item_y + 16) {
      return i;
    }
  }

  return std::nullopt;
}

void ItemInteractionHandler::SelectItem(size_t index) {
  selected_item_index_ = index;
  ctx_->NotifyEntityChanged();
}

void ItemInteractionHandler::ClearSelection() {
  selected_item_index_ = std::nullopt;
  is_dragging_ = false;
}

void ItemInteractionHandler::DeleteSelected() {
  if (!selected_item_index_.has_value() || !HasValidContext())
    return;

  auto* room = GetCurrentRoom();
  if (!room)
    return;

  auto& pot_items = room->GetPotItems();
  if (*selected_item_index_ >= pot_items.size())
    return;

  ctx_->NotifyMutation(MutationDomain::kItems);
  pot_items.erase(pot_items.begin() +
                  static_cast<ptrdiff_t>(*selected_item_index_));
  room->MarkPotItemsDirty();
  ctx_->NotifyInvalidateCache(MutationDomain::kItems);
  ClearSelection();
  ctx_->NotifyEntityChanged();
}

void ItemInteractionHandler::DeleteAll() {
  if (!HasValidContext()) {
    return;
  }

  auto* room = GetCurrentRoom();
  if (!room || room->GetPotItems().empty()) {
    return;
  }

  ctx_->NotifyMutation(MutationDomain::kItems);
  room->GetPotItems().clear();
  room->MarkPotItemsDirty();
  ctx_->NotifyInvalidateCache(MutationDomain::kItems);
  ClearSelection();
  ctx_->NotifyEntityChanged();
}

bool ItemInteractionHandler::NudgeSelected(int delta_pixel_x,
                                           int delta_pixel_y) {
  if (!selected_item_index_.has_value() || !HasValidContext()) {
    return false;
  }

  auto* room = GetCurrentRoom();
  if (!room) {
    return false;
  }

  auto& pot_items = room->GetPotItems();
  if (*selected_item_index_ >= pot_items.size()) {
    return false;
  }

  auto& pot_item = pot_items[*selected_item_index_];
  constexpr int kRoomPixelMax = 511;
  const int next_pixel_x =
      std::clamp(pot_item.GetPixelX() + delta_pixel_x, 0, kRoomPixelMax);
  const int next_pixel_y =
      std::clamp(pot_item.GetPixelY() + delta_pixel_y, 0, kRoomPixelMax);
  const int encoded_x = std::clamp(next_pixel_x / 4, 0, 255);
  const int encoded_y = std::clamp(next_pixel_y / 16, 0, 255);
  const uint16_t next_position =
      static_cast<uint16_t>((encoded_y << 8) | encoded_x);
  if (next_position == pot_item.position) {
    return false;
  }

  ctx_->NotifyMutation(MutationDomain::kItems);
  pot_item.position = next_position;
  room->MarkPotItemsDirty();
  ctx_->NotifyInvalidateCache(MutationDomain::kItems);
  ctx_->NotifyEntityChanged();
  return true;
}

void ItemInteractionHandler::PlaceItemAtPosition(int canvas_x, int canvas_y) {
  if (!HasValidContext())
    return;

  auto* room = GetCurrentRoom();
  if (!room)
    return;

  int pixel_x = canvas_x;
  int pixel_y = canvas_y;

  // PotItem position encoding:
  // high byte * 16 = Y, low byte * 4 = X
  int encoded_x = pixel_x / 4;
  int encoded_y = pixel_y / 16;

  // Clamp to valid range
  encoded_x = std::clamp(encoded_x, 0, 255);
  encoded_y = std::clamp(encoded_y, 0, 255);

  ctx_->NotifyMutation(MutationDomain::kItems);

  // Create the pot item
  zelda3::PotItem new_item;
  new_item.position = static_cast<uint16_t>((encoded_y << 8) | encoded_x);
  new_item.item = preview_item_id_;

  // Add item to room
  room->GetPotItems().push_back(new_item);
  room->MarkPotItemsDirty();

  ctx_->NotifyInvalidateCache(MutationDomain::kItems);
}

}  // namespace yaze::editor
