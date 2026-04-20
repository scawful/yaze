#Editor UI Module Pattern(RFC)

Date: 2026-04-06  
Status: Adopted for new editor UI work

## Goal

Replace ad-hoc `_panel.h` naming and broad `panels/` growth with a feature-oriented
UI module layout that scales for dungeon and overworld completion.

## Why this exists

- `_panel.h`/`panels/` made ownership blurry and encouraged copy-pasted access code.
- Large editor files accumulated mixed responsibilities (UI composition, editor state,
  persistence hooks, and save policy checks).
- We need a pattern that keeps migration incremental while preserving current behavior.

## Module Layout

For each editor feature, group UI code under a module folder and keep panel classes
co-located with feature logic.

```text
src/app/editor/<editor-domain>/ui/<feature-module>/
  <feature-module>_view.h
  <feature-module>_view.cc
  <feature-module>_state.h           # optional, view-local state
  <feature-module>_commands.h        # optional, intent/actions
```

Use `view` for ImGui-facing components and avoid `_panel` in new file names.

## Required rules for new code

- New UI files under editor domains must use `ui/<feature-module>/...` layout.
- No new `src/app/editor/**/panels/*.h|*.cc` files.
- No new `*_panel.h|*_panel.cc` files.
- Avoid direct concrete-editor `dynamic_cast` in view code; prefer typed accessors
  or interfaces provided by the owning editor.

These are CI-enforced by `scripts/dev/editor-guardrails.sh`.

## Migration guidance

- Keep existing `panels/` code working; migrate in slices, not big-bang rewrites.
- Migrate by feature cluster (example: overworld canvas, then tile selection, then map properties).
- Each migration should:
  - preserve panel/card IDs and shortcuts,
  - keep behavior parity,
  - avoid introducing new concrete downcasts.

## Current applied examples

- `src/app/editor/overworld/panels/overworld_panel_access.h` centralizes typed editor
  access so individual views no longer repeat `ContentRegistry` + `dynamic_cast`.
- `src/app/editor/system/hack_manifest_save_validation.{h,cc}` demonstrates shared
  cross-editor save-policy extraction out of editor implementations.

## Responsive ImGui Chrome Anti-Patterns

These are now treated as code smells in editor UX work:

- Long `ImGui::SameLine()` chains used to force entire toolbars onto one row.
- Repeated `SetCursorPosX()` or pixel nudging to fake right alignment.
- Hard-coded width caps per control cluster instead of sizing by region.
- Pane headers that each invent their own spacing, segment sizing, and collapse
  button placement.
- Context menus that bury frequent actions under multiple submenu layers.

### Why this is bad

- It breaks under font scale, icon width changes, DPI changes, and theme changes.
- It creates clipping and overlap instead of graceful reflow.
- It makes narrow layouts unusable and increases cognitive load.
- It spreads one-off width heuristics across files, which makes later cleanup harder.
- It encourages adding more exceptions instead of fixing the layout model.

### Preferred patterns

- Use stacked rows or a small `ImGui::Table` with clear regions instead of forcing
  every control into one horizontal strip.
- Keep one responsibility per row:
  - identity and mode controls
  - compare/split controls
  - view toggles and overflow actions
- Share pane-header chrome helpers so `Browse`, `Inspect`, and similar shelves use
  the same spacing and collapse behavior.
- Use compact overflow popups for secondary actions instead of clipping buttons.
- Put frequent canvas actions at the top level of the context menu.
- Limit nesting for common actions like reporting, opening related tools, and copy
  helpers.

### Refactor triggers

A UI surface should be considered for layout refactor when any of these appear:

- more than three consecutive `SameLine()` layout dependencies
- manual `SetCursorPosX()` alignment in normal toolbar/header code
- multiple width thresholds controlling one strip of controls
- repeated overlap/clipping fixes for the same surface
- context-menu actions nested two or more levels deep for normal testing flows

## Pre-ROM (“no cart”) editor session

Users can open **Graphics → Prototype Research** and **Assembly → code editor**
before a ROM is loaded (welcome **Quick Actions**, or the same `SwitchToEditor` +
`OpenWindow` flow elsewhere).

Implementation (desktop):

| Piece | Role |
|-------|------|
| `EditorRegistry::UpdateAllowedWithoutLoadedRom` | Returns true for `kGraphics` and `kAssembly`; extend here if another editor is safe with no ROM. |
| `EditorManager::EnsureEditorAssetsLoaded` | When the active session has no loaded ROM, still runs `InitializeEditorForType` for allowed types so `WindowContent` panels register; skips `Load()` / game data. |
| `SessionCoordinator::UpdateSessions` | Processes **unloaded** sessions only for `active_session_index_`, and only runs `editor->Update()` for editors that pass `UpdateAllowedWithoutLoadedRom`. Overworld jump-to-tab → dungeon is gated on `rom_loaded`. |
| `UICoordinator` ctor | Wires `WelcomeScreen::SetOpenPrototypeResearchCallback` / `SetOpenAssemblyEditorNoRomCallback` to `SwitchToEditor`, `OpenWindow` (`graphics.prototype_viewer`, `assembly.code_editor`), and hides the welcome shell. |
| `GraphicsEditor::DrawPrototypeViewer` | Shows a short note when no ROM is loaded; import/clipboard paths still run. |

`EditorManager::SetupWelcomeScreenCallbacks` also registers the same callbacks on
`EditorManager::welcome_screen_` for parity; the **visible** welcome is owned by
`UICoordinator`.

## Next migration targets

- Start introducing `ui/` module folders for new dungeon and overworld UI additions.
- Move one feature at a time from `panels/` into module-based views as files are touched.
- Keep `editor_manager` focused on orchestration by moving additional name/category lookup
  and small policy helpers into `EditorRegistry`/system components.
