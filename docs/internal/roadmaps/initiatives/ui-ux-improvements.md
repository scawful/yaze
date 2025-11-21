# UI/UX Refresh Plan – YAZE

Topic seed: **UI/UX & Productivity** from the keep-chatting playbook.

## Mission
Raise designer productivity by tightening keyboard shortcuts, panel ergonomics, and feedback loops
across the major editors (Overworld, Dungeon, Sprite, Graphics). These notes convert the “keep
chatting” prompts into actionable TODOs.

## Quick Wins (≤1 hour each)
1. **Shortcut Audit**
   - Inventory existing accelerators in `src/app/gui/shortcuts.cc`
   - Propose missing combos (tile palette toggle, zoom reset, card cycling)
   - Doc update: `docs/public/shortcuts.md` (needs creation)

2. **Status Chips & Queues**
   - Add build/CI watcher indicator to the Agent panel (ImGui badge)
   - Highlight active editor cards with unsaved changes (color-coded tabs)

3. **Panel Presets**
   - Save/restore layouts per editor (Overworld vs Dungeon)
   - Hook into `imgui.ini` serialization; provide preset files under `assets/layouts/`

4. **Productivity Telemetry**
   - Surface FPS/patch latency from emulator telemetry in the UI footer
   - Feed data into SSP dashboards when available

## Mid-Scope Tasks
- **“Quick Command” Palette**
  - Global search/command launcher (similar to VS Code Ctrl+P)
  - Source: `src/app/gui/command_palette.*` (new)
  - Integrate with automation tasks (z3ed prompts, agent macros)

- **Contextual Help Cards**
  - Markdown popovers using `docs/public/help/*.md`
  - Trigger via `Shift+?` or toolbar icon

- **UI Consistency Checklist**
  - Align typography/spacing across editor toolbars
  - Document color tokens in `docs/internal/ui/theme-guidelines.md`

## Long-Term Initiatives
1. **Keyboard Macro Recorder**
   - Record tool sequences for repetitive sprite/dungeon edits
   - Save macros per ROM session

2. **Competition Overlay**
   - “Leaderboard HUD” inside the Agent panel showing current points + active challenges
   - Tie into `docs/internal/agents/agent-leaderboard.md`

3. **Zarby Parity Tracker**
   - Comparison matrix vs `Zarby89/ZScreamDungeon`
   - Highlight features where YAZE is behind / ahead

## Next Steps / Owners
- **Shortcut audit** – volunteer needed (Gemini Flash?)
- **Layout presets** – Claude Core candidate (requires ImGui familiarity)
- **Command palette** – Codex + Claude pair once automation backlog clears

Log progress on the coordination board under “UI/UX Refresh” so we can award busy-task points.
