#include "app/editor/dungeon/interaction/interaction_coordinator.h"
#include "app/editor/dungeon/object_selection.h"
#include "util/i18n/tr.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <functional>
#include <optional>
#include <tuple>

// Third-party library headers
#include "absl/strings/str_format.h"
#include "imgui/imgui.h"

#include "app/editor/dungeon/dungeon_canvas_transform.h"
#include "app/editor/dungeon/dungeon_coordinates.h"
#include "app/editor/dungeon/dungeon_snapping.h"
#include "app/gui/core/agent_theme.h"
#include "zelda3/dungeon/room_object.h"
#include "zelda3/sprite/sprite.h"

namespace yaze::editor {

namespace {

constexpr double kCycleHudHoldSeconds = 1.25;
constexpr size_t kCycleHudMaxLabelChars = 54;

bool Intersects(int ax, int ay, int aw, int ah, int bx, int by, int bw,
                int bh) {
  return ax < bx + bw && ax + aw > bx && ay < by + bh && ay + ah > by;
}

std::optional<std::tuple<int, int, int, int>> GetEntityBounds(
    const zelda3::Room& room, SelectedEntity entity) {
  switch (entity.type) {
    case EntityType::Door: {
      const auto& doors = room.GetDoors();
      if (entity.index >= doors.size()) {
        return std::nullopt;
      }
      return doors[entity.index].GetEditorBounds();
    }
    case EntityType::Sprite: {
      const auto& sprites = room.GetSprites();
      if (entity.index >= sprites.size()) {
        return std::nullopt;
      }
      const auto& sprite = sprites[entity.index];
      constexpr int kSize = dungeon_coords::kSpriteTileSize;
      return std::make_tuple(sprite.x() * kSize, sprite.y() * kSize, kSize,
                             kSize);
    }
    case EntityType::Item: {
      const auto& items = room.GetPotItems();
      if (entity.index >= items.size()) {
        return std::nullopt;
      }
      const auto& item = items[entity.index];
      return std::make_tuple(item.GetPixelX(), item.GetPixelY(), 16, 16);
    }
    case EntityType::Object:
    case EntityType::None:
    default:
      return std::nullopt;
  }
}

ImVec4 EntitySelectionColor(const AgentUITheme& theme, EntityType type) {
  switch (type) {
    case EntityType::Door:
      return theme.status_warning;
    case EntityType::Sprite:
      return theme.status_success;
    case EntityType::Item:
      return theme.dungeon_selection_primary;
    case EntityType::Object:
    case EntityType::None:
    default:
      return theme.accent_color;
  }
}

bool IsCycleModifierHeld(const ImGuiIO& io) {
  return io.KeyAlt && (io.KeyCtrl || io.KeySuper);
}

std::string TruncateCycleHudLabel(std::string label) {
  if (label.size() <= kCycleHudMaxLabelChars) {
    return label;
  }
  label.resize(kCycleHudMaxLabelChars - 3);
  label += "...";
  return label;
}

}  // namespace

void InteractionCoordinator::SetContext(InteractionContext* ctx) {
  ctx_ = ctx;
  door_handler_.SetContext(ctx);
  sprite_handler_.SetContext(ctx);
  item_handler_.SetContext(ctx);
  tile_handler_.SetContext(ctx);
}

void InteractionCoordinator::SetMode(Mode mode) {
  // Cancel current mode first
  CancelCurrentMode();

  current_mode_ = mode;

  // Activate the new mode
  switch (mode) {
    case Mode::PlaceDoor:
      door_handler_.BeginPlacement();
      break;
    case Mode::PlaceSprite:
      sprite_handler_.BeginPlacement();
      break;
    case Mode::PlaceItem:
      item_handler_.BeginPlacement();
      break;
    case Mode::Select:
      // Nothing to activate
      break;
  }
}

void InteractionCoordinator::CancelCurrentMode() {
  // Cancel any active placement
  door_handler_.CancelPlacement();
  sprite_handler_.CancelPlacement();
  item_handler_.CancelPlacement();
  tile_handler_.CancelPlacement();

  current_mode_ = Mode::Select;
}

bool InteractionCoordinator::IsPlacementActive() const {
  return door_handler_.IsPlacementActive() ||
         sprite_handler_.IsPlacementActive() ||
         item_handler_.IsPlacementActive() || tile_handler_.IsPlacementActive();
}

bool InteractionCoordinator::HandleClick(int canvas_x, int canvas_y) {
  // Check placement modes first
  if (door_handler_.IsPlacementActive()) {
    return door_handler_.HandleClick(canvas_x, canvas_y);
  }
  if (sprite_handler_.IsPlacementActive()) {
    return sprite_handler_.HandleClick(canvas_x, canvas_y);
  }
  if (item_handler_.IsPlacementActive()) {
    return item_handler_.HandleClick(canvas_x, canvas_y);
  }
  if (tile_handler_.IsPlacementActive()) {
    return tile_handler_.HandleClick(canvas_x, canvas_y);
  }

  if (door_handler_.HandleOverlayClick(canvas_x, canvas_y)) {
    return true;
  }

  if (!dungeon_coords::IsWithinBounds(canvas_x, canvas_y)) {
    return false;
  }

  // In select mode, only handle the click if the cursor is over an entity or object.
  const auto hits = GetEntitiesAtPosition(canvas_x, canvas_y);
  if (hits.empty()) {
    return false;
  }

  const ImGuiIO& io = ImGui::GetIO();
  const bool cycle_modifier = IsCycleModifierHeld(io);
  if (io.KeyAlt && !cycle_modifier) {
    const bool had_entity_selection = HasEntitySelection();
    const bool had_object_selection =
        ctx_ && ctx_->selection && ctx_->selection->HasSelection();
    ClearAllEntitySelections();
    if (ctx_ && ctx_->selection) {
      ctx_->selection->ClearSelection();
    }
    cycle_last_hits_.clear();
    cycle_next_index_ = 0;
    cycle_hud_start_time_ = -1.0;
    if ((had_entity_selection || had_object_selection) && ctx_) {
      ctx_->NotifyEntityChanged();
    }
    return true;
  }
  if (cycle_modifier) {
    if (!SameCycleTarget(canvas_x, canvas_y, hits)) {
      cycle_next_index_ = 0;
    }
    const size_t selected_index = cycle_next_index_ % hits.size();
    const SelectedEntity selected = hits[selected_index];
    cycle_next_index_ = (cycle_next_index_ + 1) % hits.size();
    cycle_last_x_ = canvas_x;
    cycle_last_y_ = canvas_y;
    cycle_active_index_ = selected_index;
    cycle_hud_screen_pos_ = io.MousePos;
    cycle_hud_start_time_ = ImGui::GetCurrentContext() ? ImGui::GetTime() : 0.0;
    cycle_last_hits_ = hits;
    return ApplySelection(selected);
  }

  const auto entity = hits.front();
  const bool additive = io.KeyShift || io.KeyCtrl || io.KeySuper;
  const bool hit_already_selected = IsSelectionHitSelected(entity);

  // Cross-selection rules:
  // 1. Plain clicks select one stack participant and clear the other family.
  // 2. Shift/Ctrl/Cmd allow mixed object/entity selection.
  // 3. Alt-click clears, matching ZScream muscle memory. Ctrl/Cmd+Alt keeps
  //    Yaze's overlap cycle affordance available without stealing Alt.

  if (entity.type == EntityType::Object) {
    const bool has_multi_object_selection =
        ctx_ && ctx_->selection && ctx_->selection->GetSelectionCount() > 1;
    if (!additive && hit_already_selected &&
        (has_multi_object_selection || HasGroupDragSelection())) {
      return true;
    }
    if (!additive) {
      ClearAllEntitySelections();
      if (ctx_ && ctx_->selection) {
        ctx_->selection->ClearSelection();
      }
    }
    return tile_handler_.HandleClick(canvas_x, canvas_y);
  }

  const bool toggle = io.KeyCtrl || io.KeySuper;
  if (additive) {
    return UpdateEntitySelection(entity, io.KeyShift, toggle);
  }

  if (hit_already_selected && HasGroupDragSelection()) {
    return true;
  }

  // Plain entity clicks preserve the existing handler drag affordance.
  selected_entities_.clear();
  selected_entities_.push_back(entity);
  door_handler_.ClearSelection();
  sprite_handler_.ClearSelection();
  item_handler_.ClearSelection();
  if (ctx_ && ctx_->selection) {
    ctx_->selection->ClearSelection();
  }
  switch (entity.type) {
    case EntityType::Door:
      return door_handler_.HandleClick(canvas_x, canvas_y);
    case EntityType::Sprite:
      return sprite_handler_.HandleClick(canvas_x, canvas_y);
    case EntityType::Item:
      return item_handler_.HandleClick(canvas_x, canvas_y);
    default:
      selected_entities_.clear();
      return false;
  }
}

void InteractionCoordinator::SelectEntity(EntityType type, size_t index) {
  if (type == EntityType::Object || type == EntityType::None) {
    ClearAllEntitySelections();
    if (ctx_ && ctx_->selection) {
      ctx_->selection->ClearSelection();
    }
    return;
  }

  ClearAllEntitySelections();

  selected_entities_.push_back(SelectedEntity{type, index});

  switch (type) {
    case EntityType::Door:
      door_handler_.SelectDoor(index);
      break;
    case EntityType::Sprite:
      sprite_handler_.SelectSprite(index);
      break;
    case EntityType::Item:
      item_handler_.SelectItem(index);
      break;
    case EntityType::Object:
    case EntityType::None:
    default:
      break;
  }
}

void InteractionCoordinator::SetSelectedEntities(
    std::vector<SelectedEntity> entities) {
  door_handler_.ClearSelection();
  sprite_handler_.ClearSelection();
  item_handler_.ClearSelection();
  selected_entities_.clear();

  for (const auto entity : entities) {
    if (entity.type != EntityType::Door && entity.type != EntityType::Sprite &&
        entity.type != EntityType::Item) {
      continue;
    }
    if (std::find(selected_entities_.begin(), selected_entities_.end(),
                  entity) == selected_entities_.end()) {
      selected_entities_.push_back(entity);
    }
  }

  if (selected_entities_.size() == 1) {
    const SelectedEntity selected = selected_entities_.front();
    switch (selected.type) {
      case EntityType::Door:
        door_handler_.SelectDoor(selected.index);
        break;
      case EntityType::Sprite:
        sprite_handler_.SelectSprite(selected.index);
        break;
      case EntityType::Item:
        item_handler_.SelectItem(selected.index);
        break;
      case EntityType::Object:
      case EntityType::None:
      default:
        break;
    }
    return;
  }

  if (ctx_) {
    ctx_->NotifyEntityChanged();
  }
}

bool InteractionCoordinator::HandleMouseWheel(float delta) {
  if (door_handler_.IsPlacementActive())
    return door_handler_.HandleMouseWheel(delta);
  if (sprite_handler_.IsPlacementActive())
    return sprite_handler_.HandleMouseWheel(delta);
  if (item_handler_.IsPlacementActive())
    return item_handler_.HandleMouseWheel(delta);
  if (tile_handler_.IsPlacementActive())
    return tile_handler_.HandleMouseWheel(delta);

  return tile_handler_.HandleMouseWheel(delta);
}

void InteractionCoordinator::ClearEntitySelection() {
  const bool had_selection = HasEntitySelection();
  ClearAllEntitySelections();
  if (had_selection && ctx_) {
    ctx_->NotifyEntityChanged();
  }
}

void InteractionCoordinator::CancelPlacement() {
  door_handler_.CancelPlacement();
  sprite_handler_.CancelPlacement();
  item_handler_.CancelPlacement();
  tile_handler_.CancelPlacement();
}

std::optional<SelectedEntity> InteractionCoordinator::GetEntityAtPosition(
    int canvas_x, int canvas_y) const {
  const auto hits = GetEntitiesAtPosition(canvas_x, canvas_y);
  if (hits.empty()) {
    return std::nullopt;
  }
  return hits.front();
}

std::vector<SelectedEntity> InteractionCoordinator::GetEntitiesAtPosition(
    int canvas_x, int canvas_y) const {
  std::vector<SelectedEntity> hits;
  if (!dungeon_coords::IsWithinBounds(canvas_x, canvas_y)) {
    return hits;
  }
  if (auto door = door_handler_.GetEntityAtPosition(canvas_x, canvas_y)) {
    hits.push_back(SelectedEntity{EntityType::Door, *door});
  }
  if (auto sprite = sprite_handler_.GetEntityAtPosition(canvas_x, canvas_y)) {
    hits.push_back(SelectedEntity{EntityType::Sprite, *sprite});
  }
  if (auto item = item_handler_.GetEntityAtPosition(canvas_x, canvas_y)) {
    hits.push_back(SelectedEntity{EntityType::Item, *item});
  }
  if (auto object = tile_handler_.GetEntityAtPosition(canvas_x, canvas_y)) {
    hits.push_back(SelectedEntity{EntityType::Object, *object});
  }
  return hits;
}

SelectedEntity InteractionCoordinator::GetSelectedEntity() const {
  if (!selected_entities_.empty()) {
    return selected_entities_.front();
  }
  if (auto idx = door_handler_.GetSelectedIndex()) {
    return SelectedEntity{EntityType::Door, *idx};
  }
  if (auto idx = sprite_handler_.GetSelectedIndex()) {
    return SelectedEntity{EntityType::Sprite, *idx};
  }
  if (auto idx = item_handler_.GetSelectedIndex()) {
    return SelectedEntity{EntityType::Item, *idx};
  }
  return SelectedEntity{EntityType::None, 0};
}

void InteractionCoordinator::HandleDrag(ImVec2 current_pos, ImVec2 delta) {
  // Forward drag to handlers that have active selections
  if (entity_group_drag_active_) {
    HandleEntityGroupDrag(current_pos);
  } else if (door_handler_.HasSelection()) {
    door_handler_.HandleDrag(current_pos, delta);
  }
  if (!entity_group_drag_active_ && sprite_handler_.HasSelection()) {
    sprite_handler_.HandleDrag(current_pos, delta);
  }
  if (!entity_group_drag_active_ && item_handler_.HasSelection()) {
    item_handler_.HandleDrag(current_pos, delta);
  }

  // Tile objects (managed by ObjectSelection)
  if (tile_handler_.IsPlacementActive() ||
      (ctx_ && ctx_->selection && ctx_->selection->HasSelection())) {
    tile_handler_.HandleDrag(current_pos, delta);
  }
}

void InteractionCoordinator::HandleRelease() {
  door_handler_.HandleRelease();
  sprite_handler_.HandleRelease();
  item_handler_.HandleRelease();
  tile_handler_.HandleRelease();
  FinishEntityGroupDrag();
}

void InteractionCoordinator::ResetEntityGroupDragState() {
  entity_group_drag_active_ = false;
  entity_group_drag_last_dx_ = 0;
  entity_group_drag_last_dy_ = 0;
  entity_group_drag_doors_mutation_started_ = false;
  entity_group_drag_sprites_mutation_started_ = false;
  entity_group_drag_items_mutation_started_ = false;
  entity_group_drag_doors_changed_ = false;
  entity_group_drag_sprites_changed_ = false;
  entity_group_drag_items_changed_ = false;
}

void InteractionCoordinator::FinishEntityGroupDrag() {
  if (!entity_group_drag_active_) {
    ResetEntityGroupDragState();
    return;
  }

  if (ctx_) {
    if (entity_group_drag_doors_changed_) {
      ctx_->NotifyInvalidateCache(MutationDomain::kDoors);
    }
    if (entity_group_drag_sprites_changed_) {
      ctx_->NotifyInvalidateCache(MutationDomain::kSprites);
    }
    if (entity_group_drag_items_changed_) {
      ctx_->NotifyInvalidateCache(MutationDomain::kItems);
    }
    if (entity_group_drag_doors_changed_ ||
        entity_group_drag_sprites_changed_ ||
        entity_group_drag_items_changed_) {
      ctx_->NotifyEntityChanged();
    }
  }
  ResetEntityGroupDragState();
}

void InteractionCoordinator::DrawGhostPreviews() {
  // Draw ghost preview for active placement mode
  if (door_handler_.IsPlacementActive()) {
    door_handler_.DrawGhostPreview();
  }
  if (sprite_handler_.IsPlacementActive()) {
    sprite_handler_.DrawGhostPreview();
  }
  if (item_handler_.IsPlacementActive()) {
    item_handler_.DrawGhostPreview();
  }
  if (tile_handler_.IsPlacementActive()) {
    tile_handler_.DrawGhostPreview();
  }
}

void InteractionCoordinator::DrawSelectionHighlights() {
  if (selected_entities_.size() > 1) {
    DrawMultiEntitySelectionHighlights();
  } else {
    // Preserve the richer single-selection overlays (door pair badge, drag
    // preview) for the common one-entity inspector workflow.
    door_handler_.DrawSelectionHighlight();
    sprite_handler_.DrawSelectionHighlight();
    item_handler_.DrawSelectionHighlight();
  }

  // Draw snap indicators for door placement
  if (door_handler_.IsPlacementActive() || door_handler_.HasSelection()) {
    door_handler_.DrawSnapIndicators();
  }
  DrawSelectionCycleHud();
}

void InteractionCoordinator::DrawPostPlacementOverlays() {
  // Render placement success toasts for all handlers unconditionally so they
  // remain visible even after the user exits placement mode immediately.
  door_handler_.DrawPostPlacementToast();
  sprite_handler_.DrawPostPlacementToast();
  item_handler_.DrawPostPlacementToast();
  tile_handler_.DrawPostPlacementToast();
}

bool InteractionCoordinator::TrySelectEntityAtCursor(int canvas_x,
                                                     int canvas_y) {
  // Clear all selections first
  ClearAllEntitySelections();

  // Try to select in priority order: doors, sprites, items
  // (matches original DungeonObjectInteraction behavior)
  if (door_handler_.HandleClick(canvas_x, canvas_y)) {
    if (auto index = door_handler_.GetSelectedIndex()) {
      selected_entities_ = {SelectedEntity{EntityType::Door, *index}};
    }
    return true;
  }
  if (sprite_handler_.HandleClick(canvas_x, canvas_y)) {
    if (auto index = sprite_handler_.GetSelectedIndex()) {
      selected_entities_ = {SelectedEntity{EntityType::Sprite, *index}};
    }
    return true;
  }
  if (item_handler_.HandleClick(canvas_x, canvas_y)) {
    if (auto index = item_handler_.GetSelectedIndex()) {
      selected_entities_ = {SelectedEntity{EntityType::Item, *index}};
    }
    return true;
  }

  return false;
}

bool InteractionCoordinator::HasEntitySelection() const {
  return !selected_entities_.empty() || door_handler_.HasSelection() ||
         sprite_handler_.HasSelection() || item_handler_.HasSelection();
}

bool InteractionCoordinator::NudgeSelectedEntities(
    int delta_x, int delta_y, bool defer_drag_notifications) {
  auto* room = ctx_ ? ctx_->GetCurrentRoom() : nullptr;
  if (!room) {
    return false;
  }

  bool doors_changed = false;
  bool sprites_changed = false;
  bool items_changed = false;
  bool door_mutation_notified = false;
  bool sprite_mutation_notified = false;
  bool item_mutation_notified = false;

  auto notify_once = [&](MutationDomain domain, bool& immediate_flag,
                         bool& drag_flag) {
    if (!ctx_) {
      return;
    }
    if (defer_drag_notifications) {
      if (!drag_flag) {
        ctx_->NotifyMutation(domain);
        drag_flag = true;
      }
      return;
    }
    if (!immediate_flag) {
      ctx_->NotifyMutation(domain);
      immediate_flag = true;
    }
  };

  for (const auto entity : selected_entities_) {
    switch (entity.type) {
      case EntityType::Door: {
        auto& doors = room->GetDoors();
        if (entity.index >= doors.size()) {
          break;
        }
        auto& door = doors[entity.index];
        int position_delta = 0;
        switch (door.direction) {
          case zelda3::DoorDirection::North:
          case zelda3::DoorDirection::South:
            position_delta = delta_x;
            break;
          case zelda3::DoorDirection::West:
          case zelda3::DoorDirection::East:
            position_delta = delta_y;
            break;
        }
        if (position_delta == 0) {
          break;
        }
        const int next_position =
            std::clamp(static_cast<int>(door.position) + position_delta, 0,
                       zelda3::DoorPositionManager::kMaxDoorPositions - 1);
        if (next_position == door.position ||
            !zelda3::DoorPositionManager::IsValidPosition(
                static_cast<uint8_t>(next_position), door.direction)) {
          break;
        }
        notify_once(MutationDomain::kDoors, door_mutation_notified,
                    entity_group_drag_doors_mutation_started_);
        door.position = static_cast<uint8_t>(next_position);
        auto [b1, b2] = door.EncodeBytes();
        door.byte1 = b1;
        door.byte2 = b2;
        doors_changed = true;
        break;
      }
      case EntityType::Sprite: {
        auto& sprites = room->GetSprites();
        if (entity.index >= sprites.size()) {
          break;
        }
        auto& sprite = sprites[entity.index];
        const int next_x = std::clamp(static_cast<int>(sprite.x()) + delta_x, 0,
                                      dungeon_coords::kSpriteGridMax);
        const int next_y = std::clamp(static_cast<int>(sprite.y()) + delta_y, 0,
                                      dungeon_coords::kSpriteGridMax);
        if (next_x == sprite.x() && next_y == sprite.y()) {
          break;
        }
        notify_once(MutationDomain::kSprites, sprite_mutation_notified,
                    entity_group_drag_sprites_mutation_started_);
        sprite.set_x(next_x);
        sprite.set_y(next_y);
        sprites_changed = true;
        break;
      }
      case EntityType::Item: {
        auto& items = room->GetPotItems();
        if (entity.index >= items.size()) {
          break;
        }
        auto& item = items[entity.index];
        constexpr int kRoomPixelMax = 511;
        constexpr int kItemHorizontalNudgePixels = 8;
        constexpr int kItemVerticalNudgePixels = 16;
        const int next_pixel_x =
            std::clamp(item.GetPixelX() + delta_x * kItemHorizontalNudgePixels,
                       0, kRoomPixelMax);
        const int next_pixel_y =
            std::clamp(item.GetPixelY() + delta_y * kItemVerticalNudgePixels, 0,
                       kRoomPixelMax);
        const int encoded_x = std::clamp(next_pixel_x / 4, 0, 255);
        const int encoded_y = std::clamp(next_pixel_y / 16, 0, 255);
        const uint16_t next_position =
            static_cast<uint16_t>((encoded_y << 8) | encoded_x);
        if (next_position == item.position) {
          break;
        }
        notify_once(MutationDomain::kItems, item_mutation_notified,
                    entity_group_drag_items_mutation_started_);
        item.position = next_position;
        items_changed = true;
        break;
      }
      case EntityType::Object:
      case EntityType::None:
      default:
        break;
    }
  }

  if (!doors_changed && !sprites_changed && !items_changed) {
    return false;
  }

  if (doors_changed) {
    room->MarkObjectsDirty();
    if (defer_drag_notifications) {
      entity_group_drag_doors_changed_ = true;
    } else {
      ctx_->NotifyInvalidateCache(MutationDomain::kDoors);
    }
  }
  if (sprites_changed) {
    room->MarkSpritesDirty();
    if (defer_drag_notifications) {
      entity_group_drag_sprites_changed_ = true;
    } else {
      ctx_->NotifyInvalidateCache(MutationDomain::kSprites);
    }
  }
  if (items_changed) {
    room->MarkPotItemsDirty();
    if (defer_drag_notifications) {
      entity_group_drag_items_changed_ = true;
    } else {
      ctx_->NotifyInvalidateCache(MutationDomain::kItems);
    }
  }
  if (!defer_drag_notifications) {
    ctx_->NotifyEntityChanged();
  }
  return true;
}

bool InteractionCoordinator::NudgeSelected(int delta_x, int delta_y) {
  if (!selected_entities_.empty()) {
    return NudgeSelectedEntities(delta_x, delta_y,
                                 /*defer_drag_notifications=*/false);
  }

  if (door_handler_.HasSelection()) {
    return door_handler_.NudgeSelected(delta_x, delta_y);
  }
  if (sprite_handler_.HasSelection()) {
    return sprite_handler_.NudgeSelected(delta_x, delta_y);
  }
  if (item_handler_.HasSelection()) {
    constexpr int kItemHorizontalNudgePixels = 8;
    constexpr int kItemVerticalNudgePixels = 16;
    return item_handler_.NudgeSelected(delta_x * kItemHorizontalNudgePixels,
                                       delta_y * kItemVerticalNudgePixels);
  }
  return false;
}

void InteractionCoordinator::ClearAllEntitySelections() {
  if (entity_group_drag_active_) {
    FinishEntityGroupDrag();
  }
  selected_entities_.clear();
  door_handler_.ClearSelection();
  sprite_handler_.ClearSelection();
  item_handler_.ClearSelection();
  if (!entity_group_drag_active_) {
    ResetEntityGroupDragState();
  }
}

void InteractionCoordinator::DeleteSelectedEntity() {
  if (!selected_entities_.empty()) {
    auto* room = ctx_ ? ctx_->GetCurrentRoom() : nullptr;
    if (!room) {
      return;
    }

    std::vector<size_t> doors;
    std::vector<size_t> sprites;
    std::vector<size_t> items;
    for (const auto entity : selected_entities_) {
      switch (entity.type) {
        case EntityType::Door:
          if (entity.index < room->GetDoors().size()) {
            doors.push_back(entity.index);
          }
          break;
        case EntityType::Sprite:
          if (entity.index < room->GetSprites().size()) {
            sprites.push_back(entity.index);
          }
          break;
        case EntityType::Item:
          if (entity.index < room->GetPotItems().size()) {
            items.push_back(entity.index);
          }
          break;
        case EntityType::Object:
        case EntityType::None:
        default:
          break;
      }
    }

    auto sort_unique_desc = [](std::vector<size_t>& indices) {
      std::sort(indices.begin(), indices.end(), std::greater<size_t>());
      indices.erase(std::unique(indices.begin(), indices.end()), indices.end());
    };
    sort_unique_desc(doors);
    sort_unique_desc(sprites);
    sort_unique_desc(items);

    if (!doors.empty()) {
      ctx_->NotifyMutation(MutationDomain::kDoors);
      auto& room_doors = room->GetDoors();
      for (size_t index : doors) {
        room_doors.erase(room_doors.begin() + static_cast<ptrdiff_t>(index));
      }
      room->MarkObjectStreamDirty();
      ctx_->NotifyInvalidateCache(MutationDomain::kDoors);
    }
    if (!sprites.empty()) {
      ctx_->NotifyMutation(MutationDomain::kSprites);
      auto& room_sprites = room->GetSprites();
      for (size_t index : sprites) {
        room_sprites.erase(room_sprites.begin() +
                           static_cast<ptrdiff_t>(index));
      }
      room->MarkSpritesDirty();
      ctx_->NotifyInvalidateCache(MutationDomain::kSprites);
    }
    if (!items.empty()) {
      ctx_->NotifyMutation(MutationDomain::kItems);
      auto& room_items = room->GetPotItems();
      for (size_t index : items) {
        room_items.erase(room_items.begin() + static_cast<ptrdiff_t>(index));
      }
      room->MarkPotItemsDirty();
      ctx_->NotifyInvalidateCache(MutationDomain::kItems);
    }

    if (!doors.empty() || !sprites.empty() || !items.empty()) {
      ClearAllEntitySelections();
      ctx_->NotifyEntityChanged();
    }
    return;
  }

  if (door_handler_.HasSelection()) {
    door_handler_.DeleteSelected();
  } else if (sprite_handler_.HasSelection()) {
    sprite_handler_.DeleteSelected();
  } else if (item_handler_.HasSelection()) {
    item_handler_.DeleteSelected();
  }
}

InteractionCoordinator::Mode InteractionCoordinator::GetSelectedEntityType()
    const {
  if (!selected_entities_.empty()) {
    switch (selected_entities_.front().type) {
      case EntityType::Door:
        return Mode::PlaceDoor;
      case EntityType::Sprite:
        return Mode::PlaceSprite;
      case EntityType::Item:
        return Mode::PlaceItem;
      case EntityType::Object:
      case EntityType::None:
      default:
        break;
    }
  }
  if (door_handler_.HasSelection()) {
    return Mode::PlaceDoor;
  }
  if (sprite_handler_.HasSelection()) {
    return Mode::PlaceSprite;
  }
  if (item_handler_.HasSelection()) {
    return Mode::PlaceItem;
  }
  return Mode::Select;
}

BaseEntityHandler* InteractionCoordinator::GetActiveHandler() {
  switch (current_mode_) {
    case Mode::PlaceDoor:
      return &door_handler_;
    case Mode::PlaceSprite:
      return &sprite_handler_;
    case Mode::PlaceItem:
      return &item_handler_;
    case Mode::Select:
    default:
      return nullptr;
  }
}

bool InteractionCoordinator::ApplySelection(SelectedEntity entity) {
  ClearAllEntitySelections();

  if (entity.type == EntityType::Object) {
    if (!ctx_ || !ctx_->selection) {
      return false;
    }
    ctx_->selection->ClearSelection();
    ctx_->selection->SelectObject(entity.index,
                                  ObjectSelection::SelectionMode::Single);
    return true;
  }

  if (ctx_ && ctx_->selection) {
    ctx_->selection->ClearSelection();
  }

  switch (entity.type) {
    case EntityType::Door:
      selected_entities_.push_back(entity);
      door_handler_.SelectDoor(entity.index);
      return true;
    case EntityType::Sprite:
      selected_entities_.push_back(entity);
      sprite_handler_.SelectSprite(entity.index);
      return true;
    case EntityType::Item:
      selected_entities_.push_back(entity);
      item_handler_.SelectItem(entity.index);
      return true;
    case EntityType::Object:
    case EntityType::None:
    default:
      return false;
  }
}

bool InteractionCoordinator::UpdateEntitySelection(SelectedEntity entity,
                                                   bool additive, bool toggle) {
  if (entity.type == EntityType::Object || entity.type == EntityType::None) {
    return false;
  }

  door_handler_.ClearSelection();
  sprite_handler_.ClearSelection();
  item_handler_.ClearSelection();

  if (!additive && !toggle) {
    selected_entities_.clear();
  }

  const auto existing =
      std::find(selected_entities_.begin(), selected_entities_.end(), entity);
  if (toggle) {
    if (existing != selected_entities_.end()) {
      selected_entities_.erase(existing);
    } else {
      selected_entities_.push_back(entity);
    }
  } else if (existing == selected_entities_.end()) {
    selected_entities_.push_back(entity);
  }

  if (selected_entities_.size() == 1) {
    const auto selected = selected_entities_.front();
    switch (selected.type) {
      case EntityType::Door:
        door_handler_.SelectDoor(selected.index);
        break;
      case EntityType::Sprite:
        sprite_handler_.SelectSprite(selected.index);
        break;
      case EntityType::Item:
        item_handler_.SelectItem(selected.index);
        break;
      case EntityType::Object:
      case EntityType::None:
      default:
        break;
    }
  } else if (ctx_) {
    ctx_->NotifyEntityChanged();
  }

  return true;
}

bool InteractionCoordinator::IsSelectionHitSelected(
    SelectedEntity entity) const {
  if (entity.type == EntityType::Object) {
    return ctx_ && ctx_->selection &&
           ctx_->selection->IsObjectSelected(entity.index);
  }
  return std::find(selected_entities_.begin(), selected_entities_.end(),
                   entity) != selected_entities_.end() ||
         GetSelectedEntity() == entity;
}

bool InteractionCoordinator::HasGroupDragSelection() const {
  const bool has_object_selection =
      ctx_ && ctx_->selection && ctx_->selection->HasSelection();
  return !selected_entities_.empty() &&
         (selected_entities_.size() > 1 || has_object_selection);
}

void InteractionCoordinator::BeginSelectionDrag(ImVec2 start_pos) {
  if (!HasGroupDragSelection()) {
    ResetEntityGroupDragState();
    return;
  }

  entity_group_drag_active_ = true;
  entity_group_drag_start_ = snapping::SnapToTileGrid(start_pos);
  entity_group_drag_current_ = entity_group_drag_start_;
  entity_group_drag_last_dx_ = 0;
  entity_group_drag_last_dy_ = 0;
  entity_group_drag_doors_mutation_started_ = false;
  entity_group_drag_sprites_mutation_started_ = false;
  entity_group_drag_items_mutation_started_ = false;
  entity_group_drag_doors_changed_ = false;
  entity_group_drag_sprites_changed_ = false;
  entity_group_drag_items_changed_ = false;
}

void InteractionCoordinator::HandleEntityGroupDrag(ImVec2 current_pos) {
  if (!entity_group_drag_active_) {
    return;
  }

  entity_group_drag_current_ = snapping::SnapToTileGrid(current_pos);
  const ImVec2 drag_delta(
      entity_group_drag_current_.x - entity_group_drag_start_.x,
      entity_group_drag_current_.y - entity_group_drag_start_.y);

  constexpr int kEntityDragStepPixels = dungeon_coords::kSpriteTileSize;
  const int drag_dx = static_cast<int>(drag_delta.x) / kEntityDragStepPixels;
  const int drag_dy = static_cast<int>(drag_delta.y) / kEntityDragStepPixels;
  const int inc_dx = drag_dx - entity_group_drag_last_dx_;
  const int inc_dy = drag_dy - entity_group_drag_last_dy_;
  if (inc_dx == 0 && inc_dy == 0) {
    return;
  }

  if (NudgeSelectedEntities(inc_dx, inc_dy,
                            /*defer_drag_notifications=*/true)) {
    entity_group_drag_last_dx_ = drag_dx;
    entity_group_drag_last_dy_ = drag_dy;
  }
}

void InteractionCoordinator::SelectEntitiesInRect(
    const std::tuple<int, int, int, int>& bounds, bool additive, bool toggle) {
  auto* room = ctx_ ? ctx_->GetCurrentRoomConst() : nullptr;
  if (!room) {
    return;
  }

  const auto [raw_min_x, raw_min_y, raw_max_x, raw_max_y] = bounds;
  const int min_x = std::min(raw_min_x, raw_max_x);
  const int max_x = std::max(raw_min_x, raw_max_x);
  const int min_y = std::min(raw_min_y, raw_max_y);
  const int max_y = std::max(raw_min_y, raw_max_y);
  const int rect_w = std::max(1, max_x - min_x);
  const int rect_h = std::max(1, max_y - min_y);

  std::vector<SelectedEntity> hits;
  for (size_t i = 0; i < room->GetDoors().size(); ++i) {
    const SelectedEntity entity{EntityType::Door, i};
    if (auto entity_bounds = GetEntityBounds(*room, entity)) {
      auto [x, y, w, h] = *entity_bounds;
      if (Intersects(x, y, w, h, min_x, min_y, rect_w, rect_h)) {
        hits.push_back(entity);
      }
    }
  }
  for (size_t i = 0; i < room->GetSprites().size(); ++i) {
    const SelectedEntity entity{EntityType::Sprite, i};
    if (auto entity_bounds = GetEntityBounds(*room, entity)) {
      auto [x, y, w, h] = *entity_bounds;
      if (Intersects(x, y, w, h, min_x, min_y, rect_w, rect_h)) {
        hits.push_back(entity);
      }
    }
  }
  for (size_t i = 0; i < room->GetPotItems().size(); ++i) {
    const SelectedEntity entity{EntityType::Item, i};
    if (auto entity_bounds = GetEntityBounds(*room, entity)) {
      auto [x, y, w, h] = *entity_bounds;
      if (Intersects(x, y, w, h, min_x, min_y, rect_w, rect_h)) {
        hits.push_back(entity);
      }
    }
  }

  door_handler_.ClearSelection();
  sprite_handler_.ClearSelection();
  item_handler_.ClearSelection();
  if (!additive && !toggle) {
    selected_entities_.clear();
  }

  for (const auto entity : hits) {
    auto existing =
        std::find(selected_entities_.begin(), selected_entities_.end(), entity);
    if (toggle) {
      if (existing != selected_entities_.end()) {
        selected_entities_.erase(existing);
      } else {
        selected_entities_.push_back(entity);
      }
    } else if (existing == selected_entities_.end()) {
      selected_entities_.push_back(entity);
    }
  }

  if (selected_entities_.size() == 1) {
    UpdateEntitySelection(selected_entities_.front(), /*additive=*/false,
                          /*toggle=*/false);
  } else if (ctx_) {
    ctx_->NotifyEntityChanged();
  }
}

bool InteractionCoordinator::SameCycleTarget(
    int canvas_x, int canvas_y, const std::vector<SelectedEntity>& hits) const {
  constexpr int kSameSpotTolerancePx = 3;
  if (std::abs(canvas_x - cycle_last_x_) > kSameSpotTolerancePx ||
      std::abs(canvas_y - cycle_last_y_) > kSameSpotTolerancePx ||
      hits.size() != cycle_last_hits_.size()) {
    return false;
  }

  for (size_t i = 0; i < hits.size(); ++i) {
    if (!(hits[i] == cycle_last_hits_[i])) {
      return false;
    }
  }
  return true;
}

std::optional<size_t> InteractionCoordinator::FindSelectedCycleIndex(
    const std::vector<SelectedEntity>& hits) const {
  for (size_t i = 0; i < hits.size(); ++i) {
    const SelectedEntity hit = hits[i];
    if (hit.type == EntityType::Object) {
      if (ctx_ && ctx_->selection &&
          ctx_->selection->IsObjectSelected(hit.index)) {
        return i;
      }
      continue;
    }

    if (std::find(selected_entities_.begin(), selected_entities_.end(), hit) !=
        selected_entities_.end()) {
      return i;
    }

    const SelectedEntity selected = GetSelectedEntity();
    if (selected == hit) {
      return i;
    }
  }

  return std::nullopt;
}

void InteractionCoordinator::UpdateSelectionCycleHudPreview() {
  if (!ctx_ || !ctx_->canvas || !ctx_->canvas->IsMouseHovering()) {
    return;
  }

  const ImGuiIO& io = ImGui::GetIO();
  if (!IsCycleModifierHeld(io)) {
    return;
  }

  const DungeonCanvasTransform transform(ctx_->canvas->zero_point(),
                                         ctx_->canvas->scrolling(),
                                         ctx_->canvas->global_scale());
  const auto [canvas_x, canvas_y] =
      transform.ScreenToRoomPixelCoordinates(io.MousePos);
  const auto hits = GetEntitiesAtPosition(canvas_x, canvas_y);
  if (hits.size() < 2) {
    cycle_last_hits_.clear();
    cycle_next_index_ = 0;
    cycle_hud_start_time_ = -1.0;
    return;
  }

  if (!SameCycleTarget(canvas_x, canvas_y, hits)) {
    cycle_next_index_ = 0;
  }

  cycle_last_x_ = canvas_x;
  cycle_last_y_ = canvas_y;
  cycle_hud_screen_pos_ = io.MousePos;
  cycle_hud_start_time_ = ImGui::GetCurrentContext() ? ImGui::GetTime() : 0.0;
  cycle_last_hits_ = hits;
  if (const auto selected_index = FindSelectedCycleIndex(hits)) {
    cycle_active_index_ = *selected_index;
  } else {
    cycle_active_index_ = cycle_next_index_ % hits.size();
  }
}

void InteractionCoordinator::DrawSelectionCycleHud() {
  if (ImGui::GetCurrentContext()) {
    UpdateSelectionCycleHudPreview();
  }

  if (!ImGui::GetCurrentContext() || cycle_last_hits_.size() < 2 ||
      cycle_hud_start_time_ < 0.0) {
    return;
  }

  const double elapsed = ImGui::GetTime() - cycle_hud_start_time_;
  const ImGuiIO& io = ImGui::GetIO();
  const bool cycle_modifier_held = IsCycleModifierHeld(io);
  if (!cycle_modifier_held && elapsed > kCycleHudHoldSeconds) {
    return;
  }

  const float alpha =
      cycle_modifier_held
          ? 1.0f
          : std::max(0.0f,
                     1.0f - static_cast<float>(elapsed / kCycleHudHoldSeconds));
  const auto& theme = AgentUI::GetTheme();
  ImVec4 bg = theme.panel_bg_darker;
  bg.w *= 0.92f * alpha;
  ImVec4 border = theme.panel_border_color;
  border.w *= alpha;
  ImVec4 text = theme.text_primary;
  text.w *= alpha;
  ImVec4 secondary = theme.text_secondary_color;
  secondary.w *= alpha;
  ImVec4 active = theme.accent_color;
  active.w *= alpha;

  ImGui::SetNextWindowPos(
      ImVec2(cycle_hud_screen_pos_.x + 14.0f, cycle_hud_screen_pos_.y + 14.0f),
      ImGuiCond_Always);
  ImGui::SetNextWindowBgAlpha(bg.w);
  ImGui::PushStyleColor(ImGuiCol_WindowBg, bg);
  ImGui::PushStyleColor(ImGuiCol_Border, border);
  ImGui::PushStyleColor(ImGuiCol_Text, text);
  ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(8.0f, 6.0f));
  ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 4.0f);
  constexpr ImGuiWindowFlags kFlags =
      ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoSavedSettings |
      ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoNav |
      ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoInputs;
  if (ImGui::Begin("##DungeonSelectionCycleHud", nullptr, kFlags)) {
    ImGui::TextColored(secondary, tr("Cycle"));
    for (size_t i = 0; i < cycle_last_hits_.size(); ++i) {
      const char* marker = (i == cycle_active_index_) ? "[X]" : "[ ]";
      ImGui::TextColored(i == cycle_active_index_ ? active : secondary, "%s",
                         marker);
      ImGui::SameLine(0.0f, 5.0f);
      ImGui::TextUnformatted(
          DescribeCycleHudEntity(cycle_last_hits_[i]).c_str());
    }
  }
  ImGui::End();
  ImGui::PopStyleVar(2);
  ImGui::PopStyleColor(3);
}

void InteractionCoordinator::DrawMultiEntitySelectionHighlights() {
  auto* room = ctx_ ? ctx_->GetCurrentRoomConst() : nullptr;
  if (!room || !ctx_ || !ctx_->canvas) {
    return;
  }

  const auto& theme = AgentUI::GetTheme();
  ImDrawList* draw_list = ImGui::GetWindowDrawList();
  const DungeonCanvasTransform transform(ctx_->canvas->zero_point(),
                                         ctx_->canvas->scrolling(),
                                         ctx_->canvas->global_scale());
  const float scale = transform.scale();
  const float pulse =
      0.6f + 0.4f * std::sin(static_cast<float>(ImGui::GetTime()) * 6.0f);

  for (size_t i = 0; i < selected_entities_.size(); ++i) {
    const auto entity = selected_entities_[i];
    const auto bounds = GetEntityBounds(*room, entity);
    if (!bounds.has_value()) {
      continue;
    }

    auto [x, y, w, h] = *bounds;
    ImVec2 start = transform.RoomPixelsToScreen(
        ImVec2(static_cast<float>(x), static_cast<float>(y)));
    const ImVec2 size = transform.RoomSizeToScreen(
        ImVec2(static_cast<float>(w), static_cast<float>(h)));
    ImVec2 end(start.x + size.x, start.y + size.y);
    constexpr float kMargin = 2.0f;
    start.x -= kMargin;
    start.y -= kMargin;
    end.x += kMargin;
    end.y += kMargin;

    ImVec4 base = EntitySelectionColor(theme, entity.type);
    ImVec4 fill = base;
    fill.w = 0.14f + 0.10f * pulse;
    ImVec4 border = base;
    border.w = (i == 0) ? 0.95f : 0.72f;

    draw_list->AddRectFilled(start, end, ImGui::GetColorU32(fill));
    draw_list->AddRect(start, end, ImGui::GetColorU32(border), 0.0f, 0,
                       (i == 0) ? 2.2f : 1.6f);
    draw_list->AddText(ImVec2(start.x, start.y - 14.0f * scale),
                       ImGui::GetColorU32(theme.text_primary),
                       DescribeEntity(entity).c_str());
  }
}

std::string InteractionCoordinator::DescribeEntity(
    SelectedEntity entity) const {
  const zelda3::Room* room = ctx_ ? ctx_->GetCurrentRoomConst() : nullptr;
  switch (entity.type) {
    case EntityType::Door: {
      if (room && entity.index < room->GetDoors().size()) {
        const auto& door = room->GetDoors()[entity.index];
        const std::string direction_name(
            zelda3::GetDoorDirectionName(door.direction));
        return absl::StrFormat("Door (%s)", direction_name);
      }
      return "Door";
    }
    case EntityType::Sprite:
      if (room && entity.index < room->GetSprites().size()) {
        const auto& sprite = room->GetSprites()[entity.index];
        return absl::StrFormat("Sprite (0x%02X - %s)", sprite.id(),
                               zelda3::ResolveSpriteName(sprite.id()));
      }
      return "Sprite";
    case EntityType::Item:
      if (room && entity.index < room->GetPotItems().size()) {
        const auto& item = room->GetPotItems()[entity.index];
        return absl::StrFormat("Item (0x%02X)", item.item);
      }
      return "Item";
    case EntityType::Object:
      if (room && entity.index < room->GetTileObjects().size()) {
        const auto& object = room->GetTileObjects()[entity.index];
        return absl::StrFormat("Object (0x%03X - %s)", object.id_,
                               zelda3::GetObjectName(object.id_));
      }
      return "Object";
    case EntityType::None:
    default:
      return "None";
  }
}

std::string InteractionCoordinator::DescribeCycleHudEntity(
    SelectedEntity entity) const {
  return TruncateCycleHudLabel(DescribeEntity(entity));
}

}  // namespace yaze::editor
