# Editor readiness and feature coverage

Last reviewed: 2026-09-22 for the v0.8.0 development line.

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
| Dungeon room authoring | **Partial** | Existing room elements and guarded save, plus the candidate slices below | Header/chest undo, atomic mixed-domain operations, clipboard capacity handling, complete room operations, and GUI-to-disk/runtime acceptance remain. DA-1 and DA-2 are not complete. |
| Workbench tile-object placement improvements | **Candidate** | `7ba7d76ce`: live placement controls, once/repeat, selected inserted object, Place another, physical sizes; Source + Focused | Isolated candidate ROM and runtime validation, integration, and packaged acceptance remain. See the [validation handoff](../../internal/agents/dungeon-workbench-placement-handoff-2026-09-22.md). |
| Door/sprite/pot-item undo and shared properties | **Candidate** | `a730d6557`: DA-1 domain snapshots/restore and DA-2 shared Workbench/standalone inspector; render invalidation, reserved-sprite validation, paste selection, and deferred input isolation have focused checks | Context-mismatched connected-view editing is gated. ROM save/reopen, runtime, CI, installation, and release qualification remain; other DA-1/DA-2 domains are open. |
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
