# Dungeon Workbench placement and object inspector handoff

## Context

The 0.8.0 editing work connects the object browser, pending preview, and
selected-object inspector, then completes the remaining authoring domains.
The user requested substantial implementation work
from Codex and delegated extended validation to Claude. Avoid asking the user
to repeat a sequence of manual tests before continuing development.

## Current mixed-selection candidate (2026-09-23)

Candidate: `eac49e2bd` on `codex/editor-parity-dungeon-authoring`,
following chest source `478206247`. Worktree:
`/Users/scawful/src/hobby/yaze-worktrees/pr256-review-fixes`. Universe task:
`task_20260923T050800Z_19565`. Verification: **640 focused tests
across 39 suites passed with zero failures and zero skips**. App and unit
builds, editor guardrails, changed-line formatting, and pre-commit checks passed.
The new suites contain 34 planner cases and 36 lifecycle runs across both viewer
modes, plus one net new coordinator case.
Keep the earlier candidate evidence below as history for its exact source.

- Delete, duplicate, cut, copy, paste, nudge, and group drag cover objects,
  doors, sprites, and pot items together. A mutation is one undo action across
  all affected collections. Copy creates no history; duplicate preserves the
  prior clipboard. Stateful chest objects retain their reward records.
- Plan every participating domain before applying anything. Stale indices,
  invalid encodings, unsupported translations, bounds, and supported count
  limits reject the whole edit. Failed copy retains the previous clipboard;
  Cut deletes only after successful copy. Reuse the global chest planner and
  exact chest-region write-policy preflight when objects/chests participate.
- Objects use an 8-pixel shared movement grid; selections with sprites or pot
  items use 16 pixels. Preserve relative spacing; do not clamp members
  independently. Doors require an exact same-direction wall slot. Door-only
  arrow nudges advance slots; duplicate and paste without a canvas target keep
  a selection containing doors at its original anchors.
- Group drag contributes one history action at release. Returning to the start
  preserves existing redo history and save dirtiness. Save, room navigation,
  discrete commands, and undo/redo finish active gestures. A stale candidate
  cannot overwrite a single-entity preview committed while finishing a gesture.
  `BeginSaveTransaction` and `Save` finish gestures before capturing dirty state.
- Workbench, standalone inspector, canvas menus, and keyboard commands share
  these paths. No new parallel inspector or undo manager was introduced.

### Mixed-selection architecture and next implementation

1. [`dungeon_selection_edit.h` / `.cc`](../../../src/app/editor/dungeon/dungeon_selection_edit.h)
   owns the pure plan and clipboard types. `PlanDungeonSelectionEdit` returns
   complete before/after state plus actual changed-domain bits;
   `CopyDungeonSelection` copies authored values and paired chest rewards.
   `PlanChestObjectEdit` remains the chest correspondence authority.
2. [`DungeonObjectInteraction`](../../../src/app/editor/dungeon/dungeon_object_interaction.cc)
   owns the unified clipboard and UI commands.
   [`InteractionCoordinator::CommitSelectionEdit`](../../../src/app/editor/dungeon/interaction/interaction_coordinator.cc)
   plans the request and invokes the configured editor publication callback.
   Continuous drag preserves handler selection until the gesture finishes.
3. [`DungeonEditorV2::CommitSelectionEdit`](../../../src/app/editor/dungeon/dungeon_editor_v2_selection_edits.cc)
   preflights global chest state, closes previous history safely, verifies that
   planned source data is current, publishes all affected collections, and uses
   the existing undo manager. Restore targets the original room and retains
   another active room's selection. Sprite snapshots contain authored fields,
   not borrowed preview buffers.
4. Keep the standalone handler fallback testable, but production changes go
   through the configured editor callback. Do not reintroduce sequential
   per-domain mutation into mixed operations or add a second clipboard.

Next bounded implementation: **DA-3 reciprocal connection authoring**. Reuse
room metadata and selection transactions to preview both endpoints, validate
engine adjacency and destination/slot rules, and apply or undo both together.
Preserve intentional one-way links; a diagnostic is not permission to repair
another room. Then extend proven transactions to **DA-4 complete room cloning**.
DA-1/DA-2 remain partial for remaining domains and controls; DA-5 qualification
continues independently on the exact candidate.

### Mixed-selection verification commands

Run from the integration worktree. Discover the selected suites before running
this filter; optional ROM parity fixtures remain a separate lane.

```sh
cmake --build build/presets/mac-ai --target yaze_test_unit yaze --parallel 4 > /tmp/yaze-mixed-final-build.log 2>&1
yaze_mixed_filter='*DungeonRoomMetadata*:*DungeonRoomEditsLifecycle*:*DungeonChestEditor*'
yaze_mixed_filter+=':*DungeonEntityUndoLifecycleTest*:*DungeonUndoActionsTest*:*DungeonWorkbench*'
yaze_mixed_filter+=':*InteractionCoordinatorTest*:*SpriteInteractionHandlerTest*:*DoorInteractionHandlerTest*:*ItemInteractionHandlerTest*'
yaze_mixed_filter+=':*DungeonSelectionSnapshot*:TileObjectHandlerTest.*:DungeonCanvasViewerNavigationTest.*'
yaze_mixed_filter+=':DungeonEditorV2RomSafetyTest.*:DungeonSaveTest.*Chest*:*RoomHeader*'
yaze_mixed_filter+=':ChestEditTest.*:DungeonSaveTest.LoadObjects*:*DungeonSelectionEdit*'
yaze_mixed_filter+='-*RoomObjectRomParityTest*'
build/presets/mac-ai/bin/Debug/yaze_test_unit --gtest_list_tests --gtest_filter="$yaze_mixed_filter" > /tmp/yaze-mixed-final-selected-tests.log 2>&1
build/presets/mac-ai/bin/Debug/yaze_test_unit --gtest_filter="$yaze_mixed_filter" --gtest_output=xml:/tmp/yaze-mixed-final-tests.xml > /tmp/yaze-mixed-final-tests.log 2>&1
```

Scoped analysis uses the existing PCH-free database and the two new planner/
publication translation units:

```sh
/opt/homebrew/opt/llvm/bin/clang-tidy -p build/analysis/mac-ai --checks='-*,clang-analyzer-*' --warnings-as-errors='clang-analyzer-*' --header-filter='(dungeon_selection_edit|dungeon_editor_v2_selection_edits)\.(cc|h)$' src/app/editor/dungeon/dungeon_selection_edit.cc src/app/editor/dungeon/dungeon_editor_v2_selection_edits.cc > /tmp/yaze-mixed-final-analyzer.log 2>&1
```

Scoped analyzer checks passed for these two modules; findings outside that
header scope are not covered. This is not a full-repository tidy claim.
Focused model/editor lifecycle and synthetic in-memory save/reload tests
do not establish rendered UI acceptance or a complete application disk workflow.
Object-stream space/allocation remains a Save-time gate. Author-time manifest
preflight here covers chest ranges; do not claim every-domain preflight.
Vanilla/Oracle GUI-to-disk save/reopen, runtime traversal, manual UX, remote CI,
installation, and packaged Release acceptance remain pending. Preserve the
user's installed app and active ROM sessions during automated verification.

## Prior compound chest candidate (2026-09-23)

Candidate: `478206247` on
`codex/editor-parity-dungeon-authoring`, following `aeb0b1200`. Worktree:
`/Users/scawful/src/hobby/yaze-worktrees/pr256-review-fixes`. Universe task:
`task_20260923T034804Z_2446`. Verification: **569 tests across 37 suites passed, with zero failures and zero skips**.
App and unit builds passed. Scoped Clang analyzer checks passed for the two new mutation
modules. Preserve the earlier evidence below as history for its exact source.

- Add small chest / Add big chest starts the canvas placement tool using the
  object's canonical encoded size. A placed `F99`/`FB1` object receives a
  matching contents record, initially receipt ID `34` (1 Rupee). Preview-only
  actions do not mutate the room.
- Delete and small/big conversion change the object and contents together.
  Each operation uses one existing object undo action, now extended to include
  chest contents. Unknown reward bytes survive edits and undo.
- Ordinary object duplicate/copy/paste preserves reward bytes. Reordering
  objects and changing room-list layers remaps records without exchanging their
  rewards. Open/minigame chest graphics are not ordinary stateful chests.
- The shared inspector follows a changed canvas chest selection and can select
  its corresponding object. A manually chosen record remains selected until
  the canvas selection changes. Existing mapping mismatches retain reward-only
  editing and reject structural operations with an explanation.
- Structural edits validate six shared chest/big-key-lock (`F98`) event slots,
  with chests before locks in encoded room-list order. The existing global
  168-record planner includes unopened physical records and other rooms' dirty
  contents, and preflight applies manifest policy to its exact write ranges.
- Object loading no longer consumes persistent contents records and preserves
  unsaved contents during object-graphics reload. Undo affects the original
  room without changing another active room's selection.

Object-stream allocator capacity is still validated by Save. A successful
placement is not a guarantee that stream growth fits or that allocator-owned
space is available. Full vanilla/Oracle GUI-to-disk save/reopen, game behavior,
manual interaction acceptance, remote CI, and packaged acceptance remain
unqualified for this exact candidate. Do not install over the user's app or
interrupt an active ROM session as part of automated verification.

### Architecture and next implementation

Read this path in order to understand the transaction without tracing every
ImGui callback:

1. [`chest_edit.h` / `.cc`](../../../src/zelda3/dungeon/chest_edit.h) defines the
   pure planner. `ChestIndexForObject` maps objects through encoded list order;
   `PlanChestObjectEdit` combines candidate objects, source indices, and optional
   clipboard rewards into a candidate contents vector. It writes no ROM bytes.
2. [`TileObjectHandler::CommitCandidate`](../../../src/app/editor/dungeon/interaction/tile_object_handler.cc)
   stages the full object edit and asks the planner for contents before emitting
   mutation hooks. Failure leaves both domains and history unchanged.
3. [`DungeonEditorV2::PreflightObjectMutation`](../../../src/app/editor/dungeon/dungeon_editor_v2_chest_edits.cc)
   applies global table capacity and manifest checks. `EditChest` and
   `DeleteChest` use the same boundary; reward-only edits remain available when
   an existing object/contents mapping cannot be established.
4. [`DungeonObjectsAction`](../../../src/app/editor/dungeon/dungeon_undo_actions.h)
   stores object and contents snapshots together. The existing
   [viewer undo hooks](../../../src/app/editor/dungeon/dungeon_editor_v2_undo.cc)
   capture before publication and restore both domains on undo/redo. This
   extends the current undo manager; it does not add a second framework.
5. [`DrawDungeonChestEditor`](../../../src/app/editor/dungeon/inspectors/dungeon_chest_editor.cc)
   is the shared UI for both presentations. Keep preview state owned by the
   interaction handler, and use the model's mapping rather than raw vector
   position to identify a chest.

At this historical checkpoint, mixed-selection atomicity was the next bounded
package. The current mixed-selection increment above supersedes that assignment
and retains the chest planner and rewards. Do not restart completed chest work
or create another inspector/transaction system. DA-5 qualification remains
independent.

### Chest authoring verification commands

Run from the integration worktree. Discover the exact selected suites before
execution; optional ROM parity fixtures remain a separate qualification lane.

```sh
cmake --build build/presets/mac-ai --target yaze_test_unit yaze --parallel 4 > /tmp/yaze-chest-authoring-final-build.log 2>&1
yaze_chest_authoring_filter='*DungeonRoomMetadata*:*DungeonRoomEditsLifecycle*:*DungeonChestEditor*'
yaze_chest_authoring_filter+=':*DungeonEntityUndoLifecycleTest*:*DungeonUndoActionsTest*:*DungeonWorkbench*'
yaze_chest_authoring_filter+=':*InteractionCoordinatorTest*:*SpriteInteractionHandlerTest*:*DoorInteractionHandlerTest*:*ItemInteractionHandlerTest*'
yaze_chest_authoring_filter+=':*DungeonSelectionSnapshot*:TileObjectHandlerTest.*:DungeonCanvasViewerNavigationTest.*'
yaze_chest_authoring_filter+=':DungeonEditorV2RomSafetyTest.*:DungeonSaveTest.*Chest*:*RoomHeader*'
yaze_chest_authoring_filter+=':ChestEditTest.*:DungeonSaveTest.LoadObjects*'
yaze_chest_authoring_filter+='-*RoomObjectRomParityTest*'
build/presets/mac-ai/bin/Debug/yaze_test_unit --gtest_list_tests --gtest_filter="$yaze_chest_authoring_filter" > /tmp/yaze-chest-authoring-final-selected-tests.log 2>&1
build/presets/mac-ai/bin/Debug/yaze_test_unit --gtest_filter="$yaze_chest_authoring_filter" --gtest_output=xml:/tmp/yaze-chest-authoring-final-tests.xml > /tmp/yaze-chest-authoring-final-tests.log 2>&1
```

Scoped static analysis uses the PCH-free database, with the scope limited to the
pure planner and editor chest transaction translation units:

```sh
cmake --preset mac-ai -B build/analysis/mac-ai -G Ninja -DCMAKE_DISABLE_PRECOMPILE_HEADERS=ON -DYAZE_ENABLE_CLANG_TIDY=OFF
clang-tidy -p build/analysis/mac-ai --checks='-*,clang-analyzer-*' --warnings-as-errors='clang-analyzer-*' --header-filter='(chest_edit|dungeon_editor_v2_chest_edits)\.(cc|h)$' src/zelda3/dungeon/chest_edit.cc src/app/editor/dungeon/dungeon_editor_v2_chest_edits.cc > /tmp/yaze-chest-authoring-final-analyzer.log 2>&1
```

Report the actual build/test/analyzer outcomes from these artifacts before
promoting the evidence. The earlier full-checker/PCH limitations below still
apply; a scoped analyzer result is not a full-repository tidy pass.

## Prior room authoring candidate (2026-09-22)

The prior integration increment extended the placement/entity work with room metadata
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
- At this checkpoint, the next implementation package was compound chest creation/deletion with
  synchronized object/contents order and capacity preflight, followed by the
  remaining mixed-domain operations. Do not infer complete DA-1 or DA-2 from
  existing-record editing or the single-domain metadata batch. The current
  compound chest increment above supersedes that assignment.

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
