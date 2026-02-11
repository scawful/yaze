# Undo/Redo System Architecture

**Status**: Active
**Last Updated**: 2026-02-10
**Related Code**: `src/app/editor/undo/`, `src/app/editor/editor.h`

## Overview

The undo/redo system uses a unified `UndoManager` embedded in the `Editor` base class. Each editor inherits automatic undo/redo support through `Editor::Undo()` and `Editor::Redo()`, which delegate to the manager. Editors push `UndoAction` subclasses onto the stack to capture reversible operations.

## Core Components

### UndoManager (`src/app/editor/undo/undo_manager.h`)

Owns two stacks: `undo_stack_` and `redo_stack_`. Each entry is a `std::unique_ptr<UndoAction>`.

Key operations:
- `Push(action)` — adds an action to the undo stack, clears redo stack.
- `Undo()` — pops from undo, calls `action->Undo()`, pushes to redo.
- `Redo()` — pops from redo, calls `action->Redo()`, pushes to undo.
- `CanUndo()` / `CanRedo()` — stack emptiness checks.

### UndoAction (abstract base)

```cpp
class UndoAction {
 public:
  virtual ~UndoAction() = default;
  virtual void Undo() = 0;
  virtual void Redo() = 0;
  virtual std::string GetDescription() const = 0;
};
```

### Editor Integration

`Editor` base class (`src/app/editor/editor.h`) owns an `UndoManager`. Virtual methods `Undo()` and `Redo()` delegate to it. The menu system dispatches Ctrl+Z/Y through `Editor::Undo()/Redo()` and `MenuOrchestrator::OnUndo()/OnRedo()` (which shows toasts on failure).

## Per-Editor Actions

| Editor | Action Classes | What Gets Snapshotted |
|--------|---------------|----------------------|
| Overworld | Paint actions via `PushUndoState()` | Tile map state |
| Dungeon | `DungeonCustomCollisionAction`, `DungeonWaterFillAction`, object move/insert/delete | Collision grid, water fill zones, object list |
| Graphics | `GraphicsEditorState` snapshot stack | Pixel data per sheet |
| Music | `PushUndoState(song_index)` | Song data at explicit index |
| Message | History via UndoManager | Message text buffer |

### Dungeon Mutation Domains

Dungeon editor uses `MutationDomain` to tag which subsystem changed:
- `kTileObjects` — room objects
- `kCustomCollision` — collision grid painting
- `kWaterFill` — water fill zone painting

This allows domain-specific snapshot/finalize pairs:
- `BeginCollisionUndoSnapshot()` / `FinalizeCollisionUndoAction()`
- `BeginWaterFillUndoSnapshot()` / `FinalizeWaterFillUndoAction()`

### Graphics Editor

`GraphicsEditorState` maintains its own snapshot-based stack internally. The top-level `GraphicsEditor::Undo()/Redo()` delegates to `PixelEditorPanel` which handles it with buttons and keyboard shortcuts.

## Dispatch Flow

```
User presses Ctrl+Z
  → MenuOrchestrator::OnUndo()
    → active_editor->Undo()
      → undo_manager_.Undo()
        → action->Undo()  (restores state)
    → if failed: ToastManager shows error
```

## Adding Undo to a New Editor

1. Create an `UndoAction` subclass that captures before/after state.
2. Call `undo_manager().Push(std::make_unique<MyAction>(...))` before the destructive operation.
3. Implement `Undo()` to restore the before-state, `Redo()` to reapply the after-state.
4. The base class `Editor::Undo()/Redo()` handles the rest.
