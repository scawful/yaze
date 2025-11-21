# Keyboard Shortcuts Implementation Guide

**Feature**: Enhanced keyboard shortcuts for dungeon object editor
**Priority**: High (Quick win with major UX impact)
**Estimated Time**: 0.5 days
**Files to Modify**: `src/app/editor/dungeon/object_editor_card.cc`

---

## Overview

Adding comprehensive keyboard shortcuts will dramatically improve editor efficiency. This is the quickest high-impact feature to implement.

---

## Implementation

### Step 1: Add Keyboard Handler Method

```cpp
// In object_editor_card.h
class ObjectEditorCard {
 private:
  void HandleKeyboardShortcuts();

  // Settings
  bool show_grid_ = true;
  bool show_object_ids_ = false;
};
```

### Step 2: Implement Handler in .cc File

```cpp
// Add to object_editor_card.cc after Draw() method

void ObjectEditorCard::HandleKeyboardShortcuts() {
  // Only process shortcuts when editor window has focus
  if (!ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows)) {
    return;
  }

  const ImGuiIO& io = ImGui::GetIO();

  // Ctrl+A: Select all objects
  if (ImGui::IsKeyPressed(ImGuiKey_A) && io.KeyCtrl && !io.KeyShift) {
    SelectAllObjects();
  }

  // Ctrl+Shift+A: Deselect all
  if (ImGui::IsKeyPressed(ImGuiKey_A) && io.KeyCtrl && io.KeyShift) {
    DeselectAllObjects();
  }

  // Delete: Remove selected objects
  if (ImGui::IsKeyPressed(ImGuiKey_Delete)) {
    DeleteSelectedObjects();
  }

  // Ctrl+D: Duplicate selected objects
  if (ImGui::IsKeyPressed(ImGuiKey_D) && io.KeyCtrl) {
    DuplicateSelectedObjects();
  }

  // Ctrl+C: Copy selected objects
  if (ImGui::IsKeyPressed(ImGuiKey_C) && io.KeyCtrl) {
    CopySelectedObjects();
  }

  // Ctrl+V: Paste objects
  if (ImGui::IsKeyPressed(ImGuiKey_V) && io.KeyCtrl) {
    PasteObjects();
  }

  // Ctrl+Z: Undo
  if (ImGui::IsKeyPressed(ImGuiKey_Z) && io.KeyCtrl && !io.KeyShift) {
    if (object_editor_) {
      object_editor_->Undo();
    }
  }

  // Ctrl+Shift+Z or Ctrl+Y: Redo
  if ((ImGui::IsKeyPressed(ImGuiKey_Z) && io.KeyCtrl && io.KeyShift) ||
      (ImGui::IsKeyPressed(ImGuiKey_Y) && io.KeyCtrl)) {
    if (object_editor_) {
      object_editor_->Redo();
    }
  }

  // G: Toggle grid
  if (ImGui::IsKeyPressed(ImGuiKey_G) && !io.KeyCtrl) {
    show_grid_ = !show_grid_;
  }

  // I: Toggle object ID labels
  if (ImGui::IsKeyPressed(ImGuiKey_I) && !io.KeyCtrl) {
    show_object_ids_ = !show_object_ids_;
    if (canvas_viewer_) {
      canvas_viewer_->object_interaction().SetShowObjectIDs(show_object_ids_);
    }
  }

  // Arrow keys: Nudge selected objects
  if (!io.KeyCtrl) {
    int dx = 0, dy = 0;
    if (ImGui::IsKeyPressed(ImGuiKey_LeftArrow)) dx = -1;
    if (ImGui::IsKeyPressed(ImGuiKey_RightArrow)) dx = 1;
    if (ImGui::IsKeyPressed(ImGuiKey_UpArrow)) dy = -1;
    if (ImGui::IsKeyPressed(ImGuiKey_DownArrow)) dy = 1;

    if (dx != 0 || dy != 0) {
      NudgeSelectedObjects(dx, dy);
    }
  }

  // Tab: Cycle through objects
  if (ImGui::IsKeyPressed(ImGuiKey_Tab) && !io.KeyCtrl) {
    CycleObjectSelection(io.KeyShift ? -1 : 1);
  }

  // Escape: Deselect all
  if (ImGui::IsKeyPressed(ImGuiKey_Escape)) {
    DeselectAllObjects();
  }
}
```

### Step 3: Call Handler in Draw()

```cpp
// In ObjectEditorCard::Draw() - add at beginning or end of method

void ObjectEditorCard::Draw() {
  // Existing code...

  // Handle keyboard shortcuts
  HandleKeyboardShortcuts();

  // Rest of existing code...
}
```

---

## Helper Methods to Implement

### SelectAllObjects()

```cpp
void ObjectEditorCard::SelectAllObjects() {
  if (!canvas_viewer_) return;

  auto& interaction = canvas_viewer_->object_interaction();
  std::vector<size_t> all_indices;

  // Get all object indices
  for (size_t i = 0; i < current_room_objects_.size(); ++i) {
    all_indices.push_back(i);
  }

  interaction.SetSelectedObjects(all_indices);
}
```

### DeselectAllObjects()

```cpp
void ObjectEditorCard::DeselectAllObjects() {
  if (!canvas_viewer_) return;
  canvas_viewer_->object_interaction().ClearSelection();
}
```

### DeleteSelectedObjects()

```cpp
void ObjectEditorCard::DeleteSelectedObjects() {
  if (!object_editor_) return;

  auto& interaction = canvas_viewer_->object_interaction();
  const auto& selected = interaction.GetSelectedObjectIndices();

  if (selected.empty()) return;

  // Show confirmation for bulk delete
  if (selected.size() > 5) {
    // Show modal confirmation dialog
    if (!ConfirmBulkDelete(selected.size())) {
      return;
    }
  }

  // Delete in reverse order to maintain indices
  std::vector<size_t> sorted_indices(selected.begin(), selected.end());
  std::sort(sorted_indices.rbegin(), sorted_indices.rend());

  for (size_t idx : sorted_indices) {
    object_editor_->DeleteObject(idx);
  }

  interaction.ClearSelection();
}
```

### DuplicateSelectedObjects()

```cpp
void ObjectEditorCard::DuplicateSelectedObjects() {
  if (!object_editor_) return;

  auto& interaction = canvas_viewer_->object_interaction();
  const auto& selected = interaction.GetSelectedObjectIndices();

  if (selected.empty()) return;

  std::vector<size_t> new_indices;

  for (size_t idx : selected) {
    // Duplicate with offset
    auto new_idx = object_editor_->DuplicateObject(idx, 1, 1);
    if (new_idx.has_value()) {
      new_indices.push_back(*new_idx);
    }
  }

  // Select the new objects
  interaction.SetSelectedObjects(new_indices);
}
```

### NudgeSelectedObjects()

```cpp
void ObjectEditorCard::NudgeSelectedObjects(int dx, int dy) {
  if (!object_editor_) return;

  auto& interaction = canvas_viewer_->object_interaction();
  const auto& selected = interaction.GetSelectedObjectIndices();

  if (selected.empty()) return;

  for (size_t idx : selected) {
    object_editor_->MoveObject(idx, dx, dy);
  }
}
```

### CycleObjectSelection()

```cpp
void ObjectEditorCard::CycleObjectSelection(int direction) {
  if (!canvas_viewer_) return;

  auto& interaction = canvas_viewer_->object_interaction();
  const auto& selected = interaction.GetSelectedObjectIndices();

  size_t total_objects = current_room_objects_.size();
  if (total_objects == 0) return;

  size_t current_idx = selected.empty() ? 0 : selected.front();
  size_t next_idx = (current_idx + direction + total_objects) % total_objects;

  interaction.SetSelectedObjects({next_idx});

  // Scroll to selected object
  ScrollToObject(next_idx);
}
```

---

## Keyboard Shortcut Reference

| Shortcut | Action |
|----------|--------|
| **Ctrl+A** | Select all objects |
| **Ctrl+Shift+A** | Deselect all |
| **Delete** | Delete selected objects |
| **Ctrl+D** | Duplicate selected |
| **Ctrl+C** | Copy selected |
| **Ctrl+V** | Paste objects |
| **Ctrl+Z** | Undo |
| **Ctrl+Shift+Z** or **Ctrl+Y** | Redo |
| **G** | Toggle grid |
| **I** | Toggle object ID labels |
| **Arrow Keys** | Nudge selected objects |
| **Tab** | Cycle to next object |
| **Shift+Tab** | Cycle to previous object |
| **Escape** | Deselect all |

---

## Testing Checklist

- [ ] Each shortcut works in isolation
- [ ] Shortcuts don't conflict with ImGui defaults
- [ ] Undo/redo works with shortcut operations
- [ ] Shortcuts disabled when typing in text fields
- [ ] Visual feedback for toggle shortcuts (grid, IDs)
- [ ] Bulk operations ask for confirmation
- [ ] Arrow key nudging respects grid snap

---

## Integration Points

**Existing Methods to Use**:
- `object_editor_->Undo()` - Line 783 in dungeon_object_editor.cc
- `object_editor_->Redo()` - Line 822 in dungeon_object_editor.cc
- `interaction.GetSelectedObjectIndices()` - Line 381 in dungeon_object_interaction.cc

**New Methods in DungeonObjectInteraction** (optional):
```cpp
// Add to dungeon_object_interaction.h
void SetShowObjectIDs(bool show);
bool GetShowObjectIDs() const { return show_object_ids_; }
```

---

## Performance Considerations

- Keyboard polling has negligible performance impact
- Only process shortcuts when window focused
- Batch operations for arrow key nudging
- Use const refs for selected object access

---

## Future Enhancements

- Configurable shortcuts (save to user preferences)
- Chord shortcuts (e.g., L,A for "align left")
- Macro recording (repeat last N operations)
- Shortcut hints overlay (like VS Code)

---

**Status**: Ready to implement
**Next Step**: Begin coding in `object_editor_card.cc`
