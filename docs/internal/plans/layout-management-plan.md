# Layout Management Plan (ImGui + Panels)

**Date:** 2026-01-23  
**Owner:** imgui-frontend-engineer  
**Status:** Draft  

## Why this exists
Layouts currently feel unpredictable because we have overlapping sources of truth:

- ImGui docking state in `imgui.ini` (default path = current working directory).
- `LayoutPresets` (DockBuilder) used only on first init or reset.
- `WorkspaceManager` presets (ImGui ini snapshots only).
- `LayoutManager` in-memory named layouts (visibility + ImGui ini string).
- `UserSettings` has saved visibility/pinned/layout maps, but they are not wired.

This plan consolidates layout behavior so:
- default layouts are deterministic,
- reset is reliable,
- user customizations persist cleanly,
- layouts can be scoped (global vs project vs per-editor).

## Current State Summary (Key Findings)
1. **ImGui ini file lives in cwd**  
   We never set `ImGuiIO::IniFilename`, so ImGui writes `imgui.ini` in the current working directory. This often overrides presets and makes resets unreliable.

2. **Reset does not clear ImGui docking state**  
   `ResetCurrentEditorLayout()` triggers DockBuilder rebuild but does *not* clear ImGui’s ini state. Saved docking can immediately reassert itself.

3. **Workspace presets save only ImGui ini**  
   `WorkspaceManager::Save/LoadWorkspacePreset()` uses ImGui ini only, ignoring panel visibility/pinned state.

4. **LayoutManager named layouts are in-memory only**  
   `SaveCurrentLayout()` stores docking + panel visibility in memory only; not persisted to disk.

5. **UserSettings contains layout info but is unused**  
   `panel_visibility_state`, `pinned_panels`, `saved_layouts` are saved/loaded but not applied during layout load or editor switch.

## Goals
- **Deterministic defaults:** Layout presets always produce expected initial layout.
- **Predictable reset:** Reset reliably clears docking + panel state.
- **Persistence clarity:** Users can understand and control where layouts persist.
- **Scoping:** Support global layout, per-project layout, and per-editor layout.

## Plan

### Phase 1 — Stabilize Reset + Defaults (Quick Wins)
1. **Set ImGui ini path explicitly**
   - Set `ImGuiIO::IniFilename` to a controlled location.
   - Proposed path: `~/.yaze/layouts/global.ini` (or platform-config dir).
2. **Reset should clear docking state**
   - Use `ImGui::ClearIniSettings()` and delete active ini file on reset.
   - Rebuild dock layout afterward using `LayoutPresets`.
3. **Wire panel visibility persistence**
   - On panel visibility change: update `UserSettings::panel_visibility_state`.
   - On editor/session switch: restore panel visibility from `UserSettings`.
4. **Make defaults authoritative**
   - Ensure DockBuilder runs after any ini-reset so default positions take effect.

### Phase 2 — Unify Workspace Layouts (Mid Effort)
1. **Single layout save format**
   - Persist **panel visibility**, **pinned state**, and **ImGui ini** as one layout object.
   - Store in JSON at `~/.yaze/layouts/*.json` (or `.yaze/layouts/` per project).
2. **Replace WorkspaceManager ini-only presets**
   - Rewire `Save/LoadWorkspacePreset()` to use LayoutManager persistence.
3. **Expose layout scopes**
   - Global layout (default)
   - Per-project layout
   - Per-editor layout

### Phase 3 — UX & Safety (Follow-up)
1. **UI clarity**
   - “Reset Layout” should always do the same thing (clear ini + rebuild).
   - “Save Layout As…” and “Restore Layout” should use unified layout format.
2. **Automatic recovery**
   - If layout load fails, fallback to default preset and log toast.
3. **Optional: Layout versioning**
   - Include a layout schema version for future migrations.

## Suggested Layout Data Shape (JSON)
```json
{
  "name": "Overworld Default",
  "scope": "global",
  "editor_type": "Overworld",
  "panel_visibility": {
    "overworld.canvas": true,
    "overworld.tile16_editor": true
  },
  "pinned_panels": {
    "overworld.canvas": false
  },
  "imgui_ini": "....raw ini string..."
}
```

## ImHex Reference (Local)
ImHex source tree located at:
`/Users/scawful/src/third_party/forks/ImHex`

Suggested files to inspect:
- `lib/libimhex/source/*` (layout or settings handling)
- `lib/libimhex/include/hex/api/*`

## ImHex Behavior Notes (from code)
- **No automatic imgui.ini**: `io.IniFilename = nullptr` in `main/gui/source/window/window.cpp`.
- **Custom ImGui settings handler**: Adds `[ImHex][General]` section to ImGui ini text and uses `LayoutManager::onLoad/onStore` to persist app-specific state (e.g., MainWindowSize, view open state).
- **Layout files (`.hexlyt`)**: Stored under `paths::Layouts` (macOS: `~/Library/Application Support/imhex/layouts`). Saved via `ImGui::SaveIniSettingsToDisk()`.
- **Workspaces (`.hexws`)**: JSON with `{name, layout, builtin}` where `layout` is ImGui ini text from `SaveIniSettingsToMemory()`.
- **Load flow**: `LayoutManager::closeAllViews()` then `ImGui::LoadIniSettingsFromMemory()`; `WorkspaceManager::process()` applies new workspace layout.

## Open Questions
1. Should we default to per-project layouts or global layouts?
2. Should global layout be applied before project layout or be separate?
3. Do we want one layout per editor per project, or a single layout bundle?

## Success Criteria
- Reset Layout always yields the intended preset.
- No surprise panel jumps after reload.
- User layouts restore both visibility and docking consistently.
