# Object Selection System Architecture

## Overview

The Object Selection System provides comprehensive selection functionality for the dungeon editor. It's designed following the Single Responsibility Principle, separating selection state management from input handling and rendering.

**Version**: 1.0
**Date**: 2025-11-26
**Location**: `src/app/editor/dungeon/object_selection.{h,cc}`

## Architecture Diagram

```
┌─────────────────────────────────────────────────────┐
│          DungeonEditorV2 (Main Editor)               │
│  - Coordinates all components                        │
│  - Card-based UI system                              │
│  - Handles Save/Load/Undo/Redo                       │
└────────────────┬────────────────────────────────────┘
                 │
                 ▼
┌─────────────────────────────────────────────────────┐
│        DungeonCanvasViewer (Canvas Rendering)        │
│  - Room graphics display                             │
│  - Layer management (BG1/BG2)                        │
│  - Sprite rendering                                  │
└────────────────┬────────────────────────────────────┘
                 │
                 ▼
┌─────────────────────────────────────────────────────┐
│    DungeonObjectInteraction (Input Handling)         │
│  - Mouse input processing                            │
│  - Keyboard shortcut handling                        │
│  - Coordinate conversion                             │
│  - Drag operations                                   │
│  - Copy/Paste/Delete                                 │
└────────────────┬────────────────────────────────────┘
                 │
                 ▼
┌─────────────────────────────────────────────────────┐
│       ObjectSelection (Selection State)              │
│  - Selection state management                        │
│  - Multi-selection logic                             │
│  - Rectangle selection                               │
│  - Visual rendering                                  │
│  - Bounding box calculations                         │
└─────────────────────────────────────────────────────┘
```

## Component Responsibilities

### ObjectSelection (New Component)

**Purpose**: Manages selection state and provides selection operations

**Key Responsibilities**:
- Single object selection
- Multi-selection (Shift, Ctrl modifiers)
- Rectangle drag selection
- Select all functionality
- Visual feedback rendering
- Coordinate conversion utilities

**Design Principles**:
- **Stateless Operations**: Pure functions where possible
- **Composable**: Can be integrated into any editor
- **Testable**: All operations have unit tests
- **Type-Safe**: Uses enums instead of magic booleans

### DungeonObjectInteraction (Enhanced)

**Purpose**: Handles user input and coordinates object manipulation

**Integration Points**:
```cpp
// Before (scattered state)
std::vector<size_t> selected_object_indices_;
bool object_select_active_;
ImVec2 object_select_start_;
ImVec2 object_select_end_;

// After (delegated to ObjectSelection)
ObjectSelection selection_;
```

## Selection Modes

The system supports four distinct selection modes:

### 1. Single Selection (Default)
**Trigger**: Left click on object
**Behavior**: Replace current selection with clicked object
**Use Case**: Basic object selection

```cpp
selection_.SelectObject(index, ObjectSelection::SelectionMode::Single);
```

### 2. Add Selection (Shift+Click)
**Trigger**: Shift + Left click
**Behavior**: Add object to existing selection
**Use Case**: Building multi-object selections incrementally

```cpp
selection_.SelectObject(index, ObjectSelection::SelectionMode::Add);
```

### 3. Toggle Selection (Ctrl+Click)
**Trigger**: Ctrl + Left click
**Behavior**: Toggle object in/out of selection
**Use Case**: Fine-tuning selections by removing specific objects

```cpp
selection_.SelectObject(index, ObjectSelection::SelectionMode::Toggle);
```

### 4. Rectangle Selection (Drag)
**Trigger**: Right click + drag
**Behavior**: Select all objects within rectangle
**Use Case**: Bulk selection of objects

```cpp
selection_.BeginRectangleSelection(x, y);
selection_.UpdateRectangleSelection(x, y);
selection_.EndRectangleSelection(objects, mode);
```

## Keyboard Shortcuts

| Shortcut | Action | Implementation |
|----------|--------|----------------|
| **Left Click** | Select single object | `SelectObject(index, Single)` |
| **Shift + Click** | Add to selection | `SelectObject(index, Add)` |
| **Ctrl + Click** | Toggle in selection | `SelectObject(index, Toggle)` |
| **Right Drag** | Rectangle select | `Begin/Update/EndRectangleSelection()` |
| **Ctrl + A** | Select all | `SelectAll(count)` |
| **Delete** | Delete selected | `HandleDeleteSelected()` |
| **Ctrl + C** | Copy selected | `HandleCopySelected()` |
| **Ctrl + V** | Paste objects | `HandlePasteObjects()` |
| **Esc** | Clear selection | `ClearSelection()` |

## Visual Feedback

### Selected Objects
- **Border**: Pulsing animated outline (yellow-gold)
- **Handles**: Four corner handles (cyan-white at 0.85f alpha)
- **Animation**: Sinusoidal pulse at 4 Hz

```cpp
// Animation formula
float pulse = 0.7f + 0.3f * std::sin(ImGui::GetTime() * 4.0f);
```

### Rectangle Selection
- **Border**: Accent color at 0.85f alpha (high visibility)
- **Fill**: Accent color at 0.15f alpha (subtle background)
- **Thickness**: 2.0f pixels

### Entity Visibility Standards
All entity rendering follows yaze's visibility standards:
- **High-contrast colors**: Bright yellow-gold, cyan-white
- **Alpha value**: 0.85f for primary visibility
- **Background alpha**: 0.15f for fills

## Coordinate Systems

### Room Coordinates (Tiles)
- **Range**: 0-63 (64x64 tile rooms)
- **Unit**: Tiles
- **Origin**: Top-left corner (0, 0)

### Canvas Coordinates (Pixels)
- **Range**: 0-511 (unscaled, 8 pixels per tile)
- **Unit**: Pixels
- **Origin**: Top-left corner (0, 0)
- **Scale**: Subject to canvas zoom (global_scale)

### Conversion Functions
```cpp
// Tile → Pixel (unscaled)
std::pair<int, int> RoomToCanvasCoordinates(int room_x, int room_y) {
  return {room_x * 8, room_y * 8};
}

// Pixel → Tile (unscaled)
std::pair<int, int> CanvasToRoomCoordinates(int canvas_x, int canvas_y) {
  return {canvas_x / 8, canvas_y / 8};
}
```

## Bounding Box Calculation

Objects have variable sizes based on their `size_` field:

```cpp
// Object size encoding
uint8_t size_h = (object.size_ & 0x0F);        // Horizontal size
uint8_t size_v = (object.size_ >> 4) & 0x0F;   // Vertical size

// Dimensions (in tiles)
int width = size_h + 1;
int height = size_v + 1;
```

### Examples:
| `size_` | Width | Height | Total |
|---------|-------|--------|-------|
| `0x00` | 1 | 1 | 1x1 |
| `0x11` | 2 | 2 | 2x2 |
| `0x23` | 4 | 3 | 4x3 |
| `0xFF` | 16 | 16 | 16x16 |

## Theme Integration

All colors are sourced from `AgentUITheme`:

```cpp
const auto& theme = AgentUI::GetTheme();

// Selection colors
theme.dungeon_selection_primary       // Yellow-gold (pulsing border)
theme.dungeon_selection_secondary     // Cyan (secondary elements)
theme.dungeon_selection_handle        // Cyan-white (corner handles)
theme.accent_color                    // UI accent (rectangle selection)
```

**Critical Rule**: NEVER use hardcoded `ImVec4` colors. Always use theme system.

## Implementation Details

### Selection State Storage
Uses `std::set<size_t>` for selected indices:

**Advantages**:
- O(log n) insertion/deletion
- O(log n) lookup
- Automatic sorting
- No duplicates

**Trade-offs**:
- Slightly higher memory overhead than vector
- Justified by performance and correctness guarantees

### Rectangle Selection Algorithm

**Intersection Test**:
```cpp
bool IsObjectInRectangle(const RoomObject& object,
                         int min_x, int min_y, int max_x, int max_y) {
  auto [obj_x, obj_y, obj_width, obj_height] = GetObjectBounds(object);

  int obj_min_x = obj_x;
  int obj_max_x = obj_x + obj_width - 1;
  int obj_min_y = obj_y;
  int obj_max_y = obj_y + obj_height - 1;

  bool x_overlap = (obj_min_x <= max_x) && (obj_max_x >= min_x);
  bool y_overlap = (obj_min_y <= max_y) && (obj_max_y >= min_y);

  return x_overlap && y_overlap;
}
```

This uses standard axis-aligned bounding box (AABB) intersection.

## Testing Strategy

### Unit Tests
Location: `test/unit/object_selection_test.cc`

**Coverage**:
- Single selection (replace existing)
- Multi-selection (Shift+click add)
- Toggle selection (Ctrl+click toggle)
- Rectangle selection (all modes)
- Select all
- Coordinate conversion
- Bounding box calculation
- Callback invocation

**Test Patterns**:
```cpp
// Setup
ObjectSelection selection;
std::vector<RoomObject> objects = CreateTestObjects();

// Action
selection.SelectObject(0, ObjectSelection::SelectionMode::Single);

// Verify
EXPECT_TRUE(selection.IsObjectSelected(0));
EXPECT_EQ(selection.GetSelectionCount(), 1);
```

### Integration Points

Test integration with:
1. `DungeonObjectInteraction` for input handling
2. `DungeonCanvasViewer` for rendering
3. `DungeonEditorV2` for undo/redo

## Performance Characteristics

| Operation | Complexity | Notes |
|-----------|------------|-------|
| `SelectObject` | O(log n) | Set insertion |
| `IsObjectSelected` | O(log n) | Set lookup |
| `GetSelectedIndices` | O(n) | Convert set to vector |
| `SelectObjectsInRect` | O(m * log n) | m objects checked |
| `DrawSelectionHighlights` | O(k) | k selected objects |

Where:
- n = total objects in selection
- m = total objects in room
- k = selected object count

## API Examples

### Single Selection
```cpp
// Replace selection with object 5
selection_.SelectObject(5, ObjectSelection::SelectionMode::Single);
```

### Building Multi-Selection
```cpp
// Start with object 0
selection_.SelectObject(0, ObjectSelection::SelectionMode::Single);

// Add objects 2, 4, 6
selection_.SelectObject(2, ObjectSelection::SelectionMode::Add);
selection_.SelectObject(4, ObjectSelection::SelectionMode::Add);
selection_.SelectObject(6, ObjectSelection::SelectionMode::Add);

// Toggle object 4 (remove it)
selection_.SelectObject(4, ObjectSelection::SelectionMode::Toggle);

// Result: Objects 0, 2, 6 selected
```

### Rectangle Selection
```cpp
// Begin selection at (10, 10)
selection_.BeginRectangleSelection(10, 10);

// Update to (50, 50) as user drags
selection_.UpdateRectangleSelection(50, 50);

// Complete selection (add mode)
selection_.EndRectangleSelection(objects, ObjectSelection::SelectionMode::Add);
```

### Working with Selected Objects
```cpp
// Get all selected indices (sorted)
auto indices = selection_.GetSelectedIndices();

// Get primary (first) selection
if (auto primary = selection_.GetPrimarySelection()) {
  size_t index = primary.value();
  // Use primary object...
}

// Check selection state
if (selection_.HasSelection()) {
  size_t count = selection_.GetSelectionCount();
  // Process selected objects...
}
```

## Integration Guide

See `OBJECT_SELECTION_INTEGRATION.md` for step-by-step integration instructions.

**Key Steps**:
1. Add `ObjectSelection` member to `DungeonObjectInteraction`
2. Update input handling to use selection modes
3. Replace manual selection state with `ObjectSelection` API
4. Implement click selection helper
5. Update rendering to use `ObjectSelection::Draw*()` methods

## Future Enhancements

### Planned Features
- **Lasso Selection**: Free-form polygon selection
- **Selection Filters**: Filter by object type, layer, size
- **Selection History**: Undo/redo for selection changes
- **Selection Groups**: Named selections (e.g., "All Chests")
- **Smart Selection**: Select similar objects (by type/size)
- **Marquee Zoom**: Zoom to fit selected objects

### API Extensions
```cpp
// Future API ideas
void SelectByType(int16_t object_id);
void SelectByLayer(RoomObject::LayerType layer);
void SelectSimilar(size_t reference_index);
void SaveSelectionGroup(const std::string& name);
void LoadSelectionGroup(const std::string& name);
```

## Debugging

### Enable Debug Logging
```cpp
// In object_selection.cc
#define SELECTION_DEBUG_LOGGING

// Logs will appear like:
// [ObjectSelection] SelectObject: index=5, mode=Single
// [ObjectSelection] Selection count: 3
```

### Visual Debugging
Use the Debug Controls card in the dungeon editor:
1. Enable "Show Object Bounds"
2. Filter by object type/layer
3. Inspect selection state in real-time

### Common Issues

**Issue**: Objects not selecting on click
**Solution**: Check object bounds calculation, verify coordinate conversion

**Issue**: Selection persists after clear
**Solution**: Ensure `NotifySelectionChanged()` is called

**Issue**: Visual artifacts during drag
**Solution**: Verify canvas scale is applied correctly in rendering

## References

- **ZScream**: Reference implementation for dungeon object selection
- **ImGui Test Engine**: Automated UI testing framework
- **yaze Canvas System**: `src/app/gui/canvas/canvas.h`
- **Theme System**: `src/app/editor/agent/agent_ui_theme.h`

## Changelog

### Version 1.0 (2025-11-26)
- Initial implementation
- Single/multi/rectangle selection
- Visual feedback with theme integration
- Comprehensive unit test coverage
- Integration with DungeonObjectInteraction

---

**Maintainer**: yaze development team
**Last Updated**: 2025-11-26
