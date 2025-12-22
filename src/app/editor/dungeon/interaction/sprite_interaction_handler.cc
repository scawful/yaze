// Related header
#include "sprite_interaction_handler.h"

// C++ standard library
#include <algorithm>

// Third-party library headers
#include "absl/strings/str_format.h"
#include "imgui/imgui.h"

// Project headers
#include "app/editor/dungeon/dungeon_coordinates.h"

namespace yaze::editor {

void SpriteInteractionHandler::BeginPlacement() {
  sprite_placement_mode_ = true;
  ClearSelection();
}

bool SpriteInteractionHandler::HandleClick(int canvas_x, int canvas_y) {
  if (!HasValidContext()) return false;

  if (sprite_placement_mode_) {
    PlaceSpriteAtPosition(canvas_x, canvas_y);
    return true;
  }

  // Try to select sprite at position
  auto sprite_index = GetEntityAtPosition(canvas_x, canvas_y);
  if (sprite_index.has_value()) {
    SelectSprite(*sprite_index);
    is_dragging_ = true;
    drag_start_pos_ = ImVec2(static_cast<float>(canvas_x),
                              static_cast<float>(canvas_y));
    drag_current_pos_ = drag_start_pos_;
    return true;
  }

  ClearSelection();
  return false;
}

void SpriteInteractionHandler::HandleDrag(ImVec2 current_pos, ImVec2 delta) {
  if (!is_dragging_ || !selected_sprite_index_.has_value()) return;
  drag_current_pos_ = current_pos;
}

void SpriteInteractionHandler::HandleRelease() {
  if (!is_dragging_ || !selected_sprite_index_.has_value()) {
    is_dragging_ = false;
    return;
  }

  auto* room = GetCurrentRoom();
  if (!room) {
    is_dragging_ = false;
    return;
  }

  // Convert to sprite coordinates (16-pixel units)
  auto [tile_x, tile_y] = CanvasToSpriteCoords(
      static_cast<int>(drag_current_pos_.x),
      static_cast<int>(drag_current_pos_.y));

  // Clamp to valid range (sprites use 0-31 range)
  tile_x = std::clamp(tile_x, 0, dungeon_coords::kSpriteGridMax);
  tile_y = std::clamp(tile_y, 0, dungeon_coords::kSpriteGridMax);

  auto& sprites = room->GetSprites();
  if (*selected_sprite_index_ < sprites.size()) {
    ctx_->NotifyMutation();

    sprites[*selected_sprite_index_].set_x(tile_x);
    sprites[*selected_sprite_index_].set_y(tile_y);

    ctx_->NotifyEntityChanged();
  }

  is_dragging_ = false;
}

void SpriteInteractionHandler::DrawGhostPreview() {
  if (!sprite_placement_mode_ || !HasValidContext()) return;

  auto* canvas = ctx_->canvas;
  if (!canvas->IsMouseHovering()) return;

  const ImGuiIO& io = ImGui::GetIO();
  ImVec2 canvas_pos = canvas->zero_point();
  float scale = GetCanvasScale();

  // Convert to room coordinates (sprites use 16-pixel grid)
  int canvas_x = static_cast<int>((io.MousePos.x - canvas_pos.x) / scale);
  int canvas_y = static_cast<int>((io.MousePos.y - canvas_pos.y) / scale);

  // Snap to 16-pixel grid
  int snapped_x = (canvas_x / dungeon_coords::kSpriteTileSize) *
                  dungeon_coords::kSpriteTileSize;
  int snapped_y = (canvas_y / dungeon_coords::kSpriteTileSize) *
                  dungeon_coords::kSpriteTileSize;

  // Draw ghost rectangle for sprite preview
  ImVec2 rect_min(canvas_pos.x + snapped_x * scale,
                  canvas_pos.y + snapped_y * scale);
  ImVec2 rect_max(rect_min.x + dungeon_coords::kSpriteTileSize * scale,
                  rect_min.y + dungeon_coords::kSpriteTileSize * scale);

  // Semi-transparent green for sprites
  ImU32 fill_color = IM_COL32(50, 200, 50, 100);
  ImU32 outline_color = IM_COL32(50, 255, 50, 200);

  canvas->draw_list()->AddRectFilled(rect_min, rect_max, fill_color);
  canvas->draw_list()->AddRect(rect_min, rect_max, outline_color, 0.0f, 0,
                                2.0f);

  // Draw sprite ID label
  std::string label = absl::StrFormat("%02X", preview_sprite_id_);
  canvas->draw_list()->AddText(rect_min, IM_COL32(255, 255, 255, 255),
                                label.c_str());
}

void SpriteInteractionHandler::DrawSelectionHighlight() {
  if (!selected_sprite_index_.has_value() || !HasValidContext()) return;

  auto* room = GetCurrentRoom();
  if (!room) return;

  const auto& sprites = room->GetSprites();
  if (*selected_sprite_index_ >= sprites.size()) return;

  const auto& sprite = sprites[*selected_sprite_index_];

  // Sprites use 16-pixel coordinate system
  int pixel_x = sprite.x() * dungeon_coords::kSpriteTileSize;
  int pixel_y = sprite.y() * dungeon_coords::kSpriteTileSize;

  // If dragging, use current drag position (snapped to 16-pixel grid)
  if (is_dragging_) {
    auto [tile_x, tile_y] = CanvasToSpriteCoords(
        static_cast<int>(drag_current_pos_.x),
        static_cast<int>(drag_current_pos_.y));
    tile_x = std::clamp(tile_x, 0, dungeon_coords::kSpriteGridMax);
    tile_y = std::clamp(tile_y, 0, dungeon_coords::kSpriteGridMax);
    pixel_x = tile_x * dungeon_coords::kSpriteTileSize;
    pixel_y = tile_y * dungeon_coords::kSpriteTileSize;
  }

  ImDrawList* draw_list = ImGui::GetWindowDrawList();
  ImVec2 canvas_pos = GetCanvasZeroPoint();
  float scale = GetCanvasScale();

  ImVec2 pos(canvas_pos.x + pixel_x * scale, canvas_pos.y + pixel_y * scale);
  ImVec2 size(dungeon_coords::kSpriteTileSize * scale,
              dungeon_coords::kSpriteTileSize * scale);

  // Animated selection
  static float pulse = 0.0f;
  pulse += ImGui::GetIO().DeltaTime * 3.0f;
  float alpha = 0.5f + 0.3f * sinf(pulse);

  ImU32 color = IM_COL32(0, 255, 0, 180);  // Green
  ImU32 fill_color =
      (color & 0x00FFFFFF) | (static_cast<ImU32>(alpha * 100) << 24);

  draw_list->AddRectFilled(pos, ImVec2(pos.x + size.x, pos.y + size.y),
                            fill_color);
  draw_list->AddRect(pos, ImVec2(pos.x + size.x, pos.y + size.y), color, 0.0f,
                     0, 2.0f);

  // Draw label
  ImVec2 text_pos(pos.x, pos.y - 14 * scale);
  draw_list->AddText(text_pos, IM_COL32(255, 255, 255, 220), "Sprite");
}

std::optional<size_t> SpriteInteractionHandler::GetEntityAtPosition(
    int canvas_x, int canvas_y) const {
  if (!HasValidContext()) return std::nullopt;

  auto* room = ctx_->GetCurrentRoomConst();
  if (!room) return std::nullopt;

  // Convert screen coordinates to room coordinates
  float scale = GetCanvasScale();
  int room_x = static_cast<int>(canvas_x / scale);
  int room_y = static_cast<int>(canvas_y / scale);

  // Check sprites (16x16 hitbox)
  const auto& sprites = room->GetSprites();
  for (size_t i = 0; i < sprites.size(); ++i) {
    const auto& sprite = sprites[i];

    // Sprites use 16-pixel coordinate system
    int sprite_x = sprite.x() * dungeon_coords::kSpriteTileSize;
    int sprite_y = sprite.y() * dungeon_coords::kSpriteTileSize;

    // 16x16 hitbox
    if (room_x >= sprite_x &&
        room_x < sprite_x + dungeon_coords::kSpriteTileSize &&
        room_y >= sprite_y &&
        room_y < sprite_y + dungeon_coords::kSpriteTileSize) {
      return i;
    }
  }

  return std::nullopt;
}

void SpriteInteractionHandler::SelectSprite(size_t index) {
  selected_sprite_index_ = index;
  ctx_->NotifyEntityChanged();
}

void SpriteInteractionHandler::ClearSelection() {
  selected_sprite_index_ = std::nullopt;
  is_dragging_ = false;
}

void SpriteInteractionHandler::DeleteSelected() {
  if (!selected_sprite_index_.has_value() || !HasValidContext()) return;

  auto* room = GetCurrentRoom();
  if (!room) return;

  auto& sprites = room->GetSprites();
  if (*selected_sprite_index_ >= sprites.size()) return;

  ctx_->NotifyMutation();
  sprites.erase(sprites.begin() +
                static_cast<ptrdiff_t>(*selected_sprite_index_));
  ctx_->NotifyInvalidateCache();
  ClearSelection();
}

void SpriteInteractionHandler::PlaceSpriteAtPosition(int canvas_x,
                                                      int canvas_y) {
  if (!HasValidContext()) return;

  auto* room = GetCurrentRoom();
  if (!room) return;

  auto [sprite_x, sprite_y] = CanvasToSpriteCoords(canvas_x, canvas_y);

  // Clamp to valid range
  sprite_x = std::clamp(sprite_x, 0, dungeon_coords::kSpriteGridMax);
  sprite_y = std::clamp(sprite_y, 0, dungeon_coords::kSpriteGridMax);

  ctx_->NotifyMutation();

  // Create the sprite
  zelda3::Sprite new_sprite(preview_sprite_id_, static_cast<uint8_t>(sprite_x),
                            static_cast<uint8_t>(sprite_y), 0, 0);

  // Add sprite to room
  room->GetSprites().push_back(new_sprite);

  ctx_->NotifyInvalidateCache();
}

std::pair<int, int> SpriteInteractionHandler::CanvasToSpriteCoords(
    int canvas_x, int canvas_y) const {
  float scale = GetCanvasScale();
  // Convert to pixel coordinates, then to sprite tile coordinates
  int pixel_x = static_cast<int>(canvas_x / scale);
  int pixel_y = static_cast<int>(canvas_y / scale);
  return {pixel_x / dungeon_coords::kSpriteTileSize,
          pixel_y / dungeon_coords::kSpriteTileSize};
}

}  // namespace yaze::editor
