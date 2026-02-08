# Dungeon Interaction Refactor - Handoff

**Date**: 2026-02-08  
**Status**: Core refactor landed; remaining consolidation + parity work  
**Owner**: TBD  

## Executive Summary

The dungeon canvas interaction stack has been refactored into a delegated
handler architecture:

- `DungeonObjectInteraction` remains the canvas-facing facade (selection modes,
  drag state, and top-level input routing).
- `InteractionCoordinator` owns doors/sprites/items selection + placement and
  coordinates the entity handlers.
- `TileObjectHandler` owns tile-object mutations (move/dup/delete/resize,
  clipboard, placement ghost).
- Undo snapshots + cache invalidation are unified via `InteractionContext`
  callbacks.

Tile-object dragging is now **snapped and live-updated** (8px / 1-tile
increments) to reduce the “slide-y” feel.

## Current Architecture (As Of 2026-02-08)

```
DungeonObjectInteraction (~721 loc)
├── InteractionCoordinator
│   ├── DoorInteractionHandler
│   ├── SpriteInteractionHandler
│   ├── ItemInteractionHandler
│   └── TileObjectHandler (placement + mutations)
├── ObjectSelection (multi-select + marquee for tile objects)
└── InteractionModeManager (canvas interaction modes)
```

Key files:

- `src/app/editor/dungeon/dungeon_object_interaction.{h,cc}`
- `src/app/editor/dungeon/interaction/interaction_context.h`
- `src/app/editor/dungeon/interaction/interaction_coordinator.{h,cc}`
- `src/app/editor/dungeon/interaction/tile_object_handler.{h,cc}`
- `src/app/editor/dungeon/dungeon_snapping.h`

## What Was Added / Changed

- `InteractionContext` as the shared dependency + callback bundle for handlers.
- `InteractionCoordinator` as the single router for doors/sprites/items (and
  tile placement).
- `TileObjectHandler` for tile-object operations + clipboard + placement ghost.
- Live snapped dragging for tile objects with a single undo snapshot per drag.
- Workbench UI tweaks for compact icon buttons to avoid glyph clipping.
- Test coverage:
  - Unit tests for `TileObjectHandler` and `InteractionCoordinator`.
  - Integration test for delegation wiring through `DungeonObjectInteraction`.

### Phase 5: Handoff to Drag/Release logic (DONE)
- Migrate `UpdateObjectDragging` to `TileObjectHandler::HandleDrag` (Done).
- Migrate `HandleMouseRelease` to `TileObjectHandler::HandleRelease` (Done).
- Delegate drag events in `DungeonObjectInteraction::HandleCanvasMouseInput`.
- Update `InteractionCoordinator` to dispatch drag events to `TileObjectHandler`.

### Next Steps (Immediate)
1.  **Selection + Mode Consolidation (Phase 6)**:
    - DONE: Marquee/rectangle selection for tile objects now lives in `TileObjectHandler` (see `BeginMarqueeSelection` / `HandleMarqueeSelection`).
    - TODO: Consolidate (or delete) redundant mode state between `InteractionModeManager` and `InteractionCoordinator`.
2.  **Finish Interaction Cleanup**:
    - Remove unused/duplicated interaction modes and state (e.g. stale `RectangleSelect` / `DraggingEntity` scaffolding).
    - Ensure canvas interactions continue when the cursor leaves bounds mid-drag (release should never leave handlers “stuck”).
3.  **Continue UX Parity Audits** (Oracle-of-Secrets readiness): fix mis-rendered objects and validate against ZScream/HM baselines.

## Known Limitations / Follow-Ups

### 1) Mode Consolidation

There are currently two mode concepts:

- `InteractionModeManager` (canvas-level, in `DungeonObjectInteraction`)
- `InteractionCoordinator::Mode` (entity placement modes)

Follow-up options:

- Keep the split (canvas vs entities) but make the boundary explicit in docs.
- Or consolidate into coordinator-owned state and keep DOI as a thin facade.

### 2) Selection Model Unification

Tile objects use `ObjectSelection` (multi-select + marquee), while
doors/sprites/items selection is owned by their handlers via the coordinator.

Follow-up:

- Define and document a single “current selection” concept (tile multi-select +
  single entity selection) and make the exclusivity rules explicit.

### 3) Move Drag State Into Handler (Completed)

Tile-object drag state has been moved into `TileObjectHandler` (members `is_dragging_`, `drag_start_`, etc.).
`DungeonObjectInteraction` now delegates drag initialization and updates.

### 4) UX Parity Audits

See:

- `docs/internal/agents/dungeon-object-ux-parity-matrix.md`
- `docs/internal/agents/zscream-capture-log.md`

## Build / Test

```bash
# Build stable tests
cmake --build build_ai --target yaze_test_stable -j8

# Run interaction tests
./build_ai/bin/Debug/yaze_test_stable "*TileObjectHandler*"
./build_ai/bin/Debug/yaze_test_stable "*InteractionCoordinator*"
./build_ai/bin/Debug/yaze_test_stable "*InteractionDelegation*"
```

If you see an `exit code 130` during build, it generally indicates a SIGINT
(manual interrupt) or a transient resource issue. Retrying with lower
parallelism (e.g. `-j2`) or building targets incrementally can help isolate it.
