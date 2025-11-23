# Undo/Redo System Architecture

**Status**: Draft
**Last Updated**: 2025-11-21
**Related Code**: `src/zelda3/dungeon/dungeon_object_editor.h`, `src/zelda3/dungeon/dungeon_editor_system.h`

This document outlines the Undo/Redo architecture used in the Dungeon Editor.

## Overview

The system employs a command pattern approach where the state of the editor is snapshotted before destructive operations. Currently, there are two distinct undo stacks:

1.  **DungeonObjectEditor Stack**: Handles granular operations within the Object Editor (insert, move, delete, resize).
2.  **DungeonEditorSystem Stack**: (Planned/Partial) Intended for high-level operations and other modes (sprites, items), but currently delegates to the Object Editor for object operations.

## DungeonObjectEditor Implementation

The `DungeonObjectEditor` maintains its own local history of `UndoPoint`s.

### Data Structures

```cpp
struct UndoPoint {
  std::vector<RoomObject> objects;  // Snapshot of all objects in the room
  SelectionState selection;         // Snapshot of selection (indices, drag state)
  EditingState editing;             // Snapshot of editing mode/settings
  std::chrono::steady_clock::time_point timestamp;
};
```

### Workflow

1.  **Snapshot Creation**: Before any operation that modifies the room (Insert, Delete, Move, Resize), `CreateUndoPoint()` is called.
2.  **Snapshot Storage**: The current state (objects list, selection, mode) is copied into an `UndoPoint` and pushed to `undo_history_`.
3.  **Limit**: The history size is capped (currently 50) to limit memory usage.
4.  **Undo**:
    *   The current state is moved to `redo_history_`.
    *   The last `UndoPoint` is popped from `undo_history_`.
    *   `ApplyUndoPoint()` restores the `objects` vector and selection state to the room.
5.  **Redo**:
    *   Similar to Undo, but moves from `redo_history_` back to active state and `undo_history_`.

### Batch Operations

For batch operations (e.g., `BatchMoveObjects`, `PasteObjects`), a single `UndoPoint` is created before the loop that processes all items. This ensures that one "Undo" command reverts the entire batch operation.

## DungeonEditorSystem Role

The `DungeonEditorSystem` acts as a high-level coordinator.

*   **Delegation**: When `Undo()`/`Redo()` is called on the system while in `kObjects` mode, it forwards the call to `DungeonObjectEditor`.
*   **Future Expansion**: It has its own `undo_history_` structure intended to capture broader state (sprites, chests, entrances), but this is currently a TODO.

## Best Practices for Contributors

*   **Always call `CreateUndoPoint()`** before modifying the object list.
*   **Snapshot effectively**: The current implementation snapshots the *entire* object list. For very large rooms (which are rare in ALttP), this might be optimized in the future, but it's sufficient for now.
*   **State Consistency**: Ensure `UndoPoint` captures enough state to fully restore the context (e.g., selection). If you add new state variables that affect editing, add them to `UndoPoint`.
