# Editor UI Module Pattern (RFC)

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

## Next migration targets

- Start introducing `ui/` module folders for new dungeon and overworld UI additions.
- Move one feature at a time from `panels/` into module-based views as files are touched.
- Keep `editor_manager` focused on orchestration by moving additional name/category lookup
  and small policy helpers into `EditorRegistry`/system components.
