# Dungeon Workbench Usability Handoff (2026-04-26)

Status: IN_PROGRESS  
Owner: docs-janitor  
Created: 2026-04-26  
Last Reviewed: 2026-04-26  
Next Review: 2026-05-03  
Universe Task: `task_20260426T141851Z_1622`

## Context

Goal: make DungeonEditorV2 feel less like a pile of separate utility windows and
more like a single ZScream-familiar workbench: canvas in the center, room/selection
state in the right inspector, and local edit tools one click away without opening
more top-level panels.

The user explicitly asked for:
- ZScream-like grammar/muscle memory, but expanded.
- Less reliance on multiple windows.
- Tool content folded into existing Workbench surfaces.
- Better code quality and less AI-generated source-file scatter.
- Caution around concurrent overworld/ZS parity work by another agent.

## Current State

Implemented but not committed:

- `src/app/editor/dungeon/workspace/dungeon_workbench_content.{h,cc}`
  - Replaced Workbench-local modal tool popups and inline shelves with a right
    inspector `Tools` mode/drawer.
  - Drawer covers Object Selector, Doors, Sprites, Items, Palette, Room Graphics,
    Room Tags, Custom Collision, Water Fill, and Minecart Tracks.
  - Dungeon Map intentionally remains a popup.
  - Connected Graph intentionally remains a canvas mode.
  - Active tool drawer state survives room changes.
  - Selection auto-focus no longer steals focus away from the Tools drawer.
  - **Drawer chrome simplified (2026-04-26 follow-up):** the redundant "Tool
    Drawer" subtitle and in-drawer "Room" back button were removed (the
    inspector primary segmented selector at the top of the inspector already
    handles Room/Selection/Tools transitions). The bottom "Switch Tool"
    collapsible was replaced by a compact 2x5 icon-only `DrawInspectorToolStrip`
    above the active body, so users can hop between tools in one click without
    scrolling past the active tool. Tool icons are exposed via new
    `GetWorkbenchToolIcon` / `GetWorkbenchToolShortLabel` accessors. The body
    now grows to fill the remaining inspector height (with a 180px floor)
    instead of being capped at ~66 percent.
  - **Inspector redundancy cleanup (2026-04-26 follow-up 2):**
    - The "Transitions & Tags" section in `DrawInspectorShelf` had a "Local
      tools" sub-grid (Door Tools + Room Tags) that duplicated the Quick Tools
      grid below and the Tools-mode strip. Sub-grid removed; the section is
      renamed to "Connected Graph" since its only remaining concern is the
      connected-map-mode toggle.
    - `DrawInspectorCompactSummary` had a "Room Details" button that just set
      `inspector_mode_=Room`; the inspector primary segmented selector at the
      header (always rendered, including in compact mode) already does the
      same. Button removed.
    - `DrawInspectorShelfTools` Edit row mixed entity tools (Selector / Doors /
      Sprites / Items) with mode switches (Selection / Room Details). Mode
      switches removed; Edit row reformatted into a clean 2x2 grid of entity
      tools only. `FocusSelectionInspector` / `FocusRoomInspector` are still
      called from `dungeon_editor_v2.cc` and the canvas selection shelf, so
      no dead code was introduced.
  - **Polish pass #4 — UI/UX overhaul (2026-04-26 follow-up 5):** larger
    user-driven slice covering top bar, inspector clutter, sidebar tabs,
    selector clipping, default layout (revision 21), and a Layout C mirror
    toggle:
    - **Stitched Rooms rename**: the toolbar `Connected` text button is now
      icon-only (`ICON_MD_VIEW_QUILT`); the inspector section header,
      sidebar Quick Actions menu item, and on-hover tooltips all read
      "Stitched Rooms" — accurate to what the canvas actually renders
      (current room + adjacent rooms stitched into one continuous canvas).
    - **Top bar declutter**: `kToolbarRoomReviewLabel` dropped " Review"
      text; the room-matrix button is now icon-only. NESW arrow widget
      preserved per user request (standard muscle memory).
    - **Inspector overlay grouping**: `DrawInspectorShelfView`'s flat list
      of 11 overlay checkboxes is split into Canvas overlays (4, always
      visible: Grid, Object Bounds, Hover Coordinates, Camera Quadrants)
      and an `Authoring overlays` collapsible (7, default closed: Track
      Collision, Custom Collision, Water Fill, Minecart Pathing, Track
      Gaps, Track Routes, Custom Objects). Inner "Overlay Toggles"
      header dropped (parent section header is enough).
    - **Navigate tab buttons**: `DrawSidebarModeTabs` swapped its full-
      width text ToggleButton segments for icon-only square ToggleButtons
      (`ICON_MD_VIEW_LIST` / `ICON_MD_DOOR_FRONT`) with hover tooltips.
      Cluster width drops from ~170px to ~70px.
    - **Object Selector clipping fix**: `dungeon_object_selector.cc`
      `BeginChild("##ObjectGrid", …)` adds `ImGuiWindowFlags_HorizontalScrollbar`
      and clamps `item_size` to `min(requested, max(32, available - 16))`.
      `##CustomObjectGrid` in the popup gets the same horizontal-scrollbar
      flag for symmetry. Authored by parallel agent.
    - **Mode-parity follow-through**: subagent B's relaxation (above)
      means the new selectors / palette / room graphics are accessible in
      both Workbench and Window modes simultaneously.
    - **Layout C default (revision 21)**: `kLatestPanelLayoutDefaultsRevision`
      bumped 20→21. Revision 21 seeds default-on visibility for
      `dungeon.object_selector`, `dungeon.sprite_editor`,
      `dungeon.item_editor`, `dungeon.room_matrix`, and re-opens
      `dungeon.room_selector` (was off in rev 13). Adds a 320px width hint
      for the workbench in `right_panel_widths`. Adds new
      `Preferences::dungeon_inspector_side` (default `"right"`) +
      `UserSettings::GetDungeonInspectorSide()` /
      `SetDungeonInspectorSide()` API + JSON/INI persistence. 4 new tests
      in `user_settings_layout_defaults_test.cc`.
    - **Mirror toggle wiring**: `DungeonWorkbenchContent` gets
      `SetInspectorOnLeft(bool)` + `SetOnInspectorSideChanged(callback)`.
      `Draw()` adds a mirror-mode branch that swaps the L/R pane render
      order while preserving width-state semantics (right_width remains
      the inspector's, left_width remains the sidebar's, splitter drags
      still affect the correct pane). The inspector header gets a
      `ICON_MD_SWAP_HORIZ` button next to the collapse chevron; clicking
      it toggles `inspector_on_left_` and persists the choice through
      `UserSettings`. The collapse chevron now flips
      (`ICON_MD_CHEVRON_RIGHT` ↔ `ICON_MD_CHEVRON_LEFT`) to match which
      side the inspector exits to. `DungeonEditorV2::Update()` reads
      `GetDungeonInspectorSide()` each frame and calls
      `workbench_panel_->SetInspectorOnLeft(...)`.
  - **Polish pass #3 — mode-parity relaxation (2026-04-26 follow-up 4):**
    the `IsDungeonWorkbenchLocalToolWindow()` policy added in commit
    `46e40fb5` was deleted along with every callsite. Workbench mode no
    longer hides Object/Door/Sprite/Item Selector, Palette, Room Graphics,
    Room Tags, Custom Collision, Water Fill, or Minecart panels from the
    sidebar / Window Browser, and `SetWorkbenchWorkflowMode(true)` no
    longer auto-closes their standalone windows on entry. Users can now
    have both the Workbench drawer's embedded copy and a standalone
    window open if they prefer that layout. The Tools strip in the
    Workbench inspector is unchanged. Affected files:
    `src/app/editor/menu/window_sidebar.{h,cc}`,
    `src/app/editor/menu/window_browser.{h,cc}` (the
    `is_dungeon_workbench_mode` callback was the only consumer of that
    constructor parameter and is dropped),
    `src/app/editor/menu/activity_bar.cc`,
    `src/app/editor/dungeon/dungeon_editor_v2.cc`,
    `test/unit/editor/sidebar_sort_test.cc` (two old tests renamed +
    rewritten to assert the relaxation: `DungeonWorkbenchLocalTools…` /
    `DungeonWorkbenchKeepsNavigationWindowsVisible` are gone, replaced by
    `DungeonWindowModeTargetsCoverRoomPrefixedWindows`).
  - **Polish pass #2 (2026-04-26 follow-up 3):** five focused edits in
    `dungeon_workbench_content.cc` to compress remaining visual scatter:
    - **Room badge** — dropped the redundant `(%d)` decimal label that sat
      between the hex `InputHexWordEx` and the copy button. Hex value is
      already on screen and the dungeon-group line carries `[%03X]`.
    - **Section naming parity** — `DrawInspectorShelfRoom`'s `Quick Tools`
      section renamed to `Tools` so the full and compact summaries use the
      same label for the same target (`InspectorMode::Tools`).
    - **Room Actions tightened** — `Local Room Tools` subhead and the 2-col
      Graphics+Dungeon-Map table are gone. Graphics is one click away on the
      Tools strip; only `Dungeon Map` remains as a popup-launching action
      under Room Actions, and the Apply explainer is one line.
    - **Connected Graph** — removed the inline `Tip: …` `TextDisabled` line
      under the checkbox; the on-hover tooltip carries the same content with
      the staircase-header note appended.
    - **Layer Compositing** — replaced loose `BG1 / BG2` band labels and four
      stacked combos with a 4-row 2-col `BeginTable` (label cell + stretch
      combo cell). Combo IDs keep their `##WorkbenchBlend…` suffixes so ImGui
      state survives. Reset button stays as a footer below the table.

- `src/app/editor/dungeon/dungeon_editor_v2.cc`
  - Workbench-mode callbacks now open the right-inspector tool drawer instead of
    top-level tool windows.
  - Local utility panels are registered for the explicit Window workflow, but
    Workbench entry closes their standalone copies and embeds the same tools in
    the inspector drawer.

- `src/app/editor/menu/window_sidebar.{h,cc}`
  - Added `IsDungeonWorkbenchLocalToolWindow()` policy.
  - Sidebar hides Workbench-local Dungeon tools while Workbench mode is active.
  - The fallback workflow switch also closes visible local tool windows before
    showing the Workbench.
  - Navigation/review windows remain visible: Workbench, Room List, Room Matrix,
    Entrances, Object Tile Editor.

- `src/app/editor/menu/window_browser.{h,cc}` and `activity_bar.cc`
  - Window Browser now receives the same Workbench-mode callback as the sidebar.
  - Browser counts, rows, and bulk show/hide actions filter out local Workbench
    tools while Workbench mode is active.

- Tests:
  - `test/unit/editor/dungeon_workbench_content_test.cc`: drawer request state
    and room-change persistence; new `ToolStripCyclesAllToolsWithoutLeavingDrawer`
    pins the strip's intended state contract — every Open*Tool() call keeps the
    inspector in `tools` mode and re-opening the active tool stays idempotent.
  - `test/unit/editor/sidebar_sort_test.cc`: local-tool classification policy.
  - `test/CMakeLists.txt`: includes Workbench content tests in
    `yaze_test_quick_unit_editor`.

- Docs:
  - `docs/internal/agents/initiative-dungeon-workbench-2026-02.md`: updated
    Workbench initiative status, milestones, scope, and exit criteria.
  - `docs/public/usage/dungeon-editor.md`: reframed public guide around the
    Workbench-first flow and inspector Tools drawer.
  - `docs/internal/architecture/editor-ui-module-pattern.md`: added guidance
    for Workbench-local tools vs. top-level windows.
  - `docs/internal/agents/dungeon-ux-validation-pass.md`: added manual QA and
    focused automated coverage for the drawer/window-filtering slice.
  - `docs/internal/agents/dungeon-ui-stability-handoff-2026-04-26.md`: points
    forward to this usability handoff.

## Verified

All commands below were run from `/Users/scawful/src/hobby/yaze`.

```bash
git diff --check -- \
  src/app/editor/dungeon/workspace/dungeon_workbench_content.h \
  src/app/editor/dungeon/workspace/dungeon_workbench_content.cc \
  src/app/editor/dungeon/dungeon_editor_v2.cc \
  src/app/editor/menu/activity_bar.cc \
  src/app/editor/menu/window_browser.cc \
  src/app/editor/menu/window_browser.h \
  src/app/editor/menu/window_sidebar.cc \
  src/app/editor/menu/window_sidebar.h \
  test/unit/editor/dungeon_workbench_content_test.cc \
  test/unit/editor/sidebar_sort_test.cc \
  test/CMakeLists.txt
```

Result: passed.

```bash
cmake --build --preset mac-ai --target yaze yaze_test_quick_unit_editor --parallel 8
```

Result: passed. Existing build noise remains: Ninja reports `premature end of
file; recovering`, and the linker reports duplicate-library warnings.

```bash
./build/presets/mac-ai/bin/Debug/yaze_test_quick_unit_editor \
  --gtest_filter='DungeonWorkbenchToolbar*.*:DungeonWorkbenchContentLayoutTest.*:SidebarSortTest.*'
```

Result: passed, `22/22` tests.

Live gRPC smoke:

```bash
PORT=50064
build/presets/mac-ai/bin/Debug/yaze.app/Contents/MacOS/yaze \
  --rom_file=roms/oos168.sfc \
  --editor=Dungeon \
  --room=16 \
  --open_panels=dungeon.workbench \
  --startup_welcome=hide \
  --startup_dashboard=hide \
  --enable_test_harness \
  --test_harness_port=$PORT \
  --log_to_console

build/presets/mac-ai/bin/Debug/z3ed \
  --rom=roms/oos168.sfc \
  --gui-server-address=localhost:$PORT \
  gui-discover-tool --window "Dungeon Workbench" --format json
```

Result: `PanelWindow:Dungeon Workbench` was visible with discovery status
`Success` and `total_widgets: 1`.

Documentation hygiene:

```bash
git diff --check -- \
  docs/internal/agents/dungeon-workbench-usability-handoff-2026-04-26.md \
  docs/internal/agents/initiative-dungeon-workbench-2026-02.md \
  docs/internal/agents/dungeon-ux-validation-pass.md \
  docs/internal/architecture/editor-ui-module-pattern.md \
  docs/public/usage/dungeon-editor.md \
  docs/internal/agents/dungeon-ui-stability-handoff-2026-04-26.md

grep -R "DungeonWorkbench[P]anel\\|dungeon_workbench_[p]anel\\|build_[a]i" -n \
  docs/internal/agents/dungeon-workbench-usability-handoff-2026-04-26.md \
  docs/internal/agents/initiative-dungeon-workbench-2026-02.md \
  docs/internal/agents/dungeon-ux-validation-pass.md \
  docs/internal/architecture/editor-ui-module-pattern.md \
  docs/public/usage/dungeon-editor.md \
  docs/internal/agents/dungeon-ui-stability-handoff-2026-04-26.md
```

Result: passed; no stale Workbench-panel or old build-directory references
remained in these updated docs.

## Open Risks

- No focused commit has been made.
- The working tree is very dirty and includes unrelated/concurrent overworld,
  tile selector, user settings, gRPC, and command-context work. Do **not** use
  `git add -A` for this slice.
- Manual visual QA of the full drawer contents was not completed; the smoke only
  proves the Workbench window is live through the gRPC harness.
- The drawer embeds existing `WindowContent::Draw()` implementations. Some of
  those local tools were originally designed as standalone windows, so follow-up
  visual polish may be needed for width, scroll, and nested chrome.
- Object Tile Editor remains standalone on purpose. Do not fold it into the
  Workbench drawer unless the Object Selector directly invokes it with a clear
  asset-authoring workflow.
- `/Applications/Yaze.app` was not updated during this handoff/doc pass.

## Next Step

Stage only the Dungeon Workbench/tool-drawer and docs files, then make a focused
commit. Suggested commit message:

```text
dungeon: move local tools into workbench drawer
```

Before staging, review only these paths:

```bash
git diff -- src/app/editor/dungeon/workspace/dungeon_workbench_content.h \
  src/app/editor/dungeon/workspace/dungeon_workbench_content.cc \
  src/app/editor/dungeon/dungeon_editor_v2.cc \
  src/app/editor/menu/activity_bar.cc \
  src/app/editor/menu/window_browser.cc \
  src/app/editor/menu/window_browser.h \
  src/app/editor/menu/window_sidebar.cc \
  src/app/editor/menu/window_sidebar.h \
  test/unit/editor/dungeon_workbench_content_test.cc \
  test/unit/editor/sidebar_sort_test.cc \
  test/CMakeLists.txt \
  docs/internal/agents/dungeon-workbench-usability-handoff-2026-04-26.md \
  docs/internal/agents/initiative-dungeon-workbench-2026-02.md \
  docs/internal/agents/dungeon-ux-validation-pass.md \
  docs/internal/architecture/editor-ui-module-pattern.md \
  docs/public/usage/dungeon-editor.md \
  docs/internal/agents/dungeon-ui-stability-handoff-2026-04-26.md
```

## Useful Commands

Focused rebuild/test:

```bash
cmake --build --preset mac-ai --target yaze yaze_test_quick_unit_editor --parallel 8
./build/presets/mac-ai/bin/Debug/yaze_test_quick_unit_editor \
  --gtest_filter='DungeonWorkbenchToolbar*.*:DungeonWorkbenchContentLayoutTest.*:SidebarSortTest.*'
```

Optional app-bundle sync without launch:

```bash
rm -rf /Applications/Yaze.app.deploying
ditto build/presets/mac-ai/bin/Debug/yaze.app /Applications/Yaze.app.deploying
rm -rf /Applications/Yaze.app
mv /Applications/Yaze.app.deploying /Applications/Yaze.app
xattr -dr com.apple.quarantine /Applications/Yaze.app
```

Do not launch the app visibly unless the user asks for a manual test.
