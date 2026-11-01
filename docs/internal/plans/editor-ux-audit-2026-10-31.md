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

## Remaining findings

| Editor | Finding | Where | Rule | Lane | Status |
|---|---|---|---|---|---|
| Dungeon | Shortcut help lists `G` (grid) and `L` (layers); neither key is bound | `menu/right_drawer_manager.cc:1875-1876` | 10 | Codex shell | Checked |
| Dungeon | `P` place tool registered without a handler; `D` deletes with no confirm or undo | dungeon shortcuts | 5, 9 | Codex sequence | Audit |
| Screen | Draw/Edit buttons in the dungeon-map toolbar set a mode that only the title-screen and overworld-map canvases read | `graphics/screen_editor.cc:962-968` | 4 | open | Checked |
| Screen | Dungeon-map Save is always disabled (has a reason tooltip); Copy/Paste Floor do nothing | `graphics/screen_editor.cc:970-977` | 4 | open | Checked (Save), Audit (floor) |
| Palette | Reset All Palettes discards every change with no confirm; Apply has no outcome toast | `palette/palette_group_panel.cc:652-655` | 5, 6 | open | Checked |
| Message | Sync status computes an error/success color and never uses it | `message/message_editor.cc:1191-1193` | 1, 6 | open | Checked |
| Message | Editing tools sit inside the Font Atlas window | `message/message_editor.cc` | — | open | Audit |
| Music | Note keys fire without checking modifiers | `music/tracker_view.cc:618` | 9 | open | Checked |
| Music | Popups re-seed every frame and cannot commit; Space, Esc, `=`, `-` run twice | `music/` | 9, 11 | open | Audit |
| Sprite | Editor shortcuts are not focus-gated | `sprite/` | 9 | open | Audit |
| Graphics | Pending-save strip omits the sheet-save state; tooltip always on; no wheel zoom; no list clipper; file dialog results unchecked | `graphics/` | 6, 12 | open | Audit |
| Emulator | Ctrl+R, Ctrl+Tab, F1 collide with global bindings; two breakpoint lists; "Coming Soon" controls | `emu/` | 4, 10 | open | Audit |
| Assembly | Ctrl+B collides with the activity-bar toggle; no dirty tracking; Find does nothing; `.IgnoreError()` on user actions | `code/` | 6, 7, 10 | open | Audit |
| Hex | Search not implemented; toolbar drawn before `Begin`; a cancelled Compare loads an empty path | hex editor | 4, 6 | open | Audit |
| Agent | LLM call blocks the UI thread; Clear has no confirm; 46 color literals | `agent/` | 1, 5 | in-flight WIP (z3cli) | Audit |
| Planning | Removals have no confirm; the ID field renames on every keystroke | `planning/` | 5, 11 | open | Audit |
| Settings | Search covers only the active tab; sliders call `Save()` every tick; API keys stored in plain text | `shell/windows/settings_panel.cc` | 6 | Codex shell | Audit |
| Overworld | Keys not hover-gated, disabled controls without reasons, raw errors in the Tile16 editor, about 53 color literals | `overworld/` | 1, 4, 6, 9 | needs green light | Audit |

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
- Open policy questions: whether recent-file, CLI, and drag-drop opens show the
  load-options dialog for a ROM with no remembered choice (today they skip it);
  whether to wire or remove the dialog's upgrade checkbox (no caller sets its
  callback); whether `kLoadCustomOverworld` and `kEnableSpecialWorldExpansion`
  stay in the dialog (nothing reads them at runtime).
