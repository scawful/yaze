# Dungeon Workbench placement and object inspector handoff

## Context

The next 0.8.0 editing slice connects the object browser, pending preview, and
selected-object inspector. The user requested substantial implementation work
from Codex and delegated extended validation to Claude. Avoid asking the user
to repeat a sequence of manual tests before continuing development.

## Current State

- Branch: `codex/dungeon-placement-feedback`.
- Worktree: `/Users/scawful/src/hobby/yaze-worktrees/pr256-review-fixes`.
- Parent commit: `7585a9e27` (macOS Shift-wheel routing and placement regressions).
- This branch includes PR #256's reviewed repair base `0b6ecdaf3`.
- The Workbench object browser now shows a live placement inspector. It exposes
  physical dimensions, stream/layer, and **Keep placing copies** (default on).
  Turning that off places once and returns to the inserted object's properties.
- Each successful placement selects the inserted object. Repeat mode keeps the
  preview and browser active; **Done placing** or Escape exposes the selection.
- **Place another** copies a selected object's ID, size, and layer into a new
  preview. Preview edits do not alter room data or create undo entries.
- Selection actions include Duplicate, Delete, To front, and To back. Delete
  uses the interaction facade to clear selection; property values are copied
  before drawing because stream changes can replace the object vector.
- Ordinary wheel grows/shrinks both encoded axes of packed area objects in
  lockstep, stopping when either reaches a bound. Shift-wheel changes width.
  Width/Height fields remain independent. Newly chosen floor regions start
  square (`0x05`); platforms with asymmetric borders retain those borders.
- Walls and trim use measured Length choices (tiles/pixels), sorted by physical
  span. Fixed and custom objects retain fixed-size or named-variant controls.
- Embedded browser suppresses the redundant placement summary and keeps a
  minimum grid height inside the scrollable inspector.

## Verified

- macOS app and unit-test targets build successfully using the command below.
- 43 focused tests across 7 suites pass, with no skipped tests. They cover the
  ImGui placement-to-selection transition, repeat stamping, preview controls,
  custom/fixed semantics, uniform bounds, macOS Shift-wheel, and actual composite
  pixel changes plus texture upload after Workbench placement.
- The two new inspector tests draw and click controls at 272 px width and check
  placement policy, retained preview properties, selection, and ImGui stacks.
- An independent integration review caught and resolved the stream-change
  reference lifetime, delete-selection cleanup, and redundant global-room
  placement callback issues before delivery.
- `git diff --check` passes. No ROM files were edited during this work.

## Open Risks

- Source-level and synthetic ImGui checks do not establish ROM save/reopen or
  game-render parity. These remain candidate validation, not claimed results.
- This branch has not replaced `/Applications/yaze.app`. Barista's Zelda menu
  launches that installed app. Do not close the user's active app or overwrite
  their unsaved ROM session to test this candidate.
- Check a small inspector at normal and increased UI scale: placement controls
  should scroll while the browser grid retains a usable height.
- The legacy direct placement callback still has its existing editor-global
  room context. Coordinator mouse placement does not invoke it; its mutation
  and rendering notifications use the target room's existing handler path.
- No remote CI, merge, alert dismissal, or release claim is made for this slice.

## Next Step

Validate this exact branch in an isolated candidate, using disposable copies of
vanilla and Oracle base ROMs. Check one representative object from each family:
wall/trim, floor area, fixed object, Oracle custom variant, and special-table
torch/block. Exercise place once/repeat, preview resize/layer, Place another,
selection edit, undo/redo, room switching, and save/reopen. Report new failures
with object ID, room, build commit, and exact action; fix them on a follow-up
branch. Keep parity-gate work independent until both changes are reviewed.

## Useful Commands

Run from the worktree above:

```sh
cmake --build --preset mac-ai --target yaze_test_unit yaze --parallel 4
```

Focused discovery and execution:

```sh
yaze_inspector_filter='TileObjectHandlerTest.*Placement*:TileObjectHandlerTest.Place*:TileObjectHandlerTest.BlockedPlaceOnce*:TileObjectHandlerTest.*Wheel*:DungeonWorkbenchPlacementUiTest.*:DungeonWorkbenchObjectSizeUiTest.*:DungeonWorkbenchSizeDescriptionTest.*:DungeonWorkbenchToolbarTest.WallControlsExposeLengthAndFootprint:DungeonObjectSelectorSizeTest.*:DungeonEditorPaletteRefreshTest.WorkbenchPlacementChangesCompositePixelsAndUploadsExistingTexture'
build/presets/mac-ai/bin/Debug/yaze_test_unit --gtest_list_tests --gtest_filter="$yaze_inspector_filter"
build/presets/mac-ai/bin/Debug/yaze_test_unit --gtest_filter="$yaze_inspector_filter" --gtest_output=xml:/tmp/yaze-workbench-inspector-tests.xml
```

Evidence files:
- `/tmp/yaze-workbench-inspector-build.log`
- `/tmp/yaze-workbench-inspector-selected-tests.txt`
- `/tmp/yaze-workbench-inspector-tests.log`
- `/tmp/yaze-workbench-inspector-tests.xml`

Use `YAZE_PREPUSH_BUILD_DIR=build/presets/mac-ai` for repository pre-push checks
if publication is authorized. Main checkout and Claude's `parity-gate` worktree
contain independent work and must remain intact.
