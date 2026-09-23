# Dungeon Workbench placement and object inspector handoff

## Context

The next 0.8.0 editing slice connects the object browser, pending preview, and
selected-object inspector. The user requested substantial implementation work
from Codex and delegated extended validation to Claude. Avoid asking the user
to repeat a sequence of manual tests before continuing development.

## Current room authoring candidate (2026-09-22)

The integration branch now extends the placement/entity work with room metadata
and existing chest-content editing at `aeb0b1200`, following `a893d0ef5`.
App/unit builds and 475 tests across 36 suites passed, with zero failures/skips.
Scoped Clang analyzer checks passed for the two new mutation modules. Details
and qualification boundaries are recorded in the
[capability plan](../plans/editor-capability-parity-plan.md#room-metadata-and-existing-chest-content-increment-2026-09-22).

- Workbench Room Properties uses named BG2/effect/collision/tag choices and
  validated deferred fields. Standalone properties and Room Tags share its undo
  boundary. Destinations include the pit and four staircase room/plane slots.
- Metadata snapshots preserve the hidden BG2 mode in dark rooms, distinguish
  room-header and object-stream-header save state, and refresh dependent views
  without switching the current room. Invalid and unchanged edits add no history.
- The shared Chest contents section edits existing rewards and normal/big
  record types. Its searchable receipt-ID labels are independent of the generic
  inventory-name table. Search remains visible above a scrolling results list.
  Unknown hack IDs and project labels are preserved. These controls do not
  create/delete chest objects or synchronize object and record types.
- Connected-view Clear stale validates the complete metadata batch before
  changing any room and contributes one undo entry for the whole operation.
- The next implementation package is compound chest creation/deletion with
  synchronized object/contents order and capacity preflight, followed by the
  remaining mixed-domain operations. Do not infer complete DA-1 or DA-2 from
  existing-record editing or the single-domain metadata batch.

Review the built candidate tomorrow when convenient. Do not install it over
`/Applications/yaze.app` or interrupt an active ROM session as part of automated
validation. Synthetic persistence checks are useful evidence; vanilla/Oracle
application save transactions, game behavior, packaging, and CI remain separate
qualification work. The older placement harness evidence below does not qualify
this new increment.

### Room authoring verification commands

Run from the integration worktree. The negative filter keeps the declared
Focused run independent of optional ROM fixtures; qualify those separately.

```sh
cmake --build build/presets/mac-ai --target yaze_test_unit yaze --parallel 4
yaze_room_authoring_filter='*DungeonRoomMetadata*:*DungeonRoomEditsLifecycle*:*DungeonChestEditor*'
yaze_room_authoring_filter+=':*DungeonEntityUndoLifecycleTest*:*DungeonUndoActionsTest*:*DungeonWorkbench*'
yaze_room_authoring_filter+=':*InteractionCoordinatorTest*:*SpriteInteractionHandlerTest*:*DoorInteractionHandlerTest*:*ItemInteractionHandlerTest*'
yaze_room_authoring_filter+=':*DungeonSelectionSnapshot*:TileObjectHandlerTest.*:DungeonCanvasViewerNavigationTest.*'
yaze_room_authoring_filter+=':DungeonEditorV2RomSafetyTest.*:DungeonSaveTest.*Chest*:*RoomHeader*'
yaze_room_authoring_filter+='-*RoomObjectRomParityTest*'
build/presets/mac-ai/bin/Debug/yaze_test_unit --gtest_list_tests --gtest_filter="$yaze_room_authoring_filter"
build/presets/mac-ai/bin/Debug/yaze_test_unit --gtest_filter="$yaze_room_authoring_filter" --gtest_output=xml:/tmp/yaze-room-authoring-verified-tests.xml
```

Scoped static analysis uses the configured PCH-free analysis database:

```sh
cmake --preset mac-ai -B build/analysis/mac-ai -G Ninja -DCMAKE_DISABLE_PRECOMPILE_HEADERS=ON -DYAZE_ENABLE_CLANG_TIDY=OFF
clang-tidy -p build/analysis/mac-ai --checks='-*,clang-analyzer-*' --warnings-as-errors='clang-analyzer-*' --header-filter='(dungeon_room_edit|dungeon_editor_v2_room_edits)\.(cc|h)$' src/app/editor/dungeon/dungeon_room_edit.cc src/app/editor/dungeon/dungeon_editor_v2_room_edits.cc
```

This analyzer result covers two named translation units and their selected
header diagnostics. It is not a full-repository clang-tidy pass. Existing full
checker/PCH limitations remain documented in the capability plan. Relevant logs
are `/tmp/yaze-room-authoring-commit-build.log`,
`/tmp/yaze-room-authoring-verified-tests.log`,
`/tmp/yaze-room-authoring-verified-tests.xml`, and
`/tmp/yaze-room-authoring-final-analyzer.log`.

## Placement baseline

- Validated placement commit: `7ba7d76ce`, originally on
  `codex/dungeon-placement-feedback`. The integration worktree now uses
  `codex/editor-parity-dungeon-authoring`; the
  [capability plan](../plans/editor-capability-parity-plan.md) tracks later work.
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

## Original focused verification

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

## Remaining qualification

Claude completed the bounded validation recorded below. Qualify the current
integrated candidate through the application save transaction with project
dependencies attached; preserve source inputs and compare declared ROM domains
after an independent reopen. Retain the wheel/Shift-wheel, Escape, larger-scale
inspector, and game-runtime gaps. Block placement was not covered by the retained
round-trip harness. Keep parity-gate work independent until reviewed.

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

## Independent validation review (2026-09-22)

Claude's validation worktree is `yaze-worktrees/validate-placement`, at
`335fdf446`, based on the older placement candidate `7ba7d76ce`. Its local
`test/CMakeLists.txt`, `dungeon_editor_palette_refresh_test.cc`, and
`validation_room_readback_test.cc` harness changes remain uncommitted and were
not imported into the integration branch.

The room-switch fix from `335fdf446` was reviewed and cherry-picked as
`6e9d52e8c`. It clears index-based selections when the room or store changes and
binds cross-room navigation before selecting the destination object/door. Review
also identified unfinished marquee, tile-drag, and paint-stroke state that must
end against the old room before rebinding; the capability plan records the
integration follow-up `50d6257ad` and its 289 passing focused tests, including
seven regression cases that fail with the selection-only transition.

Retained artifacts were inspected read-only under the temporary root:
`/tmp/claude-501/-Users-scawful-src-hobby-yaze/647c1ae4-27d4-4c4b-9ecc-eba2e612299a/scratchpad/validate`.
They are local evidence and may expire, not release artifacts. Each
`harness_vanilla.log` / `harness_oracle.log` records one passing
`DungeonEditorPaletteRefreshTest.CandidatePlacementSaveReopen` and
`EDITOR_SAVE OK`. Before/after readback records show:

- Vanilla `0x009`: four appended ordinary object records (`0x0C5`, `0x001`,
  `0xFEB`, and `0xFEB` on stored layer 1).
- Vanilla `0x00B`: one torch replaced at `(30,30)`, preserving other printed
  entries, including an existing block. This does not test placing a block.
- Oracle `0x0B9`: custom `0x031` and torch copies replace their source records,
  maintaining the total object count. This proves these selected encodings can
  survive the tested write/readback, not additional stream or torch capacity.

Current SHA-256 identities of the retained scratch inputs/outputs:

| File relative to artifact root | Bytes | SHA-256 |
|---|---:|---|
| `vanilla/harness_in.sfc` | 1048576 | `66871d66be19ad2c34c927d6b14cd8eb6fc3181965b6e517cb361f7316009cfb` |
| `vanilla/harness_out.sfc` | 2097152 | `61df19cd79f3bdde1991472d589f8c33068cae903fccc0ff9935a263e4b4fcbb` |
| `oracle/Roms/harness_in.sfc` | 2097152 | `ab518cb201e4d904706f4c1950a82e574c9618ee23fa12845a1ed6c66c59d0fe` |
| `oracle/Roms/harness_out.sfc` | 2097152 | `d97a9d14e39ff762123a6740b089343ea14a0d40b3a9b2753428178008730354` |

Qualification limits:

1. The harness calls `DungeonEditorV2::Save()` then `Rom::SaveToFile()` directly.
   It bypasses the application save orchestration and pot-item confirmation.
   Oracle custom-object context is activated, but the loaded `YazeProject` is
   not attached to editor dependencies, so manifest/write-policy integration is
   not qualified.
2. Fresh-process readback prints index, ID, position, size, layer, and torch/block
   classification. It does not compare other ROM domains. `allrooms_diff.txt`
   names only vanilla `0x009` and `0x00B`, but does not retain its invocation,
   comparison code, or successful-room count. Do not claim an independently
   verified whole-ROM/no-collateral comparison; the vanilla file also expands
   from 1 MiB to 2 MiB.
3. Capacity refusal prevented the observed output-file save. The direct editor
   save writes room data before checking the torch table, so a late failure can
   follow in-memory changes. Application rollback remains a separate test.
4. Claude reported 960 related tests, a mutation check, and a fixed-build GUI
   room-switch check. Those runs were not reproduced by this artifact audit;
   current integration tests are recorded separately in the capability plan.

Follow-ups retained from the validation report: advance stream/torch-capacity
feedback; duplicate selection behavior; the `Floor 3 ?` label; dirty indicator
after undo to the original state; and the pot-item prompt after a failed Apply
Room. Reproduce each against the current candidate before assigning a cause.
