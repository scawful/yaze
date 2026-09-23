# Overworld paint regression repair — 2026-09-23

## State

This bounded repair follows the source/history audit of Tile16 painting,
selection, and map targeting at `55ada11d7`. Branch:
`codex/overworld-paint-regression-fixes`, based on `55ada11d7`.
The active Cursor Tile16 extraction is separate and was not copied into this
worktree. Qualify the combined source after integration.

Keep Tile16 domain rules in `overworld/tile16/tile16_edit_session.*`, layout in
`overworld/ui/tiles/tile16_workbench.cc`, and the façade in `tile16_editor.*`.
These repairs touch map texture synchronization and painting input, outside
that extraction. Refactoring ownership does not establish behavior correctness.

## Repairs

1. **OW-R1: preserve edited pixels until content refresh.** `EnsureMapTexture`
   must not copy a built map's stale pixel cache over a modified editor bitmap
   or consume its dirty flag. Only a newly built model supplies replacement
   pixels. The existing map refresh rebuilds from authored Tile16 IDs and then
   clears the flag. The regression originated in `bed6ff7a9` (2026-04-26).
2. **OW-R2: require a canvas-owned paint gesture.** A left press must begin on
   the hovered, active canvas item. Painting pauses outside the canvas and can
   resume when that gesture returns; a drag begun elsewhere cannot paint on
   entry. Single tiles, rectangle stamps, and Fill share this requirement.
   Hover previews remain independent of mutation permission. The missing
   rectangle guard already existed in the `3d71417f6` snapshot (2025-10-17);
   do not attribute it to Cursor's extraction.

## Verification

**46 tests across five suites passed, zero failures and zero skipped.** Both
`yaze` and `yaze_test_unit` built successfully with the `mac-ai` preset. Before
the production repairs, all 11 new cases were selected: six failed (four
rectangle ownership cases and two cache cases), while the valid-paint and
normal-rebuild controls passed. Restoring both repairs makes all 11 pass.

```sh
cmake --preset mac-ai
cmake --build --preset mac-ai --target yaze yaze_test_unit --parallel 4
yaze_paint_filter='OverworldPaintRefreshTest.*:MapRefreshCoordinatorTest.*:TilePaintingManagerTest.*:TilePaintingManagerGestureTest.*:CanvasNavigationManagerTest.*'
build/presets/mac-ai/bin/Debug/yaze_test_unit --gtest_list_tests --gtest_filter="$yaze_paint_filter"
build/presets/mac-ai/bin/Debug/yaze_test_unit --gtest_filter="$yaze_paint_filter" --gtest_output=xml:/tmp/yaze-overworld-paint-tests.xml
git diff --check
```

Build log: `/tmp/yaze-overworld-paint-fixed-build.log`; selection/result logs:
`/tmp/yaze-overworld-paint-selected.log`, `/tmp/yaze-overworld-paint-tests.log`.
Before-fix selection/result/XML artifacts use the prefix
`/tmp/yaze-overworld-paint-red`. The build is in this isolated worktree;
the installed app and Cursor's combined candidate have not been qualified.

The new cache fixture uses a real built `OverworldMap`, authored tile arrays,
editor bitmap, and `MapRefreshCoordinator`. It checks preservation before
refresh, rebuilt pixels afterward, retained content invalidation, and legitimate
synchronization of a newly built model. It does not inspect a GPU upload.

The new input fixture delivers actual ImGui mouse events through
`Canvas::DrawBackground` and captures a rectangle with right-drag input. It
checks world IDs, CPU bitmap pixels, ROM dirty state, and undo callbacks for
outside clicks/drags, outside-to-inside drags, valid painting, leaving/reentering,
release outside, and Fill. Its ROM/world data is synthetic; no personal ROM or
save is written.

## Remaining work, in order

1. **OW-R3 / OW-R7: immutable rectangular brush and coordinate contract.**
   Fill currently samples the destination instead of the captured brush.
   Re-reading source tiles each frame lets overlapping stamps alter the brush.
   Rectangle clamping mixes zoomed and world coordinates, and a blanket 512px
   seam rejection blocks legitimate multi-screen areas. Capture tile IDs and
   dimensions once; use world coordinates for preview and destination writes.
   Test nonuniform patterns, overlap, edges, and Small/Large/Wide/Tall areas at
   0.5x, 1x, and 2x. Do not hide these issues with more input guards.
2. **OW-R4: stable context-menu target.** Deferred entity insertion combines
   selected-map identity with hovered-map coordinates. Capture map identity,
   parent context, and position when opening the menu. Keep that target stable
   while the popup is open. The later local-master hover-clear change
   `621d15c69` can also make a popup fall back to the selected map; it is absent
   from this branch. Test selected A / popup B across several frames.
3. **OW-R5 / OW-R6: Tile16 publication and recovery.** Commit must invalidate
   affected map/atlas caches before publishing rebuilt graphics. Discard must
   restore all staged atlas regions. A four-definition stamp needs one undo
   snapshot covering all touched definitions, including pending state and
   presentation. Integrate with Cursor's new session/workbench boundary rather
   than recreating rules in the façade or layout.
4. **Loading budget and final integration.** Profile after correctness fixes.
   The current draw path can materialize all 64 maps despite the eight-map
   model cache. Add visibility and work-budget constraints only with measured
   behavior. Qualify paint → Undo/Redo → disposable save → independent reopen,
   then manual canvas feedback and packaged source identity.

No installed-app, manual UX, emulator, WASM, remote CI, or release-readiness
claim follows from this bounded repair. No wholesale history revert is advised;
later changes also contain bounds and safety fixes.
