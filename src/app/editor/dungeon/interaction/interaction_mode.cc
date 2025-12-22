#include "app/editor/dungeon/interaction/interaction_mode.h"

namespace yaze {
namespace editor {

void InteractionModeManager::SetMode(InteractionMode mode) {
  if (mode == current_mode_) {
    return;  // No change needed
  }

  // Store previous mode for potential undo/escape
  previous_mode_ = current_mode_;

  // Clear appropriate state based on transition type
  switch (current_mode_) {
    case InteractionMode::PlaceObject:
    case InteractionMode::PlaceDoor:
    case InteractionMode::PlaceSprite:
    case InteractionMode::PlaceItem:
      // Leaving placement mode - clear placement data
      mode_state_.ClearPlacementData();
      break;

    case InteractionMode::DraggingObjects:
    case InteractionMode::DraggingEntity:
      // Leaving drag mode - clear drag data
      mode_state_.ClearDragData();
      break;

    case InteractionMode::RectangleSelect:
      // Leaving rectangle select - clear rectangle data
      mode_state_.ClearRectangleData();
      break;

    case InteractionMode::Select:
      // Leaving select mode - nothing special to clear
      break;
  }

  current_mode_ = mode;
}

void InteractionModeManager::CancelCurrentMode() {
  // Clear all state and return to select mode
  mode_state_.Clear();
  previous_mode_ = current_mode_;
  current_mode_ = InteractionMode::Select;
}

const char* InteractionModeManager::GetModeName() const {
  switch (current_mode_) {
    case InteractionMode::Select:
      return "Select";
    case InteractionMode::PlaceObject:
      return "Place Object";
    case InteractionMode::PlaceDoor:
      return "Place Door";
    case InteractionMode::PlaceSprite:
      return "Place Sprite";
    case InteractionMode::PlaceItem:
      return "Place Item";
    case InteractionMode::DraggingObjects:
      return "Dragging Objects";
    case InteractionMode::DraggingEntity:
      return "Dragging Entity";
    case InteractionMode::RectangleSelect:
      return "Rectangle Select";
    default:
      return "Unknown";
  }
}

}  // namespace editor
}  // namespace yaze
