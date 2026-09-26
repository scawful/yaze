# Overworld editor: where to make a change

Start with the table below. `OverworldEditor` owns editor lifecycle, shared state,
service wiring, save dispatch, and undo integration. `Tile16Editor` owns the
Tile16 definition window and canvases. Its session publishes edits directly to
the open overworld document. Both remain at this directory's
root so callers and parallel refactors have stable entry points.

## Directory responsibilities

| Location | Owns | Start here |
| --- | --- | --- |
| Root | Editor lifecycle and integration | [overworld_editor.h](overworld_editor.h), [overworld_editor.cc](overworld_editor.cc) |
| Root | Tile16 window wiring and canvases | [tile16_editor.h](tile16_editor.h), [tile16_editor.cc](tile16_editor.cc) |
| `canvas/` | Hit testing, pan/zoom, map selection, canvas rendering | [canvas_navigation_manager.cc](canvas/canvas_navigation_manager.cc), [overworld_canvas_renderer.cc](canvas/overworld_canvas_renderer.cc) |
| `painting/` | Captured rectangular brushes, painting, fill, and clipboard placement | [tile_brush.h](painting/tile_brush.h), [tile_painting_manager.cc](painting/tile_painting_manager.cc) |
| `maps/` | Property edits, metadata, refresh, and texture coordination | [map_properties.cc](maps/map_properties.cc), [map_refresh_coordinator.cc](maps/map_refresh_coordinator.cc), [map_texture_coordinator.cc](maps/map_texture_coordinator.cc) |
| `entity/` | Entity rendering, editing targets, and mutation services | [entity_workbench.cc](entity/entity_workbench.cc), [entity_mutation_service.cc](entity/entity_mutation_service.cc), [entity_operations.cc](entity/entity_operations.cc) |
| `tile16/` | Document edits, metadata transactions, palette/graphics coordination, history | [tile16_edit_session.h](tile16/tile16_edit_session.h), [tile16_edit_history.cc](tile16/tile16_edit_history.cc) |
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

Menu layout lives in `MapPropertiesSystem::SetupCanvasContextMenu`; the
current layout is documented in `docs/internal/gui/context-menus.md`
("Overworld map canvas"): header (map, tile), Tile, Map (select, properties,
pin, related maps), map-properties copy/paste, Insert, and a View submenu
that mirrors the toolbar toggles.

Where things live (each per-map value is shown and edited in one place):

| Surface | Contents |
|---------|----------|
| Toolbar (`ui/navigation/overworld_toolbar.cc`) | World LW/DW/SW, map id (click: Map Properties) + pin, tool (Select/Brush/Fill), entity focus (Entrances/Exits/Items/Sprites), view (grid, entities, overlay preview, zoom, fit, center), windows menu, Map Properties toggle |
| Map Properties panel (`ui/navigation/overworld_sidebar.cc`) | Game state, area size, message, area/sprite/animated graphics, custom tile sheets, area/main/sprite palettes, background color, music, visual effects, mosaic; right-click a value to rename its project label |
| Canvas context menu | Quick per-map actions only |

The view group folds into a "More" menu when the canvas is narrow (with
hysteresis so it does not flicker). Tooltips show live ShortcutManager
bindings via `GetDisplayString`.

Gestures (Select tool unless noted): left-drag empty map or middle-drag pans;
drag an entity to move it (item moves are undoable); double-click a map opens
Map Properties; right-click opens the map menu. Brush/Fill: right-click
samples the tile16 and makes that map current, right-drag captures a
multi-tile brush, `[`/`]` cycle the
tile16, Shift+right-click opens the map menu. Keys: `1` select, `2`/`B`
brush, `F` fill, `3`-`6` entity focus (plain keys only; Cmd/Ctrl+digits
switch editors), Alt+1/2/3 world, Alt+arrows adjacent map, `=`/`-` zoom, `0`
fit, Home center, `G` grid, `E` entities, arrows nudge the selected item.

Scrolling: `CanvasNavigationManager::BeginCanvasViewport` owns all canvas
scrolling and applies it with `SetNextWindowScroll` before the child begins
(no one-frame lag); the child sets `ImGuiWindowFlags_NoScrollWithMouse`.
Wheel/trackpad pan moves in whole detents of `kOverworldPanSnapMapPx` scaled
by zoom, drops sub-detent residue after `kOverworldWheelIdleResetSec`, and
ignores deltas under `kOverworldWheelDeadzone` (momentum tails).
Cmd/Ctrl+wheel zooms about the cursor. Drag pan pins the grabbed point under
the cursor and only starts from a press on the canvas. All tunables are in
`ui/ui_constants.h`.

Unpinned map selection follows the cursor in Mouse, Brush, and Fill modes.
Pin through the toolbar, Ctrl+L, or the context menu to hold the property target.
Brush/Fill previews and painted pixels use the destination map's own tile16
blockset and palette (`TilePaintingManager::DrawBrushPreview`,
`Tile16PixelsForMap`), so a pinned map or a stroke that crosses into another
area never shows or writes the current map's graphics there.
Explicit clicks select the map under the cursor even when pinned, and the pin
then holds that map: a Select-tool left click that did not pan (on release)
and a Brush/Fill right click. Both go through
`CanvasNavigationManager::SelectMapUnderCursor` (policy: `IsMapSelectClick`);
large areas resolve to the parent area's properties.
Middle-drag only pans; it does not pin or open properties. Entity dragging holds
the selected map until release. Hover tracking and right-click targeting share
the same scaled coordinate validation.

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

## Overworld sprite persistence

`src/zelda3/overworld/overworld_sprite_io.*` owns pointer/list encoding. Both
`Overworld::Save` and `OverworldEditor::Save` prepare the sprite plan before
mutating ROM data. The editor checks actual sprite write ranges against the
project hack manifest. Publication is fenced and rolls back on failure.

Only successfully loaded parent-map lists are replaced. Other lists are read
from the ROM and preserved, including expanded entries outside current loader
coverage. Exact ordered streams share storage; duplicate sprites and coordinate
flag bits are retained. Unchanged lists produce no save plan. Sprite movement
uses world coordinates relative to the owning parent, on a 16-pixel grid.
This does not implement cross-area sprite reassignment or expand loader coverage.

Do not add a second serializer in UI code, silently truncate positions, advance
an original-load baseline after save, or allocate past the reserved region.
Unsupported pointer relocation fails closed and needs an explicit layout extension.

## Make a bounded contribution

| Task | Scope | Acceptance check |
| --- | --- | --- |
| Improve a toolbar group or sidebar label | `ui/navigation/` | Existing shortcuts, IDs, focus, and disabled behavior remain correct; inspect narrow and wide windows. |
| Improve tile selector feedback | `ui/tiles/` plus an existing helper if needed | Hover, selection, and document edits remain distinct; compare preview and actual paint. |
| Extract one property validation rule | `maps/overworld_property_edit.*` | Add a focused valid/invalid test; preserve callbacks, undo, and unrelated metadata. |
| Simplify one Tile16 interaction rule | `tile16/` helper and its caller | Verify immediate publication, compound Undo/Redo, and document Save; keep layout changes separate. |
| Extract one entity operation | `entity/` | Prove identity and history across insert/delete; keep ROM encoding unchanged. |

For a file move, change only location, includes, CMake registration, and affected
documentation. Preserve function bodies. For behavior changes, state the trigger
and expected outcome first, then add a regression that observes that outcome.
Do not combine broad formatting, renaming, and feature work in one patch.

## Tile16 document contract

Cursor's session/workbench split is integrated in this repair branch. Change
layout in [tile16_workbench.cc](ui/tiles/tile16_workbench.cc), edit rules in
[tile16_edit_session.cc](tile16/tile16_edit_session.cc), and transaction/history
rules in [tile16_edit_history.cc](tile16/tile16_edit_history.cc).

1. `Overworld::tiles16()` is the authoritative definition data for a bound
   session. `BindDocument` gives the session the owning editor's `UndoManager`.
   The owner clears history when replacing/loading the document.
2. Every definition mutation enters `RunEdit`. It publishes metadata immediately,
   marks the document dirty, and records one complete before/after batch. A
   four-definition stamp must undo all four definitions. No-op/invalid edits
   must not clear Redo. Use `ReplaceCurrentTile` for property UI; do not mutate
   the pointer returned by `GetCurrentTile16Data` from new UI code.
3. `on_document_changed` invalidates shared map/atlas caches after edit, Undo,
   and Redo. The selected map is rebuilt immediately; other maps use the existing
   deferred refresh budget. History stores metadata, not graphics tied to the
   source map's palette. Do not restore stale cached pixels on map changes.
4. Definition edits, map paint strokes, Fill, and Paste share chronological
   history. Finish an open paint stroke before another mutation. Single and
   rectangular brush drags end at mouse release; Fill/Paste are discrete actions.
   Production paint actions disable time-based merging across separate strokes.
5. The application shortcut manager owns shared Undo/Redo and Save. The Tile16
   panel must not dispatch the same history shortcut again. Standalone sessions
   retain their own history for tests/tools. Normal Tile16 UI has no staging
   queue, commit/discard controls, or tile-switch confirmation.
6. `OverworldEditor::Save` serializes definitions through `SaveMap16Tiles` or
   `SaveMap16Expanded`. Edits/Undo/Redo do not write ROM bytes. Legacy standalone
   serialization adapters remain for older tooling/tests; bound sessions reject
   those write/revert paths. Their legacy `pending` accessors describe local edit
   caches, not an additional user confirmation step.


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
outside the canvas, and undo/redo. Definition editing additionally needs edit → switch map/palette → Undo/Redo
checked against both map and selector pixels, plus Save and independent reopen.
