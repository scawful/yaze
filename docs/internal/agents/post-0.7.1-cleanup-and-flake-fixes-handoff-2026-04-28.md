# Post-0.7.1 Cleanup + Test-Pollution Fixes — Handoff

**Date:** 2026-04-28
**Branch:** `master`
**Range:** `5a77224d..ec8e3069` (5 commits)

This thread spans two arcs:

1. **UX density polish + 0.7.1 reorg cleanup** — extending the workbench
   `StyleVarGuard` relaxation pass into adjacent surfaces, then sweeping the
   orphan files left behind by the 0.7.1 editor-tree reorg.
2. **Singleton-pollution test flakes** — investigation that traced two
   apparently-unrelated unit-test segfaults to one root cause: process-wide
   singletons holding raw pointers into `EditorManager`-owned members.

---

## Commits

| Commit | Subject | Net |
|--------|---------|----:|
| `5a77224d` | memory: relax memory editor toolbar style guard | +6 / -7 |
| `5b725dde` | editor: remove orphan `.cc` files from 0.7.1 reorg | -7,852 |
| `cff0a011` | overworld: remove orphan view/panel `.cc+.h` pairs | -1,233 |
| `b24a5c75` | agent: collapse `panels/` header shims into canonical layer | -48 |
| `ec8e3069` | test: fix two test-pollution flakes from `EditorManager` teardown | +60 / -2 |

Total: ~9,133 LOC removed, 1 latent crash class fixed, 1 regression test
added. 590/590 in `yaze_test_quick_unit_editor` (was crashing twice before
arc 2).

---

## Arc 1 — Density polish + reorg cleanup

### `5a77224d` — Memory editor toolbar style guard

Extended the dungeon-workbench relaxation pass into `MemoryEditor::DrawToolbar`.
Replaced the legacy `pad * 0.67f` shrinking on `FramePadding` and `ItemSpacing`
with the gentle-compaction template:

```cpp
gui::StyleVarGuard toolbar_vars(
    {{ImGuiStyleVar_FramePadding,
      ImVec2(std::max(4.0f, style.FramePadding.x),
             std::max(3.0f, style.FramePadding.y - 1.0f))},
     {ImGuiStyleVar_ItemSpacing,
      ImVec2(std::max(4.0f, style.ItemSpacing.x),
             std::max(3.0f, style.ItemSpacing.y - 1.0f))}});
```

**Why:** same antipattern as `bc703ddb`/`d0800473`/`aff8dcd7`/`47ba0bb7` —
aggressive `* 0.67f` multiplier on `ItemSpacing.x` crushed the `SameLine()`
gaps between toolbar buttons. The user complained about cramped buttons
2026-04-27/28 in the workbench; the pattern recurred in adjacent compact
toolbars.

Memory file `feedback_workbench_style_guard_density.md` was updated to extend
its scope to "workbench + adjacent compact toolbars" and append `5a77224d` to
the relaxation-pass commit list.

### `5b725dde` — 24 orphan `.cc` files from 0.7.1 reorg

The 0.7.1 reorg patch series (`ed28f264`, `5a08fbe0`) moved files into new
homes (e.g. `editor/dungeon/panels/` → `editor/dungeon/ui/window/`) without
`git mv`. The original locations remained in the tree as zero-build-references
orphans. Deleted:

- 12 files in `src/app/editor/agent/ui/window/*_panel.cc` (replaced by
  `agent/panels/*_panel.cc`).
- 7 files in `src/app/editor/hack/oracle/{,ui/}*.cc` (replaced by `hack/oracle/`
  flat layout).
- 3 files in `src/app/editor/ui/*_dialog.cc` (replaced by `shell/dialogs/`).
- 2 files in `src/app/editor/core/{content_registry,undo_manager}.cc`
  (replaced by `editor/registry/`).

Verification gate: each candidate was confirmed orphan via two checks — basename
maps to an in-build replacement, and `cmake --build` completes without the file.

### `cff0a011` — 28 overworld dead `.cc+.h` pairs

A "view/" pattern in `src/app/editor/overworld/ui/` was scaffolded for a
view-model split that never landed; the panels keep their pre-split shape, so
`*_view.{cc,h}` pairs were dead together. Same orphan-verification rigor as
`5b725dde`. Deleted 14 pairs:

```
overworld/overworld_entity_interaction.{cc,h}
overworld/overworld_view.{cc,h}
overworld/panels/{overworld_canvas,tile16_editor,tile16_selector,tile8_selector}_panel.{cc,h}
overworld/ui/analytics/usage_statistics_view.{cc,h}
overworld/ui/debug/debug_window_view.{cc,h}
overworld/ui/graphics/{area_graphics,gfx_groups}_view.{cc,h}
overworld/ui/items/overworld_item_list_view.{cc,h}
overworld/ui/properties/{map_properties,v3_settings}_view.{cc,h}
overworld/ui/workspace/scratch_space_view.{cc,h}
```

Touched the standing rule "no edits to `src/app/editor/overworld/*` without
green-light" — explicit user "Yes" green-light obtained before this commit
landed.

### `b24a5c75` — Agent panel header shim consolidation

For each of the 12 agent panels, `panels/foo.h` was a 4-line forward shim that
`#include`d the real header at `ui/window/foo.h`. Same include-guard symbol on
both sides made overwrite consolidation safe:

```cpp
// shim form (deleted)
#ifndef YAZE_APP_EDITOR_AGENT_PANELS_AGENT_CONFIGURATION_PANEL_H_
#include "app/editor/agent/ui/window/agent_configuration_panel.h"
#define YAZE_APP_EDITOR_AGENT_PANELS_AGENT_CONFIGURATION_PANEL_H_
#endif
```

Per panel: `git rm panels/foo.h` (delete shim) + `git mv ui/window/foo.h
panels/foo.h` (move real). `agent/ui/window/` and `agent/ui/` directories
removed once empty. Net: 12 files deleted, 12 modified, -48 LOC of forwarding
cruft.

---

## Arc 2 — `ec8e3069`: Test-pollution flake fixes

### Symptoms

Two unit tests crashed when the full editor suite ran, but passed in
isolation:

- `CanvasNavigationManagerTest.ZoomInIncrementsByStep` — SIGSEGV in
  `EventBus::Publish` from `Canvas::set_global_scale`. First crash blocked the
  rest of the suite.
- `DungeonEditCommandsTest.PlaceSpriteWriteRelocatesAndRoundTrips` — SIGBUS
  in `unordered_map::find` from `ResourceLabelProvider::GetLabel` via
  `Sprite::Sprite() -> ResolveSpriteName`. Pre-existing but masked by the
  first crash.

### Root cause

Both share a structural pattern: a process-wide singleton holds a non-owning
pointer into a member of `EditorManager`. The lifetime contract is "owner
outlives the registration." `EditorManager::EditorManager()` honors the half
that says "register on construction" via:

- `ContentRegistry::Context::SetGlobalContext(editor_context_.get())`
- `ContentRegistry::Context::SetEventBus(&event_bus_)`
- `ContentRegistry::Context::SetUserSettings(&user_settings_)`
- ... + several callbacks
- (transitively, when a project is opened) `provider.SetProjectLabels(&resource_labels)`
  + `provider.SetHackManifest(&hack_manifest)`

`~EditorManager` was missing the corresponding teardown. After test fixtures
destroyed an `EditorManager`, the singleton held dangling pointers; the next
test that read the singleton crashed.

`Context::Clear()` (used at session-close) intentionally keeps `global_context`
non-null because `EditorManager` is still alive at that point. At destruction,
both `EditorManager` and its `editor_context_` `unique_ptr` die — so the dtor
must do `Clear()` *plus* `SetGlobalContext(nullptr)`.

### Polluter for flake 2

`OverworldMapMetadataTest.RenameResourceLabelNormalizesAndClearsAliases` (in
`test/unit/editor/overworld_map_metadata_test.cc`) creates a stack-local
`YazeProject`, then calls `RenameProjectResourceLabel(...)`. That production
helper calls `project->InitializeResourceLabelProvider()` which writes
`provider.project_labels_ = &project.resource_labels`. The test never cleared
the singleton, and the stack-local `project` died at end of scope.

Bisected via `--gtest_filter` from prefix groups (`Dungeon*`, `Overworld*`,
`CanvasNavigation*`, `Map*`) down to the single offending test.

### Fix

`src/app/editor/editor_manager.cc::~EditorManager` (commit `ec8e3069`):

```cpp
EditorManager::~EditorManager() {
  // ContentRegistry::Context holds non-owning pointers into our members
  // (event_bus_, editor_context_, user_settings_, layout_manager_, callbacks).
  // Clear them before our members destruct so widgets that read the singleton
  // (e.g. Canvas::set_global_scale via Context::event_bus()) don't see
  // dangling pointers in tests that instantiate multiple EditorManagers.
  // Clear() resets fallback state but intentionally keeps global_context for
  // session-close semantics, so null it explicitly here.
  ContentRegistry::Context::Clear();
  ContentRegistry::Context::SetGlobalContext(nullptr);

  // GetResourceLabels() is a process-wide singleton.
  // RefreshResourceLabelProvider hands it a pointer to
  // current_project_.hack_manifest, which dangles after we destruct.
  // Clear before our members go out of scope.
  zelda3::GetResourceLabels().SetHackManifest(nullptr);
  zelda3::GetResourceLabels().SetProjectLabels(nullptr);

  // ... (existing ThemeManager + EventBus subscription comments retained)
}
```

`test/unit/editor/overworld_map_metadata_test.cc` — converted both free
`TEST(OverworldMapMetadataTest, ...)` blocks to `TEST_F` with a fixture whose
`TearDown` resets the singleton. `TearDown` runs even on `ASSERT_*` failure;
inline cleanup at end-of-test is fragile.

`test/unit/editor/editor_manager_test.cc` — added regression test
`EditorManagerLifecycleTest.DestructorClearsSingletonPointers` that
construct-then-destructs an `EditorManager` and asserts both
`Context::event_bus()` and `GetResourceLabels().GetAllProjectLabels()` are
nullptr afterward.

### Verification

590/590 in `yaze_test_quick_unit_editor` (was 0/588 with first crash, then
588/589 after first fix isolated the second). Both flakes also reproduce +
clear in isolation via targeted `--gtest_filter`.

---

## Working-tree caveats (still WIP at handoff)

These files were modified by Codex's parallel role-aware-palette / canvas
context-menu thread and were **not** staged with my commits — they remain in
the working tree:

```
docs/internal/agents/README.md
docs/internal/architecture/{overworld_editor_system,undo_redo_system}.md
src/app/editor/dungeon/{dungeon_canvas_overlays,dungeon_object_selector}.{cc,h}
src/app/editor/graphics/{graphics_editor,link_sprite_panel,palette_controls_panel,
                         screen_editor,sheet_browser_panel}.cc
src/app/editor/graphics/ui/{browser/sheet_browser_view,palette/palette_controls_view,
                            sprite/link_sprite_view}.cc
src/app/editor/message/message_editor.cc
src/app/editor/overworld/{map_properties,overworld_property_edit}.{cc,h}
src/app/editor/sprite/sprite_editor.cc
src/app/gui/canvas/{canvas_context_menu,canvas_rendering,canvas_types}.{cc,h}
test/{,unit/editor/{gfx_group_editor_render,overworld_property_edit}}_test.cc
test/CMakeLists.txt
+ 6 untracked files
```

These belong to a separate thread and should be staged + committed by whoever
owns that work — not by anyone resuming this handoff.

---

## Remaining risks / follow-ups

1. **Other tests touching process-wide singletons without `TearDown`.** A
   small audit (`grep -rn "GetResourceLabels()\|ContentRegistry::Context::Set"
   test/`) can surface them. Each free `TEST(...)` that mutates a singleton
   is a future flake waiting for ordering to expose it.
2. **Future singleton additions to `EditorManager`.** If a new feature adds
   a third place where `EditorManager` registers raw pointers in a
   process-wide singleton, the regression test catches only the two
   currently-pinned signals (`event_bus`, `project_labels`). Worth either
   broadening the regression test or moving the singleton-pointer set to a
   single named function (`EditorManager::ClearStaticRegistrations()`)
   called from the dtor and asserted-on by the test.
3. **Working-tree cross-thread changes** (above) need their own commit pass
   from the agent that owns them.

---

## Cross-references

- `feedback_workbench_style_guard_density.md` (memory) — gentle-compaction
  pattern; updated to cover memory editor extension `5a77224d`.
- `docs/internal/agents/dungeon-workbench-usability-handoff-2026-04-26.md`
  — prior arc that triggered the workbench relaxation pass; this handoff is a
  follow-on slice.
- `docs/internal/agents/dungeon-ui-stability-handoff-2026-04-26.md` — adjacent
  thread on dungeon viewer/context-menu polish.
