# Overworld paint regression repair — 2026-09-23

## State

These bounded repairs follow the source/history audit of Tile16 painting,
selection, and map targeting at `55ada11d7`. Branch:
`codex/overworld-paint-regression-fixes`, based on `55ada11d7`.
Cursor’s Tile16 session/workbench extraction is now selectively integrated in
this branch, with immediate document edits and shared Undo/Redo. The main
checkout consolidation is documented in [the Cursor integration record](cursor-overworld-consolidation-2026-09-23.md). Unrelated deletions remain outside these commits.

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
- `bb69ee4a9`: stable context-menu targets (OW-R4), menu organization, and
  parent-relative item/sprite insertion coordinates.

The reviewed Cursor UI changes are integrated in the follow-up consolidation.
These commits have not been pushed or installed as the user's application.

## Immediate Tile16 editing and shared history candidate

This increment selectively integrates Cursor's `tile16_editor.h/.cc`,
`tile16/tile16_edit_types.h`, `tile16/tile16_edit_session.h/.cc`, and
`ui/tiles/tile16_workbench.cc`, adapting their includes to the organized tree.
This initial increment excluded the floating window-manager changes. The
follow-up consolidation reviews and integrates those changes with session-safe
requests and real ImGui frame tests.

- Tile16 definitions publish to the document immediately. The normal panel has
  no Write Pending/Discard controls or tile-switch dialog. Manual quadrant
  properties use the same transaction as paint, palette, flip, clear, paste,
  and scratch recall.
- `tile16/tile16_edit_history.cc` owns complete before/after metadata batches.
  It uses the overworld editor's history, finalizes an open map stroke first,
  avoids history entries for no-op/invalid mutations, and clears Redo on edits.
  Four-definition stamps are atomic history steps; edge clipping retains the
  existing stamp policy and records every in-range definition.
- `MapRefreshCoordinator::InvalidateTile16Definitions` invalidates all worlds.
  The selected map refreshes immediately, other maps use deferred refresh.
  Undo regenerates previews from current graphics, not captured old pixels.
- Single and rectangular map strokes use release boundaries, with time-based
  merging disabled for production actions. Shared Undo shortcuts are handled
  once by the application. Standalone Tile16 test/tool sessions keep local
  history and legacy serialization adapters, which bound sessions reject.
- ImGui workbench errors accumulate until child/group/table scopes are closed.
  Three panel sources now include their logging dependency directly. The Clang
  analyzer's padding diagnostic was resolved by grouping session fields.

Validation: **120 unit tests across 16 suites and 19 synthetic Tile16 panel
cases passed; no failures or skips.** All three targets built:

```sh
cmake --build --preset mac-ai --target yaze yaze_test_unit yaze_test_integration --parallel 4
yaze_history_filter='Tile16DocumentHistoryTest.*:*TilePaintingManager*:OverworldTilePaintActionTest.*:OverworldPaintRefreshTest.*:MapRefreshCoordinatorTest.*:CanvasNavigationManagerTest.*:OverworldEditorStateTest.*:Tile16EditorActionStateTest.*:Tile16EditorShortcutsTest.*:Tile8SourceInteractionTest.*:MapPropertiesContextMenuTest.*:OverworldContextTargetTest.*:CanvasContextMenuOpenTest.*:CanvasContextMenuRoleTest.*'
build/presets/mac-ai/bin/Debug/yaze_test_unit --gtest_list_tests --gtest_filter="$yaze_history_filter"
build/presets/mac-ai/bin/Debug/yaze_test_unit --gtest_filter="$yaze_history_filter" --gtest_output=xml:/tmp/yaze-immediate-tests.xml
build/presets/mac-ai/bin/Debug/yaze_test_integration --gtest_filter='Tile16EditorSyntheticFixture.*' --gtest_list_tests
build/presets/mac-ai/bin/Debug/yaze_test_integration --gtest_filter='Tile16EditorSyntheticFixture.*' --gtest_output=xml:/tmp/yaze-immediate-ui.xml
```

The final rebuild used `--parallel 2` alongside one analyzer process. Scoped
`clang-analyzer-*` checks passed with warnings treated as errors for six units:
`tile16_edit_history.cc`, `tile16_edit_session.cc`, `tile16_workbench.cc`,
`tile16_editor.cc`, `map_refresh_coordinator.cc`, and `tile_painting_manager.cc`.
The temporary Debug compilation database is `/tmp/yaze-immediate-analysis`,
prepared with the SDK/PCH procedure described below. Logs use the
`/tmp/yaze-immediate-` prefix (`final-build.log`, `analyzer.log`, `tests.log`,
`ui.log`); XML evidence records selection and skip counts.

The new history tests cover definition/paint interleaving, normal in-memory
`SaveMap16Tiles` serialization followed by Undo/Redo, complete stamps, invalid
and no-op edits, shared Redo invalidation, manual properties, clipboard/scratch,
and rendering after a source-graphics change. These are synthetic ROM bytes,
not qualification of a personal ROM, expanded-format round trip, native GPU
rendering, or the installed Barista-launched app. No ROM files were modified.

Next acceptance: identify the packaged source, then use a disposable ROM for
edit → paint → Undo/Redo → Save → independent reopen, including map/palette
changes and narrow/wide layouts. The owner can defer this review; do not ask
for repeated manual tests while implementing the next bounded source task.

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

## Context-menu repair (OW-R4)

The main canvas now builds its menu only when it opens. The captured
`OverworldContextTarget` contains physical map, parent area, world-pixel
position, game state, and Tile16 ID. Menu callbacks capture it by value; the
live Entity Workbench consumes an `OverworldEntityInsertionRequest` containing
both type and target. It never substitutes the selected map or game state from
the next frame. Loading clears pending insertion and menu actions. The service
rejects invalid targets or changed parent mappings before mutation.

The obsolete `ProcessPendingEntityInsertion` implementation and loose mutable
type/position accessors were removed. Integrations must queue and consume the
typed request. Tile16 sampling/editing uses the captured ID through
`RequestTile16Selection`, preserving its pending-edit guard. Area Configuration,
background, and effects actions select the captured map before opening panels.

The visible hierarchy is: map/tile identity, Select/Pin, Sample/Edit Tile16,
Insert Entity, Map, Map Info, View. Existing metadata copy/paste, related-map
navigation, labels, and version-dependent controls remain available. The View
submenu owns zoom/reset, grid, and labels; shared built-ins are hidden to avoid
duplicates. Canvas snapshots now preserve configuration fields, including the
built-in visibility and canvas role. Reset View runs later in the canvas child
window; the renderer no longer forces Grid Size back to 64 every frame.

Item and sprite creation also had a parent-quadrant defect: child screen `0x09`
of parent `0x00` at world `(528,544)` produced game coordinates `(1,2)` instead
of `(33,34)`. Production value builders now validate screen/parent/bounds and
retain parent-relative coordinates before appending an entity. Item save/load
is tested through the real serializers in an in-memory synthetic ROM.

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

### Current context-menu increment

`yaze` and `yaze_test_unit` built successfully. **105 tests in 12 suites passed,
zero failures and zero skipped.** Tests cover value capture, deferred request
consumption, panel targeting, menu availability/hierarchy, zoom/scroll/world
coordinates, and the previous brush/navigation/cache regressions. Real ImGui
frames exercise capture-before-render, popup persistence, reopening, rejection,
outside clicks/right drags, and built-in menu visibility in rendered text.
The headless fixture enables keyboard navigation, matching the app, so Escape
dismisses the popup before the next-open test.

```sh
cmake --build --preset mac-ai --target yaze yaze_test_unit --parallel 4
yaze_context_filter='MapPropertiesContextMenuTest.*:OverworldContextTargetTest.*:CanvasContextMenuOpenTest.*:OverworldItemOperationsTest.*:CanvasContextMenuRoleTest.*:CanvasNavigationManagerTest.*:*TilePaintingManager*:OverworldTilePaintActionTest.*:OverworldPaintRefreshTest.*:MapRefreshCoordinatorTest.*'
build/presets/mac-ai/bin/Debug/yaze_test_unit --gtest_list_tests --gtest_filter="$yaze_context_filter"
build/presets/mac-ai/bin/Debug/yaze_test_unit --gtest_filter="$yaze_context_filter" --gtest_output=xml:/tmp/yaze-overworld-context-tests.xml
/opt/homebrew/bin/bash scripts/dev/editor-guardrails.sh ddc2fb07c bb69ee4a9
git diff --check
```

Logs: `/tmp/yaze-overworld-context-build.log`,
`/tmp/yaze-overworld-context-final-build.log`,
`/tmp/yaze-overworld-context-selected.log`,
`/tmp/yaze-overworld-context-tests.log`.

The following scoped analyzer command passed using a Debug compilation database
prepared as described in the brush section below:

```sh
/opt/homebrew/opt/llvm/bin/clang-tidy \
  -p /tmp/yaze-overworld-context-analysis \
  --checks='-*,clang-analyzer-*' \
  --warnings-as-errors='clang-analyzer-*' \
  --header-filter='app/(editor/overworld|gui/canvas)/.*' \
  src/app/editor/overworld/canvas/overworld_context_actions.cc \
  src/app/editor/overworld/entity/entity_operations.cc \
  src/app/editor/overworld/entity/entity_workbench.cc \
  src/app/editor/overworld/maps/map_properties.cc \
  src/app/gui/canvas/canvas_context_menu.cc
```

Two excluded non-user-code warnings were suppressed. Log:
`/tmp/yaze-overworld-context-analyzer.log`. This does not establish a whole-repo
clang-tidy result, native UI acceptance, or compatibility with Cursor's dirty
combined candidate. No personal ROM/save files were modified.

### Earlier brush and organization increment

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

1. **OW-R5 / OW-R6 implemented: qualify the immediate-edit candidate.** The
   requested UX replaces staging/commit/discard with document edits and shared
   Undo/Redo. The candidate imports Cursor's session/workbench boundary, adds
   complete metadata-batch history, and invalidates caches across all worlds.
   Follow [the document contract](../../../src/app/editor/overworld/README.md#tile16-document-contract).
   Native/palette/ROM qualification remains distinct from synthetic evidence.
2. **Overworld sprite persistence implemented.** The follow-up connects both
   save paths to a bounded, atomic serializer with independent sprite tests.
   See [the sprite persistence handoff](overworld-sprite-persistence-2026-09-23.md).
   Native UI and ROM-file checks remain deferred in the manual checklist.
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
