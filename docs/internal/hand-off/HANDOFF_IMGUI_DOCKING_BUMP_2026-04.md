# Handoff: ImGui `docking` submodule bump (2026-04)

This note is for agents implementing features or fixes on top of the updated vendored Dear ImGui. It summarizes **what changed upstream**, **what was patched in yaze**, and **patterns to follow** so new code matches the current API.

## Submodule state

- **Path:** `ext/imgui`
- **Branch tracked:** `docking` (see [`.gitmodules`](../../../.gitmodules))
- **Approximate jump:** `28dabdcb9` (v1.92.5 WIP) → `329c5a6b3` (v1.92.8 WIP)
- **Verify locally:** `cd ext/imgui && git log -1 --oneline && head -3 imgui.h`

Always sync the parent repo’s submodule pointer when updating ImGui (`git add ext/imgui` after checking out the new commit in the submodule).

## Breaking patterns you are likely to hit

ImGui 1.90+ moved several **`BeginChild`** behaviors from **`ImGuiWindowFlags`** to **`ImGuiChildFlags`**. Yaze had legacy names/flags that **no longer exist** on the window-flags enum.

### 1. Child borders and padding

| Old (remove) | New |
|--------------|-----|
| `ImGuiChildFlags_Border` | `ImGuiChildFlags_Borders` |
| `ImGuiWindowFlags_AlwaysUseWindowPadding` (as 4th arg to `BeginChild`) | `ImGuiChildFlags_AlwaysUseWindowPadding` (OR into child flags; 4th arg stays window flags only) |
| `ImGuiWindowFlags_NavFlattened` on `BeginChild` | `ImGuiChildFlags_NavFlattened` |

**Example:**

```cpp
// Good
ImGui::BeginChild("id", size,
                  ImGuiChildFlags_Borders | ImGuiChildFlags_AlwaysUseWindowPadding,
                  ImGuiWindowFlags_NoScrollbar);

// Bad (will not compile on current imgui.h)
ImGui::BeginChild("id", size, true, ImGuiWindowFlags_AlwaysUseWindowPadding);
```

### 2. `LayoutHelpers::BeginContentChild`

Helper lives in [`src/app/gui/core/layout_helpers.h`](../../../src/app/gui/core/layout_helpers.h).

**Signature (current):**

```cpp
static bool BeginContentChild(const char* id, const ImVec2& min_size,
                              ImGuiChildFlags child_flags = 0,
                              ImGuiWindowFlags window_flags = 0);
```

- **Borders:** use `ImGuiChildFlags_Borders`, not a `bool`.
- **Scrollbar-only window flags:** pass as the **fourth** argument (e.g. `ImGuiWindowFlags_AlwaysVerticalScrollbar`).
- **Padding without border:** `ImGuiChildFlags_AlwaysUseWindowPadding` alone is valid.

Reference call sites: `right_drawer_manager.cc`, `dungeon_object_selector.cc`, `window_sidebar.cc`, `dungeon_workbench_panel.cc`.

### 3. `ImGui::Combo` with a dynamic item list

The old **5-argument** combo helper that took a **lambda** `(void*, int, const char**)->bool` is gone.

**Current API** (see `imgui.h`): getter must be

`const char* (*)(void* user_data, int idx)` — return `nullptr` for invalid indices.

**Yaze example:** [`palette_editor_widget.cc`](../../../src/app/gui/widgets/palette_editor_widget.cc) (`RomPaletteGroupNameGetter`).

### 4. gRPC-only `Application` API

`Application::GetEmulatorBackend()` is declared **only when `YAZE_WITH_GRPC` is defined** ([`application.h`](../../../src/app/application.h)).

Any use from editor code must be wrapped:

```cpp
#ifdef YAZE_WITH_GRPC
  if (auto* backend = Application::Instance().GetEmulatorBackend()) { ... }
#endif
```

**Fixed in:** [`editor_manager.cc`](../../../src/app/editor/editor_manager.cc) (`RunCurrentProject` path).

## Files touched for this bump (reference)

Use `git log` / `git show` on the ImGui bump commit for the exact list. At handoff time, edits included:

- `ext/imgui` (submodule)
- `src/app/gui/core/style.cc`
- `src/app/gui/core/layout_helpers.{h,cc}`
- `src/app/gui/widgets/asset_browser.cc`
- `src/app/gui/widgets/palette_editor_widget.cc`
- `src/app/editor/agent/agent_chat.cc`
- `src/app/editor/dungeon/dungeon_object_selector.cc`
- `src/app/editor/dungeon/ui/window/` (dungeon `WindowContent` modules; workbench may be `dungeon_workbench_content.cc` in `dungeon/workspace/`)
- `src/app/editor/editor_manager.cc`
- `src/app/editor/menu/right_drawer_manager.cc`
- `src/app/editor/menu/window_sidebar.cc`
- `src/app/editor/music/piano_roll_view.cc`

## If your build fails after merging

1. **Compile errors mentioning `ImGuiChildFlags_Border`, `ImGuiWindowFlags_AlwaysUseWindowPadding`, or `ImGuiWindowFlags_NavFlattened`:** apply the mapping table above.
2. **Combo overload errors:** switch to the `getter + user_data` form.
3. **`GetEmulatorBackend` missing:** guard with `#ifdef YAZE_WITH_GRPC` or use a non-gRPC code path.
4. **Link errors (Abseil, etc.):** usually **preset / dependency mismatch**, not ImGui. Reconfigure with the same preset as CI (e.g. `dev`, `ci`) and ensure Abseil is linked consistently.

## Optional follow-ups (not required for the bump)

- **`ext/imgui_test_engine`:** separate submodule; bump if test harness or bundled ImPlot drifts from ImGui.
- **Font scale:** yaze still uses `io.FontGlobalScale` via prefs; ImGui 1.92+ prefers `ImGui::GetStyle().FontScaleMain` for new code—only migrate project-wide if product wants alignment with upstream guidance.
- **Changelog:** skim `ext/imgui/docs/CHANGELOG.txt` for your feature area (tables, docking, inputs).

## Quick verification

```bash
cmake --preset dev -B build && cmake --build build -j8 --target yaze
```

Adjust preset to match your machine/CI. A clean build after submodule update is the best regression signal for agents.
