#include "app/editor/dungeon/interaction/interaction_coordinator.h"
#include "app/editor/dungeon/dungeon_selection_edit.h"
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
    return;
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
}

void InteractionCoordinator::FinishEntityGroupDrag() {
  const bool was_active = entity_group_drag_active_;
  // Clear first: finalization can restore selection or flush other gestures.
  ResetEntityGroupDragState();
  if (was_active && ctx_ && ctx_->on_selection_edit_finished) {
    ctx_->on_selection_edit_finished();
  }
}

void InteractionCoordinator::FinishSelectionGesture() {
  FinishEntityGroupDrag();
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

std::vector<SelectedEntity> InteractionCoordinator::SelectedEntitiesForEdit()
    const {
  if (!selected_entities_.empty()) {
    return selected_entities_;
  }
  const auto selected = GetSelectedEntity();
  return selected.type == EntityType::None
             ? std::vector<SelectedEntity>{}
             : std::vector<SelectedEntity>{selected};
}

int InteractionCoordinator::SelectionMoveStepPixels() const {
  const auto entities = SelectedEntitiesForEdit();
  return std::any_of(entities.begin(), entities.end(),
                     [](const auto entity) {
                       return entity.type == EntityType::Sprite;
                     })
             ? dungeon_coords::kSpriteTileSize
             : dungeon_coords::kTileSize;
}

absl::Status InteractionCoordinator::ReportSelectionEditStatus(
    absl::Status status) {
  const bool new_error = !status.ok() && status != selection_edit_status_;
  selection_edit_status_ = std::move(status);
  if (new_error && ctx_ && ctx_->on_selection_edit_error) {
    ctx_->on_selection_edit_error(selection_edit_status_);
  }
  return selection_edit_status_;
}

absl::Status InteractionCoordinator::CommitSelectionEdit(
    const DungeonSelectionEditRequest& request, bool continuous) {
  auto* room = ctx_ ? ctx_->GetCurrentRoom() : nullptr;
  if (!room) {
    return ReportSelectionEditStatus(absl::FailedPreconditionError(
        "No room is available for selection editing"));
  }
  auto planned = PlanDungeonSelectionEdit(*room, request);
  if (!planned.ok()) {
    return ReportSelectionEditStatus(planned.status());
  }
  if (!planned->changed()) {
    return ReportSelectionEditStatus(absl::OkStatus());
  }
  if (ctx_->on_selection_edit) {
    const auto status = ctx_->on_selection_edit(*planned, continuous);
    if (!status.ok()) {
      return ReportSelectionEditStatus(status);
    }
  } else {
    // Standalone handlers retain legacy notifications; the editor callback
    // stages global constraints and contributes one shared undo action.
    if (!continuous)
      FinishSelectionGesture();
    for (const auto [mask, domain] :
         {std::pair{kSelectionObjects, MutationDomain::kTileObjects},
          std::pair{kSelectionDoors, MutationDomain::kDoors},
          std::pair{kSelectionSprites, MutationDomain::kSprites},
          std::pair{kSelectionItems, MutationDomain::kItems}}) {
      if (planned->domains & mask)
        ctx_->NotifyMutation(domain);
    }
    ApplyDungeonSelectionEditState(*room, planned->after, planned->domains);
  }
  // A drag keeps its indices and active state. Re-selecting handlers during
  // each increment would end their drag and could recursively finalize undo.
  if (!continuous) {
    if (ctx_->selection) {
      ctx_->selection->ClearSelection();
      for (const auto index : planned->after.selected_objects) {
        ctx_->selection->SelectObject(index,
                                      ObjectSelection::SelectionMode::Add);
      }
    }
    SetSelectedEntities(planned->after.selected_entities);
  }
  if (!ctx_->on_selection_edit) {
    for (const auto [mask, domain] :
         {std::pair{kSelectionObjects, MutationDomain::kTileObjects},
          std::pair{kSelectionDoors, MutationDomain::kDoors},
          std::pair{kSelectionSprites, MutationDomain::kSprites},
          std::pair{kSelectionItems, MutationDomain::kItems}}) {
      if (planned->domains & mask)
        ctx_->NotifyInvalidateCache(domain);
    }
    ctx_->NotifyEntityChanged();
  }
  return ReportSelectionEditStatus(absl::OkStatus());
}

bool InteractionCoordinator::NudgeSelected(int delta_x, int delta_y) {
  DungeonSelectionEditRequest request;
  request.kind = DungeonSelectionEditKind::kMove;
  if (ctx_ && ctx_->selection)
    request.objects = ctx_->selection->GetSelectedIndices();
  request.entities = SelectedEntitiesForEdit();
  if (request.objects.empty() && request.entities.empty())
    return false;
  if (request.objects.empty() && request.entities.size() == 1 &&
      request.entities.front().type == EntityType::Door) {
    // A door-only arrow command advances its authored wall slot. Mixed moves
    // use one physical displacement and may not silently detach the door.
    const auto* room = ctx_ ? ctx_->GetCurrentRoomConst() : nullptr;
    const size_t index = request.entities.front().index;
    if (!room || index >= room->GetDoors().size()) {
      return CommitSelectionEdit(request).ok();
    }
    const auto& door = room->GetDoors()[index];
    const bool horizontal = door.direction == zelda3::DoorDirection::North ||
                            door.direction == zelda3::DoorDirection::South;
    const int64_t next_position =
        static_cast<int64_t>(door.position) + (horizontal ? delta_x : delta_y);
    if (next_position == door.position)
      return false;
    if (next_position < 0 ||
        next_position >= zelda3::DoorPositionManager::kMaxDoorPositions ||
        !zelda3::DoorPositionManager::IsValidPosition(
            static_cast<uint8_t>(next_position), door.direction)) {
      ReportSelectionEditStatus(
          absl::OutOfRangeError("No door slot in that direction"));
      return false;
    }
    auto next = door;
    next.position = static_cast<uint8_t>(next_position);
    const auto [old_x, old_y] = door.GetPixelCoords();
    const auto [new_x, new_y] = next.GetPixelCoords();
    request.delta_x_pixels = new_x - old_x;
    request.delta_y_pixels = new_y - old_y;
    return CommitSelectionEdit(request).ok();
  }
  const int step = SelectionMoveStepPixels();
  const int64_t dx = static_cast<int64_t>(delta_x) * step;
  const int64_t dy = static_cast<int64_t>(delta_y) * step;
  if (dx < -512 || dx > 512 || dy < -512 || dy > 512) {
    ReportSelectionEditStatus(
        absl::OutOfRangeError("Selection movement leaves the room"));
    return false;
  }
  request.delta_x_pixels = static_cast<int>(dx);
  request.delta_y_pixels = static_cast<int>(dy);
  return CommitSelectionEdit(request).ok();
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

absl::Status InteractionCoordinator::DeleteSelectedEntity() {
  DungeonSelectionEditRequest request;
  request.kind = DungeonSelectionEditKind::kDelete;
  request.entities = SelectedEntitiesForEdit();
  return CommitSelectionEdit(request);
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
}

void InteractionCoordinator::HandleEntityGroupDrag(ImVec2 current_pos) {
  if (!entity_group_drag_active_) {
    return;
  }

  entity_group_drag_current_ = snapping::SnapToTileGrid(current_pos);
  const ImVec2 drag_delta(
      entity_group_drag_current_.x - entity_group_drag_start_.x,
      entity_group_drag_current_.y - entity_group_drag_start_.y);

  const int step = SelectionMoveStepPixels();
  const int drag_dx = static_cast<int>(drag_delta.x) / step * step;
  const int drag_dy = static_cast<int>(drag_delta.y) / step * step;
  const int inc_dx = drag_dx - entity_group_drag_last_dx_;
  const int inc_dy = drag_dy - entity_group_drag_last_dy_;
  if (inc_dx == 0 && inc_dy == 0)
    return;

  DungeonSelectionEditRequest request;
  request.kind = DungeonSelectionEditKind::kMove;
  if (ctx_ && ctx_->selection)
    request.objects = ctx_->selection->GetSelectedIndices();
  request.entities = SelectedEntitiesForEdit();
  request.delta_x_pixels = inc_dx;
  request.delta_y_pixels = inc_dy;
  if (CommitSelectionEdit(request, true).ok()) {
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
