## Canvas Slimming Plan

Owner: imgui-frontend-engineer  
Status: In Progress (Phases 1, 3, 5-Medium Complete)
Scope: `src/app/gui/canvas/` + editor call-sites  
Goal: Reduce `gui::Canvas` bloat, align lifecycle with ImGui-style, and enable safer per-frame usage without hidden state mutation.

### Current Problems
- Too many constructors and in-class defaults; state is scattered across legacy fields (`custom_step_`, `global_scale_`, enable_* duplicates).
- Per-frame options are applied via mutations instead of “Begin arguments,” diverging from ImGui patterns.
- Heavy optional subsystems (bpp dialogs, palette editor, modals, automation) live on the core type.
- Helpers rely on implicit internal state (`draw_list_`, `canvas_p0_`) instead of a passed-in context.

### Target Shape
- **Persistent state:** `CanvasConfig`, `CanvasSelection`, stable IDs, scrolling, rom/game_data pointers.
- **Transient per-frame:** `CanvasRuntime` (draw_list, geometry, hover flags, resolved grid step/scale). Constructed in `Begin`, discarded in `End`.
- **Options:** `CanvasFrameOptions` (and `BitmapPreviewOptions`) are the “Begin args.” Prefer per-frame opts over setters.
- **Helpers:** Accept a `CanvasRuntime&` (or `CanvasContext` value) rather than reading globals; keep wrappers for compatibility during migration.
- **Optional features:** Move bpp/palette/modals/automation behind lightweight extension pointers or a separate host struct.
- **Construction:** One ctor + `Init(CanvasConfig)` (or default). Deprecate overloads; keep temp forwarding for compatibility.

### Phased Work
1) **Runtime extraction (core)** [COMPLETE]
   - [x] Add `CanvasRuntime` (geometry, draw_list, hover, resolved grid step/scale, content size).
   - [x] Add `CanvasRuntime`-based helpers: `DrawBitmap`, `DrawBitmapPreview`, `RenderPreviewPanel`.
   - [x] Make `Begin/End` create and tear down runtime; route grid/overlay/popups through it.
     - Implemented `BeginCanvas(Canvas&, CanvasFrameOptions)` returning `CanvasRuntime`
     - Implemented `EndCanvas(Canvas&, CanvasRuntime&, CanvasFrameOptions)` handling grid/overlay/popups
   - [ ] Deprecate internal legacy mirrors (`custom_step_`, `global_scale_`, enable_* duplicates); keep sync shims temporarily.

2) **API narrowing** [PENDING]
   - [ ] Collapse ctors to a single default + `Init(config)`.
   - [ ] Mark setters that mutate per-frame state (`SetCanvasSize`, `SetGridSize`, etc.) as "compat; prefer frame options."
   - [ ] Add `BeginInTable(label, CanvasFrameOptions)` that wraps child sizing and returns a runtime-aware frame.

3) **Helper refactor** [COMPLETE]
   - [x] Change helper signatures to accept `CanvasRuntime&`: `DrawBitmap`, `DrawBitmapPreview`, `RenderPreviewPanel`.
   - [x] Refactor remaining helpers: `DrawTilemapPainter`, `DrawSelectRect`, `DrawTileSelector`.
     - All three now have stateless `CanvasRuntime`-based implementations
     - Member functions delegate to stateless helpers via `BuildCurrentRuntime()`
   - [x] Keep thin wrappers that fetch the current runtime for legacy calls.
     - Added `Canvas::BuildCurrentRuntime()` private helper
   - [x] Added `DrawBitmap(CanvasRuntime&, Bitmap&, BitmapDrawOpts)` overload for options-based drawing.

4) **Optional modules split** [PENDING]
   - [ ] Move bpp dialogs, palette editor, modals, automation into an extension struct (`CanvasExtensions`) held by unique_ptr on demand.
   - [ ] Core `Canvas` remains lean even when extensions are absent.

5) **Call-site migration** [IN PROGRESS]
   - [x] Update low-risk previews first: graphics thumbnails (`sheet_browser_panel`), small previews (`object_editor_panel`, `link_sprite_panel`).
   - [x] Medium: screen/inventory canvases, sprite/tileset selectors.
     - `DrawInventoryMenuEditor`: migrated to `BeginCanvas/EndCanvas` + stateless `DrawBitmap`
     - `DrawDungeonMapsRoomGfx`: migrated tilesheet and current_tile canvases to new pattern
   - [ ] High/complex: overworld/dungeon main canvases; migrate last after patterns are stable.

6) **Cleanup & deprecations** [PENDING]
   - [ ] Remove deprecated ctors/fields after call-sites are migrated.
   - [ ] Document "per-frame opts" pattern and add brief usage examples in `canvas.h`.

### Where to Start (low risk, high leverage)
- **COMPLETED (Phase 1 - Low Risk):** Migrated low-risk graphics thumbnails and previews:
  - `src/app/editor/graphics/sheet_browser_panel.cc` (Thumbnails use `DrawBitmapPreview(rt, ...)` via `BeginCanvas`)
  - `src/app/editor/dungeon/panels/object_editor_panel.cc` (Static editor preview uses `RenderPreviewPanel(rt, ...)`)
  - `src/app/editor/graphics/link_sprite_panel.cc` (Preview canvas uses `DrawBitmap(rt, ...)` with manual begin/end)

- **COMPLETED (Phase 2 - Medium Risk):** Migrated screen editor canvases:
  - `src/app/editor/graphics/screen_editor.cc`:
    - `DrawInventoryMenuEditor`: Uses `BeginCanvas/EndCanvas` + stateless `gui::DrawBitmap(rt, ...)`
    - `DrawDungeonMapsRoomGfx`: Tilesheet canvas and current tile canvas migrated to `BeginCanvas/EndCanvas` pattern with `CanvasFrameOptions` for grid step configuration

### Editor-Specific Strategies (for later phases)

- **Overworld Editor (high complexity)**
  - Surfaces: main overworld canvas (tile painting, selection, multi-layer), scratch space, tile16 selector, property info grids.
  - Approach:
    - Keep **main canvas last**. Start with property/info grids and selectors already close to preview patterns.
    - For the main canvas: preserve selection and drag behavior. Move interaction state into `CanvasRuntime` (hover, click positions) and keep `CanvasSelection` for persistence. Ensure grid step matches map tile size; pass via frame options.
    - Use helpers instead of `draw_list` math for overlays (entity highlights, selection rects) by feeding `CanvasRuntime` into overlay helpers.
    - Context menus/persistent popups are important; leave `render_popups` enabled. Verify zoom/scroll (use runtime geometry).
    - Property canvases using `UpdateInfoGrid` can stay; when converting, ensure label rendering uses context-aware helpers.

- **Dungeon Editor**
  - Surfaces: room graphics viewer, object selector preview, integrated editor panels, room canvases with palettes/blocks.
  - Approach:
    - Keep the **room canvas** migration after the previews already done. Room canvas relies on textures from `gfx::Arena`; keep `ensure_texture` paths when drawing bitmaps/tiles.
    - Use `CanvasRuntime` for grid-aligned draws; set grid step to the block/tile size (32). Replace manual `zero_point()` math with `AddImageAt` equivalents that accept runtime/context.
    - Selection/placement: keep `CanvasSelection` persistent; per-frame hover/click lives in runtime to avoid hidden state drift.
    - Context menu/popup flows are used; keep `render_popups` true.

- **Screen/Inventory Editors** [MIGRATED]
  - `DrawInventoryMenuEditor` and `DrawDungeonMapsRoomGfx` now use `BeginCanvas/EndCanvas` with `CanvasFrameOptions`.
  - Stateless `gui::DrawBitmap(rt, ...)` and `gui::DrawTileSelector(rt, ...)` used throughout.
  - Grid step configured via `CanvasFrameOptions` (32 for screen, 16/32 for tilesheet).

- **Graphics/Pixels Panels**
  - Low risk; continue to replace manual `draw_list` usage with `DrawBitmapPreview` and runtime-aware helpers. Ensure `ensure_texture=true` for arena-backed bitmaps.

- **Automation/Testing Surfaces**
  - Leave automation API untouched until core migration is stable. When ready, have it consume `CanvasRuntime` so tests don’t depend on hidden members.

### Testing Focus per Editor
- Overworld: selection rect correctness, tile ID math, zoom/scroll invariants, context menu actions, persistent popups.
- Dungeon: grid alignment for blocks/objects, palette-application correctness, texture creation/update via `Arena`.
- Screen/Inventory: zoom buttons, grid overlay alignment, selectors.
- Graphics panels: texture creation on demand, grid overlay spacing, tooltip/selection hits.

### Context Menu & Popup Cleanup
- Problem: Caller menus stack atop generic defaults; composition is implicit. Popups hang off internal state instead of the menu contract.
- Target shape:
  - `CanvasMenuHost` (or similar) owns menu items for the frame. Generic items are registered explicitly via `RegisterDefaultCanvasMenu(host, runtime, config)`.
  - Callers add items with `AddItem(label, shortcut, callback, enabled_fn)` or structured entries (id, tooltip, icon).
  - Rendering is single-pass per frame: `RenderContextMenu(host, runtime, config)`. No hidden additions elsewhere.
  - Persistent popups are tied to menu actions and rendered via the same host (or a `PopupRegistry` owned by it), gated by `CanvasFrameOptions.render_popups`.
- Migration plan:
  1) Extract menu item POD (id, label, shortcut text, optional enable/predicate, callback).
  2) Refactor `DrawContextMenu` to:
     - Create/clear a host each frame
     - Optionally call `RegisterDefaultCanvasMenu`
     - Provide a caller hook to register custom items
     - Render once.
  3) Deprecate ad-hoc menu additions inside draw helpers; require explicit registration.
  4) Keep legacy editor menus working by shimming their registrations into the host; remove implicit defaults once call-sites are migrated.
  5) Popups: route open/close through the host/registry; render via a single `RenderPersistentPopups(host)` invoked from `End` when `render_popups` is true.
- Usage pattern for callers (future API):
  - `auto& host = canvas.MenuHost();`
  - `host.Clear();`
  - `RegisterDefaultCanvasMenu(host, runtime, config);  // optional`
  - `host.AddItem("Custom", "Ctrl+K", []{ ... });`
  - `RenderContextMenu(host, runtime, config);`

### Design Principles to Follow
- ImGui-like: options-as-arguments at `Begin`, minimal persistent mutation, small POD contexts for draw helpers.
- Keep the core type thin; optional features live in extensions, not the base.
- Maintain compatibility shims temporarily, but mark them and plan removal once editors are migrated.

### Quick Reference for Future Agents
- Core files to touch: `src/app/gui/canvas/canvas.{h,cc}`, `canvas_menu.{h,cc}`, `canvas_context_menu.{h,cc}`, `canvas_menu_builder.{h,cc}`, `canvas_utils.{h,cc}`. Common call-sites: `screen_editor.cc`, `link_sprite_panel.cc`, `sheet_browser_panel.cc`, `object_editor_panel.cc`, `dungeon_object_selector.cc`, overworld/dungeon editors.
- Avoid legacy patterns: direct `draw_list()` math in callers, `custom_step_`/`global_scale_` duplicates, scattered `DrawBackground/DrawGrid/DrawOverlay` chains, implicit menu stacking in `DrawContextMenu`.
- Naming/API standardization:
  - Per-frame context: `CanvasRuntime`; pass it to helpers.
  - Options at begin: `CanvasFrameOptions` (via `BeginCanvas/EndCanvas` or `CanvasFrame`), not mutating setters (setters are compat-only).
  - Menu host: use `CanvasMenuAction` / `CanvasMenuActionHost` (avoid `CanvasMenuItem` collision).
  - **Implemented stateless helpers:**
    - `DrawBitmap(rt, bitmap, ...)` - multiple overloads including `BitmapDrawOpts`
    - `DrawBitmapPreview(rt, bitmap, BitmapPreviewOptions)`
    - `RenderPreviewPanel(rt, bitmap, PreviewPanelOpts)`
    - `DrawTilemapPainter(rt, tilemap, current_tile, out_drawn_pos)` - returns drawn position via output param
    - `DrawTileSelector(rt, size, size_y, out_selected_pos)` - returns selection via output param
    - `DrawSelectRect(rt, current_map, tile_size, scale, CanvasSelection&)` - updates selection struct
  - **Frame management:**
    - `BeginCanvas(canvas, CanvasFrameOptions)` → returns `CanvasRuntime`
    - `EndCanvas(canvas, runtime, CanvasFrameOptions)` → draws grid/overlay/popups based on options
  - **Legacy delegation:** Member functions delegate to stateless helpers via `Canvas::BuildCurrentRuntime()`
  - Context menu flow: host.Clear → optional `RegisterDefaultCanvasMenu(host, rt, cfg)` → caller adds items → `RenderContextMenu(host, rt, cfg)` once per frame.
- Migration checklist (per call-site):
  1) Replace manual DrawBackground/DrawGrid/DrawOverlay/popup sequences with `CanvasFrame` or `BeginCanvas/EndCanvas` using `CanvasFrameOptions`.
  2) Replace `draw_list()/zero_point()` math with `AddImageAt`/`AddRectFilledAt`/`AddTextAt` or overlay helpers that take `CanvasRuntime`.
  3) For tile selection, use `TileIndexAt` with grid_step from options/runtime.
  4) For previews/selectors, use `RenderPreviewPanel` / `RenderSelectorPanel` (`ensure_texture=true` for arena bitmaps).
  5) For context menus, switch to a `CanvasMenuActionHost` + explicit render pass.
- Deprecations to remove after migration: `custom_step_`, `global_scale_` duplicates, legacy `enable_*` mirrors, direct `draw_list_` access in callers.
- Testing hints: pure helpers for tests (`TileIndexAt`, `ComputeZoomToFit`, `ClampScroll`). Manual checks per editor: grid alignment, tile hit correctness, zoom/fit, context menu actions, popup render, texture creation (`ensure_texture`).
- Risk order: low (previews/thumbnails) → medium (selectors, inventory/screen) → high (overworld main, dungeon main). Start low, validate patterns, then proceed.

### Future Feature Ideas (for follow-on work)

Interaction & UX
- Smooth zoom/pan (scroll + modifiers), double-click zoom-to-fit, snap-to-grid toggle.
- Rulers/guides aligned to `grid_step`, draggable guides, and a measure tool that reports delta in px and tiles.
- Hover/tooltips: configurable hover info (tile id, palette, coordinates) and/or a status strip.
- Keyboard navigation: arrow-key tile navigation, PgUp/PgDn for layer changes, shortcut-able grid presets (8/16/32/64).

Drawing & overlays
- Layer-aware overlays: separate channels for selection/hover/warnings with theme-driven colors.
- Mask/visibility controls: per-layer visibility toggles and opacity sliders for multi-layer editing.
- Batch highlight API: accept a span of tile ids/positions and render combined overlays to reduce draw calls.

Selection & tools
- Lasso/rect modes with additive/subtractive (Shift/Ctrl) semantics and a visible mode indicator.
- Clamp-to-region selection for maps; expose a “clamp to region” flag in options/runtime.
- Quick selection actions: context submenu for fill/eyedropper/replace palette/duplicate to scratch.

Panels & presets
- Preset grid/scale sets (“Pixel”, “Tile16”, “Map”) that set grid_step + default zoom together.
- Per-canvas profiles to store/recall `CanvasConfig` + view (scroll/scale) per editor.

Performance & rendering
- Virtualized rendering: auto skip off-screen tiles/bitmaps; opt-in culling flag for large maps.
- Texture readiness indicator: badge/text when a bitmap lacks a texture; one-click “ensure texture.”
- Draw-call diagnostics: small overlay showing batch count and last-frame time.

Automation & testing
- Deterministic snapshot API: export runtime (hover tile, selection rect, grid step, scale, scroll) for tests/automation.
- Scriptable macro hooks: tiny API to run canvas actions (zoom, pan, select, draw tile) for recorded scripts.

Accessibility & discoverability
- Shortcut cheatsheet popover for canvas actions (zoom, grid toggle, fit, selection modes).
- High-contrast overlays and grids; configurable outline thickness.
