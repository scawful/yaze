# Interaction Refactor - Next Steps

**Date**: 2026-02-08
**Status**: Phase 5 verified; Phase 6 partially complete (marquee + drag robustness)
**Priority**: High

This file is a quick entrypoint. The canonical handoff doc is:
- `docs/internal/hand-off/dungeon_interaction_refactoring_handoff.md`

## Current State (As Of 2026-02-08)

- Tile-object drag/release is owned by `TileObjectHandler` (snapped, live updates).
- Marquee/rectangle selection for tile objects is owned by `TileObjectHandler`:
  - `BeginMarqueeSelection()`
  - `HandleMarqueeSelection()`
- `DungeonObjectInteraction` now dispatches drag updates every frame (doors/sprites/items drag works) and delegates marquee selection to the tile handler.

## Verification

```bash
cmake --build build_ai --target yaze_test_unit yaze_test_integration -j8

./build_ai/bin/Debug/yaze_test_unit "*TileObjectHandler*"
./build_ai/bin/Debug/yaze_test_unit "*InteractionCoordinator*"
./build_ai/bin/Debug/yaze_test_integration "*InteractionDelegation*"
./build_ai/bin/Debug/yaze_test_integration --gtest_filter="DungeonEditorV2IntegrationTest*"
```

If you see an `exit code 130` during build, it generally indicates a SIGINT
(manual interrupt) or a transient resource issue. Retrying with lower
parallelism (e.g. `-j2`) or building targets incrementally can help isolate it.

## Remaining Work (Phase 6)

1. Consolidate mode/state:
   - `InteractionModeManager` still contains unused modes/state (`RectangleSelect`, `DraggingEntity`, etc).
   - `InteractionCoordinator::Mode` is largely redundant with handler state.
   - Pick a single source of truth and delete the rest.

2. Continue OoS readiness work:
   - Dungeon object rendering parity audits and validation against ZScream/HM baselines.

## Reference Files

- `src/app/editor/dungeon/dungeon_object_interaction.cc`
- `src/app/editor/dungeon/interaction/interaction_coordinator.cc`
- `src/app/editor/dungeon/interaction/tile_object_handler.cc`
- `test/unit/editor/tile_object_handler_test.cc`
