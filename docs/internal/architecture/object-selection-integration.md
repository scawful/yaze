# Object Selection System Integration Guide

## Overview

The `ObjectSelection` class provides a clean, composable selection system for dungeon objects. It follows the Single Responsibility Principle by focusing solely on selection state management and operations, while leaving input handling and canvas interaction to `DungeonObjectInteraction`.

## Architecture

```
DungeonCanvasViewer
  └── DungeonObjectInteraction (handles input, coordinates)
       └── ObjectSelection (manages selection state)
```

## Integration Steps

### 1. Add ObjectSelection to DungeonObjectInteraction

**File**: `src/app/editor/dungeon/dungeon_object_interaction.h`

```cpp
#include "object_selection.h"

class DungeonObjectInteraction {
 public:
  // ... existing code ...

  // Expose selection system
  ObjectSelection& selection() { return selection_; }
  const ObjectSelection& selection() const { return selection_; }

 private:
  // Replace existing selection state with ObjectSelection
  ObjectSelection selection_;

  // Remove these (now handled by ObjectSelection):
  // std::vector<size_t> selected_object_indices_;
  // bool object_select_active_;
  // ImVec2 object_select_start_;
  // ImVec2 object_select_end_;
};
```

### 2. Update HandleCanvasMouseInput Method

**File**: `src/app/editor/dungeon/dungeon_object_interaction.cc`

```cpp
void DungeonObjectInteraction::HandleCanvasMouseInput() {
  const ImGuiIO& io = ImGui::GetIO();

  if (!canvas_->IsMouseHovering()) {
    return;
  }

  ImVec2 mouse_pos = io.MousePos;
  ImVec2 canvas_pos = canvas_->zero_point();
  ImVec2 canvas_mouse_pos = ImVec2(mouse_pos.x - canvas_pos.x,
                                   mouse_pos.y - canvas_pos.y);

  // Determine selection mode based on modifiers
  ObjectSelection::SelectionMode mode = ObjectSelection::SelectionMode::Single;
  if (ImGui::IsKeyDown(ImGuiKey_LeftShift) || ImGui::IsKeyDown(ImGuiKey_RightShift)) {
    mode = ObjectSelection::SelectionMode::Add;
  } else if (ImGui::IsKeyDown(ImGuiKey_LeftCtrl) || ImGui::IsKeyDown(ImGuiKey_RightCtrl)) {
    mode = ObjectSelection::SelectionMode::Toggle;
  }

  // Handle left click - single object selection or object placement
  if (ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
    if (object_loaded_) {
      // Place object at click position
      auto [room_x, room_y] = CanvasToRoomCoordinates(
          static_cast<int>(canvas_mouse_pos.x),
          static_cast<int>(canvas_mouse_pos.y));
      PlaceObjectAtPosition(room_x, room_y);
    } else {
      // Try to select object at cursor position
      TrySelectObjectAtCursor(static_cast<int>(canvas_mouse_pos.x),
                              static_cast<int>(canvas_mouse_pos.y), mode);
    }
  }

  // Handle right click drag - rectangle selection
  if (ImGui::IsMouseClicked(ImGuiMouseButton_Right) && !object_loaded_) {
    selection_.BeginRectangleSelection(static_cast<int>(canvas_mouse_pos.x),
                                       static_cast<int>(canvas_mouse_pos.y));
  }

  if (selection_.IsRectangleSelectionActive()) {
    if (ImGui::IsMouseDragging(ImGuiMouseButton_Right)) {
      selection_.UpdateRectangleSelection(static_cast<int>(canvas_mouse_pos.x),
                                          static_cast<int>(canvas_mouse_pos.y));
    }

    if (ImGui::IsMouseReleased(ImGuiMouseButton_Right)) {
      if (rooms_ && current_room_id_ >= 0 && current_room_id_ < 296) {
        auto& room = (*rooms_)[current_room_id_];
        selection_.EndRectangleSelection(room.GetTileObjects(), mode);
      } else {
        selection_.CancelRectangleSelection();
      }
    }
  }

  // Handle Ctrl+A - Select All
  if ((ImGui::IsKeyDown(ImGuiKey_LeftCtrl) || ImGui::IsKeyDown(ImGuiKey_RightCtrl)) &&
      ImGui::IsKeyPressed(ImGuiKey_A)) {
    if (rooms_ && current_room_id_ >= 0 && current_room_id_ < 296) {
      auto& room = (*rooms_)[current_room_id_];
      selection_.SelectAll(room.GetTileObjects().size());
    }
  }

  // Handle dragging selected objects (if any selected and not placing)
  if (selection_.HasSelection() && !object_loaded_) {
    HandleObjectDragging(canvas_mouse_pos);
  }
}
```

### 3. Add Helper Method for Click Selection

```cpp
void DungeonObjectInteraction::TrySelectObjectAtCursor(
    int canvas_x, int canvas_y, ObjectSelection::SelectionMode mode) {
  if (!rooms_ || current_room_id_ < 0 || current_room_id_ >= 296) {
    return;
  }

  auto& room = (*rooms_)[current_room_id_];
  const auto& objects = room.GetTileObjects();

  // Convert canvas coordinates to room coordinates
  auto [room_x, room_y] = CanvasToRoomCoordinates(canvas_x, canvas_y);

  // Find object at cursor (check in reverse order to prioritize top objects)
  for (int i = objects.size() - 1; i >= 0; --i) {
    auto [obj_x, obj_y, obj_width, obj_height] =
        ObjectSelection::GetObjectBounds(objects[i]);

    // Check if cursor is within object bounds
    if (room_x >= obj_x && room_x < obj_x + obj_width &&
        room_y >= obj_y && room_y < obj_y + obj_height) {
      selection_.SelectObject(i, mode);
      return;
    }
  }

  // No object found - clear selection if Single mode
  if (mode == ObjectSelection::SelectionMode::Single) {
    selection_.ClearSelection();
  }
}
```

### 4. Update Rendering Methods

Replace existing selection highlight methods:

```cpp
void DungeonObjectInteraction::DrawSelectionHighlights() {
  if (!rooms_ || current_room_id_ < 0 || current_room_id_ >= 296) {
    return;
  }

  auto& room = (*rooms_)[current_room_id_];
  selection_.DrawSelectionHighlights(canvas_, room.GetTileObjects());
}

void DungeonObjectInteraction::DrawSelectBox() {
  selection_.DrawRectangleSelectionBox(canvas_);
}
```

### 5. Update Delete/Copy/Paste Operations

```cpp
void DungeonObjectInteraction::HandleDeleteSelected() {
  if (!selection_.HasSelection() || !rooms_) {
    return;
  }
  if (current_room_id_ < 0 || current_room_id_ >= 296) {
    return;
  }

  if (mutation_hook_) {
    mutation_hook_();
  }

  auto& room = (*rooms_)[current_room_id_];

  // Get sorted indices in descending order
  auto indices = selection_.GetSelectedIndices();
  std::sort(indices.rbegin(), indices.rend());

  // Delete from highest index to lowest (avoid index shifts)
  for (size_t index : indices) {
    room.RemoveTileObject(index);
  }

  selection_.ClearSelection();

  if (cache_invalidation_callback_) {
    cache_invalidation_callback_();
  }
}

void DungeonObjectInteraction::HandleCopySelected() {
  if (!selection_.HasSelection() || !rooms_) {
    return;
  }
  if (current_room_id_ < 0 || current_room_id_ >= 296) {
    return;
  }

  auto& room = (*rooms_)[current_room_id_];
  const auto& objects = room.GetTileObjects();

  clipboard_.clear();
  for (size_t index : selection_.GetSelectedIndices()) {
    if (index < objects.size()) {
      clipboard_.push_back(objects[index]);
    }
  }

  has_clipboard_data_ = !clipboard_.empty();
}
```

## Keyboard Shortcuts

The selection system supports standard keyboard shortcuts:

| Shortcut | Action |
|----------|--------|
| **Left Click** | Select single object (replace selection) |
| **Shift + Left Click** | Add object to selection |
| **Ctrl + Left Click** | Toggle object in selection |
| **Right Click + Drag** | Rectangle selection |
| **Ctrl + A** | Select all objects |
| **Delete** | Delete selected objects |
| **Ctrl + C** | Copy selected objects |
| **Ctrl + V** | Paste objects |

## Visual Feedback

The selection system provides clear visual feedback:

1. **Selected Objects**: Pulsing animated border with corner handles
2. **Rectangle Selection**: Semi-transparent box with colored border
3. **Multiple Selection**: All selected objects highlighted simultaneously

## Testing

See `test/unit/object_selection_test.cc` for comprehensive unit tests covering:
- Single selection
- Multi-selection (Shift/Ctrl)
- Rectangle selection
- Select all
- Coordinate conversion
- Bounding box calculation

## Benefits of This Design

1. **Separation of Concerns**: Selection logic is isolated from input handling
2. **Testability**: Pure functions for selection operations
3. **Reusability**: ObjectSelection can be used in other editors
4. **Maintainability**: Clear API with well-defined responsibilities
5. **Performance**: Uses `std::set` for O(log n) lookups and automatic sorting
6. **Type Safety**: Uses enum for selection modes instead of booleans
7. **Theme Integration**: All colors sourced from `AgentUITheme`

## Future Enhancements

Potential future improvements:
- Lasso selection (free-form polygon)
- Selection filters (by object type, layer)
- Selection history (undo/redo selection changes)
- Selection groups (named selections)
- Marquee zoom (zoom to selected objects)
