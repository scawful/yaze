// Related header
#include "interaction_coordinator.h"

// Third-party library headers
#include "imgui/imgui.h"

namespace yaze::editor {

void InteractionCoordinator::SetContext(InteractionContext* ctx) {
  ctx_ = ctx;
  door_handler_.SetContext(ctx);
  sprite_handler_.SetContext(ctx);
  item_handler_.SetContext(ctx);
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

  current_mode_ = Mode::Select;
}

bool InteractionCoordinator::IsPlacementActive() const {
  return door_handler_.IsPlacementActive() ||
         sprite_handler_.IsPlacementActive() ||
         item_handler_.IsPlacementActive();
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

  // In select mode, try to select entity
  return TrySelectEntityAtCursor(canvas_x, canvas_y);
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
}

void InteractionCoordinator::HandleRelease() {
  // Forward release to all handlers
  door_handler_.HandleRelease();
  sprite_handler_.HandleRelease();
  item_handler_.HandleRelease();
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
