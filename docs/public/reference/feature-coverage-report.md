# Editor readiness and feature coverage

Last reviewed: 2026-09-23 for the v0.8.0 development line.

This is the public capability and persistence ledger. The
[editor capability parity plan](../../internal/plans/editor-capability-parity-plan.md)
owns implementation order and acceptance gates. The
[capability assessment](capability-assessment.md) summarizes the Hyrule Magic /
ZScream comparison. No whole-editor percentage establishes parity.

The September 22 audit baseline is mainline `d609e6254` and placement candidate
`7ba7d76ce`; the latter's app build and 43 focused tests passed without a
publication, installation, or release claim. Subsequent candidate `a730d6557`
adds door/sprite/pot-item undo and shared property controls, following plan
commit `5fc5af950`. Its app and unit-test builds succeeded; 150 selected tests
across 21 suites passed with no skips, including 22 entity lifecycle and five
shared inspector UI cases. The only post-test changes were formatting
whitespace, verified through whitespace-normalized staged content. These are
Source + Focused results; ROM save/reopen, Runtime, CI, installation, and Release
qualification remain pending. Verify Git and artifact state before promotion.

The prior room/chest-content candidate is `aeb0b1200` on
`codex/editor-parity-dungeon-authoring`. Room metadata, existing chest contents,
named controls, and atomic multiroom **Clear stale** are described in the
[formal plan's historical increment](../../internal/plans/editor-capability-parity-plan.md#room-metadata-and-existing-chest-content-increment-2026-09-22).
App and unit builds passed; **475 selected tests across 36 suites passed with
zero failures and zero skips**, including 94 new cases. Scoped Clang analyzer
checks passed on two mutation modules. These are Source + Focused results,
including synthetic editor save/decode/byte-preservation checks; mainline,
installed app, vanilla/Oracle application-to-disk/runtime qualification, CI, and
Release status are unchanged.

The prior compound chest candidate is `478206247` on the same
branch. It pairs object and contents mutations for placement, deletion, type
changes, and undo; ordinary object copy/paste preserves rewards and layer/order
edits preserve correspondence. Its shared inspector starts placement in the
canvas and follows selected chests. Source + Focused evidence; focused verification:
**569 tests across 37 suites passed, with zero failures and zero skips**.
App and unit builds passed. Scoped Clang analyzer checks passed for the two new mutation modules.
Preflight validates six shared chest/big-key-lock slots with chests preceding
locks, global 168-record capacity including dirty/unopened rooms, and exact
chest-region manifest policy. Object-stream allocation remains a Save-time
check. See the [chest increment](../../internal/plans/editor-capability-parity-plan.md#compound-chest-authoring-increment-2026-09-23)
for its evidence and residual qualification limits.

The prior mixed-selection candidate is `eac49e2bd`. Existing
Workbench/inspector/canvas commands now share one validated operation for
objects, doors, sprites, pot items, and paired chest rewards. Delete, duplicate,
cut/paste, nudge, and group drag produce one undo action; failed copy preserves
the clipboard. Rigid spacing and exact door slots replace independent per-domain
movement. Source + Focused evidence: **640 tests across
39 suites passed with zero failures and zero skips**. Save closes
pending gestures; no-op drags retain redo history and prior save dirtiness.
This does not add rendered UI, vanilla/Oracle application-to-disk/runtime, CI,
installation, or Release evidence. See the
[mixed-selection increment](../../internal/plans/editor-capability-parity-plan.md#atomic-mixed-selection-increment-2026-09-23).

The current reciprocal normal-door candidate is `be973563f`.
The shared inspector previews both rooms, chooses upper/lower layer, and creates
or updates the ordinary outer-wall return door only through **Create Return Door**
or **Update Pair**. Lazy target loading preserves clean project WaterFill tiles,
mask, and count, and rejects dirty partial state. Both
endpoints validate and undo together through the existing batch action. Exact
slots, adjacency boundaries, passage conflicts, stale previews, ROM identity,
and unloaded target parsing are guarded. Verification: **807
tests across 44 suites — passed with zero failures and zero skips**. App/unit
builds: passed. Scoped analyzer: passed for the six selected implementation files and their explicit header scope.
DA-3 remains partial for stairs, pits, special doors, and runtime qualification.
Next implementation is DA-4 complete room clone/import with explicit inclusion,
unsupported-data preservation, and capacity preflight. See the
[connection increment](../../internal/plans/editor-capability-parity-plan.md#reciprocal-normal-door-connection-increment-2026-09-23).

## Status and evidence are separate

| Status | Meaning |
| --- | --- |
| **Implemented** | The named capability exists. This does not imply a qualified release or completion of adjacent capabilities. |
| **Partial** | Part of the workflow exists, but a named editing, undo, persistence, or acceptance requirement is incomplete. |
| **Missing** | The named implementation is absent or an explicit stub. A search that did not locate a feature should be reported as an audit gap rather than an absolute absence. |
| **Blocked** | The application deliberately refuses the workflow until its required implementation or safety proof exists. |
| **Candidate** | An implementation exists on a named change or branch and awaits integration or acceptance. |

| Evidence | What must be recorded |
| --- | --- |
| **Source** | Exact code commit, entry points, and relevant read/write/undo paths inspected. |
| **Focused** | Discovered test names, exact command, tested commit, result, and any skips. Includes synthetic ImGui tests; does not imply ROM proof. |
| **ROM** | ROM profile/hash, disposable input/output, write/reopen/readback comparison, and expected write ranges. Direct serializer and full application save are distinct paths. |
| **Runtime** | Named emulator/game scenario, capture identity, and observed result. Static pixels do not prove gameplay behavior. |
| **Release** | Exact packaged artifact and digest, platform, CI, install/launch acceptance, and required workflow checks. A local source build is not this evidence. |

Promote the specific workflow, not every feature in its editor. Prior
"Tester ready" labels described bounded beta attempts; they did not certify
all entity types, undo domains, or save paths. A GUI smoke test or a test called
"E2E" is not automatically a complete **edit → save → close → reopen** test.

## Desktop editor matrix

All baseline statements below are Source observations. Existing test coverage
is useful but must be linked to a specific workflow and commit before it is
used for promotion.

| Editor / workflow | Status | Existing path | Remaining boundary |
| --- | --- | --- | --- |
| Dungeon room authoring | **Partial** | Existing room elements and guarded save, plus the candidate slices below | Remaining authoring domains, connection families, complete room operations, and GUI-to-disk/runtime acceptance remain. DA-1 and DA-2 are not complete. |
| Workbench tile-object placement improvements | **Candidate** | `7ba7d76ce`: live placement controls, once/repeat, selected inserted object, Place another, physical sizes; Source + Focused | Isolated candidate ROM and runtime validation, integration, and packaged acceptance remain. See the [validation handoff](../../internal/agents/dungeon-workbench-placement-handoff-2026-09-22.md). |
| Door/sprite/pot-item undo and shared properties | **Candidate** | `a730d6557`: DA-1 domain snapshots/restore and DA-2 shared Workbench/standalone inspector; render invalidation, reserved-sprite validation, paste selection, and deferred input isolation have focused checks | Context-mismatched connected-view editing is gated. ROM save/reopen, runtime, CI, installation, and release qualification remain; other DA-1/DA-2 domains are open. |
| Room metadata undo and named property controls | **Candidate**, Source + Focused at `aeb0b1200` | Typed header/tag/layout/floor/message/destination edits; room-bound deferred input; snapshots preserve hidden BG2 state and separate layout/floor save dirtiness; atomic multiroom **Clear stale** batch | Full application save/reopen, runtime, and packaged acceptance remain. This single-domain batch does not complete general compound editing. |
| Compound chest authoring | **Candidate**, Source + Focused at `478206247` | Shared placement, record/reward/type selection, canvas selection, and delete controls; paired object/contents undo; ordinary object clipboard preserves rewards; layer/order remapping; event-slot/global-table/manifest preflight | Save still validates object-stream allocation. This chest-only evidence predates the mixed-selection candidate below. Vanilla/Oracle full-application disk save/reopen, game behavior, manual UX, CI, and packaged acceptance remain unqualified. |
| Atomic mixed-selection editing | **Candidate**, Source + Focused at `eac49e2bd` | Shared delete/duplicate/cut/copy/paste/nudge/group-drag planner and one undo; chest rewards retained; supported count/encoding/coordinate checks; exact door anchors; no partial publication on rejection | Object-stream allocation remains Save-time. Author-time manifest preflight covers chest ranges only. Full application disk persistence, rendered UX, game behavior, CI, and packaged qualification remain. |
| Reciprocal normal-door connections | **Candidate** at `be973563f`; verification above | Shared endpoint preview and Create Return Door / Update Pair; exact outer slots and adjacency; guarded target load retaining project WaterFill data; one batch Undo/Redo preserving other domains | Stairs, pits, special door families, internal seams, and arbitrary destinations are outside this slice. Save-time allocation, full application disk persistence, runtime traversal, human UX, CI, and packaged qualification remain. |
| Overworld map/entrance/exit/item/property editing | **Partial** | Domain-specific save methods called by `OverworldEditor::Save()` when their flags are enabled | Entity undo and full application acceptance need completion. Save support must be checked per domain and ROM layout. |
| Overworld sprite persistence | **Missing** | Editing and three game-state collections exist | The application save path does not call a sprite serializer. Do not report sprite edits as saved because Save ROM succeeds. |
| Message | **Partial** | Transactional save of valid parsed text through coordinated save when enabled | GUI-to-disk reopen and runtime acceptance for the advertised ROM profile. |
| Palette | **Partial** | Palette **Save to ROM** commits the model to the shared ROM buffer; **File > Save ROM** writes disk | Two-step workflow remains. JSON import/export is Implemented when JSON support is enabled. |
| Assembly | **Partial** | **Save File** writes active ASM source; Asar patch application is a separate ROM operation | Source reopen/dirty-close and separately fenced patch-application qualification. |
| Sprite | **Partial** | Custom `.zsm` editing/saving; vanilla sprite viewing | Qualify `.zsm` roundtrip and complete/disable incomplete vanilla controls. Dungeon sprite placement is a separate workflow. |
| Settings | **Implemented** for configuration-file persistence | Settings/layouts save outside the ROM | Panel-to-restart acceptance is separate from ROM capability and release proof. |
| Graphics pixel-sheet persistence | **Blocked** | Pixel editing, undo, import surfaces, and a writer implementation exist | Pending sheet edits block coordinated Save ROM. Compression/allocation/write safety and readback must be established before enabling it. |
| Graphics groups and polyhedral editing | **Partial** | Existing registered editors and model operations | Audit and qualify their specific persistence paths; do not list the tools as missing or assume pixel-sheet proof covers them. |
| Screen | **Blocked** for coordinated save with any pending domain | Dungeon-map, Tile16, title-screen, and pause-map models; partial inventory UI | All pending Screen domains block central save. Direct title/pause ROM writers also fail closed. Naming screen is empty; credits/ending screen authoring needs a targeted audit. |
| Music | **Partial**; instrument/sample saving **Missing** | Tracker/piano roll, playback, song serialization, separate editor save path | Not in coordinated Save ROM. `SaveInstruments` and `SaveSamples` return Unimplemented; WAV import is a placeholder; event clipboard is incomplete. |
| Hex / Memory | **Partial** | Raw-ROM inspection and expert editing surface | No complete editor dirty/undo/save contract; outside the supported tester edit workflow. |
| Emulator | **Partial** | Play testing and runtime inspection | Save-state UI and conditional-breakpoint workflows remain incomplete; independent emulator evidence is still required. |
| Agent | **Partial** | Build/provider-specific chat and tools | Not a coordinated ROM-save participant; tool output is not proof of a desktop editing workflow. |

## What coordinated Save ROM currently covers

[`EditorManager::SaveRom()`](../../../src/app/editor/editor_manager.cc) is the
application persistence boundary. Before serializers mutate the ROM, it rejects
pending Graphics sheet edits and **all** pending Screen edits. For eligible
loaded editors it then coordinates Screen's allowed unchanged-state path,
Dungeon, Overworld, and enabled Message serialization, followed by ROM safety
checks, conflict handling, backup policy, and the disk write.

Important exceptions:

- [`OverworldEditor::Save()`](../../../src/app/editor/overworld/overworld_editor.cc)
  covers maps, entrances, exits, items, and enabled properties/music/custom
  overworld data. It omits overworld sprites.
- Palette must commit its model through **Save to ROM** before the disk save.
- A dungeon-map writer existing in `ScreenEditor::Save()` does not bypass the
  central pending-Screen block. Neither enabling `kSaveDungeonMaps` nor
  `kSaveGraphicsSheet` establishes safe general persistence.
- [`ScreenEditor::SaveTitleScreenToRom()` and `SaveOverworldMapToRom()`](../../../src/app/editor/graphics/screen_editor.cc)
  return errors until their readback requirements are met. A custom-map export
  is a separate file workflow, not a ROM save.
- Music, Assembly, Sprite, Settings, Emulator, and Agent have separate or
  partial persistence models and are not invoked by coordinated ROM save.
- Hex / Memory modifies raw ROM data without the complete supported editor
  transaction contract.

## Source anchors for implementation work

| Claim / task | Source entry point |
| --- | --- |
| Dungeon shared mutation/undo hooks, including candidate door/sprite/pot-item domains | [`DungeonEditorV2::ConfigureViewerUndoHooks()` / `RestoreRoomEntities()`](../../../src/app/editor/dungeon/dungeon_editor_v2_undo.cc) |
| Shared entity property controls and connected-view context guard | [`DrawDungeonEntityInspector()`](../../../src/app/editor/dungeon/inspectors/dungeon_entity_inspector.cc), called by the Workbench and standalone inspector |
| Candidate lifecycle and shared-inspector evidence | [`dungeon_undo_actions_test.cc`](../../../test/unit/editor/dungeon_undo_actions_test.cc), [`dungeon_workbench_content_test.cc`](../../../test/unit/editor/dungeon_workbench_content_test.cc) |
| Candidate room metadata transactions | [`DungeonEditorV2::EditRoomMetadata()`](../../../src/app/editor/dungeon/dungeon_editor_v2_room_edits.cc), [`ApplyRoomMetadataEdit()`](../../../src/app/editor/dungeon/dungeon_room_edit.cc), [`Room::MetadataSnapshot`](../../../src/zelda3/dungeon/room.h) |
| Compound chest planner, authoring preflight, and paired undo | [`PlanChestObjectEdit()`](../../../src/zelda3/dungeon/chest_edit.cc), [`TileObjectHandler::CommitCandidate()`](../../../src/app/editor/dungeon/interaction/tile_object_handler.cc), [`DungeonEditorV2::EditChest()` / `DeleteChest()` / `PreflightObjectMutation()`](../../../src/app/editor/dungeon/dungeon_editor_v2_chest_edits.cc), [`DungeonObjectsAction`](../../../src/app/editor/dungeon/dungeon_undo_actions.h) |
| Mixed-selection planner and publication | [`PlanDungeonSelectionEdit()` / `CopyDungeonSelection()`](../../../src/app/editor/dungeon/dungeon_selection_edit.cc), [`InteractionCoordinator::CommitSelectionEdit()`](../../../src/app/editor/dungeon/interaction/interaction_coordinator.cc), [`DungeonEditorV2::CommitSelectionEdit()`](../../../src/app/editor/dungeon/dungeon_editor_v2_selection_edits.cc); existing undo manager and chest preflight |
| Mixed-selection candidate verification | [`dungeon_selection_edit_test.cc`](../../../test/unit/editor/dungeon_selection_edit_test.cc), [`dungeon_selection_edits_lifecycle_test.cc`](../../../test/unit/editor/dungeon_selection_edits_lifecycle_test.cc), [`interaction_coordinator_test.cc`](../../../test/unit/editor/interaction_coordinator_test.cc); exact commands/artifacts in the [handoff](../../internal/agents/dungeon-workbench-placement-handoff-2026-09-22.md#mixed-selection-verification-commands) |
| Candidate room and chest controls | [`DrawInspectorShelfRoom()`](../../../src/app/editor/dungeon/workspace/dungeon_workbench_room_inspector.cc), [`DrawDungeonChestEditor()`](../../../src/app/editor/dungeon/inspectors/dungeon_chest_editor.cc), [`RoomTagEditorPanel`](../../../src/app/editor/dungeon/ui/window/room_tag_editor_panel.cc) |
| 94 new passing metadata/chest cases at `aeb0b1200` | [`dungeon_room_edit_test.cc`](../../../test/unit/editor/dungeon_room_edit_test.cc), [`dungeon_room_edits_lifecycle_test.cc`](../../../test/unit/editor/dungeon_room_edits_lifecycle_test.cc), [`dungeon_room_metadata_ui_test.cc`](../../../test/unit/editor/dungeon_room_metadata_ui_test.cc), [`dungeon_chest_editor_test.cc`](../../../test/unit/editor/dungeon_chest_editor_test.cc) |
| Compound chest candidate verification surface | [`chest_edit_test.cc`](../../../test/unit/zelda3/dungeon/chest_edit_test.cc), [`tile_object_handler_test.cc`](../../../test/unit/editor/tile_object_handler_test.cc), [`dungeon_room_edits_lifecycle_test.cc`](../../../test/unit/editor/dungeon_room_edits_lifecycle_test.cc), [`dungeon_chest_editor_test.cc`](../../../test/unit/editor/dungeon_chest_editor_test.cc); exact commands and artifacts in the [handoff](../../internal/agents/dungeon-workbench-placement-handoff-2026-09-22.md#chest-authoring-verification-commands) |
| Overworld paste/paint undo | [`OverworldEditor` undo and clipboard paths](../../../src/app/editor/overworld/overworld_editor.cc) |
| Persistent overworld scratch space | [`LoadScratchPad`, `SaveScratchPad`, `FlushScratchPadIfDirty`](../../../src/app/editor/overworld/scratch_space.cc) |
| Palette JSON exchange | [`PaletteGroupPanel::ExportToJson()` / `ImportFromJson()`](../../../src/app/editor/palette/palette_group_panel.cc) |
| Graphics groups | [`OverworldEditor::UpdateGfxGroupEditor()`](../../../src/app/editor/overworld/overworld_editor.cc) |
| Existing polyhedral editor | [`PolyhedralEditorPanel`](../../../src/app/editor/graphics/polyhedral_editor_panel.cc) |
| Empty naming screen and partial inventory | [`ScreenEditor::DrawNamingScreenEditor()` / `DrawInventoryMenuEditor()`](../../../src/app/editor/graphics/screen_editor.cc) |
| Unimplemented instruments/samples and placeholder WAV import | [`MusicBank::SaveInstruments()`, `SaveSamples()`, `ImportSampleFromWav()`](../../../src/zelda3/music/music_bank.cc) |
| Placeholder sample import UI and missing event clipboard | [`SampleEditorView`](../../../src/app/editor/music/sample_editor_view.cc), [`MusicEditor::Copy()` / `Paste()`](../../../src/app/editor/music/music_editor.cc) |

Search the current checkout before changing an entry point: these are anchors,
not permission to replace a newer implementation with an older design.

## Promotion requirements

1. Complete the named editing path and shared undo transaction. Preview-only
   changes must not mutate room data; failed actions must not add undo entries.
2. Prove the writer and application save integration independently, including
   refusal/rollback behavior and preservation of unrelated domains.
3. Save to a disposable copy, close/reopen the disk file, and compare the named
   domains. Record input identity, output identity, code commit, and write ranges.
4. Verify the representative game behavior with an independent emulator where
   applicable. Capture/ROM mismatches invalidate parity evidence.
5. Complete the platform/artifact acceptance required by the release plan.

Existing synthetic, serializer, CLI, and visual tests remain valuable evidence.
Do not silently upgrade their scope to packaged GUI save acceptance. The
[formal plan](../../internal/plans/editor-capability-parity-plan.md) lists the
next implementation slices; do not duplicate its task state here.

## Other products

`z3ed` provides scriptable inspection, guarded edits, validation, snapshots, and
agent workflows. See the [z3ed CLI guide](../usage/z3ed-cli.md). A CLI writer test
does not promote the corresponding desktop workflow automatically.

The web build uses browser storage/downloads and does not include the emulator.
It requires separate browser acceptance; see the [Web App guide](../usage/web-app.md).
