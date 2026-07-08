# GUI Library Standardization — Handoff

Status: Phase A complete, unshipped. Phases B–D pending.
Owner on handoff: next available (ai-infra-architect or imgui-frontend-engineer)
Created: 2026-04-22
Plan file: `~/.claude/plans/the-ui-ux-of-yaze-snappy-hartmanis.md`

## Why this initiative exists

User asked two questions on 2026-04-22:
1. How viable is a visual layout editor for yaze?
2. How should we refactor the gui library so it's easier to customize?

After clarification, user chose "widget library first — standalone, landing
before any layout work." Layout editor is a downstream goal; current pain is
an uneven widget library. The plan is four phases:

- **Phase A** — typed property inspector
- **Phase B** — resize handles primitive
- **Phase C** — font picker + icon browser
- **Phase D** — widget library index (README)

Layout editor work (DockNode persistence, CRUD, WYSIWYG designer) is
**deferred** per user direction. Do not start on it without explicit
green-light.

## Phase A — DONE (uncommitted)

### What shipped

- `src/app/gui/widgets/property_inspector.{h,cc}` — typed
  `DrawProperty<T>()`, `DrawPropertyHex<T>()`, `DrawPropertyCombo<EnumT>()`.
  Returns `true` on commit (matches `ImGui::Input*`). Auto-binds to
  `BeginPropertyTable` grid when inside one; renders inline otherwise.
  Supports `bool`, `int`, `uint8_t`, `uint16_t`, `uint32_t`, `float`,
  `double`, `std::string`, `ImVec4`, `gui::Color`, and enums.
  `PropertyOptions` carries `min`, `max`, `step`, `step_fast`, `format`,
  `tooltip`, `read_only`, `text_flags`. **No undo coupling** — wrap the
  return at the call site.
- `src/app/gui/gui_library.cmake` — registered `property_inspector.cc` in
  `GUI_WIDGETS_SRC`.
- `test/unit/gui/property_inspector_test.cc` — 14 tests. Registered in
  both the overall test list and `QUICK_UNIT_EDITOR_TEST_SOURCES` so they
  run under the `fast_unit_editor` label.
- `src/app/editor/shell/windows/settings_panel.cc` — adoption site.
  Converted the 4-field backup settings block (lines ~542–566). Clamping
  moved from call-site `std::max` to `PropertyOptions::min/max`.

### Validation (done, repeatable)

```
cmake --build build/presets/mac-ai --target yaze -j
ctest --test-dir build/presets/mac-ai -L fast_unit_editor -j
```

315/315 pass in 10.7s (14 new + 301 pre-existing). `yaze` builds clean.

### Commit split (suggested)

Two commits keeps history readable:
1. `gui(widgets): add typed property inspector` — header, cc, cmake, test.
2. `editor(settings): adopt DrawProperty for backup settings block` —
   settings_panel.cc only.

## Phase B — PENDING: Resize handles primitive

### Goal

`src/app/gui/widgets/resize_handles.{h,cc}` (new). One widget that renders
8-point drag handles on an `ImRect` and returns commit deltas. Used by a
future layout designer (Phase 4 in plan) but also by any editor that needs
a draggable selection rect (dungeon canvas viewer overlay is the obvious
first adopter — see `src/app/editor/dungeon/dungeon_canvas_viewer.cc`).

### API shape (from plan)

```cpp
enum class HandleMask : uint8_t {
  kCorners = 0x0F,
  kEdges = 0xF0,
  kAll = 0xFF,
};

struct ResizeResult {
  bool changed;
  ImRect new_rect;
};

ResizeResult ResizeHandles(ImRect& rect,
                           HandleMask mask = HandleMask::kAll,
                           float snap = 0.0f,
                           ImGuiID id = 0);
```

Theme-aware colors via `GetAccentColor()` and/or
`theme.selection_handle` (see `src/app/gui/core/theme_manager.h`).
Snap to `snap` when > 0. Cursor changes per side
(`ImGuiMouseCursor_ResizeEW` etc.).

### Critical files to read first

- `src/app/gui/canvas/canvas_interaction_handler.{h,cc}` — existing hit
  test + selection rect drawing. Likely has math you can lift.
- `src/app/gui/core/ui_helpers.h` — `GetAccentColor`, semantic color
  getters to keep theme-aware styling consistent.
- `src/app/editor/dungeon/dungeon_canvas_viewer.cc` — selection overlay
  already draws a rect; this is the natural first adoption site.

### Test target

`test/unit/gui/resize_handles_test.cc`. Test snap math, bounds clamping,
and mask behavior (corners-only, edges-only, all). Register in
`QUICK_UNIT_EDITOR_TEST_SOURCES` in `test/CMakeLists.txt`.

### Constraint

**Do not touch `src/app/editor/overworld/*`** without explicit green-light
from the user. Standing rule in MEMORY.md. Adopt in dungeon canvas viewer
or skip the adoption entirely in Phase B's PR.

## Phase C — PENDING: Font picker + icon browser

### Font picker

`src/app/gui/widgets/font_picker.{h,cc}`. ~60 LOC. Dropdown over
`ImGui::GetIO().Fonts->Fonts`, preview sample text in-combo, returns
selected index.

**Check first**: does yaze register more than one font at startup? If
`io.Fonts->Fonts.Size == 1` the picker has nothing to pick from and Phase
C also has to add font loading. Grep for `AddFontFromFileTTF` /
`AddFontDefault` to find the registration site. Likely in
`src/app/core/platform/` or the app init path.

### Icon browser

`src/app/gui/widgets/icon_browser.{h,cc}`. ~120 LOC. Enumerates
`ICON_MD_*` macros from `src/app/gui/core/icons.h`. Search-as-you-type
filter. Click to copy the macro name, double-click to insert a snippet.

**Recommendation**: start with a hand-maintained `GetIconList()` table
seeded from Material Icons. Placed in
`src/app/gui/core/icons.h` + `icons.cc`. Defer the build-time codegen
(parse `icons.h` for `#define ICON_MD_*`) to a follow-up if churn is
high — YAGNI for v1.

### Persistence (font picker only)

Mirror the `last_theme_name` pattern in Phase 5.1 (see MEMORY.md under
"UI/UX Plan (pure-wishing-castle) Status"). Add `font_family_index` +
`font_scale` to `UserSettings`. `ThemeManager::SetOnThemeChangedCallback`
is the template — add an equivalent font-changed callback so settings
persist automatically.

## Phase D — PENDING: Widget library index (docs only)

`src/app/gui/widgets/README.md`. One-line summary + signature + file
path per primitive. Sections:

- Buttons (`themed_widgets.h`)
- Inputs (`layout_helpers.h`, `property_inspector.h`)
- Selectors (`tile_selector_widget.h`, `asset_browser.h`,
  `palette_editor_widget.h`, `font_picker.h`, `icon_browser.h`)
- Canvas (link to `canvas/canvas.h`)
- Layout (`resize_handles.h`, `BeginPropertyTable` from `ui_helpers.h`)
- Colors (`color.h`, `ui_helpers.h` semantic getters)

Add a pointer from `src/app/gui/CLAUDE.md` (create if missing, following
the pattern of `src/app/CLAUDE.md`) so `imgui-frontend-engineer` and
`ai-infra-architect` agents pick it up.

## Decisions already locked in

Don't re-litigate these:

1. **Widget library first, layout editor deferred.** User confirmed
   2026-04-22.
2. **No undo coupling in property_inspector.** Return `bool` on commit;
   call site wraps with its own command manager. Keeps widget lib
   independent of editor undo contexts (which vary per editor — palette,
   dungeon, overworld each have their own).
3. **Header-only template for `DrawPropertyCombo<EnumT>`.** Concrete
   overloads live in `.cc`. Works via C++ overload resolution, no user-
   visible `template <typename T> DrawProperty` primary.
4. **Per-type overloads, not a single templated `DrawProperty<T>`.**
   Avoids specialization gymnastics; gives each type its own ImGui
   widget flavor (Checkbox for bool, InputScalar for uint*,
   ColorEdit4 for color, etc.).
5. **Clamping via `PropertyOptions::min/max`, only when `max > min`.**
   Cleaner than `std::max`/`std::min` at each call site. Settings panel
   adoption site demonstrates this.
6. **`ImGui::BeginDisabled(opts.read_only)` wraps every widget.**
   Consistent feel across types; also lets `read_only=true` return false
   without touching the value.

## Gotchas

- **`property_inspector.h` was linted to remove the `unsigned int`
  overload.** Only `std::uint{8,16,32}_t` variants exist. If a caller
  passes `unsigned` (not from cstdint), they get an overload error. Add
  the overload back if this trips anyone.
- **`BeginPropertyTable` existing display-only `PropertyRow` overloads
  (in `ui_helpers.{h,cc}`) advance columns via `TableNextColumn`
  twice.** `property_inspector_internal::BeginRow` does the same.
  Mixing display-only and editable rows in one table works correctly.
- **String callback relies on `value->data()` being contiguous and
  mutable.** C++17+ guarantees this. yaze is C++20. Don't
  backport to C++14 targets.
- **Tests can't simulate user input without ImGui Test Engine.** Current
  tests cover helper introspection + "no input → returns false,
  value unchanged." That's the proof-of-no-crash contract. Don't try
  to synthesize mouse/keyboard — use the e2e GUI suite in
  `test/e2e/` if you need that.
- **`yaze_test_quick_unit_editor` is the relevant test target.** Build
  with `--target yaze_test_quick_unit_editor -j`. Run with
  `ctest --test-dir build/presets/mac-ai -L fast_unit_editor -j`.
  The `mac-ai-unit` preset points somewhere that returned "No tests
  were found!!!" in this session — use `--test-dir` form.
- **Build dir: `build/presets/mac-ai`**, not `build/`. The project moved
  to per-preset `binaryDir` in commit `a16d84cc` (2026-04-18). Override
  `YAZE_PREPUSH_BUILD_DIR` if using the pre-push hook.

## Sequencing

Phase A → B → C → D is cheapest, but A is the anchor and C/D don't
depend on B. If parallel agent work is desired, B and C are disjoint
file sets — different agents can take one each.

Each phase gets its own commit. Each phase must pass
`fast_unit_editor` before the next lands.

## Out of scope (do not do)

- DockNode position serialization.
- In-app layout selector / CRUD.
- Visual layout designer (WYSIWYG drag-panels-on-thumbnail).
- Any edit to `src/app/editor/overworld/*` without explicit user
  green-light.
- Adding undo coupling to `property_inspector.{h,cc}`.

Ship Phase A's commit first — ancillary work starts blocking on it
otherwise (Phase D's index references the final API surface).
