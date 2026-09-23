# Overworld editor: where to make a change

Start with the table below. `OverworldEditor` owns editor lifecycle, shared state,
service wiring, save dispatch, and undo integration. `Tile16Editor` owns the
Tile16 definition window and its pending edits. Both remain at this directory's
root so callers and parallel refactors have stable entry points.

## Directory responsibilities

| Location | Owns | Start here |
| --- | --- | --- |
| Root | Editor lifecycle and integration | [overworld_editor.h](overworld_editor.h), [overworld_editor.cc](overworld_editor.cc) |
| Root | Tile16 definition editing and pending changes | [tile16_editor.h](tile16_editor.h), [tile16_editor.cc](tile16_editor.cc) |
| `canvas/` | Hit testing, pan/zoom, map selection, canvas rendering | [canvas_navigation_manager.cc](canvas/canvas_navigation_manager.cc), [overworld_canvas_renderer.cc](canvas/overworld_canvas_renderer.cc) |
| `painting/` | Captured rectangular brushes, painting, fill, and clipboard placement | [tile_brush.h](painting/tile_brush.h), [tile_painting_manager.cc](painting/tile_painting_manager.cc) |
| `maps/` | Property edits, metadata, refresh, and texture coordination | [map_properties.cc](maps/map_properties.cc), [map_refresh_coordinator.cc](maps/map_refresh_coordinator.cc), [map_texture_coordinator.cc](maps/map_texture_coordinator.cc) |
| `entity/` | Entity rendering, editing targets, and mutation services | [entity_workbench.cc](entity/entity_workbench.cc), [entity_mutation_service.cc](entity/entity_mutation_service.cc), [entity_operations.cc](entity/entity_operations.cc) |
| `tile16/` | Small Tile16 state, shortcut, source-selection, and undo helpers | [tile16_editor_action_state.h](tile16/tile16_editor_action_state.h), [tile8_source_interaction.h](tile16/tile8_source_interaction.h), [tile16_undo_actions.h](tile16/tile16_undo_actions.h) |
| `ui/navigation/` | Toolbar and sidebar layout | [overworld_toolbar.cc](ui/navigation/overworld_toolbar.cc), [overworld_sidebar.cc](ui/navigation/overworld_sidebar.cc) |
| `ui/tiles/` | Tile selector/window content and scratch workspace | [tile16_selector_view.cc](ui/tiles/tile16_selector_view.cc), [tile16_editor_view.cc](ui/tiles/tile16_editor_view.cc), [scratch_space.cc](ui/tiles/scratch_space.cc) |
| `ui/canvas/` | Canvas window content registration | [overworld_canvas_view.cc](ui/canvas/overworld_canvas_view.cc) |
| `ui/debug/` | Debug and usage-statistics content | [debug_window_card.cc](ui/debug/debug_window_card.cc), [usage_statistics_card.cc](ui/debug/usage_statistics_card.cc) |
| `ui/` and `ui/shared/` | Shared editor modes and window context | [ui_constants.h](ui/ui_constants.h), [overworld_window_context.h](ui/shared/overworld_window_context.h) |
| `core/` | Dispatch from keyboard/toolbar actions to editor callbacks | [interaction_coordinator.cc](core/interaction_coordinator.cc) |
| `panels/` | Existing compatibility wrappers and remaining window content | [overworld_panel_access.h](panels/overworld_panel_access.h) |

[automation.cc](automation.cc) stays beside the editor because it connects
multiple features. [overworld_undo_actions.h](overworld_undo_actions.h) also stays
there: it currently contains tile painting, entity, map-property, and project-label
history actions. Moving it into one feature would imply an ownership split that
has not happened.

The `panels/` compatibility directory remains supported. Do not combine a source
move with a migration of window IDs, content registration, or shell contracts.

## Trace a workflow

### Paint tiles on a map

1. Window content in `ui/canvas/` calls the editor. `canvas/` determines the
   hovered physical map and provides the canvas interaction state.
2. [TilePaintingManager](painting/tile_painting_manager.h) captures a rectangular
   selection into a `TileBrush`: width, height, and Tile16 IDs in row-major order.
   Painting and preview use that captured snapshot. Moving the hover or editing
   the source map must not recapture the selection.
3. The model in `src/zelda3/overworld/` owns tile IDs. Bitmap updates and the
   `maps/` coordinators make those changes visible.
4. `OverworldEditor::CreateUndoPoint` and `FinalizePaintOperation` record history.
   `OverworldEditor::Save` dispatches the model's ROM serializers.

Keep the hovered physical map distinct from its parent area. A multi-area room
can share graphics while each child map has its own tile positions. Treat Light,
Dark, and Special World coordinates explicitly when clipping a stamp or fill.

The painting implementation resolves each destination cell in world coordinates
and clips it against that world's bounds. A rectangular stamp may cross physical
screen boundaries. Undo records the cells that changed across those screens;
each stamp, fill, or paste finalizes its edit. Release also finalizes pending
single-tile painting outside the canvas. History retains its existing timed
merge window for consecutive paint actions.

Copy and saving to scratch read the captured brush snapshot. Scratch preserves
its existing 32-by-32 size limit. Clipboard paste uses the hovered canvas cell as
its anchor and the same painting path as a stamp. The fill operation repeats the
captured rectangle across the hovered screen. These contracts need focused tests
and visual acceptance; their presence in this guide is not a completed runtime
qualification claim.

### Edit a Tile16 definition

[Tile16Editor](tile16_editor.h) edits the four Tile8 entries, palette, flips, and
priority of a definition. Its pending model and bitmap previews are distinct
from the overworld's committed definitions. Trace `CommitAllChanges`,
`DiscardAllChanges`, and the callbacks installed in `OverworldEditor::Load`
together when changing that workflow.

Do not assume updating a definition updates every cached map or atlas. Check the
model, the active map's derived graphics, the selector atlas, and texture refresh
separately. A commit/discard cache fix needs both transitions covered.

Palette metadata stores a direct row (`0-7`). Recolor source pixels with
`(selected_row * 16) + (pixel & 0x0F)`. Read the current implementation and
[Tile16 data flow](../../../../docs/internal/architecture/tile16-data-flow.md)
before changing palette or graphics ownership.

### Edit map properties or entities

The canvas menu is prepared once when it opens. Its
[context target](canvas/overworld_context_target.h) captures the physical map,
parent area, world position, game state, and Tile16 ID. The
[context actions](canvas/overworld_context_actions.cc) resolve that value from
the opening click; menu callbacks must capture it by value. Do not rebuild the
menu from the hovered or selected map while a popup is open.

Menu layout lives in `MapPropertiesSystem::SetupCanvasContextMenu`. Common
selection and Tile16 actions stay at the top. Entity placement, map editing,
read-only map information, and view controls have separate submenus. The main
canvas supplies its own View menu, so shared built-in controls remain hidden.
Reset View is deferred to the canvas child window; resetting scroll while
inside an ImGui popup would affect the popup instead.

Use `maps/overworld_property_edit.h` for a property change and
`maps/overworld_map_metadata.h` for metadata resolution. Property UI must route
through the editor's edit callbacks so undo and required refresh remain coupled.

For entities, begin with `entity/entity_workbench.cc` and
`entity/entity_mutation_service.cc`. Keep stable entity identity separate from
vector position when changing selection, deletion, or history. ROM storage
formats belong under `src/zelda3/overworld/`, not in ImGui drawing code.

Deferred insertion consumes an
[entity insertion request](entity/entity_insertion_request.h) containing both
the entity type and captured destination. The Entity Workbench must not replace
that destination with the next frame's selected map or game state. Item and
sprite game coordinates are relative to the parent area's origin, including
child-screen offsets; modulo 512 discards those offsets.

## Make a bounded contribution

| Task | Scope | Acceptance check |
| --- | --- | --- |
| Improve a toolbar group or sidebar label | `ui/navigation/` | Existing shortcuts, IDs, focus, and disabled behavior remain correct; inspect narrow and wide windows. |
| Improve tile selector feedback | `ui/tiles/` plus an existing helper if needed | Hover, selection, and pending Tile16 edits remain distinct; compare preview and actual paint. |
| Extract one property validation rule | `maps/overworld_property_edit.*` | Add a focused valid/invalid test; preserve callbacks, undo, and unrelated metadata. |
| Simplify one Tile16 interaction rule | `tile16/` helper and its caller | Verify commit, discard, and undo boundaries; keep layout changes separate. |
| Extract one entity operation | `entity/` | Prove identity and history across insert/delete; keep ROM encoding unchanged. |

For a file move, change only location, includes, CMake registration, and affected
documentation. Preserve function bodies. For behavior changes, state the trigger
and expected outcome first, then add a regression that observes that outcome.
Do not combine broad formatting, renaming, and feature work in one patch.

The separate Cursor Tile16 session/workbench refactor is pending integration.
This layout does not claim that extraction has landed. Reconcile its ownership
and callbacks explicitly before merging; do not copy its uncommitted files into
an unrelated cleanup.

## Validation and handoff

1. Check `git status --short`, then inspect the touched feature and its tests.
   Register moved/new `.cc` files in
   [editor_library.cmake](../editor_library.cmake). Use canonical include paths
   such as `app/editor/overworld/maps/map_refresh_coordinator.h`.
2. Format changed C++ ranges with the repository `.clang-format`. For diagnostics,
   use the existing build's `compile_commands.json` and run `clang-tidy` on the
   touched translation unit. Review fixes individually; do not apply a repository
   wide `-fix` pass while debugging behavior.
3. Build once with the chosen preset. On macOS, the repository default is
   `cmake --preset mac-ai && cmake --build --preset mac-ai --parallel 4`.
   Other platforms should select their supported preset with
   `cmake --list-presets`; do not copy another machine's absolute build paths.
4. List tests before filtering, then run the affected suites. Useful source
   entry points are `test/unit/editor/tile_painting_manager_test.cc`,
   `canvas_navigation_manager_test.cc`, `map_refresh_coordinator_test.cc`,
   `overworld_map_metadata_test.cc`, `overworld_property_edit_test.cc`,
   `tile16_editor_action_state_test.cc`, and `tile8_source_interaction_test.cc`.
   Tile16 integration coverage lives in
   `test/integration/editor/tile16_editor_test.cc`.
5. Hand off the exact commit/worktree, owned files, reproducer, command/filter,
   observed test count, and one remaining acceptance step. Distinguish source
   checks, synthetic tests, ROM round trips, and hands-on UI acceptance. Passing
   one does not establish the others.

For visual acceptance, use a disposable ROM copy. Check paint/preview agreement,
fill boundaries, rectangular stamp dimensions, map/world transitions, release
outside the canvas, and undo/redo. Definition editing additionally needs commit
and discard checked against both map and selector pixels.
