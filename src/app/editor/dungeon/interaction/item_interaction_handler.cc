// Related header
#include "item_interaction_handler.h"

// C++ standard library
#include <algorithm>

// Third-party library headers
#include "absl/strings/str_format.h"
#include "imgui/imgui.h"

// Project headers
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
    PlaceItemAtPosition(canvas_x, canvas_y);
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

  float scale = GetCanvasScale();

  // Convert to pixel coordinates
  int pixel_x = static_cast<int>(drag_current_pos_.x / scale);
  int pixel_y = static_cast<int>(drag_current_pos_.y / scale);

  // PotItem position encoding:
  // high byte * 16 = Y, low byte * 4 = X
  int encoded_x = pixel_x / 4;
  int encoded_y = pixel_y / 16;

  // Clamp to valid range
  encoded_x = std::clamp(encoded_x, 0, 255);
  encoded_y = std::clamp(encoded_y, 0, 255);

  auto& pot_items = room->GetPotItems();
  if (*selected_item_index_ < pot_items.size()) {
    ctx_->NotifyMutation();

    pot_items[*selected_item_index_].position =
        static_cast<uint16_t>((encoded_y << 8) | encoded_x);

    ctx_->NotifyEntityChanged();
  }

  is_dragging_ = false;
}

void ItemInteractionHandler::DrawGhostPreview() {
  if (!item_placement_mode_ || !HasValidContext())
    return;

  auto* canvas = ctx_->canvas;
  if (!canvas->IsMouseHovering())
    return;

  const ImGuiIO& io = ImGui::GetIO();
  ImVec2 canvas_pos = canvas->zero_point();
  float scale = GetCanvasScale();

  // Convert to room coordinates (items use 8-pixel grid for fine positioning)
  int canvas_x = static_cast<int>((io.MousePos.x - canvas_pos.x) / scale);
  int canvas_y = static_cast<int>((io.MousePos.y - canvas_pos.y) / scale);

  // Snap to 8-pixel grid
  int snapped_x =
      (canvas_x / dungeon_coords::kTileSize) * dungeon_coords::kTileSize;
  int snapped_y =
      (canvas_y / dungeon_coords::kTileSize) * dungeon_coords::kTileSize;

  // Draw ghost rectangle for item preview
  ImVec2 rect_min(canvas_pos.x + snapped_x * scale,
                  canvas_pos.y + snapped_y * scale);
  ImVec2 rect_max(rect_min.x + 16 * scale, rect_min.y + 16 * scale);

  // Semi-transparent yellow for items
  ImU32 fill_color = IM_COL32(200, 200, 50, 100);
  ImU32 outline_color = IM_COL32(255, 255, 50, 200);

  canvas->draw_list()->AddRectFilled(rect_min, rect_max, fill_color);
  canvas->draw_list()->AddRect(rect_min, rect_max, outline_color, 0.0f, 0,
                               2.0f);

  // Draw item ID label
  std::string label = absl::StrFormat("%02X", preview_item_id_);
  canvas->draw_list()->AddText(rect_min, IM_COL32(255, 255, 255, 255),
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
    float scale = GetCanvasScale();
    pixel_x = static_cast<int>(drag_current_pos_.x / scale);
    pixel_y = static_cast<int>(drag_current_pos_.y / scale);
    // Snap to 8-pixel grid
    pixel_x = (pixel_x / dungeon_coords::kTileSize) * dungeon_coords::kTileSize;
    pixel_y = (pixel_y / dungeon_coords::kTileSize) * dungeon_coords::kTileSize;
  }

  ImDrawList* draw_list = ImGui::GetWindowDrawList();
  ImVec2 canvas_pos = GetCanvasZeroPoint();
  float scale = GetCanvasScale();

  ImVec2 pos(canvas_pos.x + pixel_x * scale, canvas_pos.y + pixel_y * scale);
  ImVec2 size(16 * scale, 16 * scale);

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
  if (!HasValidContext())
    return std::nullopt;

  auto* room = ctx_->GetCurrentRoomConst();
  if (!room)
    return std::nullopt;

  // Convert screen coordinates to room coordinates
  float scale = GetCanvasScale();
  int room_x = static_cast<int>(canvas_x / scale);
  int room_y = static_cast<int>(canvas_y / scale);

  // Check pot items
  const auto& pot_items = room->GetPotItems();
  for (size_t i = 0; i < pot_items.size(); ++i) {
    const auto& pot_item = pot_items[i];

    int item_x = pot_item.GetPixelX();
    int item_y = pot_item.GetPixelY();

    // 16x16 hitbox
    if (room_x >= item_x && room_x < item_x + 16 && room_y >= item_y &&
        room_y < item_y + 16) {
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

  ctx_->NotifyMutation();
  pot_items.erase(pot_items.begin() +
                  static_cast<ptrdiff_t>(*selected_item_index_));
  ctx_->NotifyInvalidateCache();
  ClearSelection();
}

void ItemInteractionHandler::PlaceItemAtPosition(int canvas_x, int canvas_y) {
  if (!HasValidContext())
    return;

  auto* room = GetCurrentRoom();
  if (!room)
    return;

  float scale = GetCanvasScale();

  // Convert to pixel coordinates
  int pixel_x = static_cast<int>(canvas_x / scale);
  int pixel_y = static_cast<int>(canvas_y / scale);

  // PotItem position encoding:
  // high byte * 16 = Y, low byte * 4 = X
  int encoded_x = pixel_x / 4;
  int encoded_y = pixel_y / 16;

  // Clamp to valid range
  encoded_x = std::clamp(encoded_x, 0, 255);
  encoded_y = std::clamp(encoded_y, 0, 255);

  ctx_->NotifyMutation();

  // Create the pot item
  zelda3::PotItem new_item;
  new_item.position = static_cast<uint16_t>((encoded_y << 8) | encoded_x);
  new_item.item = preview_item_id_;

  // Add item to room
  room->GetPotItems().push_back(new_item);

  ctx_->NotifyInvalidateCache();
}

}  // namespace yaze::editor
