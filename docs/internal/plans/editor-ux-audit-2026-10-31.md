# Editor UX Audit (2026-10-31)

Status: ACTIVE
Owner: `imgui-frontend-engineer`
Created: 2026-10-31
Base: `origin/master` 86c9f5989

## Result

A read-through of every editor against the yaze ImGui UX rules (skill
`yaze-imgui-ux`, in `afs-scawful/skills/yaze-imgui-ux/SKILL.md`; rule numbers
below refer to it). This pass fixed the open/save findings in the shell. The
table lists what is left. Structure rules stay in
[editor-ui-module-pattern.md](../architecture/editor-ui-module-pattern.md).

`Checked` means the cited lines were re-read at the base commit. `Audit` means
the finding comes from the audit pass and needs a re-read before a fix.
Rows checked on 2026-11-01 were re-read at `f29d77fd4`, whose copies of those
files match the base.

## Fixed in this pass

- ROM load options are remembered per ROM path. A remembered choice applies
  before assets load and announces itself with a toast that offers
  "Change…"; the command palette has `ROM Load Options…`. A choice is
  dropped when the ROM's ZSCustomOverworld version byte changes. A skipped
  dialog never upgrades the ROM.
- Opening a raw ROM no longer copies default project workspace values into
  global preferences (it used to turn autosave on globally). A project's
  workspace values are an overlay that `UserSettings::Save()` does not write.
- Settings: Backup Before Save moved out of the autosave group because it
  gates every save; the auto-save group says when the project overlay is
  active; Default Editor can open Messages.
- Closing a project's last ROM clears the project workspace overlay, so
  global preferences return and later global changes save (`826553df7`).
- Interactive raw ROM opens end in one step (`FinishRawRomOpen`). File > Open
  ROM, Recent Files, and WASM drag-drop show the load-options dialog for a ROM
  with no remembered choice. Startup `--rom_file`, gRPC, the emulator
  adapter, project creation, and test helpers pass
  `LoadOptionsPrompt::kNoPrompt` and never show it.
- The load-options dialog no longer offers to apply ZSCustomOverworld ASM;
  nothing set that checkbox's callback. The dialog never changes the ROM, so
  a remembered choice cannot upgrade one. The Overworld toolbar's Upgrade
  button applies the ASM.
- The dialog keeps both overworld load options. Both are read when the
  overworld loads, so the dialog says when a changed option waits for a
  reopen.

## Qualification (2026-11-01)

- Flow tests: `EditorManagerProjectActionsTest.LoadOptionsDialog*AcrossFrames`
  (three tests, `f3c7851b8`) drive the dialog through real ImGui frames:
  Continue stores a choice and the next open skips the dialog; Cancel stores
  nothing; Continue with Remember cleared stores nothing. Local mac-ai: 70
  tests in `EditorManagerProjectActionsTest.*:RomLoadOptionsDialogTest.*`
  (69 passed, 1 skipped); the three flow tests passed 5 of 5 repeats.
- Windows (medical-mechanica, `scripts/remote/mm-test.sh`): the focused run on
  `88e418cbb` passed 126 tests; the run on `f3c7851b8` passed 129 with 0
  failures, the three flow tests included (ci-windows RelWithDebInfo).
- clang-format 22.1.5 (the CI pin): Codex's check passed all 16 changed
  C/C++ files at `523be87d4`, after an earlier pass on `319a2643a`. Files
  changed by the P1 repair below have only the local 23.1.4 check.
- Known failures that predate this pass, in `yaze_test_focus_graphics_art`:
  `InitializedOverworldPreservesUneditedBytesDuringDungeonSave` and
  `RailPaletteStampUndoSaveAsReopens` (`f7f857887`),
  `FailedRevertKeepsDungeonObjectAndUndo` (`3ccd53a5a`).
- Release baseline: scawful chose C on 2026-11-01. The five commits go into
  #338 (`codex/release-080-hardening-20261031`) through the local branch
  `claude/release-080-ux-candidate-20261101` at `523be87d4` (merge
  `e38aa7f5e` plus a CHANGELOG commit). It is not pushed. It waits on the end
  of scawful's training hold and on the Codex-Oracle-Coordinator's OK. The
  merged tree has not been built or tested.
- Codex review of `523be87d4`, P1: the load options dialog is a normal
  window, so the user could switch session tabs while it was open, and Apply
  then wrote session A's options into session B and remembered them for B's
  file. Repair: the dialog binds the stable session ID and ROM file name when
  it opens (interactive open and review). A non-transient switch, a close, a
  reload, or a rename of that ROM closes it without changes and says why in
  a toast; `DrawInterface()` and the confirm callback both refuse a stale
  owner. Per-frame transient session visits leave it open. The dialog no
  longer writes feature flags itself. Regressions:
  `EditorManagerProjectActionsTest.LoadOptions{ReviewClosesWhenAnotherRawSessionBecomesActive,ReviewClosesWhenAProjectSessionBecomesActive,ReviewClosesWhenItsRomCloses,ReviewSurvivesFramesAndAppliesToItsSession,AfterOpenClosesWhenAnOlderSessionBecomesActive,ReviewClosesWhenRomIsRenamed}`.
  Not built and not run: written under the training hold. The reload path
  (`ReplaceActiveSessionRom`) has no test.

## Remaining findings

| Editor | Finding | Where | Rule | Lane | Status |
|---|---|---|---|---|---|
| Dungeon | Shortcut help lists `G` (grid) and `L` (layers); neither key is bound | `menu/right_drawer_manager.cc:1875-1876` | 10 | Codex shell | Checked |
| Dungeon | `P` place tool registered without a handler; `D` deletes with no confirm or undo | dungeon shortcuts | 5, 9 | Codex sequence | Audit |
| Screen | Draw/Edit buttons in the dungeon-map toolbar set a mode that only the title-screen and overworld-map canvases read | `graphics/screen_editor.cc:962-968` | 4 | open | Checked |
| Screen | Dungeon-map Save is always disabled (has a reason tooltip); Copy/Paste Floor set flags nothing reads | `graphics/screen_editor.cc:970-977`, `:734-739` | 4 | open | Checked |
| Palette | Reset All Palettes discards every change with no confirm; Apply has no outcome toast | `palette/palette_group_panel.cc:652-655` | 5, 6 | open | Checked |
| Message | Sync status computes an error/success color and never uses it | `message/message_editor.cc:1191-1193` | 1, 6 | Codex Y15 | Checked |
| Message | Editing tools sit inside the Font Atlas window | `message/message_editor.cc` | — | Codex Y15 | Audit |
| Music | Note keys fire with Ctrl, Alt, or Super held, so Ctrl+Z also enters a note | `music/tracker_view.cc:617-619` | 9 | codex-music-workstation: guarded on unmerged `codex/music-workstation-20261026` | Checked |
| Music | The note popup re-seeds pitch and duration every frame, so Apply writes the old values; Space, Esc, `=`, `-` each have two handlers (Space three) | `music/tracker_view.cc:399-400`, `:711`; `music/music_editor.cc:882-892`; `system/commands/shortcut_configurator.cc:104-110` | 9, 11 | codex-music-workstation: both reworked on its branch; recheck after merge | Checked |
| Music | Export/Import ASM menu items do nothing: no caller sets their callbacks | `music/song_browser_view.cc:277-288` | 4 | codex-music-workstation: still unset on its branch | Checked |
| Sprite | `[`, `]`, Ctrl+Up/Down run every frame with no focus or text-input check, so typing `[` or `]` in the routine ASM box changes the frame; `]` has no upper bound | `sprite/sprite_editor.cc:132`, `:143-172`, `:1526` | 9 | open | Checked |
| Graphics | Pixel tooltip always on with no toggle; no wheel zoom (canvas wheel zoom is touch-only); sheet browser draws up to 223 thumbnails a frame with no clipper; CGX/SCR/COL/BIN dialogs do not check for an empty path | `graphics/pixel_editor_panel.cc:553-554`; `gui/canvas/canvas.cc:770`; `graphics/sheet_browser_panel.cc:770-771`; `graphics/graphics_editor.cc:759,794,837,927` | 6, 12 | open | Checked |
| Emulator | Tooltip promises Ctrl+R reset but nothing handles it (global Ctrl+R loads the last ROM); F1/Tab handlers sit in an uncalled function; the Breakpoints panel lists a set the CPU never checks and has no Add; "Save States - Coming Soon" | `emu/ui/emulator_ui.cc:112,708,728,737`; `emu/ui/debugger_ui.cc:246,281`; `emu/emulator.cc:1107,1319` | 4, 10 | open | Checked |
| Assembly | Ctrl+B shown for Validate, but the global activity-bar toggle owns it; edits are not unsaved work (tabs close without a prompt; Ctrl+S saves the session, not the file); Find is a TODO; 10 `.IgnoreError()` calls on user actions | `code/assembly_editor.cc:1921,1973-1974,2133,2159-2160`; `editor_manager.cc:5530` | 6, 7, 10 | open | Checked |
| Hex | Search popup shows "not implemented" and a disabled button; toolbar drawn before `Begin`; a cancelled Compare loads an empty path | `code/memory_editor.cc:81,170,191-192`, `:20-23` | 4, 6 | open | Checked (Search, toolbar); Audit (Compare) |
| Agent | LLM call blocks the UI thread; Clear has no confirm; 46 color literals | `agent/` | 1, 5 | in-flight WIP (z3cli) | Audit |
| Planning | Remove step/item act on one click with no confirm or undo; the Id field renames on every keystroke | `planning/planning_editor.cc:694-695,809-810`, `:661-662` | 5, 11 | open | Checked |
| Settings | Search covers only the active tab; sliders call `Save()` every tick; API keys stored in plain text | `shell/windows/settings_panel.cc` | 6 | Codex shell | Audit |
| Overworld | Keys not hover-gated, disabled controls without reasons, raw errors in the Tile16 editor, about 53 color literals | `overworld/` | 1, 4, 6, 9 | needs green light | Audit |
| Overworld (Tile16) | Copy and paste take one tile at a time; no range select, move/swap, or reorder | `overworld/tile16/*`, `overworld/tile16_editor.*`, `overworld/ui/tiles/tile16_*_view.*` | — | bounded green light for one workflow, after the live owner agrees on files | Checked |

## Save writers (F3)

The default-off save flags, by writer. No default has changed. A default
changes only after Save/reopen on a ROM copy and a ZScream check of the saved
copy both pass; neither has run for any row. Each writer refuses the whole
save while its edits are pending and its flag is off, so a flag that stays
off never drops an edit silently. `dungeon.kSavePalettes`, `kSaveMessages`,
and the six overworld flags are already on and are outside F3.

| Flag | Writer | What a save writes | ROM formats | Refuses | Evidence |
|---|---|---|---|---|---|
| `kSaveGfxGroups` | `zelda3::SaveGfxGroups` (`zelda3/game_data.cc:362`), from the editor save (`editor_manager_project_io.cc:950`) | Bytes that differ in four fixed tables: main blocksets (through `kGfxGroupsPointer`), room blocksets, spritesets, palettesets. Reads every byte back. | US and JP. `kVersionConstantsMap` lists SD and RANDO with no table addresses. Follows a moved main table through its pointer; never moves a table. | SD, RANDO, or an unknown version; a table past the ROM end; a read-back mismatch | Read-back on every save. No file Save/reopen test. ZScream not run. |
| `kSaveGraphicsSheet` | `GraphicsEditor::Save` → `zelda3::WriteGfxSheet` (`zelda3/gfx_sheet_storage.cc:311`) | Modified sheets only. Each is written in place, or moved into registered free space with a pointer update. | Compressed and raw 3BPP sheets; 2BPP sheets are read-only. A move needs `graphics_sheet_regions.allocation_regions` in the project's hack manifest; the ROM never grows. | 2BPP sheets; sheets and 16x16 blocks the project reserves; a sheet that fits neither in place nor in registered free space; a read-back mismatch | `GraphicsSheetCodecRoundTripTest` (6 ROM-backed cases, including save/reload and adjacent data); `gfx_sheet_storage_test.cc` (11). ZScream not run. |
| `kSaveDungeonMaps` | `ScreenEditor` save → `zelda3::SaveDungeonMaps` (`zelda3/screen/dungeon_map.cc:253`) via `BuildDungeonMapWritePlan` | Repacks all 14 maps (room and gfx streams; floor, boss-room, and boss-floor tables) inside the vanilla map region, in one ROM transaction | Vanilla Tile16 table (marker 0xB9 at `kDungeonMapExpCheck`) or a relocated one (marker 0x20) | Other markers; fewer than 14 maps; bad floor counts; a boss room on no floor; streams over capacity; overlapping writes | `ScreenEditorSaveTest` (8 ROM-backed cases). ZScream not run. Loses data; see below. |
| `kSaveAllPalettes` | none of its own | Nothing. It authorizes the Palette editor's save when `dungeon.kSavePalettes` is off (`editor_manager_project_io.cc:769`). `zelda3::SaveGameData` reads it, but nothing calls `SaveGameData`. | — | — | With `dungeon.kSavePalettes` on by default, turning it on changes nothing. |
| `kSaveWithChangeQueue` | none | Nothing; no reader in `src/` | — | — | — |

Dungeon maps lose data on any save. The repack writes each floor word as
`(map.nbr_of_floor << 4) | map.nbr_of_basement` (`dungeon_map.cc:195-196`),
which zeroes the high byte; the loader reads only the low byte
(`dungeon_map.cc:44`, `:47`). In `roms/alttp_vanilla.sfc` the high byte is 02
for dungeons 7 and 9 and 01 for dungeons 10, 12, and 13, so a save zeroes
0x575E8, 0x575EC, 0x575EE, 0x575F2, and 0x575F4.

The high byte holds pause-map backdrop flags (traced in usdasm `d53311a`,
`bank_0A.asm:4351-4493` and `:7504-7518`). The table is
`DungeonMapFloorCountData` ($0A:F5D9).
`Module0E_03_01_02_DrawFloorsBackdrop` ($0A:E1F3) reads each entry as a word:

- Bit 0 (`AND #$0100` at $0A:E207) draws mountain stripes behind the floors
  (`DungeonMap_MountainStripes`). Vanilla sets it for Tower of Hera, Turtle
  Rock, and Ganon's Tower (dungeons 10, 12, 13).
- Bit 1 (`AND #$0200` at $0A:E29B) adds $0400 to each floor-gradient tile,
  which moves the tile to the next palette. The disassembly names the branch
  `.not_ocean`. Vanilla sets it for Misery Mire and Ice Palace (dungeons 7, 9).

A yaze save therefore removes those backdrops from the pause-menu map.
ZScream writes the same word (`ZeldaFullEditor/Gui/MainTabs/ScreenEditor.cs:2313-2315`,
`ROM.WriteShort`), so a ZScream save loses them too. Not run in an emulator.

`ScreenEditorSaveTest.RoundTrip_NoModification` compares decoded
rooms for three dungeons, not bytes, so it cannot see the loss. ZScream's
boss floors differ from yaze's in 9 dungeons (scratch Python model of the
repack, not a yaze test).

ZScreamCLI covers little of F3. `validate-yaze --feature=graphics` compares
only the 37 main blocksets (`RomComparer.CompareGraphicsRegions`; the room,
sprite, and palette tables are left as a TODO there). ZScreamCLI has no
graphics-sheet or dungeon-map writer or comparison.

Needed before a default change:

1. Gfx groups: a file Save/reopen test on a ROM copy. Then
   `validate-yaze --feature=graphics` against a ZScreamCLI-saved copy, plus a
   byte compare of the room, sprite, and palette tables.
2. Graphics sheets: open the saved copy in ZScream, or add a sheet comparison
   to ZScreamCLI.
3. Dungeon maps: load the high byte into `DungeonMap` and write it back. Then
   a byte-level Save/reopen test that checks 0x575E8-0x575F4, and a ZScream
   open of the saved copy. Whether the Screen editor shows the two flags as a
   backdrop control is scawful's call.
4. All palettes and change queue: remove or merge; the owner decides.

## Cross-cutting

- `scripts/agents/imgui-literal-audit.sh` fails at the base commit:
  `ImVec4(` 426 of 417, `IM_COL32(` 173 of 133, `ImGui::PushStyleColor` 7
  of 0. No CI job or hook runs it.
- `src/app/editor/hack/oracle/ui/*.h` duplicate `src/app/editor/oracle/panels/*.h`
  and nothing includes them. They add 10 `ImVec4(` and 15 `IM_COL32(` to the
  budget. Either finish the move or delete them; the owner decides.
- Helpers that would remove repeated code: `DisabledHint` (rule 4),
  `StatusLine` (rule 6), `SaveScopeBanner` (rule 7), an editor key guard
  (rule 9), `SearchBox`, `ZoomControl`. Confirmation already has
  `gui::ConfirmationDialog`.
- Overworld load flags (traced): `kLoadCustomOverworld` is read when the
  overworld loads (`zelda3/overworld/overworld_map.cc:128`). On a vanilla
  ROM it fills the ZSCustomOverworld fields with vanilla-equivalent values; a
  ROM with the ASM applied ignores it. `kEnableSpecialWorldExpansion` gates
  maps 0xA0-0xBF together with `HasExpandedPointerTables()`
  (`zelda3/overworld/overworld.cc:935`, `:1174`). The `:1174` check runs when
  a map is first built, so part of a change applies before a reopen. Its help
  text lists ZSCustomOverworld v3 ASM as required
  (`gui/app/feature_flags_menu.h:74-75`); the code checks only the
  pointer-table marker.
- `kApplyZSCustomOverworldASM` has no runtime reader. The Feature Flags menu,
  the WASM bridge, and project files store it; nothing applies ASM from it.
  Remove it or wire it; the owner decides.
- Open-path gaps that remain:
  1. File > Open ROM (`LoadRomInternal`) skips `CheckRomOpenPolicy`;
     `OpenRomOrProject` runs it (`editor_manager.cc:3501`).
  2. File > Open ROM moves the ROM to the top of Recent Files
     (`editor_manager_project_io.cc:669`); the raw branch of
     `OpenRomOrProject` (menu Recent Files, WASM drop) does not.
  3. The main window ignores native drag-drop; only the standalone emulator
     reads `DropFile` (`emu/emu.cc:221`).
  4. A raw ROM opened while a project is open never asks; it uses the
     project's flags.
  5. "Create associated project file" starts checked for a path missing from
     Recent Files, such as the first File > Open ROM of a new path.
     "Remember for this ROM file" starts checked whenever the dialog asks
     (`rom_load_options_dialog.h:60`). One Continue with both defaults stores
     a choice, so the dialog stops asking for that file (flow test
     `LoadOptionsDialogContinueRemembersChoiceAcrossFrames`, run with the
     project box cleared), and creates `<rom_base>_project.yaze` beside the
     ROM, because the dialog leaves the project path empty
     (`ResolveProjectCreationPath`, `project_manager.cc:344`). On desktop a
     second create for that ROM fails with "Project file already exists". On
     WASM the project goes to browser storage (`WasmStorage::SaveProject`,
     `core/project.cc:2468`) and the existing-file check is compiled out
     (`project_manager.cc:374`); because of gap 2, a dropped file starts
     with the box checked on every open that asks. The project-file
     behavior is traced, not run.
