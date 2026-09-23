# Overworld paint regression repair — 2026-09-23

## State

These bounded repairs follow the source/history audit of Tile16 painting,
selection, and map targeting at `55ada11d7`. Branch:
`codex/overworld-paint-regression-fixes`, based on `55ada11d7`.
The active Cursor Tile16 extraction is separate and was not copied into this
worktree. Qualify the combined source after integration.

Keep Tile16 domain rules in `overworld/tile16/tile16_edit_session.*`, layout in
`overworld/ui/tiles/tile16_workbench.cc`, and the façade in `tile16_editor.*`.
These repairs touch map texture synchronization, painting input, rectangular
brushes, clipboard/scratch transfer, and paint history. Refactoring ownership
does not establish behavior correctness.

The local source commits are:

- `bfcdad775`: texture synchronization and canvas gesture ownership (OW-R1/R2).
- `b9887bc91`: source organization, canonical includes, and CMake path migration.
- `c6a843cea`: captured rectangular brush and coordinate repairs (OW-R3/R7),
  including clipboard/scratch transfer and multi-map paint history.

The main checkout's uncommitted Cursor changes remain separate. These commits
have not been pushed or installed as the user's application.

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
3. **OW-R3: capture a brush independently of its source and preview.** A completed
   canvas-owned right-drag captures dimensions and Tile16 IDs once. Fill repeats
   that snapshot instead of reading destination tiles. Overlapping stamps cannot
   change it. Copy and scratch-space transfer read the same snapshot; cropping a
   wide source to the scratch workspace preserves the original row stride.
4. **OW-R7: share one world-coordinate destination contract.** Preview, stamp,
   and Paste use the same cursor anchor. Each changed cell resolves its physical
   map, so a rectangle can cross horizontal and vertical screen boundaries.
   Out-of-world cells are clipped without moving the anchor or wrapping. Special
   World stops at its allocated fourth map row. Modified bitmaps and refresh
   callbacks cover all changed maps; unchanged cells do not create history.
5. **Paint history: preserve all changed maps and earliest values.** Single-tile
   painting remains pending until release, including release outside the canvas.
   Each stamp, fill, or paste finalizes its edit; the existing timed merge window
   remains. Undo/redo derives the affected maps from the full recorded cell set,
   including merged actions, and marks each bitmap modified before refresh.
   Revisited coordinates keep the first old value and last new value.

## Source organization

The overworld root now contains six C++ files instead of 43. The mechanical
commit moves 37 files and updates their includes, CMake registration, and active
documentation paths. It also applies required formatting to touched files.
Function behavior is preserved in that commit; brush behavior is separate.

| Directory | Ownership |
| --- | --- |
| `canvas/` | Navigation, hit testing, zoom, and canvas rendering |
| `maps/` | Properties, metadata, content refresh, and texture coordination |
| `painting/` | Captured brush value, painting, Fill, and Paste |
| `entity/` | Existing entity domain plus relocated rendering/operations |
| `tile16/` | Tile16 action, shortcut, source-selection, and undo helpers |
| `ui/navigation/` | Sidebar and toolbar |
| `ui/tiles/` | Tile windows/selectors and scratch workspace |
| `ui/debug/` | Debug and usage-statistics content |

The editor facades, automation, and mixed-feature undo actions remain at the
root. Existing `panels/` wrappers and window contracts are unchanged. The
[overworld contributor guide](../../../src/app/editor/overworld/README.md)
records entry points, ownership rules, and bounded UI contributions.

When integrating Cursor's extraction, keep its session domain under `tile16/`
and its workbench layout under `ui/tiles/`. Resolve its CMake/include edits
against the new paths. Do not restore the deleted root files as duplicate
implementations or forwarding headers. Qualify the combined source before
claiming the extraction and painting repairs work together.

## Verification

### Current brush and organization increment

**99 tests across 14 suites passed, zero failures and zero skipped.** `yaze` and
`yaze_test_unit` built with `mac-ai`. The filter was enumerated before execution.
These are synthetic/headless results from this worktree, not native UI or ROM
round-trip acceptance.

```sh
cmake --build --preset mac-ai --target yaze yaze_test_unit --parallel 4
yaze_brush_filter='*TilePaintingManager*:OverworldTilePaintActionTest.*:OverworldPaintRefreshTest.*:MapRefreshCoordinatorTest.*:CanvasNavigationManagerTest.*:OverworldEditorStateTest.*:OverworldMapMetadataTest.*:OverworldPropertyEditTest.*:MapPropertiesContextMenuTest.*:Tile16EditorActionStateTest.*:Tile16EditorShortcutsTest.*:Tile8SourceInteractionTest.*'
build/presets/mac-ai/bin/Debug/yaze_test_unit --gtest_list_tests --gtest_filter="$yaze_brush_filter"
build/presets/mac-ai/bin/Debug/yaze_test_unit --gtest_filter="$yaze_brush_filter" --gtest_output=xml:/tmp/yaze-overworld-brush-tests.xml
git diff --check
```

The final incremental build used `--parallel 2` while one analyzer process ran.
Logs from this run: `/tmp/yaze-overworld-brush-final-build.log`,
`/tmp/yaze-overworld-brush-selected.log`, `/tmp/yaze-overworld-brush-tests.log`.

Coverage includes nonuniform Fill, overlapping capture/stamps, recapturing the
same coordinates, 0.5x/1x/2x preview/stamp alignment, four-screen intersections,
negative and outer-world clipping, Special World limits, stale selected-map
state during Paste, invalid clipboard payloads, gesture release, and multi-map
Undo/Redo. The preview case inspects real ImGui draw-command vertices and clip
rectangles using a tagged test texture; it does not execute a GPU backend.
Direct editor Copy/scratch UI flows still need integration acceptance.

Scoped `clang-analyzer-*` checks passed for three translation units with analyzer
warnings treated as errors. Two redundant status-label initializations were
removed. The command used the installed LLVM binary and a temporary analysis
database derived from this build's Debug commands:

```sh
/opt/homebrew/opt/llvm/bin/clang-tidy \
  -p /tmp/yaze-overworld-brush-analysis \
  --checks='-*,clang-analyzer-*' \
  --warnings-as-errors='clang-analyzer-*' \
  --header-filter='app/editor/overworld/.*' \
  src/app/editor/overworld/painting/tile_painting_manager.cc \
  src/app/editor/overworld/overworld_editor.cc \
  src/app/editor/overworld/ui/tiles/scratch_space.cc
```

For reproduction on another machine, use its LLVM executable and compile
database. This run stripped binary PCH flags from the three analysis commands,
retained the textual forced header, and supplied the SDK and libc++ include
paths returned by `xcrun --show-sdk-path`. It did not edit build configuration
or disable analyzer checks globally. Log: `/tmp/yaze-overworld-brush-analyzer.log`.
An excluded non-user-code warning was suppressed; this is not a whole-repository
clang-tidy result.

The path audit resolved all 37 moved destinations, 35 overworld CMake sources,
and 35 contributor-guide links. No deleted-path references remain in active
source, tests, CMake, or scripts. Commit hooks passed formatting, build
parallelism policy, and release/version checks. Editor guardrails passed for
the source increment:

```sh
/opt/homebrew/bin/bash scripts/dev/editor-guardrails.sh bfcdad775 c6a843cea
```

Use Bash 4 or later on other hosts; the script uses `mapfile`.

### Initial texture and gesture increment

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

1. **OW-R4: stable context-menu target.** Deferred entity insertion combines
   selected-map identity with hovered-map coordinates. Capture map identity,
   parent context, and position when opening the menu. Keep that target stable
   while the popup is open. The later local-master hover-clear change
   `621d15c69` can also make a popup fall back to the selected map; it is absent
   from this branch. Test selected A / popup B across several frames.
2. **OW-R5 / OW-R6: Tile16 publication and recovery.** Commit must invalidate
   affected map/atlas caches before publishing rebuilt graphics. Discard must
   restore all staged atlas regions. A four-definition stamp needs one undo
   snapshot covering all touched definitions, including pending state and
   presentation. Integrate with Cursor's new session/workbench boundary rather
   than recreating rules in the façade or layout.
3. **ROM and native UI qualification.** Verify Small/Large/Wide/Tall areas and
   Light/Dark/Special World with disposable ROMs. Check source/destination areas
   with different graphics and palettes, Copy and scratch-space transfer, then
   paint → Undo/Redo → disposable save → independent reopen. Synthetic geometry
   and same-atlas pixel tests do not establish destination palette fidelity or
   ROM serialization. Identify the packaged source before testing the app.
4. **Loading budget.** Profile after correctness fixes.
   The current draw path can materialize all 64 maps despite the eight-map
   model cache. Add visibility and work-budget constraints only with measured
   behavior. Keep loading/culling optimization separate from mutation and
   context-target fixes.

No installed-app, manual UX, emulator, WASM, remote CI, or release-readiness
claim follows from this bounded repair. No wholesale history revert is advised;
later changes also contain bounds and safety fixes.
