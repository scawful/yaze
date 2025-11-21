# Dungeon Editor Architecture

**Status**: Complete
**Last Updated**: 2025-11-21
**Related Code**: `src/app/editor/dungeon/`, `src/zelda3/dungeon/`

This document outlines the architecture of the Dungeon Editor in YAZE, detailing the key components, their responsibilities, interaction patterns, and best practices for contributors.

## High-Level Overview

The Dungeon Editor uses a modular, component-based architecture following the coordinator pattern. The main editor class, `DungeonEditorV2`, acts as a coordinator rather than a monolithic implementer. It delegates specific tasks (rendering, object manipulation, loading) to specialized classes. This design allows each component to focus on a single responsibility and makes the codebase more maintainable and testable.

**Key Pattern**: Large editor classes are decomposed into smaller, single-responsibility modules. Each module can be tested and modified independently.

### Key Components

| Component | Location | Responsibility |
|-----------|----------|----------------|
| **DungeonEditorV2** | `src/app/editor/dungeon/dungeon_editor_v2.h` | Main UI coordinator. Inherits from `Editor` base class. Manages editor cards (Room Selector, Object Editor, etc.), handles user input, and integrates sub-systems. Entry point for the GUI editor. |
| **DungeonEditorSystem** | `src/zelda3/dungeon/dungeon_editor_system.h` | High-level system coordinator. Manages overall editor state (modes, current room), undo/redo history, and provides APIs for manipulating dungeon elements (objects, sprites, items, entrances, doors, chests). Bridges ROM data with editing operations. |
| **DungeonObjectEditor** | `src/zelda3/dungeon/dungeon_object_editor.h` | Specialized logic for editing *room objects* (layout/walls/floors). Handles insert, delete, move, resize, and layer operations. Maintains its own undo/redo stack for granular changes. Supports copy/paste and template operations. |
| **DungeonCanvasViewer** | `src/app/editor/dungeon/dungeon_canvas_viewer.h` | Handles rendering the dungeon view. Draws room backgrounds, objects, grid, and overlays using `gui::Canvas`. Manages coordinate conversion between screen pixels, canvas pixels, and tile coordinates. |
| **ObjectEditorCard** | `src/app/editor/dungeon/object_editor_card.h` | UI panel for object editing. Contains object selector, interaction mode controls, and layer management. Communicates with `DungeonObjectEditor` via callbacks. |
| **DungeonRoomLoader** | `src/app/editor/dungeon/dungeon_room_loader.h` | Handles low-level loading of room data from ROM. Manages pointer tables, calculates room sizes for safety, handles multithreaded bulk loading (up to 8 threads). Provides room graphics and palette management. |
| **DungeonRoomSelector** | `src/app/editor/dungeon/dungeon_room_selector.h` | UI component for selecting which room to edit (0x000-0x127). Shows room previews and metadata. |
| **Room** | `src/zelda3/dungeon/room.h` | Data model for a single dungeon room. Manages objects (`std::vector<RoomObject>`), sprites, graphics buffers (`bg1`, `bg2` as `BackgroundBuffer`), properties, and entrances. Handles decompression and rendering of room data. |
| **DungeonObjectValidator** | `src/zelda3/dungeon/dungeon_validator.h` | Validates room data for consistency. Checks for object conflicts, overlaps, and ROM safety issues. Used before saving. |

## Interaction Flow

1.  **Loading**:
    *   `DungeonEditorV2` initializes and calls `DungeonRoomLoader` to load room data from ROM into `Room` objects.
    *   `DungeonRoomLoader` parses the compressed ROM data and populates the `Room` structures.

2.  **Rendering**:
    *   `Room` renders its graphics into two background buffers (`bg1` and `bg2`) based on blockset and palette settings.
    *   `DungeonCanvasViewer` takes these buffers and draws them to the `gui::Canvas`.
    *   It also renders overlays for objects, grid lines, and selection highlights.

3.  **Object Editing**:
    *   User interacts with `ObjectEditorCard` or `DungeonCanvasViewer`.
    *   Input events are routed to `DungeonObjectEditor` (often via `DungeonEditorSystem`).
    *   `DungeonObjectEditor` modifies the `Room` object data (e.g., adding a new `RoomObject`).
    *   `DungeonObjectEditor` pushes the change to its local undo stack.
    *   The `Room` marks itself as dirty, triggering a re-render of the graphics buffers if necessary.

## Component Details

### DungeonEditorSystem

A facade that simplifies interaction with the complex dungeon data. It supports multiple modes:
*   `kObjects`: Layout editing (walls, floors).
*   `kSprites`: Enemy/NPC placement.
*   `kItems`: Pots, drops, etc.
*   `kEntrances`/`kDoors`: Connectivity.

It manages a global undo/redo stack that snapshots the state of the editor.

### DungeonObjectEditor

Handles the specific logic for manipulating the compressed object format of ALttP.
*   **Coordinates**: Handles 0-63 grid coordinates.
*   **Layers**: Manages the 3 logical layers (BG1, BG2, collision).
*   **Validation**: Ensures objects are valid and don't conflict.
*   **Clipboard**: Implements Copy/Paste/Duplicate functionality.

### DungeonCanvasViewer

The "View" component.
*   **Coordinate Systems**:
    *   **Canvas Pixels**: Unscaled pixels (512x512 for a full room).
    *   **Screen Pixels**: Scaled by zoom level.
    *   **Tile Coordinates**: 0-63 grid used by the game engine.
*   **Scrolling**: Supports programmatically scrolling to specific coordinates (e.g., to focus on an object).

## Data Model (Room)

The `Room` class encapsulates all data for a single room ID (0x000 - 0x127).
*   **Tile Objects**: The structural elements (walls, floors). Stored as `RoomObject`s.
*   **Sprites**: Entities like enemies.
*   **Graphics**: Each room has its own `BackgroundBuffer`s for rendering. This allows for lazy rendering where only the active room's graphics are in memory/VRAM.

## Best Practices for Contributors

### Adding New Editor Modes

When adding a new editor mode (e.g., chest editing):

1.  **Add to `DungeonEditorSystem::EditorMode` enum** (in `dungeon_editor_system.h`)
2.  **Create a dedicated system class** (e.g., `DungeonChestEditor`) that handles the specific logic
3.  **Add callback support** in the system coordinator for the new mode
4.  **Integrate into `DungeonEditorV2`** by creating a new card panel (e.g., `ChestEditorCard`)
5.  **Add undo/redo support** by delegating to the dedicated system class
6.  **Test thoroughly** with `test/unit/dungeon_*_test.cc`

### Object Editing Best Practices

**Always call `CreateUndoPoint()` before modifying room objects**:
```cpp
auto status = object_editor_->CreateUndoPoint();  // Snapshot current state
// Then perform modifications like InsertObject, DeleteObject, etc.
```

**Use callbacks for communication** between components:
```cpp
object_editor_->SetRoomChangedCallback([this]() {
    canvas_viewer_->InvalidateRoomRender();  // Trigger re-render
});
```

**Batch operations for efficiency**:
```cpp
// Single undo point covers entire batch
object_editor_->CreateUndoPoint();
for (auto index : selection) {
    object_editor_->ResizeObject(index, new_size);  // No snapshot per item
}
```

### Coordinate System Management

ALttP uses **0-63 grid coordinates** for objects within a room:
*   **Screen Pixels**: Canvas coordinates (0-511 for a full room)
*   **Tile Coordinates**: 0-63 grid used by game engine
*   **Conversion**: Provided by `DungeonCanvasViewer::ScreenToTileCoordinates()`

Always use `DungeonCanvasViewer` for coordinate conversions to maintain consistency.

## Future Improvements

*   **Sprite Editor Integration**: Currently `DungeonObjectEditor` handles layout objects well. Sprite/Enemy editing logic needs to be fully integrated into the new system with dedicated UI.
*   **Door/Entrance Editor**: Dedicated UI for room connectivity (currently handled in properties panel only).
*   **Collision Visualization**: Real-time visualization of collision/walkability data derived from objects.
*   **Template System Expansion**: Pre-built room layouts library for rapid room creation.
*   **Multithreaded Rendering**: Off-thread graphics rendering for large rooms to maintain UI responsiveness.
*   **Room Copy/Paste**: Ability to duplicate entire rooms with optional offset adjustments.
