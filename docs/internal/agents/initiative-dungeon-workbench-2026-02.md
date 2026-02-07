# Dungeon Workbench + Panel System Overhaul (2026-02)

Status: ACTIVE
Owner: imgui-frontend-engineer
Created: 2026-02-07
Last Reviewed: 2026-02-07
Next Review: 2026-02-14

## Summary
- Lead agent/persona: imgui-frontend-engineer
- Supporting agents: ai-infra-architect (PanelManager behavior + persistence), zelda3-hacking-expert (selection semantics + ROM data), test-infrastructure-expert (ImGui test harness)
- Problem statement: Dungeon editing is currently split across multiple panels with tall always-on controls, brittle per-room window behavior, and weak defaults. Users want a more “workbench-like” flow with stable layout, good defaults, and power-user navigation.
- Success metrics:
  - A single default “Dungeon Workbench” layout is usable without manual docking.
  - Room navigation never moves/respawns windows unintentionally.
  - Inspector-driven UI reduces vertical chrome vs. always-on properties.
  - PanelManager supports explicit panel scopes so defaults and persistence are predictable.
  - A command palette makes navigation/panel actions discoverable and fast.

## Scope
- In scope:
  - (1) Room header polish (pin placement + compact controls) and immediate layout fixes.
  - (2) “Room Workbench”: one stable window that hosts the room canvas + supporting panes.
  - (3) Context-sensitive Inspector replacing tall always-on controls.
  - (5) Panel scopes + better default rules for visibility/pinning/persistence.
  - (6) Room tabs: MRU ordering, split view, and predictable focus.
  - (7) Command palette for room navigation + panel actions.
- Out of scope:
  - New ROM editing features (new data formats, new editors) unrelated to the UI/UX shape.
  - Deep refactor of the underlying dungeon rendering pipeline.
- Dependencies / upstream projects:
  - `gui::PanelWindow` / PanelManager behavior + settings persistence
  - Dungeon selection model (`DungeonCanvasViewer` + interaction coordinator)

## Risks & Mitigations
- Risk: ImGui docking + per-frame window ID churn can corrupt layout/persistence.
  - Mitigation: enforce stable window identities (slot IDs) and reduce multi-window churn by moving to Workbench.
- Risk: “Workbench” becomes too opinionated and blocks advanced workflows.
  - Mitigation: ship layout presets and allow restoring legacy multi-window mode initially.
- Risk: Panel scopes introduce regressions in existing editors.
  - Mitigation: add scope defaults as “no behavior change” until each editor opts in; add focused tests for PanelManager rules.

## Testing & Validation
- Required test targets:
  - `cmake --build build_agent --config RelWithDebInfo --target yaze`
  - `ctest --test-dir build_agent -C RelWithDebInfo -L stable --output-on-failure`
  - Add/update an ImGui e2e smoke test for dungeon panel persistence + navigation stability.
- ROM/test data requirements:
  - Known-good ALTTP ROM (and optionally an OOS project for stress).
- Manual validation steps:
  - Open Dungeon editor, open a room, dock/float workbench/panels.
  - Navigate rooms via arrows/hotkeys/command palette; verify window positions persist.
  - Switch editors and back; verify scope rules (which panels persist, which close).
  - Split view: edit in one room, compare against another; verify selection/tools target the intended pane.

## Documentation Impact
- Public docs to update:
  - Release notes / changelog entry once Workbench is on by default.
- Internal docs/templates to update:
  - This initiative file
  - Coordination board entry
- Coordination board entry link:
  - `docs/internal/agents/coordination-board.md`
- Helper scripts to use/log:
  - `scripts/agents/smoke-build.sh`
  - `scripts/install-nightly-local.sh`

## Timeline / Checkpoints
- Milestone 1 (Step 1): Fix clipping/cutoff + preserve panel/viewer state when navigating.
- Milestone 2 (Step 2): Ship “Dungeon Workbench” behind a feature flag with a default layout preset.
- Milestone 3 (Step 3): Add Context Inspector (selection-driven UI) and migrate tall always-on controls into it.
- Milestone 4 (Step 5): Introduce Panel Scopes (opt-in) and migrate Dungeon to scoped behavior.
- Milestone 5 (Step 6): MRU room tabs + split view inside Workbench.
- Milestone 6 (Step 7): Command palette for room navigation + panel actions.

## Implementation Order (Requested)
1. Pin placement + clipping fixes (no more cut-off controls), and preserve canvas/panel state when navigating.
2. Room Workbench window (single stable container, no per-room windows by default).
3. Context Inspector (selection-driven; reduces always-on chrome).
5. Panel scopes (Global/Per-Dungeon/Per-Room/Per-Selection) + default policy engine.
6. Room tabs (MRU) + split view.
7. Command palette.

## Step Plans (Details)
### Step 1: Header + Navigation Stability
- Make the room header “two-tier” by default: always show nav + core room id/blockset/palette/layout/spriteset, hide advanced rows behind a one-click expand affordance.
- Ensure arrow navigation does not reset panel state:
  - Preserve ImGui window identity (slot IDs).
  - Preserve viewer state (canvas pan/zoom, toggles) across swaps.
- Ensure the nav strip never clips at higher DPI by using frame-height-based sizing rather than fixed widths.

### Step 2: Room Workbench (Single Stable Window)
- Add a new `DungeonWorkbenchPanel` that owns a dedicated dockspace:
  - Center: room canvas
  - Right: Inspector (Step 3)
  - Bottom (optional): log/diagnostics
  - Left (optional): room browser / MRU list
- Provide “Good defaults”:
  - Workbench opens in a sane layout without user docking.
  - Legacy multi-window room panels remain available behind a toggle initially.

### Step 3: Context Inspector
- Replace tall always-on controls with selection-driven sections:
  - No selection: show room metadata + a few “room” actions.
  - Object selected: show object properties and placement/mode tools.
  - Sprite/item selected: show entity properties.
- Keep high-frequency actions reachable from the canvas (context menu + command palette), but move low-frequency settings out of the header.

### Step 5: Panel Scopes
- Extend PanelManager policy to support:
  - Global: persists across editor switches
  - Per-Dungeon: persists within dungeon editor but closes elsewhere
  - Per-Room: tied to a room context (rare; mostly for inspector subviews)
  - Per-Selection: ephemeral, selection-driven
- Default policy should avoid “mystery panels”:
  - Most panels are Per-Dungeon.
  - Only explicitly pinned panels become Global.

### Step 6: MRU Tabs + Split View
- MRU room tabs inside Workbench (not separate floating windows).
- Split view:
  - Two canvases, independent viewer state, shared selection model with explicit “active pane”.
  - Defaults: off; easy toggle and easy “swap panes”.

### Step 7: Command Palette
- Fuzzy actions: open room by id/label, toggle overlays, open panels, switch tools/modes.
- Power-user defaults:
  - `Ctrl+P` (or configurable) opens palette.
  - “Recent rooms” as top results.

## Exit Criteria (for “Workbench GA”)
- Workbench layout is default for new users (fresh settings file).
- No known “layout lost” issues when navigating rooms or switching editors.
- At least one stable e2e test covers: open dungeon, open room, navigate rooms, verify panel/window persistence.
- User can revert to legacy layout mode via a setting.
