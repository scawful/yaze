#include "app/editor/dungeon/interaction/interaction_coordinator.h"
#include "app/editor/dungeon/object_selection.h"

// Third-party library headers
#include "imgui/imgui.h"

namespace yaze::editor {

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
         item_handler_.IsPlacementActive() ||
         tile_handler_.IsPlacementActive();
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

  // In select mode, only handle the click if the cursor is over an entity or object.
  const auto entity = GetEntityAtPosition(canvas_x, canvas_y);
  if (!entity.has_value()) {
    return false;
  }

  const ImGuiIO& io = ImGui::GetIO();
  const bool additive = io.KeyShift || io.KeyCtrl || io.KeySuper;

  // Cross-selection rules:
  // 1. Entities (Door, Sprite, Item) are exclusive to each other.
  // 2. Entities are exclusive to Tile Objects.
  // 3. Tile Objects support multi-selection.

  if (entity->type == EntityType::Object) {
    // If selecting an object, always clear entities.
    ClearAllEntitySelections();
    // If not additive, also clear existing object selection.
    if (!additive && ctx_ && ctx_->selection) {
      ctx_->selection->ClearSelection();
    }
    return tile_handler_.HandleClick(canvas_x, canvas_y);
  } else {
    // If selecting an entity, always clear everything.
    ClearAllEntitySelections();
    if (ctx_ && ctx_->selection) {
      ctx_->selection->ClearSelection();
    }

    switch (entity->type) {
      case EntityType::Door:
        return door_handler_.HandleClick(canvas_x, canvas_y);
      case EntityType::Sprite:
        return sprite_handler_.HandleClick(canvas_x, canvas_y);
      case EntityType::Item:
        return item_handler_.HandleClick(canvas_x, canvas_y);
      default:
        return false;
    }
  }
}

bool InteractionCoordinator::HandleMouseWheel(float delta) {
  if (door_handler_.IsPlacementActive()) return door_handler_.HandleMouseWheel(delta);
  if (sprite_handler_.IsPlacementActive()) return sprite_handler_.HandleMouseWheel(delta);
  if (item_handler_.IsPlacementActive()) return item_handler_.HandleMouseWheel(delta);
  if (tile_handler_.IsPlacementActive()) return tile_handler_.HandleMouseWheel(delta);
  
  return tile_handler_.HandleMouseWheel(delta);
}

void InteractionCoordinator::SelectEntity(EntityType type, size_t index) {
  ClearAllEntitySelections();

  // Entity selection takes exclusive priority over tile object selection.
  if (ctx_ && ctx_->selection) {
    ctx_->selection->ClearSelection();
  }

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
  if (auto door = door_handler_.GetEntityAtPosition(canvas_x, canvas_y))
    return SelectedEntity{EntityType::Door, *door};
  if (auto sprite = sprite_handler_.GetEntityAtPosition(canvas_x, canvas_y))
    return SelectedEntity{EntityType::Sprite, *sprite};
  if (auto item = item_handler_.GetEntityAtPosition(canvas_x, canvas_y))
    return SelectedEntity{EntityType::Item, *item};
  if (auto object = tile_handler_.GetEntityAtPosition(canvas_x, canvas_y))
    return SelectedEntity{EntityType::Object, *object};
  return std::nullopt;
}

SelectedEntity InteractionCoordinator::GetSelectedEntity() const {
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
  if (door_handler_.HasSelection()) {
    door_handler_.HandleDrag(current_pos, delta);
  }
  if (sprite_handler_.HasSelection()) {
    sprite_handler_.HandleDrag(current_pos, delta);
  }
  if (item_handler_.HasSelection()) {
    item_handler_.HandleDrag(current_pos, delta);
  }
  
  // Tile objects (managed by ObjectSelection)
  if (tile_handler_.IsPlacementActive() || (ctx_ && ctx_->selection && ctx_->selection->HasSelection())) {
    tile_handler_.HandleDrag(current_pos, delta);
  }
}

void InteractionCoordinator::HandleRelease() {
  door_handler_.HandleRelease();
  sprite_handler_.HandleRelease();
  item_handler_.HandleRelease();
  tile_handler_.HandleRelease();
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
  // Draw selection highlights for all entity types
  door_handler_.DrawSelectionHighlight();
  sprite_handler_.DrawSelectionHighlight();
  item_handler_.DrawSelectionHighlight();

  // Draw snap indicators for door placement
  if (door_handler_.IsPlacementActive() || door_handler_.HasSelection()) {
    door_handler_.DrawSnapIndicators();
  }
}

bool InteractionCoordinator::TrySelectEntityAtCursor(int canvas_x,
                                                      int canvas_y) {
  // Clear all selections first
  ClearAllEntitySelections();

  // Try to select in priority order: doors, sprites, items
  // (matches original DungeonObjectInteraction behavior)
  if (door_handler_.HandleClick(canvas_x, canvas_y)) {
    return true;
  }
  if (sprite_handler_.HandleClick(canvas_x, canvas_y)) {
    return true;
  }
  if (item_handler_.HandleClick(canvas_x, canvas_y)) {
    return true;
  }

  return false;
}

bool InteractionCoordinator::HasEntitySelection() const {
  return door_handler_.HasSelection() || sprite_handler_.HasSelection() ||
         item_handler_.HasSelection();
}

void InteractionCoordinator::ClearAllEntitySelections() {
  door_handler_.ClearSelection();
  sprite_handler_.ClearSelection();
  item_handler_.ClearSelection();
}

void InteractionCoordinator::DeleteSelectedEntity() {
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

}  // namespace yaze::editor
