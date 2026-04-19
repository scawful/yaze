Status: active
Owner (Agent ID): imgui-frontend-engineer
Created: 2026-04-19
Last Reviewed: 2026-04-19
Next Review: 2026-05-03
Coordination: local implementation session; no universe task ID assigned

Agent handoff (IN_PROGRESS, validation log, follow-ups):
[overworld-tile16-ui-migration-handoff-2026-04-19.md](../agents/overworld-tile16-ui-migration-handoff-2026-04-19.md)

# Overworld Tile16 Editor Fix Plan

## Summary

The overworld Tile16 editing stack currently has two architectural faults:

1. Tile selection is split across the overworld editor, selector panel, eyedropper, and Tile16 editor, so some paths bypass staged-change protection and some do not.
2. Tile16 commits write ROM data but do not reliably refresh the authoritative in-memory overworld Tile16 definitions and derived blockset/map surfaces, so the UI can remain stale after a successful save.

This plan documents the bugs found in review and the staged implementation needed to make Tile16 editing predictable, synchronized, and testable.

## Findings

### Correctness Bugs

- Tile16 commits can succeed in ROM while the overworld blockset and map view still render old data.
- `MapRefreshCoordinator::RefreshTile16Blockset()` was gated on blockset ID changes, so tile-definition changes with the same blockset ID often no-op'd.
- `SetCurrentTile()` preferred bitmap data from the shared blockset/atlas, which made stale preview surfaces a source of truth for the editor.
- Tile switching through the overworld selector, drag/drop, and eyedropper bypassed `RequestTileSwitch()` and therefore bypassed staged-change protection.
- The embedded Tile16 editor state and overworld paint selection state could drift because they were maintained independently.
- Palette refreshes updated the current map view but not every Tile16-bound surface (`tile16_blockset_bmp_`, atlas preview, current editor bitmap).

### UI / UX Issues

- Tile switching behavior was inconsistent depending on which surface the user clicked.
- The bottom action rail exposed `Undo` but not `Redo`, even though redo existed in code and keyboard shortcuts.
- Advanced palette controls included dead or misleading preview toggles that did not affect behavior.
- Unsaved-change confirmation UI was duplicated and had already drifted from the real discard path.

## Root Causes

- The overworld editor treated ROM writes, atlas preview updates, and in-memory overworld Tile16 definitions as separate systems without a single authoritative refresh contract.
- Interactive tile selection had no shared owner; different surfaces wrote directly into different state variables.
- Refresh code conflated "area blockset changed" with "Tile16 definitions changed", which are related but not identical invalidation events.

## Implementation Stages

### Stage 1: Shared Selection Contract

- Add one overworld-owned Tile16 selection API and route selector, eyedropper, drag/drop, and tile cycling through it.
- Keep `OverworldEditor::current_tile16_` synchronized from `Tile16Editor::SetCurrentTile()` via a callback rather than duplicated writes.
- Ensure staged Tile16 edits always trigger the same guard behavior before a tile switch is accepted.

### Stage 2: Authoritative Commit Refresh

- Make `CommitAllChanges()` hand the committed Tile16 definitions back to `OverworldEditor`.
- Update `Overworld::tiles16_` immediately after commit.
- Rebuild the active Tile16 blockset from `Overworld::tiles16_` and refresh visible overworld maps from that rebuilt data.
- Stop relying on existing atlas contents as the editor's reload source.

### Stage 3: Palette / Presentation Sync

- Refresh `tile16_blockset_bmp_`, the tilemap atlas, and the current Tile16 editor bitmap whenever the active overworld palette changes.
- Keep the Tile8 source surface and Tile16 preview surfaces using the same current palette contract.

### Stage 4: UI Cleanup

- Remove dead palette preview toggles or convert them into explanatory text.
- Expose `Redo` beside `Undo` in the primary action rail.
- Consolidate the unsaved-change dialog into one code path that calls the real discard helper.

### Stage 5: UI Module Migration Slice

- Keep the typed overworld window-context helper in `ui/shared/` as the single
  typed access path for overworld `WindowContent` surfaces.
- Migrate the main canvas shell into `ui/canvas/overworld_canvas_view`.
- Migrate the Tile8/Tile16 selector and Tile16 editor shells into `ui/tiles/`.
- Migrate the remaining thin wrappers into feature modules under `ui/graphics`, `ui/properties`, `ui/items`, `ui/workspace`, `ui/analytics`, and `ui/debug`.
- Keep panel IDs and behavior unchanged while moving active overworld window surfaces fully onto the `ui/` layout.

### Stage 6: Coverage

- Add integration coverage for:
  - selection synchronization between the overworld editor and Tile16 editor
  - staged-change guard enforcement on non-editor tile switch paths
  - commit -> in-memory overworld Tile16 update -> blockset refresh
  - callback payloads for committed Tile16 changes

## Exit Criteria

- Any Tile16 selection change uses the same guarded path.
- After a Tile16 commit, overworld blockset/map views and the editor all show the same committed data without requiring a reload.
- Palette changes recolor all Tile16 editing and selection surfaces consistently.
- No dead Tile16 palette toggles remain visible.
- Tests cover the selection-sync and commit-refresh regressions.

## Validation

- Build with `cmake --preset mac-ai-fast && cmake --build --preset mac-ai-fast`
- Run focused tests with:
  - `ctest --preset mac-ai-unit -R 'Tile16Editor|OverworldEditor|Tile16ActionState' --output-on-failure`
