# Object Selection System - Interaction Flow

## Visual Flow Diagrams

### 1. Single Object Selection (Left Click)

```
┌─────────────────────────────────────────────────────┐
│  User Input: Left Click on Object                   │
└────────────────────┬────────────────────────────────┘
                     │
                     ▼
┌─────────────────────────────────────────────────────┐
│  DungeonObjectInteraction::HandleCanvasMouseInput()  │
│  - Detect left click                                 │
│  - Get mouse position                                │
│  - Convert to room coordinates                       │
└────────────────────┬────────────────────────────────┘
                     │
                     ▼
┌─────────────────────────────────────────────────────┐
│  TrySelectObjectAtCursor(x, y, mode)                 │
│  - Iterate objects in reverse order                  │
│  - Check if cursor within object bounds              │
│  - Find topmost object at cursor                     │
└────────────────────┬────────────────────────────────┘
                     │
                     ▼
┌─────────────────────────────────────────────────────┐
│  ObjectSelection::SelectObject(index, Single)        │
│  - Clear previous selection                          │
│  - Add object to selection (set.insert)              │
│  - Trigger selection changed callback                │
└────────────────────┬────────────────────────────────┘
                     │
                     ▼
┌─────────────────────────────────────────────────────┐
│  Visual Feedback                                     │
│  - Draw pulsing border (yellow-gold, 0.85f alpha)   │
│  - Draw corner handles (cyan-white, 0.85f alpha)    │
│  - Animate pulse at 4 Hz                            │
└─────────────────────────────────────────────────────┘
```

### 2. Multi-Selection (Shift+Click)

```
┌─────────────────────────────────────────────────────┐
│  User Input: Shift + Left Click                     │
└────────────────────┬────────────────────────────────┘
                     │
                     ▼
┌─────────────────────────────────────────────────────┐
│  HandleCanvasMouseInput()                            │
│  - Detect Shift key down                             │
│  - Set mode = SelectionMode::Add                     │
└────────────────────┬────────────────────────────────┘
                     │
                     ▼
┌─────────────────────────────────────────────────────┐
│  ObjectSelection::SelectObject(index, Add)           │
│  - Keep existing selection                           │
│  - Add new object (set.insert)                       │
│  - Trigger callback                                  │
└────────────────────┬────────────────────────────────┘
                     │
                     ▼
┌─────────────────────────────────────────────────────┐
│  Visual Feedback                                     │
│  - Highlight ALL selected objects                    │
│  - Each with pulsing border + handles                │
└─────────────────────────────────────────────────────┘
```

### 3. Toggle Selection (Ctrl+Click)

```
┌─────────────────────────────────────────────────────┐
│  User Input: Ctrl + Left Click                      │
└────────────────────┬────────────────────────────────┘
                     │
                     ▼
┌─────────────────────────────────────────────────────┐
│  HandleCanvasMouseInput()                            │
│  - Detect Ctrl key down                              │
│  - Set mode = SelectionMode::Toggle                  │
└────────────────────┬────────────────────────────────┘
                     │
                     ▼
┌─────────────────────────────────────────────────────┐
│  ObjectSelection::SelectObject(index, Toggle)        │
│  - If selected: Remove (set.erase)                   │
│  - If not selected: Add (set.insert)                 │
│  - Trigger callback                                  │
└────────────────────┬────────────────────────────────┘
                     │
                     ▼
┌─────────────────────────────────────────────────────┐
│  Visual Feedback                                     │
│  - Update highlights for current selection           │
│  - Removed objects no longer highlighted             │
└─────────────────────────────────────────────────────┘
```

### 4. Rectangle Selection (Right Click + Drag)

```
┌─────────────────────────────────────────────────────┐
│  User Input: Right Click + Drag                     │
└────────────────────┬────────────────────────────────┘
                     │
                     ▼
┌─────────────────────────────────────────────────────┐
│  Phase 1: Mouse Down (Right Button)                 │
│  ObjectSelection::BeginRectangleSelection(x, y)      │
│  - Store start position                              │
│  - Set rectangle_selection_active = true             │
└────────────────────┬────────────────────────────────┘
                     │
                     ▼
┌─────────────────────────────────────────────────────┐
│  Phase 2: Mouse Drag                                 │
│  ObjectSelection::UpdateRectangleSelection(x, y)     │
│  - Update end position                               │
│  - Draw rectangle preview                            │
│    • Border: accent_color @ 0.85f alpha             │
│    • Fill: accent_color @ 0.15f alpha               │
└────────────────────┬────────────────────────────────┘
                     │
                     ▼
┌─────────────────────────────────────────────────────┐
│  Phase 3: Mouse Release                              │
│  ObjectSelection::EndRectangleSelection(objects)     │
│  - Convert canvas coords to room coords              │
│  - For each object:                                  │
│    • Get object bounds                               │
│    • Check AABB intersection with rectangle          │
│    • If intersects: Add to selection                 │
│  - Set rectangle_selection_active = false            │
│  - Trigger callback                                  │
└────────────────────┬────────────────────────────────┘
                     │
                     ▼
┌─────────────────────────────────────────────────────┐
│  Visual Feedback                                     │
│  - Highlight all selected objects                    │
│  - Remove rectangle preview                          │
└─────────────────────────────────────────────────────┘
```

### 5. Select All (Ctrl+A)

```
┌─────────────────────────────────────────────────────┐
│  User Input: Ctrl + A                                │
└────────────────────┬────────────────────────────────┘
                     │
                     ▼
┌─────────────────────────────────────────────────────┐
│  HandleCanvasMouseInput()                            │
│  - Detect Ctrl + A key combination                   │
│  - Get current room object count                     │
└────────────────────┬────────────────────────────────┘
                     │
                     ▼
┌─────────────────────────────────────────────────────┐
│  ObjectSelection::SelectAll(object_count)            │
│  - Clear previous selection                          │
│  - Add all object indices (0..count-1)               │
│  - Trigger callback                                  │
└────────────────────┬────────────────────────────────┘
                     │
                     ▼
┌─────────────────────────────────────────────────────┐
│  Visual Feedback                                     │
│  - Highlight ALL objects in room                     │
│  - May cause performance impact if many objects      │
└─────────────────────────────────────────────────────┘
```

## State Transitions

```
┌────────────┐
│ No         │◄─────────────────────┐
│ Selection  │                      │
└──────┬─────┘                      │
       │                            │
       │ Left Click                 │ Esc or Clear
       ▼                            │
┌────────────┐                      │
│ Single     │◄─────────┐           │
│ Selection  │          │           │
└──────┬─────┘          │           │
       │                │           │
       │ Shift+Click    │ Ctrl+Click│
       │ Right+Drag     │ (deselect)│
       ▼                │           │
┌────────────┐          │           │
│ Multi      │──────────┘           │
│ Selection  │──────────────────────┘
└────────────┘
```

## Rendering Pipeline

```
┌─────────────────────────────────────────────────────┐
│  DungeonCanvasViewer::DrawDungeonCanvas(room_id)    │
└────────────────────┬────────────────────────────────┘
                     │
                     ▼
┌─────────────────────────────────────────────────────┐
│  1. Draw Room Background Layers (BG1, BG2)           │
│     - Load room graphics                             │
│     - Render to bitmaps                              │
│     - Draw bitmaps to canvas                         │
└────────────────────┬────────────────────────────────┘
                     │
                     ▼
┌─────────────────────────────────────────────────────┐
│  2. Draw Sprites                                     │
│     - Render sprite markers (8x8 squares)            │
│     - Color-code by layer                            │
└────────────────────┬────────────────────────────────┘
                     │
                     ▼
┌─────────────────────────────────────────────────────┐
│  3. Handle Object Interaction                        │
│     DungeonObjectInteraction::HandleCanvasMouseInput()│
│     - Process mouse/keyboard input                   │
│     - Update selection state                         │
└────────────────────┬────────────────────────────────┘
                     │
                     ▼
┌─────────────────────────────────────────────────────┐
│  4. Draw Selection Visuals (TOP LAYER)               │
│     ObjectSelection::DrawSelectionHighlights()       │
│     - For each selected object:                      │
│       • Convert room coords to canvas coords         │
│       • Apply canvas scale                           │
│       • Draw pulsing border                          │
│       • Draw corner handles                          │
│     ObjectSelection::DrawRectangleSelectionBox()     │
│     - If active: Draw rectangle preview              │
└────────────────────┬────────────────────────────────┘
                     │
                     ▼
┌─────────────────────────────────────────────────────┐
│  5. Draw Canvas Overlays                             │
│     - Grid lines                                     │
│     - Debug overlays (if enabled)                    │
└─────────────────────────────────────────────────────┘
```

## Data Flow for Object Operations

### Delete Selected Objects

```
┌─────────────────────────────────────────────────────┐
│  User Input: Delete Key                              │
└────────────────────┬────────────────────────────────┘
                     │
                     ▼
┌─────────────────────────────────────────────────────┐
│  DungeonObjectInteraction::HandleDeleteSelected()    │
│  1. Get selected indices from ObjectSelection        │
│  2. Sort indices in descending order                 │
│  3. For each index (high to low):                    │
│     - Call room.RemoveTileObject(index)              │
│  4. Clear selection                                  │
│  5. Trigger cache invalidation (re-render)           │
└────────────────────┬────────────────────────────────┘
                     │
                     ▼
┌─────────────────────────────────────────────────────┐
│  Room::RenderRoomGraphics()                          │
│  - Re-render room with deleted objects removed       │
└─────────────────────────────────────────────────────┘
```

### Copy/Paste Selected Objects

```
Copy Flow:
┌─────────────────────────────────────────────────────┐
│  User Input: Ctrl+C                                  │
└────────────────────┬────────────────────────────────┘
                     │
                     ▼
┌─────────────────────────────────────────────────────┐
│  DungeonObjectInteraction::HandleCopySelected()      │
│  1. Get selected indices from ObjectSelection        │
│  2. Copy objects to clipboard_ vector                │
│  3. Set has_clipboard_data_ = true                   │
└─────────────────────────────────────────────────────┘

Paste Flow:
┌─────────────────────────────────────────────────────┐
│  User Input: Ctrl+V                                  │
└────────────────────┬────────────────────────────────┘
                     │
                     ▼
┌─────────────────────────────────────────────────────┐
│  DungeonObjectInteraction::HandlePasteObjects()      │
│  1. Get mouse position                               │
│  2. Calculate offset from first clipboard object     │
│  3. For each clipboard object:                       │
│     - Create copy with offset position               │
│     - Clamp to room bounds (0-63)                    │
│     - Add to room                                    │
│  4. Trigger re-render                                │
└─────────────────────────────────────────────────────┘
```

## Performance Considerations

### Selection State Storage
```
std::set<size_t> selected_indices_;

Advantages:
✓ O(log n) insert/delete/lookup
✓ Automatic sorting
✓ No duplicates
✓ Cache-friendly for small selections

Trade-offs:
✗ Slightly higher memory overhead
✗ Not as cache-friendly for iteration (vs vector)

Decision: Justified for correctness guarantees
```

### Rendering Optimization
```
void DrawSelectionHighlights() {
  if (selected_indices_.empty()) {
    return;  // Early exit - O(1)
  }

  // Only render visible objects (canvas culling)
  for (size_t index : selected_indices_) {
    if (IsObjectVisible(index)) {
      DrawHighlight(index);  // O(k) where k = selected count
    }
  }
}

Complexity: O(k) where k = selected object count
Typical case: k < 20 objects selected
Worst case: k = 296 (all objects) - rare
```

## Memory Layout

```
ObjectSelection Instance (~64 bytes)
├── selected_indices_ (std::set<size_t>)
│   └── Red-Black Tree
│       ├── Node overhead: ~32 bytes per node
│       └── Typical selection: 5 objects = ~160 bytes
├── rectangle_selection_active_ (bool) = 1 byte
├── rect_start_x_ (int) = 4 bytes
├── rect_start_y_ (int) = 4 bytes
├── rect_end_x_ (int) = 4 bytes
├── rect_end_y_ (int) = 4 bytes
└── selection_changed_callback_ (std::function) = 32 bytes

Total: ~64 bytes + (32 bytes × selected_count)

Example: 10 objects selected = ~384 bytes
Negligible compared to room graphics (~2MB)
```

## Integration Checklist

When integrating ObjectSelection into DungeonObjectInteraction:

- [ ] Add `ObjectSelection selection_;` member
- [ ] Remove old selection state variables
- [ ] Update `HandleCanvasMouseInput()` to use selection modes
- [ ] Add `TrySelectObjectAtCursor()` helper
- [ ] Update `DrawSelectionHighlights()` to delegate to ObjectSelection
- [ ] Update `DrawSelectBox()` to delegate to ObjectSelection
- [ ] Update `HandleDeleteSelected()` to use `selection_.GetSelectedIndices()`
- [ ] Update `HandleCopySelected()` to use `selection_.GetSelectedIndices()`
- [ ] Update clipboard operations
- [ ] Add Ctrl+A handler for select all
- [ ] Test single selection
- [ ] Test multi-selection (Shift+click)
- [ ] Test toggle selection (Ctrl+click)
- [ ] Test rectangle selection
- [ ] Test select all (Ctrl+A)
- [ ] Test copy/paste/delete operations
- [ ] Verify visual feedback (borders, handles)
- [ ] Verify theme color usage
- [ ] Run unit tests
- [ ] Test performance with many objects

---

**Diagram Format**: ASCII art compatible with markdown viewers
**Last Updated**: 2025-11-26
