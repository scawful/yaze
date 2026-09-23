# Editor capability baseline and completion plan

**Status:** IN_PROGRESS  
**Owner:** codex-imgui-frontend-engineer (integration); domain owners below  
**Created:** 2026-09-22  
**Last Reviewed:** 2026-09-23
**Next Review:** 2026-10-06  
**Universe Task:** `task_20260923T131011Z_25069` (current room clone/import increment);
prior normal-door connection task `task_20260923T053944Z_6854`;
prior mixed-selection task `task_20260923T050800Z_19565`;
prior compound chest task `task_20260923T034804Z_2446`;
prior room metadata task `task_20260923T025637Z_4379`;
initial plan task `task_20260923T000806Z_4560`

## Summary

Make Yaze capable of the complete editing workflows provided by Hyrule Magic
and ZScream, while preserving supported ROM formats and existing user data.
The immediate milestone is complete dungeon authoring for 0.8.0. Overworld,
graphics, screens, and audio follow as separate completion milestones.

This is the canonical implementation plan. The
[capability assessment](../../public/reference/capability-assessment.md) is the
public status view; the [coverage report](../../public/reference/feature-coverage-report.md)
describes evidence; the [release ladder](release-ladder-0x-2026.md) assigns scope.
The [dungeon backlog](dungeon-0.8.0-issue-test-backlog-2026-06-28.md) retains
rendering and qualification details. Historical plans are not completion proof.

## Decisions and constraints

1. A capability is a complete user workflow: choose/create, edit, preview,
   undo/redo where applicable, save, reopen, and behave correctly in-game.
   A menu, model class, or passing isolated test does not establish parity.
2. Reuse existing inspectors, interaction handlers, `UndoAction`/`UndoManager`,
   and save transactions. Extend their missing domains before adding parallel
   implementations. Snapshot-backed undo actions are already used in dungeons.
3. Keep the main ALTTP editors primary. CLI, AI, emulator tooling, and platform
   work enter this plan only when they unblock a named editor package.
4. Preserve unknown fields, reserved bits, other rooms, and unrelated domains.
   Keep unsupported writes blocked. Never remove a save gate just to enable a
   button or make a test green.
5. Renderer qualification proceeds independently from new editing features.
   Do not ask the user to repeatedly validate incremental implementation.
   Assigned validation agents run candidate checks on disposable ROM copies.
6. Keep human ownership of interaction design explicit. Agents may prepare
   mechanical cleanup and verification, while the maintainer chooses and codes
   bounded selector/layout features. Follow the
   [UI guidelines](../architecture/ui-design-guidelines.md) and
   [refactor guardrails](../architecture/refactor-quality-guardrails.md); do not
   grow a second architecture or a separate backlog for every extraction.

## Baseline and evidence contract

The September 22 source audit inspected the main checkout at `d609e6254` and
the placement candidate at `7ba7d76ce`, based on PR #256 repair `0b6ecdaf3`.
The candidate's app build and 43 focused tests passed. It was local, unpublished,
and not installed over the Barista-launched app. Recheck Git state before using
these identifiers; the date/hash is an audit snapshot, not a moving guarantee.

At this snapshot, PRs #256 (`0b6ecdaf3`), #257 (`a1484bab3`), #258 (`9557ff3bb`),
and #259 (`5d2ecac28`) were open. PR #259 carries the reviewed tilemap parity
gate; do not assume it is present on another branch or imply CI/merge success.

Record implementation state separately from evidence:

| State | Meaning |
|---|---|
| Implemented | The inspected source contains the complete stated operation; evidence below still controls qualification claims. |
| Partial | Some operations exist, but the complete workflow is unavailable. |
| Missing | A required operation has no implemented path in the inspected scope. |
| Blocked | A known safety or format constraint deliberately prevents the operation. |
| Candidate | A local/PR change implements a slice but has not completed integration and release qualification. |

Evidence levels are **Source**, **Focused** (selected automated tests), **ROM**
(write, independent reopen, decoded comparison), **Runtime** (game behavior),
and **Release** (packaged candidate/CI on declared platforms). Record each
independently, with commit, command, selected/passed/skipped counts, artifact,
and ROM/capture identity. A skipped suite or mismatch-printing test is not a pass.
Do not publish estimated parity percentages.

### Reference editors and format scope

- Hyrule Magic reference source: `MathOnNapkins/hmagic`, audited local commit
  `7d17cc2`. Use the actual editor operations, including music/sample editing,
  dungeon duplication, starting positions, world editing, screens and palettes.
  [Reference repository](https://bitbucket.org/MathOnNapkins/hmagic/).
- ZScream reference source: `Zarby89/ZScreamDungeon`, audited local commit
  `0f6812d`; compare public release **3.2.5** as well, since that local snapshot
  is not a claim of current release completeness.
  [3.2.5 release](https://github.com/Zarby89/ZScreamDungeon/releases/tag/3.2.5),
  [3.2.4 feature ledger](https://github.com/Zarby89/ZScreamDungeon/releases/tag/v3.2.4).
- Explicit comparison targets include expanded Tile16/Tile32/entrance tables,
  per-area graphics, overworld dimensions and states, overlays/animations,
  entrance camera behavior, hole overlays, graphics/player-sprite exchange,
  game properties, music, and screen resources. Existing Yaze support must be
  inventoried before any target is called missing.
- Record vanilla region/version, copier header, expansion/patch version, and
  manifest policy for every ROM-dependent result. Vanilla, ZSCustomOverworld,
  Oracle base ROMs, and legacy HM/PW layouts are separate support claims.
  Never infer support from successful file loading alone.
- Oracle `oos<VERSION>.sfc` is the edit target. `oos<VERSION>x.sfc` is the patched
  runtime target. Do not write editor changes into the patched runtime ROM.
- Study reference behavior and format documentation. Do not blindly copy
  reference implementation code; preserve source-origin/license attribution
  where reference code is actually used.

## Work packages and dependencies

| ID | Deliverable | Current implementation state | Depends on | Primary owner |
|---|---|---|---|---|
| DA-1 | Complete dungeon edit undo across domains | Partial; entity/metadata, paired chest, and atomic mixed-selection Candidates below | Existing mutation hooks and undo actions | imgui-frontend-engineer |
| DA-2 | One editable inspector for every room element | Partial; tile/entity controls, named room properties, shared chest placement/contents/deletion Candidate | DA-1 for added edits | imgui-frontend-engineer |
| DA-3 | Visual room connection authoring | Partial; ordinary outer normal-door pair Candidate below; stairs, pits, special doors, and runtime qualification remain | DA-1 header/compound coverage | zelda3-hacking-expert |
| DA-4 | Complete room clone/import and reusable selections | Partial; versioned authored-room clone/clipboard import Candidate below; asset compatibility and portable project resources remain | DA-1 compound coverage | zelda3-hacking-expert |
| DA-5 | Dungeon render, persistence, and packaged-candidate qualification | Partial; independent active lane | Exact candidate from DA-1–DA-4 | test-infrastructure-expert |
| OW-1 | Overworld sprite persistence for all supported states | Missing writer in inspected save paths | Format/capacity inventory | zelda3-hacking-expert |
| OW-2 | Consistent overworld entity editing and undo | Partial | Existing entity workbench; OW-1 for full sprite loop | imgui-frontend-engineer |
| GF-1 | Safe graphics-sheet save and asset exchange | Blocked coordinated save | Compressed allocation/write plan | zelda3-hacking-expert |
| SC-1 | Coordinated screen and map persistence | Blocked coordinated save | Verified writers/transaction integration | zelda3-hacking-expert |
| SC-2 | Complete remaining screen authoring surfaces | Partial | SC-1 for persistence claims | imgui-frontend-engineer |
| AU-1 | Real sample import and instrument/sample save | Missing writers; WAV import placeholder | BRR/allocation/format inventory | snes-emulator-expert |
| AU-2 | Music event clipboard and editing completion | Partial | Existing song model; AU-1 for sample workflows | imgui-frontend-engineer |
| CO-1 | Reference-feature and ROM-format compatibility ledger | Partial inventory | Evidence per affected package | zelda3-hacking-expert |

Work-package status must name the completed sub-slice. The mixed-selection
candidate covers objects, doors, sprites, pot items, and paired chest contents.
It does not close DA-1/DA-2 for remaining domains, complete room workflows, or
full application/runtime qualification.

### DA-1 and DA-2: edit any room element in place

**Reuse:** `src/app/editor/dungeon/dungeon_editor_v2_undo.cc`,
`dungeon_undo_actions.h`, both viewer hook sets in `dungeon_editor_v2.cc`,
`interaction/interaction_context.h`, `interaction/*_interaction_handler.*`,
`inspectors/object_editor_content.*`, and `workspace/dungeon_workbench_content.*`.

1. Add undo coverage for doors, sprites, and pot items through the existing
   mutation domains. Capture before mutation and finalize after the logical
   action; one drag is one undo entry. Wire both Workbench and standalone rooms.
2. Route property edits through the same validated handlers as canvas actions.
   Extract shared inspector controls from the standalone inspector and use them
   in the Workbench. Remove the superseded controls; do not maintain two editors.
3. Preserve exact room identity, collection metadata, encoded values, selection,
   dirty state, and render invalidation on restore. Handle room switching,
   invalid/no-op edits, and absent selection without creating history entries.
4. Preserve the implemented header/tag/destination and paired object/chest
   transaction paths. Preserve the mixed-selection boundary implemented below
   when adding domains: plan the whole action, reject before publication, and
   restore every participating domain through one undo action.
5. Keep the placement handler authoritative for ghost state. Preview-only
   changes must not dirty the room. Packed area wheel sizing changes axes in
   lockstep; Shift-wheel changes width. Fixed/custom variant semantics differ.

**Exit criteria:** each supported element can be selected, edited, duplicated
or removed where allowed, and undone/redone with exact state restoration.
Property controls agree across presentations. Named choices explain values;
raw fields remain only when useful and validated. A real editor-level test
must exercise hook-to-history-to-restore, not only an isolated action object.

### Entity DA-1 / DA-2 increment (2026-09-22)

**Candidate:** `a730d6557`, branch `codex/editor-parity-dungeon-authoring`,
following plan commit `5fc5af950`. This supersedes the read-only Workbench
entity-summary limitation of the audited `7ba7d76ce` baseline on this branch.
It does not change the status of mainline, an open PR, or an installed app.

Implemented:

- Shared door/sprite/pot-item property controls in the Workbench and standalone
  selection inspector, through `inspectors/dungeon_entity_inspector.*` and the
  existing interaction handlers. Door type/direction/slot, sprite identity,
  position/subtype/layer/key drop, and pot-item type/position are editable.
- Per-domain undo/redo for those three entity collections in both viewer modes,
  preserving room identity, selection, encoded metadata, and save dirtiness.
  A same-domain group drag is one history entry. Paste redo selects the inserted
  entities. Text entry commits through `InputScalarDeferred`; combos and step
  buttons make discrete edits.
- Runtime object-buffer refresh for pot icons and sprite key-drop annotations,
  including undo/redo. Sprite/item edits do not dirty the tile-object save stream.
- Reserved sprite terminator/key-marker encodings are rejected before property,
  movement, or translated entity-paste mutation. The inspector refuses to edit
  when the displayed room and interaction context target different rooms.

**Evidence:** Source + Focused. App and unit targets built with four workers.
**150 selected tests across 21 suites passed; zero skipped.** This includes
22 editor lifecycle cases across both viewer modes and five shared-inspector
UI cases. The pixel tests use generated ROM/graphics data; they are not vanilla
or Oracle runtime/parity evidence. The final commit hook passed formatting,
build-worker policy, and release-version checks. Only formatting whitespace
changed between the passing test run and source commit.

Exact focused commands, from the configured worktree:

```sh
cmake --build build/presets/mac-ai --target yaze_test_unit yaze --parallel 4
build/presets/mac-ai/bin/Debug/yaze_test_unit --gtest_list_tests --gtest_filter='*DungeonEntityUndoLifecycleTest*:*DungeonUndoActionsTest*:*DungeonWorkbench*:*InteractionCoordinatorTest*:*SpriteInteractionHandlerTest*:*DoorInteractionHandlerTest*:*ItemInteractionHandlerTest*:*DungeonSelectionSnapshot*'
build/presets/mac-ai/bin/Debug/yaze_test_unit --gtest_filter='*DungeonEntityUndoLifecycleTest*:*DungeonUndoActionsTest*:*DungeonWorkbench*:*InteractionCoordinatorTest*:*SpriteInteractionHandlerTest*:*DoorInteractionHandlerTest*:*ItemInteractionHandlerTest*:*DungeonSelectionSnapshot*'
```

**Open at this increment:** header/tag/destination and chest undo; atomic mixed-domain
operations; capacity-aware clipboard completion; sprite sort-mode editing;
full contextual controls for the remaining room elements. The connected-view
context guard prevents wrong-room writes; it does not complete connected-view
authoring. Source/handler validation does not replace serializer validation for
other mutation callers. No new application save/reopen, game runtime, CI,
installation, or release qualification is claimed.

**Follow-on:** the room metadata and existing chest-content increment below
extends this entity slice. Do not reimplement the three shared entity inspectors.

### Room-transition review and integration (2026-09-22)

**Candidate:** `50d6257ad` on `codex/editor-parity-dungeon-authoring`, following
`d42ff4d3f`. Claude's validated stale-selection fix `335fdf446` was cherry-picked
as `6e9d52e8c`; its original branch and dirty validation harness remain intact.
No PR into the older placement branch is needed for local integration.

Room or store changes now clear index-based object/entity selection. Object
Coverage navigation and reciprocal-door navigation bind the destination before
selecting their target. The follow-up ends incremental tile drags and paint
strokes against the old room before rebinding, cancels unfinished marquee and
single-entity previews, and preserves selected placement/paint tools. Same-room
redraw does not cancel an active gesture.

**Evidence:** app/unit targets built; **289 selected tests across 23 suites
passed, zero skipped**. Seven regression cases fail when the selection-only
transition from `6e9d52e8c` is substituted, then pass with the follow-up. The
editor-level cases verify tile-drag completion precedes a later entity edit in
history and actual Object Coverage navigation preserves destination selection
through the next binding. The existing mismatched-context inspector test now
selects deliberately after binding so it still exercises its independent guard.

```sh
cmake --build build/presets/mac-ai --target yaze_test_unit yaze --parallel 4
yaze_transition_filter='*DungeonEntityUndoLifecycleTest*:*DungeonUndoActionsTest*:*DungeonWorkbench*:*InteractionCoordinatorTest*:*SpriteInteractionHandlerTest*:*DoorInteractionHandlerTest*:*ItemInteractionHandlerTest*:*DungeonSelectionSnapshot*:TileObjectHandlerTest.*:DungeonCanvasViewerNavigationTest.*'
build/presets/mac-ai/bin/Debug/yaze_test_unit --gtest_list_tests --gtest_filter="$yaze_transition_filter"
build/presets/mac-ai/bin/Debug/yaze_test_unit --gtest_filter="$yaze_transition_filter" --gtest_output=xml:/tmp/yaze-room-transition-tests.xml
```

The broader feature packages remain partial. This integration adds no app-level
save/reopen, runtime, remote CI, installation, or release evidence. See DA-5 and
the placement handoff for the inspected older ROM-validation artifacts and their
limits.

### Room metadata and existing chest-content increment (2026-09-22)

**Candidate:** `aeb0b1200` on `codex/editor-parity-dungeon-authoring`, following
`a893d0ef5`. This local increment does not change mainline, an open PR, or the
installed application.

The candidate extends DA-1 / DA-2 with these bounded operations:

- Room metadata edits use a typed validation boundary and the existing undo
  manager: header properties, tags, message, layout, BG1/BG2 floors, pit
  destination, and each staircase destination/plane. Snapshot restoration
  preserves the dark-room flag and its hidden BG2 mode, encoded values, and
  unrelated room domains. Layout/floors retain their distinct object-stream
  header save dirtiness. Existing ROM reserved-bit preservation remains intact. Tag authoring uses
  `00-3F`; fixed byte-backed tag/effect/collision types preserve unknown loaded
  values without widening the supported authoring choices.
- Workbench properties use full labels and named BG2/effect/collision/tag
  choices. Raw graphics and destination fields remain available with validation.
  Deferred text is tied to the room identity; preview text does not mutate the
  room. Standalone property controls and the Room Tags tool use the same edit
  boundary. Metadata changes refresh room graphics, palette consumers, tag
  usage, and connected-room graph state without navigating away from the
  current room.
- Existing chest contents records have shared Workbench/standalone controls:
  record selection, searchable named rewards, explicit raw reward IDs, and the
  normal/big record type. Each edit is undoable in its original room and
  preserves other records. The panel explains object/record count mismatches.
  Changing a record type does not replace the visible chest object.
- Connected-view **Clear stale** validates all affected room metadata first,
  then publishes one undo action for the complete multiroom edit. Invalid
  batches cannot partially change rooms; net-zero batches preserve redo history.
  Undo/redo also validates every target before restoring any room.
- `DrawInspectorShelfRoom` moves out of the large Workbench source into
  `workspace/dungeon_workbench_room_inspector.cc`; the old implementation is
  removed. `inspectors/dungeon_chest_editor.*` is reused by both presentations.
  This extraction supports feature ownership; file size is not correctness
  evidence.

**Evidence:** Source + Focused at `aeb0b1200`. App and unit targets built with
four workers. **475 selected tests across 36 suites passed, zero failures and
zero skips.** The increment adds 94 cases: 52 metadata model/boundary cases,
28 editor lifecycle cases across both viewer modes, six metadata ImGui cases,
and eight chest chooser cases. The final test filter excludes optional
`RoomObjectRomParityTest` fixtures; an earlier broader run skipped the missing
expanded-ROM fixture and does not qualify that ROM profile.

The rendered controls tests enter text, change rooms, choose named values,
filter chest rewards through the actual search field, and verify rejection
feedback. The synthetic `SaveRoom` lifecycle test saves, independently decodes
headers/chests, compares all bytes outside the permitted records, then verifies
undo/save restores the original complete ROM buffer. This does not establish
vanilla/Oracle application-to-disk or game-runtime qualification.

Scoped Clang analyzer checks passed on `dungeon_room_edit.cc` and
`dungeon_editor_v2_room_edits.cc`, with `clang-analyzer-*` findings promoted to
errors and the header filter limited to those new modules. Diagnostics outside
that scope were suppressed by the filter. This is not a full-repository tidy
pass. Changed-line clang-format, `git diff --check`, editor guardrails from
`a893d0ef5`, and the normal pre-commit checks passed.

The [handoff verification commands](../agents/dungeon-workbench-placement-handoff-2026-09-22.md#room-authoring-verification-commands)
record the exact build, test discovery/filter, and analyzer commands. Artifacts:
`/tmp/yaze-room-authoring-commit-build.log`,
`/tmp/yaze-room-authoring-verified-selected-tests.log`,
`/tmp/yaze-room-authoring-verified-tests.log`,
`/tmp/yaze-room-authoring-verified-tests.xml`, and
`/tmp/yaze-room-authoring-final-analyzer.log`. No full application save/reopen,
runtime, remote CI, installation, or Release acceptance is added by these results.

**Open at this historical increment:** creating or deleting a chest must update
its tile object and contents record together, with capacity/ordering validation
and one undo action. Those controls only edited existing records. General
mixed-domain compound operations, capacity-aware clipboard completion,
sprite sort-mode editing, and
remaining room-element controls also keep DA-1 / DA-2 partial. The multiroom
metadata batch is one data domain; it does not establish general compound
transactions or complete DA-3 connections / DA-4 clone/import.

**Follow-on implemented below:** paired object/chest creation and deletion use
the existing undo manager. The later mixed-selection increment follows that
work before DA-3 connection edits and DA-4 room clone/import. DA-5 qualification
remains an independent lane against the exact candidate.

### Compound chest authoring increment (2026-09-23)

**Candidate:** `478206247` on
`codex/editor-parity-dungeon-authoring`, following the room metadata and
contents-only increment `aeb0b1200`. Universe task:
`task_20260923T034804Z_2446`.

**Implemented scope:**

- Placing a stateful small chest (`F99`) or big chest (`FB1`) creates its visual
  object and contents record together. New chests start with receipt ID `34`
  (1 Rupee). Delete removes both parts; changing small/big type updates both.
  Each operation contributes one existing object undo action that now captures
  both object and contents snapshots.
- Ordinary object duplicate/copy/paste retains the source chest reward,
  including unknown hack IDs. Reorder and layer edits remap records using
  encoded room-list order, preserving the reward associated with each chest.
  Open chest graphics and minigame chests do not create ordinary contents.
- The shared inspector provides Add small chest, Add big chest, Select in
  canvas, Delete chest, named rewards, and a paired Big chest toggle. Add uses
  the existing preview tool; selection follows the chest chosen in the canvas.
  Existing record/object mismatches retain reward-only editing while structural
  chest edits fail with an explanation rather than guessing a correspondence.
- Changed chest mappings validate the shared six-slot limit for stateful
  chests plus big-key locks (`F98`), with chests preceding locks in room-list
  order. Global preflight uses the existing 168-record save planner, including
  unopened rooms and pending edits in other loaded rooms, then applies the
  project's write policy to the exact planned chest-table regions.
- Object loading annotates chest objects without consuming the persistent
  contents vector, and it preserves unsaved reward changes when reloading
  object graphics. Undo restores the original room and keeps another active
  room's selection intact.

**Evidence:** Source at `478206247`. This increment adds 94 cases: 31 pure
planner cases, three loader regressions, 23 handler cases, nine headless ImGui
cases, and 28 lifecycle cases across the two viewer modes. Coverage includes
protected-region rejection, pending-drag boundaries, and in-memory ROM
save/reload/undo/resave with unrelated-byte fences. Focused verification:
**569 tests across 37 suites passed, with zero failures and zero skips**.
App and unit builds passed on the exact source. Scoped Clang analyzer checks
passed for the pure planner and editor chest transaction modules. The declared
filter was discovered and rerun after the final canonical-preview correction.
Exact commands are in the
[chest authoring handoff](../agents/dungeon-workbench-placement-handoff-2026-09-22.md#chest-authoring-verification-commands).
Artifacts: `/tmp/yaze-chest-authoring-final-build.log`,
`/tmp/yaze-chest-authoring-final-selected-tests.log`,
`/tmp/yaze-chest-authoring-final-tests.log`,
`/tmp/yaze-chest-authoring-final-tests.xml`, and
`/tmp/yaze-chest-authoring-final-analyzer.log`.

**Limits:** object-stream allocation and available serialized bytes are still
validated by Save. Successful placement does not guarantee that stream growth
can be saved without allocator-owned space. This increment does not establish
general atomic mixed-selection editing. Vanilla/Oracle full-application disk
save/reopen, game behavior, manual interaction acceptance, remote CI, and
packaged release qualification remain unqualified for this exact candidate.

**Follow-on implemented below:** ordinary mixed-selection transactions now use
this chest planner and the existing undo manager. The chest candidate's tests
and limits above remain evidence for its own source, not the later increment.

### Atomic mixed-selection increment (2026-09-23)

**Candidate:** `eac49e2bd` on
`codex/editor-parity-dungeon-authoring`, following `478206247`. Universe task:
`task_20260923T050800Z_19565`.

- Delete, duplicate, cut, copy, paste, nudge, and group drag cover tile objects,
  doors, sprites, and pot items. A mutation publishes all affected collections
  together and creates one existing undo-manager action. Copy does not create
  history; duplicate does not replace the clipboard. Chest objects retain
  their paired rewards, including unknown reward bytes.
- The planner rejects stale selections, invalid encodings, bounds violations,
  and supported count limits before publication. Rejected operations preserve
  the room and selection; failed copy preserves the prior clipboard. Global
  chest capacity and exact chest-region manifest checks reuse the existing
  preflight. This is not author-time manifest coverage for every data domain.
- Movement preserves relative pixel spacing on a shared 8-pixel grid, or a
  16-pixel grid when sprites/pot items participate. Doors must land on an exact
  valid slot without rotating. Door-only arrow nudges advance wall slots;
  duplication and paste with no canvas target retain door selections in place.
- A group drag is one history action. Returning to its starting state preserves
  redo history and the prior save-dirty state. Room changes, discrete commands,
  undo/redo, and save boundaries finish pending gestures before proceeding.
  `BeginSaveTransaction` and `Save` finish them before capturing dirty state.
- Existing Workbench, inspector, canvas menu, and clipboard routes use the same
  operation; no parallel inspector or undo manager was added.

**Architecture:** `dungeon_selection_edit.*` builds pure before/after plans and
clipboard payloads, reusing `PlanChestObjectEdit`. The interaction facade and
coordinator translate gestures into requests. `dungeon_editor_v2_selection_edits.cc`
applies global chest preflight, publishes data, and records/restores a
`DungeonSelectionAction` through the existing manager. Snapshots retain authored
sprite fields without borrowing preview buffers.

**Evidence:** Source at `eac49e2bd`; **640 focused tests
across 39 suites passed with zero failures and zero skips**. App and unit
builds passed. The new suites contribute 34 planner cases and 36 lifecycle runs
across both viewer modes, plus one net new coordinator case. Changed-line
formatting, editor guardrails, and repository pre-commit checks passed.
The exact build, discovery, execution filter, and artifacts are recorded in the
[mixed-selection handoff](../agents/dungeon-workbench-placement-handoff-2026-09-22.md#mixed-selection-verification-commands).
Scoped Clang analyzer checks passed on the two new planner/publication modules;
this is not a full-repository tidy pass. Artifacts use `/tmp/yaze-mixed-final-*`.
Synthetic model/editor lifecycle and
in-memory persistence checks are distinct from rendered application acceptance.

**Limits and next implementation:** object-stream allocation and serialized
space remain Save-time checks. This candidate does not establish every-domain
manifest preflight, complete room authoring, vanilla/Oracle GUI-to-disk or game
behavior, manual UX, remote CI, installation, or packaged acceptance. DA-1/DA-2
remain partial. The reciprocal normal-door increment below extends this
transaction boundary; the mixed-selection evidence above remains tied to its
own source. Do not silently repair diagnostic findings or add a second
transaction system.

### Reciprocal normal-door connection increment (2026-09-23)

**Candidate:** `be973563f` on
`codex/editor-parity-dungeon-authoring`, following mixed-selection source
`eac49e2bd`. Universe task: `task_20260923T053944Z_6854`.

- The shared door inspector previews the source and adjacent destination in a
  diagram, lets the author choose the upper or lower layer, and commits only on
  **Create Return Door** or **Update Pair**. Opening the destination is navigation,
  not an edit.
- This slice supports ordinary normal doors on outer walls. A missing return
  door is created, or an existing ordinary pair changes layer together. North
  and west slots `0..5` pair exactly with south and east slots `6..11` by adding
  or subtracting six. Adjacency never wraps a room-table row or 256-room page.
- Conflicting records in the same passage, unsupported door types, internal
  seams, invalid slots, and unavailable destination capacity reject the edit.
  Do not invent arbitrary room destinations, silently overwrite a special
  door, or treat an intentional one-way connection as a repair request.
- `dungeon_connection_edit.*` plans both door lists without mutation.
  `dungeon_editor_v2_connection_edits.cc` strictly parses an unopened target's
  object stream before loading it; a dirty partially loaded room is preserved.
  Clean project WaterFill tiles, SRAM mask, and cached tile count survive that
  load, so saving another room's zone cannot remove the target's existing zone.
  Apply rechecks ROM identity and both lists against the displayed preview,
  including after finishing an earlier gesture.
- The existing selection action now accepts a batch of room plans. It validates
  all rooms, publishes changed door lists before refreshing views, and restores
  both endpoints through one Undo/Redo. Other authored domains remain intact;
  only changed endpoints receive object-stream save dirtiness.

**Verification:** **807 tests across 44
suites — passed with zero failures and zero skips**. App/unit builds: passed.
Scoped Clang analyzer: passed for the six selected implementation files and their explicit header scope. The
[connection handoff](../agents/dungeon-workbench-placement-handoff-2026-09-22.md#connection-verification-commands)
records exact discovery/execution commands. The selected surface includes the
prior 640-case mixed-selection lane, connection planner/inspector/lifecycle
tests, and strict stream-reader tests. Three additional lifecycle tests run in
both viewer modes for real `ReloadWaterFillZones`, clean overlay retention,
saving another zone, and dirty partial-state rejection. Headless inspector frames
and synthetic in-memory persistence checks remain separate from human UX and
game evidence.
The analyzer scope covers six changed translation units and their own headers;
known legacy `LayerMergeType` padding findings in `room.h` remain outside that
scope. No full-repository or blanket clean analyzer claim is made.

**Limits and next implementation:** DA-3 is still partial. Staircase/pit
authoring, special door families, arbitrary destination rules, application disk
save/reopen, and runtime traversal require their own work and evidence. Stream
allocation and serialized space remain Save-time gates. No remote CI,
installation, or packaged acceptance is implied. The next implementation at
this checkpoint was DA-4 room clone/import, recorded below. Its later evidence
does not retroactively qualify this connection checkpoint for runtime use.

### Supporting cleanup and human UI ownership

Use the [dungeon editor contribution guide](../../public/developer/dungeon-editor-contribution-guide.md)
for a small first coding task, source entry points, graded UI exercises, focused
build/analysis commands, and bounded agent assignments. This plan remains the
roadmap and evidence authority.

Cleanup should make the next DA-1/DA-2 change easier to understand. It is not a
separate parity milestone, and reduced line counts do not prove fewer defects.
The September 22 cleanup slice moves the existing sprite placement selector's
implementation from a 442-line header into its matching `.cc`; the 75-line
header retains the interface and state. The drawing stages remain explicit:
placement controls, type selection, and existing room sprites. Method bodies,
widget IDs, callbacks, and behavior are preserved.

The same slice adds explicit compilation-database selection and required tidy
analysis to `scripts/lint.sh`. Formatting, compiler diagnostics, static analysis,
focused tests, and human interaction acceptance must be reported separately.
Do not claim a file was analyzed when the tool/database was unavailable or a
PCH/header parse error stopped analysis. Existing style warnings stay advisory
unless a specific check family is intentionally gated.

Suggested division for the next UI slice:

| Owner | Bounded task | Completion evidence |
|---|---|---|
| Maintainer | Implement a searchable named sprite-type chooser in `inspectors/dungeon_entity_inspector.cc`, using the existing handler | Finding a type, keyboard selection/cancel, narrow layout, and one undoable committed edit work as designed |
| Agent | Review the chooser's mutation and selection contracts; prepare only the extraction needed to make the change local | Existing handler/undo coverage, build, targeted analysis, and any new regression for changed behavior |
| Maintainer | Choose the next layout improvement, such as a responsive `dungeon_status_bar.cc` | Readable status at representative canvas widths and scale |
| Agent | Qualify the combined DA-1–DA-4 candidate through DA-5 before another broad feature | Exact build/ROM identity; application save/reopen, undo, failure rollback, and runtime evidence; preserve the existing transactions |

These are proposed human tasks, not an instruction for agents to implement them
preemptively. For future cleanup, name one responsibility and its callers, move
and remove the old implementation in one slice, and prove preserved behavior.
Avoid broad renaming, tree-wide autofixes, or splitting files solely to satisfy
a line-count budget. The current canvas selection path remains authoritative;
older selectors can supply design lessons without creating a second selection
state or resurrecting their old architecture.

**Cleanup evidence:** source candidate `fb02519ea`, following `44aa0ebcc`, on
`codex/editor-parity-dungeon-authoring`. The 18 moved method bodies are unchanged
at the C++ token level; direct header includes decreased from 19 to 3. The app
and unit targets built with the command above; the same discovered 150 tests in
21 suites passed with zero skips. Those tests exercise downstream interaction,
inspector, and undo contracts; there is no direct `SpriteEditorPanel` rendering
test or new human visual acceptance for this extraction.

The tooling contract tests, Bash syntax checks, and ShellCheck pass. Required
lint exposed incompatible PCH and missing SDK flags in the original analysis
setup, then an LLVM 22.1.8 Abseil-check crash after a separate no-PCH/single-config
database with the active SDK was configured. The analyzer-only run parsed both
`sprite_editor_panel.cc` and `dungeon_entity_inspector.cc`, reporting two existing
padding findings in dependent types; its warning gate exited 1. Full tidy and a
clean analyzer gate are **not** claimed. Exact setup and commands are in
[scripts/README.md](../../../scripts/README.md#lint-hooks-and-quality-gates).
The CMake/CI tidy integration gaps described in the refactor guardrails remain a
bounded follow-up. This cleanup does not change installed-app or release status.

### DA-3: visual connections

Reuse `dungeon_canvas_connected_view.cc`, `dungeon_canvas_connected_matrix.cc`,
door helpers, entrance panels, and the existing room destination fields.
The matrix remains a navigation/diagnostic view. The ordinary outer normal-door
inspector candidate above adds paired previews, upper/lower selection, and an
explicit transaction through the existing undo manager. Extend connection
coverage only with the relevant engine rule and persistence evidence.
Normal doors follow vanilla adjacency; they are not arbitrary teleports.
Staircase order-to-slot assumptions need ROM proof. Missing reciprocal doors,
unused slots, and intentional one-way connections must remain distinguishable.
Changing both sides must be one validated, undoable operation; do not repair
other rooms merely because a diagnostic exists.

**Exit criteria:** author and revisit a supported connection, undo/redo both
affected rooms, save/reopen with matching headers/objects, and verify traversal
in the correct runtime ROM. Navigation alone does not close this package.

### DA-4: room and selection reuse

**Authored-room clone/import candidate (2026-09-23):**
`2520aa6b0` on `codex/editor-parity-dungeon-authoring`,
following source `be973563f` and documentation `6a1196218`.

The shared Room UI/dialog supports same-project **Clone**, **Copy Room JSON**,
and clipboard/text **Import JSON**, with **Preview Replacement** before
**Apply Replacement**. The new `yaze.room` version 1 document has seven selectable
domains: objects with chest rewards and special-object metadata, doors, sprites,
pot items, room properties, custom collision, and water fill zones. Core defaults
select the first five; collision and water require explicit inclusion.

Destination room/plane fields stay with the target by default. **Copy destination
links** is an explicit option; it does not create reciprocal links or remap
incoming connections. The document includes the pit destination plane and
special-object fields, but references graphics, layouts, and messages by numeric
ID. It does not package those assets, entrances, global pit-damage membership,
palette data, project source, or other external resources. Destination reserved
header bits and the sprite stream's sort byte remain destination-owned.

Implementation boundaries:

1. `dungeon_room_transfer.*` owns authored snapshots, selected-domain planning,
   and publication; `dungeon_room_transfer_json.cc` validates the versioned
   document before any live replacement. Legacy room-template JSON and
   `ApplyRoomLayoutTemplate` remain separate; do not route this UI through their
   direct clear/refill path.
2. `dungeon_editor_v2_room_transfer.cc` rejects stale source/target previews,
   finishes prior gestures, and publishes one replacement through the existing
   `UndoManager`. Undo restores the target without rewriting the source.
3. Preflight serializes a detached ROM/editor copy through the existing room
   save path. It includes dirty/materialized shared-table state, actual stream
   capacity/allocation, disabled domain-save flags, and project write policy.
   Actual changed byte ranges supplement estimated manifest ranges. No disk
   file, live ROM bytes, palette state, or undo history is published by preview.
4. Water-mask assignment retains other rooms' masks and gives a new zone a free
   bit. Water-table persistence merges saved zones from unmaterialized rooms;
   replacing one zone cannot drop an unopened room's saved zone.

The same candidate includes review corrections for flagged pot-item type edits,
coupled chest/object saves, object-only Delete controls, and the dungeon
spriteset authoring limit. The handoff records their regression scope and the
remaining pot-coordinate codec audit.

**Verification:** `968` tests across
`49` suites; `all passed, zero failures and zero skipped`. Exact commands,
build/analyzer scope, and artifacts belong in the
[room transfer handoff](../agents/dungeon-workbench-placement-handoff-2026-09-22.md#room-transfer-verification-commands).
The prior 807-test connection result remains historical evidence for
`be973563f`, not this candidate's result. Synthetic persistence and ImGui tests
remain separate from application-to-disk, human UX, and game-runtime acceptance.

**Remaining DA-4 scope:** project-file export/import and asset-compatibility
checks/remapping, portable reusable selections/assets, and qualified save/reopen.
The clipboard document is authored room data, not a complete portable asset pack.
No native file-picker or cross-project asset compatibility is claimed. Store
future reusable resources in project/configured storage, not a machine path.

**Exit criteria:** clone/import previews the affected domains, rejects invalid
or over-capacity data without mutation, commits as one undo action, and survives
save/reopen. Selection prefabs preserve type-specific semantics and offsets.

**Next bounded package: DA-5 qualification of this combined candidate.** Prove
the application disk transaction and rollback, independent reopen/readback,
and representative game behavior before broadening features. DA-4 project-file
compatibility and DA-3 stairs/pits with verified engine rules follow as separate
bounded tasks. Do not call either entire package complete from this increment.

### DA-5: independent qualification lane

Use the existing [dungeon backlog](dungeon-0.8.0-issue-test-backlog-2026-06-28.md),
Object Coverage evidence, ROM fixtures, and reviewed parity gate. Keep synthetic
geometry, renderer replay, captured game tilemaps, and actual gameplay as
separate evidence. Record capture/ROM identity and reviewed expected differences;
reject new mismatches. Do not dismiss CodeQL alerts from a sink line alone: inspect
the complete source-to-sink trace, including new test fixture callers.

The [September 22 placement validation review](../agents/dungeon-workbench-placement-handoff-2026-09-22.md#independent-validation-review-2026-09-22)
records inspected scratch-ROM hashes and targeted object readback. It qualifies
selected ordinary/custom/torch serialization on the older `7ba7d76ce` candidate;
it does not close DA-5 for the integrated branch. The harness bypassed app save
orchestration and did not attach Oracle project dependencies. Promote the next
save check to the application transaction with project policy active, including
late failure rollback and the reported pot-item confirmation sequence.

Capacity feedback is a named editing gap: object placement currently can exceed
room-stream or shared torch-table space before Save refuses. Reuse serializer
planning/capacity rules for advance feedback; preserve save-time guards. A
successful replacement at unchanged count does not prove capacity growth.
Whole-ROM safety claims require comparison beyond the readback helper's object
fields. Wheel/Shift-wheel, Escape, scaled layout, block placement, and game
runtime remain separate open qualification items.

### OW-1 and OW-2: overworld entity completion

Inspect `src/app/editor/overworld/entity_operations.cc`,
`entity/entity_workbench.cc`, `overworld_editor.cc`, and
`src/zelda3/overworld/overworld.cc`. Sprite edits populate state-specific vectors
but the inspected ordinary and model save paths have no sprite writer.
Implement shared state-aware serialization with table/pointer/capacity rules,
then connect both callers. Preserve the other game states and unrelated maps.
Property edits and insertion currently bypass entity undo; extend the existing
history rather than introducing a second manager.

**Exit criteria:** create/edit/remove each entity, undo/redo, save/reopen all
three supported states, and verify the correct state in-game. Painting/paste
undo and persistent scratch space already exist and are not new deliverables.

### GF-1 and SC-1/SC-2: persistent assets and screens

The central guards are in `src/app/editor/editor_manager.cc`. The old
`GraphicsEditor::Save` writes recompressed data at existing addresses without
proving allocation capacity. Build a compression/write plan, validate spans
and shared references, and use approved relocation only where supported.
Retain the gate until the coordinated save can include the complete operation.
Graphics-group editing/saving already exists and differs from sheet persistence.

The Screen Editor central gate covers all pending screen domains, including
dungeon maps. Direct title/pause-map writers also reject writes. Complete
the relevant codecs and transaction integration; verify all affected maps,
palette/tile references, and exact persisted data. Naming-screen UI is empty;
inventory controls and clipboard/find are partial. Credits/ending coverage
requires an explicit resource inventory. Polyhedral crystal/Triforce editing
already exists and must not be incorrectly classified as absent.

**Exit criteria:** an imported/edited sheet or screen survives save/reopen,
matches the intended decoded asset, and displays in-game. A rejected save
preserves ROM bytes and pending edits. Test payload growth and shared pointers.

### AU-1 and AU-2: audio authoring

Use `src/zelda3/music/music_bank.cc` and `src/app/editor/music/music_editor.cc`.
`SaveInstruments` and `SaveSamples` are unimplemented; the inspected WAV import
creates dummy PCM rather than decoding the file. Replace the placeholder with
real validated input and implement BRR encoding, loop points, directory/bank
allocation, instrument records, and write planning. Reuse song saving, tracker,
piano roll, SPC playback, and existing ASM interchange. Add event copy/paste
with timing/channel semantics; do not represent it as implemented from UI alone.

**Exit criteria:** an actual imported sample and instrument survive save/reopen,
loop as intended, and play through the game audio path. Invalid input or bank
overflow must leave the current song/bank and ROM unchanged.

### CO-1: compatibility and complete reference coverage

Inventory each reference editor feature by user task, exact version, format,
Yaze entrypoint, and persistence path. Include effects/tags, maps, sprite/game
properties, special entities, overlays/animations, graphics exchange, screens,
audio, starting locations, and project metadata. Mark uninspected tasks
**not yet audited**, not missing or complete. Check region/header and patch
profiles separately. Legacy HM/Parallel Worlds offsets in old proposals remain
unverified; [that proposal](hyrule-magic-support-plan.md) is not an implementation
contract. Do not guess relocated tables or automatically doctor source ROMs.

The [ZScream 3.2.5 release](https://github.com/Zarby89/ZScreamDungeon/releases/tag/3.2.5)
adds these explicit CO-1 audit targets beyond the 3.2.4 ledger:

- Tall (1×2) and Wide (2×1) overworld areas, including camera and tile-loading
  behavior at their boundaries.
- Different Light World and Dark World area layouts; test warps between
  differently sized areas and record the reference release's known constraint.
- Extra Special World areas and their items, entrances/exits, sprite
  palettes/graphics, message IDs, and entrance overlays.
- Rain-phase (phase 0) sprites on the Dark World and Special World, preserving
  the other game states when saving.
- Relocation of formerly fixed Special World exits and transports between
  worlds, including their destination/camera semantics.
- Tile16 usage highlighting on the overworld and graphics-tile usage
  highlighting within Tile16 definitions; dungeon directional navigation,
  graves display, and additional overworld grid options.

For each, record whether the operation requires an engine patch, which patch
version Yaze recognizes, the relevant UI/model/writer, and all applicable
evidence levels. These are **not yet audited** comparison targets, not a blanket
claim of missing Yaze support. Loading a patched ROM or exposing a property is
insufficient to close its editing and save/runtime workflow.

## Instructions for the implementing model

1. Claim a bounded package/sub-slice through `scripts/agents/coord`. Inspect
   branch/head, dirty files, other active tasks, and applicable PRs. Record the
   source baseline. Avoid editing another agent's worktree or validation ROM.
2. Search call sites and current serializers before editing. State what exists,
   the precise missing operation, owned files, dependencies, and non-goals.
   Skills and old notes are discovery aids; current code and verified formats
   decide behavior. Use existing user authorization; do not invent approvals.
3. Implement one coherent workflow through existing mutation/save boundaries.
   No speculative abstraction, silent coercion, placeholder success, missing
   body hidden behind a control, or disabling a guard to finish a task.
4. Run the smallest meaningful checks. Discover test filters first; check that
   expected cases actually ran. Add regression cases for real failure modes,
   failure rollback, and encoding limits. Do not repeatedly run unrelated suites.
5. Update this package's sub-slice status and the public status view, including
   evidence and remaining limits. Commit a reviewable change, then hand the exact
   candidate to the validation owner. Do not claim install, CI, merge, or release
   until that action has actually been verified.

## Verification and delivery

For the desktop dungeon slice, use repository presets (default max four workers):

```sh
cmake --preset mac-ai
cmake --build --preset mac-ai --target yaze_test_unit yaze --parallel 4
build/presets/mac-ai/bin/Debug/yaze_test_unit --gtest_list_tests --gtest_filter='<selected suites>'
build/presets/mac-ai/bin/Debug/yaze_test_unit --gtest_filter='<selected suites>'
```

Resolve the configured executable path on other platforms. The angle-bracket
filter above is a placeholder: substitute discovered suites and report counts.
Use `YAZE_PREPUSH_BUILD_DIR=build/presets/mac-ai` for relevant publication checks.
ROM tests require disposable inputs and source hashes; runtime tests require
the matching built/patched ROM. Keep the user's installed app/session untouched
unless installation or replacement is authorized.

Record: package/sub-slice, branch/commit, changed files, discovered tests and
counts, commands/artifacts, ROM/capture identities when relevant, pending
limits, and one exact next action. For docs-only changes, check relative links,
stale claims, `git diff --check`, and protocol checks if routing files changed.

## Plan exit criteria

- Every reference workflow has a versioned ledger entry and a justified status.
- All required editing operations have a working persistence path and appropriate
  undo; blocked/missing operations are resolved or explicitly excluded from the
  declared supported-format baseline.
- ROM-dependent claims have independent reopen evidence; behavior claims have
  runtime evidence; packaged releases identify their source and platform results.
- Public capability docs, the roadmap, and release checklist agree with the
  ledger. No estimated percentage substitutes for unresolved rows.
