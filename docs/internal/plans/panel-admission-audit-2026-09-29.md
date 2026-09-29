# Panel Admission Audit (2026-09-29)

Status: ACTIVE
Owner: `imgui-frontend-engineer`
Created: 2026-09-29
Base: `origin/master` 403fa148f (PR #262)

## Result

Before this change no panel outside the Dungeon editor declared a
`WindowPresentationPolicy`, and nothing checked that a new panel chose one.
The default is a listed optional pop-out, so an undeclared panel became a
first-class window. Every registered window now declares a policy, a test and
a source audit fail when one does not, and eleven clearly diagnostic or stub
windows left the window browser. The rule and enforcement are in
[editor-ui-module-pattern.md](../architecture/editor-ui-module-pattern.md#panel-admission).

| | Count |
|---|---|
| Registered windows (ids; the three dynamic room and song patterns count once each) | 115 |
| Declared before | 17 (Dungeon only; the room-list one was a hand-built literal the new audit rejects) |
| Declared now | 102 |
| Deferred (Overworld, Codex-coordinated) | 13 |
| Demoted now | 11 (Agent 6, Oracle 2, Graphics 1, Screen 1, Emulator 1) |

Demoted windows stay reachable: Tools > Hack Workflows (agent/oracle), the
welcome screen action (Prototype Research), and the command palette for all of
them. Revision 26 of the layout defaults closes their persisted visibility;
pins, saved layouts, and named layouts are untouched.

## Table

Policy: Core, Pop-out (OptionalPopOut), Embedded, Diagnostic, or
`deferred` (declaration blocked, see Overworld). Open: where the window is
open on first use. Effort: S = policy and migration only, M = needs a host
surface wired, L = new surface. Rows marked **Codex** sit in
`src/app/editor/overworld/*` or its in-flight branches
(`codex/overworld-large-selection-preview`, `codex/terrain-catalog`); they are
proposals only.

### Dungeon

Declared in #262 (`dungeon_editor_v2.cc`); this change only added hosts and
fixed two undeclared cases. The Workbench and Navigator surfaces are unchanged.

| Window | Policy now | Open | Proposal |
|---|---|---|---|
| dungeon.workbench | Core | yes | keep |
| dungeon.room_selector | Pop-out, unlisted | no | keep; Navigator hosts it |
| dungeon.entrance_properties | Pop-out | no | unlist; Navigator "Entrances" tab hosts it (S) |
| dungeon.room_matrix | Pop-out | no | unlist; Navigator "Matrix" tab hosts it (S) |
| dungeon.room_graphics, object_selector, door_editor, palette_editor, sprite_editor, item_editor, custom_collision, water_fill, room_tags, minecart_tracks | Embedded (Workbench inspector) | no | keep; Ctrl+Shift+G and Ctrl+Shift+Alt+P still open two standalone |
| dungeon.object_coverage | Diagnostic (canvas menu) | no | keep |
| dungeon.object_tile_editor | Embedded (canvas selection menu) | no | add a Workbench tool tab or canvas popup (M) |
| dungeon.overlay_manager | Embedded (command palette) | no | fold into the toolbar "Canvas view options" popup, then delete (M) |
| dungeon.room_N (dynamic) | Pop-out | on demand | keep; standalone workflow only |

Named presets Minimal, Modder, and Dungeon Master list standalone dungeon ids
and no Workbench, so applying one closes the Workbench (S, needs Codex).

### Overworld (Codex-coordinated; proposals only)

All 13 are `REGISTER_PANEL` classes with no policy. They are listed in
`DeferredPolicyDeclarations()` in `panel_admission.cc`; delete each entry in
the change that declares its policy.

| Window | Open | Proposal |
|---|---|---|
| overworld.canvas | yes | Core |
| overworld.tile16_selector | yes | Pop-out, keep default |
| overworld.properties | yes | Embedded, host "Canvas context menu > Map Properties..." (already opens it; M) |
| overworld.tile16_editor | no | Embedded, host "Canvas context menu > Edit Tile16..." (S) |
| overworld.tile8_selector | no | Embedded, host Tile16 Editor source pane (S) |
| overworld.area_graphics | no | Embedded, host Map Properties (already edits area graphics; S) |
| overworld.scratch | no | Pop-out, keep |
| overworld.gfx_groups | no | Embedded or remove: same `GfxGroupEditor` as graphics.gfx_group_editor (S) |
| overworld.usage_stats | no | Diagnostic (S) |
| overworld.item_list | no | Embedded, host entity context menu (M) |
| overworld.v3_settings | no | Embedded, host Map Properties Area Size; nothing opens it today (S) |
| overworld.debug | no | Diagnostic; frame-time readout only (S) |
| overworld.entity_workbench | no | Embedded, host "Canvas entity context menu" (already uses the content; S) |
| overworld.cutscene_camera (cutscene/) | no | declared Pop-out; propose Embedded, host canvas context menu (M) |

### Graphics, Screen, Sprite, Message

| Window | Policy now | Open | Proposal |
|---|---|---|---|
| graphics.sheet_browser_v2 | Pop-out | yes | keep |
| graphics.pixel_editor | Core | yes | keep |
| graphics.usage_preview | Pop-out | no | Embedded, host Pixel Editor preview pane; nothing opens it today (M) |
| graphics.palette_controls | Pop-out | no | Embedded, host Pixel Editor palette row (M) |
| graphics.link_sprite_editor, gfx_group_editor, paletteset_editor | Pop-out | no | keep |
| graphics.polyhedral | Pop-out | no | Diagnostic (S) |
| graphics.prototype_viewer | Diagnostic (welcome action) | no | demoted |
| screen.dungeon_maps, inventory_menu, overworld_map | Pop-out | dungeon_maps per preset | keep |
| screen.title_screen | Pop-out | yes (opened by `Initialize`) | preset says dungeon_maps; pick one (S) |
| screen.naming_screen | Diagnostic (palette) | no | demoted: stub draws nothing |
| sprite.vanilla_editor | Core | yes | keep |
| sprite.custom_editor | Pop-out | no | keep |
| sprite.catalog | Pop-out | no | Embedded, host Custom Sprite import (M) |
| message.message_list | Pop-out | yes | keep |
| message.message_editor | Core | preset | keep |
| message.font_atlas, dictionary | Pop-out | no | tabs in the Message editor (M) |

### Palette

| Window | Policy now | Open | Proposal |
|---|---|---|---|
| palette.control_panel | Core | yes | keep; hub for the group windows |
| palette.ow_main | Pop-out | preset | keep |
| palette.ow_animated, dungeon_main, sprites_aux1-3, quick_access, custom, global_sprites, armors | Pop-out | no | Embedded, host "Palette Controls checklist" (S, after the id fix below) |
| palette.sprites, palette.equipment | Pop-out (descriptor only) | no | ghost windows: no content is registered under these ids; alias them to global_sprites and armors (S) |

### Music, Assembly, Emulator, Hex

| Window | Policy now | Open | Proposal |
|---|---|---|---|
| music.song_browser, tracker | Pop-out | yes (auto-show) | keep |
| music.piano_roll | Core | yes | keep |
| music.instrument_editor, sample_editor | Pop-out | no | keep |
| music.assembly | Pop-out | no | Embedded, host Assembly editor (S) |
| music.song_N, piano_roll_N (dynamic) | Pop-out | on demand | keep |
| assembly.code_editor | Core | yes | keep |
| assembly.file_browser, disassembly | Pop-out | yes | keep |
| assembly.toolbar | Pop-out | yes | fold Open Folder and Save into the file browser header (S) |
| assembly.symbols, build_output | Pop-out | no | keep |
| emulator.ppu_viewer | Core | yes | keep |
| emulator.cpu_debugger, memory_viewer, breakpoints, performance, virtual_controller, apu_debugger, audio_mixer | Pop-out | no | keep; the emulator's own tool set |
| emulator.ai_agent | Pop-out | no | Embedded, host AI Agent drawer (S) |
| emulator.keyboard_config | Pop-out | no | Embedded, host Settings > Shortcuts (S) |
| emulator.save_states | Diagnostic (palette) | no | demoted: placeholder |
| memory.hex_editor | Core | yes | keep |

### Agent, Oracle, workflow, shell

| Window | Policy now | Open | Proposal |
|---|---|---|---|
| agent.chat | Core | yes | keep |
| agent.configuration, status | Pop-out | yes | keep |
| agent.prompt_editor, profiles, builder, knowledge | Pop-out | no | Embedded, host Agent Configuration (M) |
| agent.mesen_debug, mesen_screenshot, oracle_states, feature_flags, manifest, sram_viewer | Diagnostic (Hack Workflows) | no | demoted |
| agent.oracle_ram | Pop-out | no | Diagnostic; overlaps sram_viewer, has no workflow group (S) |
| oracle.annotations, progression_dashboard, story_event_graph | Pop-out | no | keep |
| oracle.validation, menu_inspector | Diagnostic (Hack Workflows) | no | demoted |
| workflow.output | Pop-out | no | Embedded, host Project drawer (already shows build/run status; S) |
| dashboard.main | Core | yes | keep (startup chooser) |
| yaze.about | Pop-out | no | Embedded, host Help > About popup (S) |
| layout.designer | Pop-out | no | keep |

## Decisions for scawful

1. Overworld: move Map Properties, Tile16 Editor, Item List, and Entity
   Properties behind the canvas context menu as popups (rows above). Needs your
   OK and Codex's branches settled first.
2. Overworld: fold Tile8 Selector and Area Graphics into Tile16 Editor and Map
   Properties, and drop `overworld.gfx_groups` in favor of the Graphics editor.
3. Palette: hide the ten group windows behind the Palette Controls checklist,
   after the two ghost ids are aliased.
4. Dungeon: unlist Entrances and Room Matrix now that the Navigator hosts them.
5. Dungeon: delete `dungeon.overlay_manager`; the toolbar popup covers it.
6. Screen: title screen opens on first use while the preset says dungeon maps.
7. Named workspace presets Minimal, Modder, and Dungeon Master close the
   Dungeon Workbench.
8. Emulator: hide the debugger windows too? They are the emulator's tool set,
   so this audit kept them listed.
9. Assembly: fold the toolbar into the file browser.
10. Agent: embed Prompt Editor, Profiles, Builder, and Knowledge in Agent
    Configuration.

## Findings outside admission

- Palette descriptors `palette.sprites` and `palette.equipment` have no
  content; the content classes register as `palette.global_sprites` and
  `palette.armors`. Two Palette Controls checkboxes and the Designer preset
  open empty windows.
- Dead or duplicate sources: `src/app/editor/hack/oracle/ui/*` duplicates
  `oracle/panels/*` and is not compiled; `graphics/ui/*_view.h` duplicates
  `graphics/panels/*`; `ChestEditorPanel`, `DungeonEmulatorPreviewPanel`,
  `DungeonRoomPanel`, `MusicAudioDebugPanel`, and `MusicHelpPanel` are never
  registered. `scripts/audit_panel_policies.py` ignores the stale `hack/oracle/ui`
  copies by path.
- `OpenWindow(..., "Hex Editor")` in `editor_activator.cc` and
  `menu_orchestrator.cc` matches no id; only "Memory Editor" is aliased.
- `UsageStatisticsCard`, `DebugWindowCard`, and `OverlayManagerPanel` call
  `ImGui::Begin` inside `Draw`.
- `assembly.disassembly` is default-visible in the preset but
  `IsVisibleByDefault()` is false; `assembly.toolbar` is the reverse.

## Enforcement

| Check | Runs in | Catches |
|---|---|---|
| `PanelAdmission*` in `panel_admission_test.cc` | `yaze_test_unit` (CI stable, Windows box) | undeclared, Core or default-visible without an allowlist entry, Embedded or Diagnostic without a host, hidden windows that do not open from the palette |
| `scripts/audit_panel_policies.py` | `editor-guardrails.sh` (lint job), release readiness | registered `WindowContent` classes with no policy, including ROM-only panels |
| `UserSettingsLayoutDefaultsTest.RevisionTwentySix*` | `yaze_test_unit` | demotions that would destroy pins, saved or named layouts |
| `PanelAdmissionBudgetTest` | `yaze_test_unit` | a default layout that opens more than three windows, or opens one that is not allowlisted |

Not built: an ImGuiTestEngine pass over the full app with a ROM. The unit test
cannot register panels whose editors need ROM data (Message, Music, and the
dungeon and overworld `Load()` panels); the source audit covers those classes.
