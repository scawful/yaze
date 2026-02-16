# Handoff: Dungeon Panel/Sidebar Flow + Crash Guard (2026-02-14)

**Owner:** imgui-frontend-engineer  
**Status:** In progress (core fixes landed, needs manual UX pass + confirmation)  
**Related board thread:** `2026-02-14 imgui-frontend-engineer – Sidebar Workflow UX + Lightweight UI Regression Gate`

## What prompted this handoff
User reported:
- Sidebar and panel management still felt rough on both sides.
- Pin button behavior regressed again.
- Crash in dungeon editor while switching workbench/panel workflow (stack showed `ImGui::EndChild` / `LayoutHelpers::EndToolbar` unwind issue).

## Root cause summary (crash)
Workflow mode switches were still mutating panel visibility inside active draw scopes in some paths, even after earlier toolbar defer work.

Observed crash path (user trace):
- `DungeonWorkbenchToolbar::Draw` -> `DungeonWorkbenchPanel::Draw` -> `PanelManager::DrawAllVisiblePanels` -> assert/abort in ImGui window recovery.

## Changes implemented in this pass

### 1) Deferred workflow mode switches to next update frame
- Added queued mode switch API and pending state in `DungeonEditorV2`.
- `SetWorkbenchWorkflowMode(...)` remains the mutation path; calls now route through `QueueWorkbenchWorkflowMode(...)` and are applied by `ProcessPendingWorkflowMode()` at the top of `Update()`.

Code refs:
- `src/app/editor/dungeon/dungeon_editor_v2.h:200`
- `src/app/editor/dungeon/dungeon_editor_v2.h:375`
- `src/app/editor/dungeon/dungeon_editor_v2.cc:395`
- `src/app/editor/dungeon/dungeon_editor_v2.cc:703`
- `src/app/editor/dungeon/dungeon_editor_v2.cc:1297`
- `src/app/editor/dungeon/dungeon_editor_v2.cc:1886`
- `src/app/editor/editor_manager.cc:710`

### 2) Pin button regression fix (ID collisions + session scope)
- Pin toggles now push unique ImGui IDs per row/control before rendering icon buttons.
- Pin operations in Activity sidebar + Panel Browser now call session-scoped pin setters explicitly.
- Pin control size increased for better hit target.

Code refs:
- `src/app/editor/menu/activity_bar.cc:527`
- `src/app/editor/menu/activity_bar.cc:568`
- `src/app/editor/menu/activity_bar.cc:646`
- `src/app/editor/menu/activity_bar.cc:959`

### 3) Sidebar button overhaul (left)
- Dungeon workflow controls changed from tiny icon toggles to labeled buttons (`Workbench`, `Panels`).
- Category actions changed to labeled buttons (`Browser`, `Show`, `Hide`).
- Since workflow switching is now deferred, sidebar mode state is updated optimistically in-frame to keep card interactions correct.

Code refs:
- `src/app/editor/menu/activity_bar.cc:339`
- `src/app/editor/menu/activity_bar.cc:370`
- `src/app/editor/menu/activity_bar.cc:398`
- `src/app/editor/menu/activity_bar.cc:460`

### 4) Sidebar button overhaul (right)
- Added right-panel cycling controls (prev/next) in panel header, while keeping popup switcher.

Code refs:
- `src/app/editor/menu/right_panel_manager.cc:79`
- `src/app/editor/menu/right_panel_manager.cc:88`
- `src/app/editor/menu/right_panel_manager.cc:731`
- `src/app/editor/menu/right_panel_manager.cc:747`

## Validation performed

Build:
- `cmake --build --preset mac-ai --target yaze --parallel 8`
- Result: pass

Tests:
- `ctest --test-dir build_ai -R yaze_test_quick_unit_editor --output-on-failure`
- Result: `116/116` pass

- `ctest --test-dir build_ai -R DungeonEditorV2IntegrationTest --output-on-failure`
- Result: `16` pass, `4` disabled, `0` fail

## Manual verification checklist for next agent
1. Launch app and open Dungeon Workbench.
2. Rapidly toggle workflow mode from toolbar and from Dungeon category sidebar.
3. Verify no crash/assert while switching modes repeatedly.
4. In Dungeon sidebar and Panel Browser, pin/unpin multiple panels repeatedly.
5. Confirm each click affects the intended row only (no cross-row toggles).
6. Verify Workbench -> Panel switch + immediate panel card click still opens/shows requested panel.
7. Verify right sidebar header prev/next buttons cycle expected panel order.

## Remaining follow-up opportunities
- Refine sidebar spacing/alignment now that buttons are labeled (especially narrow widths).
- Add coverage for deferred workflow switching path (unit/integration around mode transition timing).
- Consider keyboard shortcuts for right-panel cycling to match new header controls.

## Workspace notes
- Repo is heavily dirty from concurrent work; do not revert unrelated changes.
- Files touched in this pass:
  - `src/app/editor/dungeon/dungeon_editor_v2.h`
  - `src/app/editor/dungeon/dungeon_editor_v2.cc`
  - `src/app/editor/editor_manager.cc`
  - `src/app/editor/menu/activity_bar.cc`
  - `src/app/editor/menu/right_panel_manager.cc`
  - `docs/internal/agents/coordination-board.md`
