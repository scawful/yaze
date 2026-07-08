# Visual Layout Designer — Handoff (Phases 5–10)

Status: Phases 1 + 2 + 3 + 4 shipped (6 commits, test count 341 → 399).
Phase 5 onward pending.
Created: 2026-04-22 (Phase 3+4 update: 2026-04-22 later in day).
Plan file: `~/.claude/plans/prancy-discovering-rivest.md` (authoritative —
read end-to-end before starting).
Owner on handoff: ai-infra-architect or imgui-frontend-engineer.

## What this initiative is

Greenfield rebuild of the WYSIWYG layout designer. The existing lab
implementation at `src/lab/layout_designer/` (14 files, 5,004 LOC, gated by
`-DYAZE_BUILD_LAB=ON`) is kept untouched during the rebuild and deleted in
Phase 10 once feature parity is reached. The new designer lives in
`src/app/editor/layout/layout_designer/`, is reachable from the shipping
editor (command palette + activity bar), and persists full DockTree JSON
(splits + ratios + tab groups), not just the legacy visibility-only map.

Scope decisions locked in with user at session start:
- Promote + greenfield redesign (not a port of the lab code).
- Persistence = full DockTree JSON, not visibility-only.
- Panel-layout mode only. WidgetDesign mode dropped.

## What has shipped (Phases 1 + 2 + 3 + 4)

### Phase 1 — DockTree data model + JSON serializer

- `2dddda32 layout: add DockTree data model`
  - New files: `src/app/editor/layout/layout_designer/dock_tree.{h,cc}`,
    `test/unit/editor/layout/dock_tree_test.cc`.
  - `DockNode` has `kLeaf` / `kSplit` types, factory methods
    (`MakeLeaf`, `MakeSplit`), `Clone`, `SplitInPlace`,
    `PromoteSingleChild`, `FindPanel`.
  - `DockTree` wraps a name/description/schema_version/root, with
    `Clone` and `Validate(std::string* error)` that enforces non-null
    root, non-null split children, split_ratio in [0.05, 0.95], legal
    active_tab_index, non-empty + unique panel_ids.
  - `SplitDirection` is `{kLeft, kRight, kUp, kDown}`, independent of
    `ImGuiDir`. The data model has **no imgui dependency** — conversion
    happens at the LayoutManager boundary.
  - 19 tests.
- `33fe791f layout: add DockTree JSON serializer`
  - New files: `dock_tree_json.{h,cc}`, `dock_tree_json_test.cc`.
  - `DockTreeToJson(tree) -> nlohmann::json` (infallible).
  - `DockTreeFromJson(json) -> absl::StatusOr<DockTree>` (forward-
    compatible: unknown keys ignored, optional field defaults applied,
    strict rejection only for unknown types/directions and missing
    required fields).
  - 13 tests.

### Phase 2 — LayoutManager integration + persistence

- `cd5a2203 layout: add LayoutManager DockTree apply + capture`
  - Modified: `src/app/editor/layout/layout_manager.{h,cc}`.
  - New methods:
    ```cpp
    absl::Status ApplyDockTree(const layout_designer::DockTree& tree,
                               ImGuiID dockspace_id);
    absl::StatusOr<layout_designer::DockTree> CaptureDockTree(
        ImGuiID dockspace_id) const;
    ```
  - Apply: validates the tree, clears + rebuilds via
    `DockBuilder{RemoveNode,AddNode,SplitNode,DockWindow,Finish}`.
    Resolves `panel_id → window title` through
    `WorkspaceWindowManager::GetWindowDescriptor` /
    `GetWorkspaceWindowName`, with a fallback that reconstructs the
    window name from `PanelEntry.icon + " " + display_name + "##" +
    panel_id` (same formula as `WindowDescriptor::GetImGuiWindowName`)
    so panels registering late still pick up their dock target.
  - Capture: walks the ImGuiDockNode tree. **Lossy**: X-axis splits
    always serialize as `kLeft`, Y-axis as `kUp` (since kLeft/kRight
    are visually identical with inverted ratios). Unknown windows are
    dropped. `SelectedTabId` maps back to `active_tab_index`.
  - 11 tests (including apply→capture round-trip on a 2-level nested
    split with split ratios recovered within 2%).
- `6941085c layout: UserSettings::named_layouts + revision 16 migration`
  - Modified: `src/app/editor/system/session/user_settings.{h,cc}`.
  - New fields on `Preferences`:
    ```cpp
    std::unordered_map<std::string, std::string> named_layouts;
    std::string last_applied_layout_name;
    ```
    Values are **opaque JSON strings** so UserSettings never has to
    understand DockTree internals.
  - Persistence: JSON (primary) embeds each body as a parsed object
    for readability, with a string fallback when the body fails to
    parse. INI (legacy) uses compact single-line JSON per entry.
  - Revision 16 migration converts each `saved_layouts[name]`
    (visibility-only map) into a flat single-leaf DockTree under
    `named_layouts[name]`. Existing `named_layouts` entries are
    preserved (never overwritten).
  - `kLatestPanelLayoutDefaultsRevision` bumped 15 → 16.
  - 7 tests.

### Phase 3 — Layout Designer panel skeleton

- `12c6e4f1 editor(layout): add Layout Designer panel skeleton`
  - New files: `src/app/editor/layout/layout_designer/layout_designer_panel.{h,cc}`.
  - `LayoutDesignerPanel` is a `WindowContent` subclass,
    `GetEditorCategory() == "Settings"` (matches About/Settings — see
    review-fix note under Phase 4), `GetWindowLifecycle() ==
    CrossEditor`, `GetIcon() == ICON_MD_DASHBOARD_CUSTOMIZE`.
  - Draw renders: a "File:" row with four disabled
    `SmallButton`s (`New`, `Open...`, `Save`, `Save As...`); a
    3-column `BeginTable` body with fixed ~240 px palette column,
    stretch canvas, fixed ~280 px properties; footer
    `Editing: (unnamed)` until a tree is loaded/named.
  - Registered via `REGISTER_PANEL(LayoutDesignerPanel)` inside
    `namespace layout_designer`. Auto-surfaces in the command
    palette as "Show: Layout Designer" via
    `src/app/editor/system/commands/command_palette.cc:328-390`.
  - No tests (UI scaffolding only). Test count unchanged.

### Phase 4 — Canvas renderer for DockTree

- `9587c116 editor(layout): render DockTree on designer canvas`
  - New files:
    `src/app/editor/layout/layout_designer/dock_tree_renderer.{h,cc}`,
    `test/unit/editor/layout/dock_tree_renderer_test.cc`.
  - `ComputeLayout(tree, viewport) -> DockTreeLayout`: pure geometry.
    `absl::flat_hash_map<const DockNode*, ImRect>` keyed by node
    address; walks tree, carves rects along split axis (kLeft/kRight
    → X, kUp/kDown → Y), `child_a` always left/top per DockNode's
    documented layout. Splits whose children would drop below
    `kMinCellSize` (20 px) along the split axis collapse: the split's
    own rect is still registered but its children are not.
  - `RenderDockTree(tree, layout, selected, dl)`: paints leaf cells
    (themed `FrameBg` fill + `Border` outline + active panel
    `display_name` label), draws split-gutter lines between sibling
    rects, and (if `selected` is non-null and present in the map) an
    accent-colored 2.5 px outline on top. `gui::GetAccentColor()`
    provides the selection tint.
  - Panel now owns `DockTree tree_ = MakeEmptyTree("Untitled")` and
    calls `ComputeLayout` + `RenderDockTree` from the canvas cell's
    content region. `ImGui::Dummy(canvas_size)` reserves input-area
    space for Phase 5's click-to-select and split-boundary drag.
  - Review fixes bundled (from session review):
    - Category changed from `"System"` to `"Settings"` to match
      existing cross-editor shell panels (About, Settings, etc.). The
      workspace manager's category-match check
      (`workspace_window_manager.cc:802-813`) is literal strings; a
      third shell-bucket name would fragment the UI.
    - Rev-17 `UserSettings` migration default-pins `layout.designer`
      so `"Show: Layout Designer"` from any editor is enough to make
      the panel drawable. The pattern mirrors rev-7's
      `agent.oracle_ram` / `workflow.output` force-pin. Regression
      test `AppliesRevisionAndResetsPanelLayoutState` updated to
      expect `pinned_panels.size() == 3U` after the full 0→17 chain.
  - 3 tests (ratio-accurate rect sizes on a 3-level tree, sibling
    non-overlap, min-size child collapse). All pure geometry; no
    `HeadlessEditorTest` context required.

Test trajectory: 341 → 373 (+32 Phase 1) → 384 (+11 Phase 2.1) →
391 (+7 Phase 2.2) → 396 (+5 concurrent work on
dungeon_selection_snapshot_test.cc, unrelated to this initiative) →
399 (+3 Phase 4). All on `fast_unit_editor` label.

## Pending phases (what you are picking up)

Each phase below lists files new/modified, the critical APIs the code
wires into, tests to add, and the risks I noted while working the
earlier phases. Every phase must pass the per-phase gate before the
next is committed:

```
cmake --build build/presets/mac-ai --target yaze_test_quick_unit_editor -j
ctest --test-dir build/presets/mac-ai -L fast_unit_editor -j
```

Before shipping the full `yaze_editor` target, do a full-tree build
too (`cmake --build build/presets/mac-ai --config Debug --target yaze -j`).
The Phase 2.1 apply path inlines `imgui_internal.h` types that the
editor target exercises, and the Phase 3 panel only actually lands in
the binary when `libyaze_editor.a` is force-loaded (see "force-load
archive" rule in the CMakeLists section below).

### Phase 5 — Selection + split-ratio drag (1 commit)

**Commit message**: `editor(layout): add selection and split-ratio drag`

**Already in place from Phase 4**:
- `RenderDockTree(tree, layout, selected, dl)` already takes a
  `const DockNode* selected` parameter and draws a 2.5 px accent
  outline when non-null; selection is a pure data input. Phase 5 just
  has to route the click → pointer.
- `layout_designer_panel.cc`'s canvas cell already reserves input
  area via `ImGui::Dummy(canvas_size)` after the render pass.
  `ImGui::IsItemClicked()` / `ImGui::IsItemHovered()` against that
  dummy is the hit-test entry point.

**Files new**:
- `src/app/editor/layout/layout_designer/dock_tree_hit_test.{h,cc}` —
  pure-geometry `HitTestNode(const DockTreeLayout&, ImVec2 mouse) ->
  const DockNode*`. Returns the deepest leaf-or-collapsed-split whose
  rect contains `mouse`, or nullptr outside. Keep as a free function
  so tests don't need an ImGui context.
- `src/app/editor/layout/layout_designer/split_boundary_drag.{h,cc}` —
  detect mouse-over a split's gutter line (±4 px wide) and emit a
  `SplitBoundaryHit { DockNode* split_node; float delta_ratio; }`
  API. Drag delta converts pixel movement along the split axis into a
  clamped `[0.05, 0.95]` `split_ratio` update.

**Files modified**:
- `layout_designer_panel.{h,cc}` — add `const DockNode* selected_`
  member, `HitTestNode` + `HitTestSplitBoundary` invocations, and a
  click handler that writes to `selected_` / mutates
  `selected_split->split_ratio`.

**Reuse**:
- `src/app/gui/widgets/resize_handles.h` —
  `resize_handles_internal::SnapCoord` / `ApplyDragDelta` /
  `HitTestZone` are reusable primitives. You likely want `SnapCoord`
  (to snap ratios to 0.25, 0.5, 0.75 stops) and `ApplyDragDelta` (to
  clamp delta to `[0.05, 0.95]`). Full 8-zone handle widget is
  overkill.

**Tests**: extend `dock_tree_renderer_test.cc` (or a new
`dock_tree_hit_test_test.cc` if the hit-test module's surface gets
wide). Three cases: HitTest returns expected leaf for a center
click; HitTest returns nullptr for an outside click; split-boundary
drag of +20 px on a 200 px horizontal split produces
`ratio += 0.1`. Pure geometry; no ImGui context.

### Phase 6 — Palette + drag-drop onto canvas (2 commits)

**Commit 6.1**: `editor(layout): add panel palette column`

**Files new**:
- `src/app/editor/layout/layout_designer/panel_palette.{h,cc}` —
  stateless palette widget that takes a `WorkspaceWindowManager*`
  and returns the hovered/clicked panel id.

**Files modified**:
- `layout_designer_panel.cc` — embed the palette in the left column.

**Data source**:
- `WorkspaceWindowManager::GetWindowsInCategory(category)` (see
  `src/app/editor/system/workspace/workspace_window_manager.h`).
- `WorkspaceWindowManager::GetAllCategories(session_id)` to iterate
  groups.
- `WorkspaceWindowManager::GetAllWindowDescriptors()` for a flat
  view if needed.

**Search filter**: reuse
`src/app/gui/widgets/icon_browser.cc::MatchesQuery` (the Phase C.2
helper). Consider promoting it to `src/app/gui/core/text_search.h`
if a second consumer appears — **don't do it in this commit**;
a one-off-call-site `#include` is fine.

**Commit 6.2**: `editor(layout): wire drag-drop from palette to canvas`

**Files modified**:
- `src/app/gui/core/drag_drop.h` — add `kDragPayloadPanel` constant
  and `BeginPanelDragSource(panel_id)` / `AcceptPanelDrop()`
  wrappers.
- `panel_palette.cc` — call `BeginPanelDragSource` on each palette
  row.
- `layout_designer_panel.cc` — on canvas hover during drag, compute
  drop zones via a `DropZoneSuggester` helper (nearest leaf edge →
  split direction + ratio; center-of-leaf → append to tab group);
  on accept, mutate the tree (call `SplitInPlace` or append to
  `leaf.panels`).

**Files new**:
- `src/app/editor/layout/layout_designer/drop_zone_suggester.{h,cc}` —
  pure geometry: mouse pos + tree layout → suggested mutation.
- `test/unit/editor/layout/drop_zone_suggester_test.cc`.

**Tests**:
- Drop on leaf center → panel appended to tabs.
- Drop near left edge → new leaf on left with ratio=0.3.
- Drop outside canvas → no-op.

## CMakeLists registration

New test files go into **both** list sections of
`test/CMakeLists.txt`:

- `QUICK_UNIT_EDITOR_TEST_SOURCES` (line ~218) — runs under the
  `fast_unit_editor` label.
- `STABLE_UNIT_EDITOR_TEST_SOURCES` (line ~530) — runs under the
  full test matrix.

Every layout-designer test added through Phase 4
(`dock_tree_test.cc`, `dock_tree_json_test.cc`,
`layout_manager_dock_tree_test.cc`,
`dock_tree_renderer_test.cc`, and
`user_settings_named_layouts_test.cc`) sits in both lists. Follow
that pattern for Phase 5+ tests.

Source lists for designer code in `editor_library.cmake`:

- **`YAZE_EDITOR_SYSTEM_PANELS_SRC` (libyaze_editor_system_panels.a)** —
  pure data + UI plumbing that does **not** use `REGISTER_PANEL`.
  `dock_tree.cc`, `dock_tree_json.cc`, `dock_tree_renderer.cc`,
  `layout_manager.cc` live here. This archive is linked **without**
  `-Wl,-force_load`, so any TU whose only external reference is a
  REGISTER_PANEL static registrar will be silently dropped by the
  Mach-O linker and the panel will never appear in the command
  palette or sidebar.
- **`YAZE_APP_EDITOR_SRC` (libyaze_editor.a)** — source files that use
  `REGISTER_PANEL`. `about_panel.cc`, `dashboard_panel.cc`, every
  dungeon/overworld panel, and **`layout_designer_panel.cc`** (added
  in Phase 3) live here. `libyaze_editor.a` is linked with
  `-Wl,-force_load` on `yaze.app`, `yaze_test_quick_unit_editor`,
  and the other test binaries, so static registrars always fire.

Phase 3 diagnostic (verified in practice): Phase 3's initial landing
placed `layout_designer_panel.cc` in `YAZE_EDITOR_SYSTEM_PANELS_SRC`.
The tree built, tests passed, `libyaze_editor_system_panels.a`
contained the panel symbols — but `nm yaze.app/.../yaze` showed zero
`LayoutDesignerPanel` references because Mach-O drops TUs from
non-force-loaded archives. Moving the entry to `YAZE_APP_EDITOR_SRC`
fixed it. Rule of thumb for Phase 5+ files: panels and anything a
panel references transitively go in `YAZE_APP_EDITOR_SRC`; pure
helper TUs with symbols explicitly referenced from other `.cc`s
(like `dock_tree_renderer.cc`, which is called by
`layout_designer_panel.cc`) stay in `YAZE_EDITOR_SYSTEM_PANELS_SRC`.
When in doubt, verify after build:
`nm build/presets/mac-ai/bin/Debug/yaze.app/Contents/MacOS/yaze |
grep YourPanel` — zero hits means the TU was dropped.

Do **not** introduce a new `layout_designer.cmake` sub-cmake. No
subdirectory cmake pattern exists anywhere in `src/app/editor/`;
inline paths in the relevant SRC list is the project convention.

## Standing rules and gotchas from this session

### Build rules

- Always use the `mac-ai` preset:
  `build/presets/mac-ai`. Avoid `build/` (used by old CI scripts
  before the per-preset `binaryDir` landmine — see MEMORY.md).
- Pre-commit runs `clang-format` + `release/versioning` checks.
  Format new files before staging: `clang-format -i <files>`.
- **Do not edit `src/app/editor/overworld/*`** without explicit user
  green-light. Standing rule in MEMORY.md.
- **Do not run destructive git operations** (`reset --hard`, `stash
  drop`, branch deletes) without asking. In the prior session one
  `stash drop` wiped all edits in flight.

### ImGui / DockBuilder gotchas

- `DockBuilderAddNode` crashes if
  `io.ConfigFlags & ImGuiConfigFlags_DockingEnable` is not set. When
  writing tests that exercise DockBuilder directly, set the flag
  after `CreateContext()` and before `NewFrame()`. See
  `test/unit/editor/layout/layout_manager_dock_tree_test.cc:40-51`
  for the working setup.
- `DockBuilderSplitNode` uses: `id_at_dir` = return value (node at
  direction); `id_other` = opposite. Convention in this initiative:
  `child_a` = in direction of `split_direction`, `child_b` = opposite.
- `ImGuiDockNode::Size` may be 0 immediately after `SplitNode` —
  falls back to `SizeRef`. Capture path in
  `layout_manager.cc` uses `Size` with a 0-check; if you see test
  flakes, fall back to `SizeRef`.
- `ImGui::Begin`-created windows don't appear in
  `ImGuiDockNode::Windows` until the first frame that calls `Begin`
  for that window after `DockBuilderDockWindow`. Round-trip tests in
  `layout_manager_dock_tree_test.cc` verify **structure** only (split
  axes, ratios); they don't require panel windows to exist.

### Window name / panel ID mapping

Stable mapping layer is already in place:
- `panel_id` → `WindowDescriptor*` via
  `WorkspaceWindowManager::GetWindowDescriptor(session_id, panel_id)`.
- `WindowDescriptor` → ImGui window title via
  `WorkspaceWindowManager::GetWorkspaceWindowName(desc)` or
  `WindowDescriptor::GetImGuiWindowName()` (same string formula:
  `icon + " " + display_name + "##" + panel_id`).

When capturing, build a reverse map window_name → panel_id by
iterating `GetAllWindowDescriptors()`. This is what
`LayoutManager::CaptureDockTree` does and the palette code should
reuse it (extract the lookup into a helper if you find yourself
copy-pasting).

### Persistence

- `UserSettings::SetSettingsFilePathForTesting(path)` lets tests
  round-trip JSON without touching the user's real settings file.
  See `test/unit/editor/user_settings_named_layouts_test.cc` for
  the pattern.
- Revision-based migrations run on load. Bump
  `kLatestPanelLayoutDefaultsRevision` when adding a migration
  block. Phase 2.2 added revision 16 (saved_layouts → named_layouts
  conversion); Phase 4 added revision 17 (default-pin
  `layout.designer`). `AppliesRevisionAndResetsPanelLayoutState`
  in `user_settings_layout_defaults_test.cc` starts from revision 0
  and asserts the end state, so it's a load-bearing regression test
  for the whole migration chain — expect to adjust the
  `pinned_panels.size()` assertion when a migration adds a new
  force-pin.

### WindowContent::Draw conventions (learned in Phase 3)

- `WindowContent::Draw(bool* p_open)` must NOT call
  `ImGui::Begin` / `ImGui::End`. The outer `PanelWindow` wrapper
  owns the window chrome. Draw only the content.
- `ImGui::BeginMenuBar()` requires the outer window to pass
  `ImGuiWindowFlags_MenuBar`, which the default `PanelWindow`
  does NOT. Three paths forward when a menubar is wanted:
    1. Render a disabled `SmallButton` row as a stand-in (Phase 3's
       "File:" row does this — trivially works, looks native).
    2. Find or add a `PanelWindow` opt-in for menubar flags.
    3. Host the designer in a custom top-level window bypassing the
       normal panel shell (cross-editor pinning breaks; avoid).
- Category-match gate is literal: a panel with
  `GetEditorCategory() == "Foo"` draws only when the active
  editor category is also `"Foo"` — or when the panel is pinned.
  Cross-editor reachability via the command palette requires a
  default pin (see `user_settings.cc` rev-17 for
  `layout.designer`'s pin).

### Widget library primitives shipped this session

From Phase A–D of the earlier GUI standardization plan (handoff at
`docs/internal/agents/gui-library-standardization-handoff-2026-04-22.md`),
available in `src/app/gui/widgets/`:

- `DrawProperty<T>` / `DrawPropertyHex<T>` / `DrawPropertyCombo<EnumT>`
  in `property_inspector.h` — use this for the Properties column
  in Phase 7. Returns `true` on commit.
- `ResizeHandles(rect, mask, snap, id)` in `resize_handles.h` — 8-zone
  drag primitive. Internal helpers in `resize_handles_internal` are
  reusable for split-boundary drag in Phase 5.
- `IconBrowser(label)` in `icon_browser.h` — returns selected glyph
  C string. Good fit for the panel's icon picker in Phase 7.
- `FontPicker(label, active_index_ptr)` in `font_picker.h` — not
  needed for the designer but useful to know it exists.

### MEMORY.md hooks

Every non-trivial commit updates
`/Users/scawful/.claude/projects/-Users-scawful-src-hobby-yaze/memory/`.
You don't need to manage this yourself — the session auto-memory
saves feedback and project facts — but if you notice a behavior worth
preserving across sessions, write a per-topic memory file and link
it from `MEMORY.md`. Keep `MEMORY.md` index lines under ~200 chars
(file is already over the 24.4KB soft limit).

## Verification (per commit)

```
cmake --build build/presets/mac-ai --target yaze_test_quick_unit_editor -j
ctest --test-dir build/presets/mac-ai -L fast_unit_editor -j
```

Test count trajectory:
- Phase 1 shipped: 341 → 373 (+32).
- Phase 2 shipped: 373 → 391 (+18).
- Phase 3 shipped: 391 (UI scaffolding only; no new tests).
- Phase 4 shipped: 391 → 399 (+3 renderer geometry; +5 concurrent
  dungeon_selection_snapshot_test.cc from an unrelated initiative
  that landed between Phase 2 and Phase 4).
- After Phase 5: ~402 (+3 hit-test + drag delta).
- After Phase 6: ~409 (+7 palette + drop zone).

Full-tree build is non-optional on every phase going forward. The
Phase 3 "shipped-but-dropped" episode (panel compiled into the
panels lib but not linked into the binary) would have been caught
immediately by a `yaze` target build — the unit tests exercise the
panels archive directly but the yaze.app binary is what actually
runs. Always:

```
cmake --build build/presets/mac-ai --config Debug --target yaze -j
```

## Scope boundaries

- **No overworld edits.** Standing rule in MEMORY.md.
- **No lab code edits.** `src/lab/layout_designer/` stays untouched
  until Phase 10.
- **No editor-level undo integration.** The designer's undo stack
  (Phase 7) is local to the panel only.
- **No dynamic migration of `saved_layouts`** beyond the flat-tree
  conversion already in place. The old format had no split info;
  users re-split in the designer.

## Risks flagged in the plan (verbatim)

1. DockBuilder API surface is ImGui-docking-branch specific. yaze
   uses the upstream docking branch; traversal via
   `DockBuilderGetNode` may be less stable than `DockBuilderSplitNode`.
   The Phase 2.1 capture path is already defensive (null checks, size-
   or-SizeRef fallback). Extend that pattern if you add new walks.
2. Floating windows complicate capture. First ship: skip floating
   windows (not in `ImGuiDockNode::Windows`), document the limit,
   handle as a follow-up if users flag it.
3. `kLatestPanelLayoutDefaultsRevision` = 17 now (Phase 4 added
   rev 17; 16 was Phase 2). Bumps for new migrations must be
   sequential and must ship a regression test (pattern:
   `user_settings_layout_defaults_test.cc::AppliesRevisionAndResetsPanelLayoutState`).
4. Build-time cost will grow by ~2,000 LOC across Phases 3–7.
   Watch the `yaze_editor` link time on CI; if it regresses,
   consider splitting the designer into its own static lib before
   Phase 10's cleanup.
5. Force-load archive placement (Phase 3 discovery): panels with
   `REGISTER_PANEL` must live in `YAZE_APP_EDITOR_SRC`. Pure-helper
   TUs referenced directly can stay in
   `YAZE_EDITOR_SYSTEM_PANELS_SRC`. Verify per commit with `nm` on
   the yaze binary — zero hits for the panel symbols means the TU
   was linker-dropped.

## Commit sequence

Shipped:
1. `2dddda32 layout: add DockTree data model` (Phase 1.1)
2. `33fe791f layout: add DockTree JSON serializer` (Phase 1.2)
3. `cd5a2203 layout: add LayoutManager DockTree apply + capture` (Phase 2.1)
4. `6941085c layout: UserSettings::named_layouts + revision 16 migration` (Phase 2.2)
5. `12c6e4f1 editor(layout): add Layout Designer panel skeleton` (Phase 3)
6. `9587c116 editor(layout): render DockTree on designer canvas` (Phase 4)

Remaining:
7. `editor(layout): add selection and split-ratio drag` (Phase 5)
8. `editor(layout): add panel palette column` (Phase 6.1)
9. `editor(layout): wire drag-drop from palette to canvas` (Phase 6.2)
10. `editor(layout): add properties panel + undo stack` (Phase 7 — not
    covered here; see the plan file)
11. `editor(layout): save/load named layouts` (Phase 8.1)
12. `editor(layout): apply designed layout to live dockspace` (Phase 8.2)
13. `editor(settings): add Workspace layout picker` (Phase 9)
14. `layout: retire lab designer, ship widget library index entry`
    (Phase 10)

Standalone reviewable, revertible commits. Phase 10 waits until 3–9
reach feature parity with the lab version.

## Quick-start checklist for the next agent

1. Read the plan file end-to-end:
   `~/.claude/plans/prancy-discovering-rivest.md`.
2. Read the shipped modules to feel the shape:
   - `src/app/editor/layout/layout_designer/dock_tree.{h,cc}`
   - `src/app/editor/layout/layout_designer/dock_tree_json.{h,cc}`
   - `src/app/editor/layout/layout_designer/dock_tree_renderer.{h,cc}`
     — note `ComputeLayout` returns
     `absl::flat_hash_map<const DockNode*, ImRect>` keyed by node
     pointer; that map is what Phase 5's hit-test consumes.
   - `src/app/editor/layout/layout_designer/layout_designer_panel.{h,cc}`
     — the canvas cell already reserves input via
     `ImGui::Dummy(canvas_size)`; `ImGui::IsItemClicked()` on that
     dummy is the Phase 5 hit-test entry point.
   - `src/app/editor/layout/layout_manager.{h,cc}` — `ApplyDockTree` +
     `CaptureDockTree` for the Phase 8 live-dockspace round trip.
3. Verify the baseline before touching anything:
   ```
   ctest --test-dir build/presets/mac-ai -L fast_unit_editor -j
   ```
   Should be 399/399 green.
4. Pick Phase 5. Selection + split-ratio drag. Build the hit-test
   module as a pure-geometry helper so unit tests don't need an
   ImGui context, mirroring the shape of `dock_tree_renderer_test.cc`.
5. After each commit, run BOTH gates:
   ```
   cmake --build build/presets/mac-ai --target yaze_test_quick_unit_editor -j
   ctest --test-dir build/presets/mac-ai -L fast_unit_editor -j
   cmake --build build/presets/mac-ai --config Debug --target yaze -j
   ```
   Never commit with the yaze binary broken even if the test target
   is green — Phase 3 shipped a silently-dropped panel because the
   test target alone hid the symbol loss.
6. Smoke-test manually: run `yaze.app`, open the command palette
   (Cmd+Shift+P), run `"Show: Layout Designer"`. The panel should
   appear with palette/canvas/properties columns, a File row, and
   (after Phase 4) a single "Untitled" leaf rect in the canvas. If
   it doesn't draw, the panel was probably linker-dropped or the
   rev-17 default-pin didn't apply — check
   `prefs.pinned_panels["layout.designer"]` in the user settings.
