## Canvas Slimming Plan

Owner: imgui-frontend-engineer  
Status: In Progress (Phases 1-5 Complete for Dungeon + Overworld; Phase 6 Pending)
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

2) **API narrowing** [COMPLETE]
   - [x] Collapse ctors to a single default + `Init(config)`.
     - Added `Init(const CanvasConfig& config)` and `Init(const std::string& id, ImVec2 canvas_size)` methods
     - Legacy constructors marked with `[[deprecated("Use default ctor + Init() instead")]]`
   - [x] Mark setters that mutate per-frame state (`SetCanvasSize`, `SetGridSize`, etc.) as "compat; prefer frame options."
     - Added COMPAT comments to `SetGridSize`, `SetCustomGridStep`, `SetCanvasSize`, `SetGlobalScale`, `set_global_scale`
   - [x] Add `BeginInTable(label, CanvasFrameOptions)` that wraps child sizing and returns a runtime-aware frame.
     - Implemented `BeginInTable(label, CanvasFrameOptions)` returning `CanvasRuntime`
     - Implemented `EndInTable(CanvasRuntime&, CanvasFrameOptions)` handling grid/overlay/popups

3) **Helper refactor** [COMPLETE]
   - [x] Change helper signatures to accept `CanvasRuntime&`: `DrawBitmap`, `DrawBitmapPreview`, `RenderPreviewPanel`.
   - [x] Refactor remaining helpers: `DrawTilemapPainter`, `DrawSelectRect`, `DrawTileSelector`.
     - All three now have stateless `CanvasRuntime`-based implementations
     - Member functions delegate to stateless helpers via `BuildCurrentRuntime()`
   - [x] Keep thin wrappers that fetch the current runtime for legacy calls.
     - Added `Canvas::BuildCurrentRuntime()` private helper
   - [x] Added `DrawBitmap(CanvasRuntime&, Bitmap&, BitmapDrawOpts)` overload for options-based drawing.

4) **Optional modules split** [COMPLETE]
   - [x] Move bpp dialogs, palette editor, modals, automation into an extension struct (`CanvasExtensions`) held by unique_ptr on demand.
     - Created `canvas_extensions.h` with `CanvasExtensions` struct containing: `bpp_format_ui`, `bpp_conversion_dialog`, `bpp_comparison_tool`, `modals`, `palette_editor`, `automation_api`
     - Created `canvas_extensions.cc` with lazy initialization helpers: `InitializeModals()`, `InitializePaletteEditor()`, `InitializeBppUI()`, `InitializeAutomation()`
     - Canvas now uses single `std::unique_ptr<CanvasExtensions> extensions_` with `EnsureExtensions()` lazy accessor
   - [x] Core `Canvas` remains lean even when extensions are absent.
     - Extensions only allocated on first use of optional features
     - All Show* methods and GetAutomationAPI() delegate to EnsureExtensions()

5) **Call-site migration** [COMPLETE]
   - [x] Update low-risk previews first: graphics thumbnails (`sheet_browser_panel`), small previews (`object_editor_panel`, `link_sprite_panel`).
   - [x] Medium: screen/inventory canvases, sprite/tileset selectors.
     - `DrawInventoryMenuEditor`: migrated to `BeginCanvas/EndCanvas` + stateless `DrawBitmap`
     - `DrawDungeonMapsRoomGfx`: migrated tilesheet and current_tile canvases to new pattern
   - [x] High/complex - Dungeon Editor: `DungeonCanvasViewer::DrawDungeonCanvas` migrated
     - Uses `BeginCanvas(canvas_, frame_opts)` / `EndCanvas(canvas_, canvas_rt, frame_opts)` pattern
     - Entity rendering functions updated to accept `CanvasRuntime&` parameter
     - All `canvas_.DrawRect/DrawText` calls replaced with `gui::DrawRect(rt, ...)` / `gui::DrawText(rt, ...)`
     - Grid visibility controlled via `frame_opts.draw_grid = show_grid_`
   - [x] High/complex - Overworld Editor: `DrawOverworldCanvas` and secondary canvases migrated
     - Main canvas uses `BeginCanvas/EndCanvas` with `CanvasFrameOptions`
     - Entity renderer (`OverworldEntityRenderer`) updated with `CanvasRuntime`-based methods
     - Scroll bounds implemented via `ClampScroll()` helper in `HandleOverworldPan()`
     - Secondary canvases migrated: `scratch_canvas_`, `current_gfx_canvas_`, `graphics_bin_canvas_`
     - Zoom support deferred (documented in code); positions not yet scaled with global_scale

6) **Cleanup & deprecations** [PENDING]
   - [ ] Remove deprecated ctors/fields after call-sites are migrated.
   - [ ] Document "per-frame opts" pattern and add brief usage examples in `canvas.h`.
   - [ ] Remove legacy `BeginCanvas(Canvas&, ImVec2)` overload (only used by `message_editor.cc`)
   - [ ] Audit remaining `AlwaysVerticalScrollbar` usage in `GraphicsBinCanvasPipeline`/`BitmapCanvasPipeline`
   - [ ] Remove `custom_step_`, `global_scale_` duplicates once all editors use `CanvasFrameOptions`
   - [ ] Consider making `CanvasRuntime` a first-class return from all `Begin` variants

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

- **Overworld Editor (high complexity)** [MIGRATED]
  - Surfaces: main overworld canvas (tile painting, selection, multi-layer), scratch space, tile16 selector, property info grids.
  - **Completed Migration:**
    - `DrawOverworldCanvas`: Uses `BeginCanvas(ow_map_canvas_, frame_opts)` / `EndCanvas()` pattern
    - `OverworldEntityRenderer`: Added `CanvasRuntime`-based methods (`DrawEntrances(rt, world)`, `DrawExits(rt, world)`, etc.)
    - `HandleOverworldPan`: Now clamps scrolling via `gui::ClampScroll()` to prevent scrolling outside map bounds
    - `CenterOverworldView`: Properly centers on current map with clamped scroll
    - `DrawScratchSpace`: Migrated to `BeginCanvas/EndCanvas` pattern
    - `DrawAreaGraphics`: Migrated `current_gfx_canvas_` to new pattern
    - `DrawTile8Selector`: Migrated `graphics_bin_canvas_` to new pattern
  - **Key Implementation Details:**
    - Context menu setup happens BEFORE `BeginCanvas` via `map_properties_system_->SetupCanvasContextMenu()`
    - Entity drag/drop uses `canvas_rt.scale` and `canvas_rt.canvas_p0` from runtime
    - Hover detection uses `canvas_rt.hovered` instead of `ow_map_canvas_.IsMouseHovering()`
    - `IsMouseHoveringOverEntity(entity, rt)` overload added for runtime-based entity detection
  - **Zoom Support:** [IMPLEMENTED]
    - `DrawOverworldMaps()` now applies `global_scale()` to both bitmap positions and scale
    - Placeholder rectangles for unloaded maps also scaled correctly
    - Entity rendering already uses `rt.scale` via stateless helpers - alignment works automatically
  - **Testing Focus:**
    - [x] Pan respects canvas bounds (can't scroll outside map)
    - [x] Entity hover detection works
    - [x] Entity drag/drop positioning correct
    - [x] Context menu opens in correct mode
    - [x] Zoom scales bitmaps and positions correctly

- **Dungeon Editor** [MIGRATED]
  - Surfaces: room graphics viewer, object selector preview, integrated editor panels, room canvases with palettes/blocks.
  - **Completed Migration:**
    - `DungeonCanvasViewer::DrawDungeonCanvas`: Uses `BeginCanvas/EndCanvas` with `CanvasFrameOptions`
    - Entity rendering functions (`RenderSprites`, `RenderPotItems`, `DrawObjectPositionOutlines`) accept `const gui::CanvasRuntime&`
    - All drawing calls use stateless helpers: `gui::DrawRect(rt, ...)`, `gui::DrawText(rt, ...)`
    - `DungeonObjectSelector`: Already used `CanvasFrame` pattern (no changes needed)
    - `ObjectEditorPanel`: Already used `BeginCanvas/EndCanvas` pattern (no changes needed)
  - **Key Lessons:**
    - Context menu setup (`ClearContextMenuItems`, `AddContextMenuItem`) must happen BEFORE `BeginCanvas`
    - The `DungeonObjectInteraction` class still uses `canvas_` pointer internally - this works because canvas state is set up by `BeginCanvas`
    - Debug overlay windows (`ImGui::Begin/End`) work fine inside the canvas frame since they're separate ImGui windows
    - Grid visibility is now controlled via `frame_opts.draw_grid` instead of manual `if (show_grid_) { canvas_.DrawGrid(); }`

- **Screen/Inventory Editors** [MIGRATED]
  - `DrawInventoryMenuEditor` and `DrawDungeonMapsRoomGfx` now use `BeginCanvas/EndCanvas` with `CanvasFrameOptions`.
  - Stateless `gui::DrawBitmap(rt, ...)` and `gui::DrawTileSelector(rt, ...)` used throughout.
  - Grid step configured via `CanvasFrameOptions` (32 for screen, 16/32 for tilesheet).

- **Graphics/Pixels Panels**
  - Low risk; continue to replace manual `draw_list` usage with `DrawBitmapPreview` and runtime-aware helpers. Ensure `ensure_texture=true` for arena-backed bitmaps.

- **Tile16Editor** [MIGRATED]
  - Three canvases migrated from `DrawBackground()/DrawContextMenu()/DrawGrid()/DrawOverlay()` to `BeginCanvas/EndCanvas`:
    - `blockset_canvas_`: Two usages in `UpdateBlockset()` and `DrawContent()` now use `CanvasFrameOptions`
    - `tile8_source_canvas_`: Tile8 source selector now uses `BeginCanvas/EndCanvas` with grid step 32.0f
    - `tile16_edit_canvas_`: Tile16 edit canvas now uses `BeginCanvas/EndCanvas` with grid step 8.0f
  - Tile selection and painting logic preserved; only frame management changed

- **Automation/Testing Surfaces**
  - Leave automation API untouched until core migration is stable. When ready, have it consume `CanvasRuntime` so tests don't depend on hidden members.

### Testing Focus per Editor
- Overworld [VALIDATED]: scroll bounds ✓, entity hover ✓, entity drag/drop ✓, context menu ✓, tile painting ✓, zoom ✓.
- Dungeon [VALIDATED]: grid alignment ✓, object interaction ✓, entity overlays ✓, context menu ✓, layer visibility ✓.
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

### Critical Insights (Lessons Learned)

**Child Window & Scrollbar Behavior:**
- The canvas has its own internal scrolling mechanism via `scrolling_` state and pan handling in `DrawBackground()`.
- Wrapping in `ImGui::BeginChild()` with scrollbars is **redundant** and causes visual issues.
- `CanvasFrameOptions.use_child_window` defaults to `false` to match legacy `DrawBackground()` behavior.
- Only use `use_child_window = true` when you explicitly need a scrollable container (rare).
- All `BeginChild` calls in canvas code now use `ImGuiWindowFlags_NoScrollbar` by default.

**Context Menu Ordering:**
- Context menu setup (`ClearContextMenuItems`, `AddContextMenuItem`) must happen BEFORE `BeginCanvas`.
- The menu items are state on the canvas object, not per-frame data.
- `BeginCanvas` calls `DrawBackground` + `DrawContextMenu`, so items must be registered first.

**Interaction Systems:**
- Systems like `DungeonObjectInteraction` that hold a `canvas_` pointer still work because canvas internal state is valid after `BeginCanvas`.
- The runtime provides read-only geometry; interaction systems can still read canvas state directly.

**Scroll Bounds (Overworld Lesson):**
- Large canvases (4096x4096) need explicit scroll clamping to prevent users from panning beyond content bounds.
- Use `ClampScroll(scroll, content_px * scale, viewport_px)` after computing new scroll position.
- The overworld's `HandleOverworldPan()` now demonstrates this pattern with `gui::ClampScroll()`.

**Multi-Bitmap Canvases (Overworld Lesson):**
- When a canvas renders multiple bitmaps (e.g., 64 map tiles), positions must be scaled with `global_scale_` for zoom to work.
- The overworld's zoom is deferred because bitmap positions are not yet scaled (see TODO in `DrawOverworldMaps()`).
- Fix approach: `map_x = static_cast<int>(xx * kOverworldMapSize * scale)` and pass `scale` to `DrawBitmap`.

**Entity Renderer Refactoring:**
- Separate entity renderers (like `OverworldEntityRenderer`) should accept `const gui::CanvasRuntime&` for stateless rendering.
- Legacy methods can be kept as thin wrappers that build runtime from canvas state.
- The `IsMouseHoveringOverEntity(entity, rt)` overload demonstrates runtime-based hit testing.

### Design Principles to Follow
- ImGui-like: options-as-arguments at `Begin`, minimal persistent mutation, small POD contexts for draw helpers.
- Keep the core type thin; optional features live in extensions, not the base.
- Maintain compatibility shims temporarily, but mark them and plan removal once editors are migrated.
- **Avoid child window wrappers** unless truly needed for scrollable regions.

### Quick Reference for Future Agents
- Core files to touch: `src/app/gui/canvas/canvas.{h,cc}`, `canvas_extensions.{h,cc}`, `canvas_menu.{h,cc}`, `canvas_context_menu.{h,cc}`, `canvas_menu_builder.{h,cc}`, `canvas_utils.{h,cc}`. Common call-sites: `screen_editor.cc`, `link_sprite_panel.cc`, `sheet_browser_panel.cc`, `object_editor_panel.cc`, `dungeon_object_selector.cc`, overworld/dungeon editors.
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
    - `DrawRect(rt, x, y, w, h, color)` - draws filled rectangle (added for entity overlays)
    - `DrawText(rt, text, x, y)` - draws text at position (added for entity labels)
    - `DrawOutline(rt, x, y, w, h, color)` - draws outline rectangle
  - **Frame management:**
    - `BeginCanvas(canvas, CanvasFrameOptions)` → returns `CanvasRuntime`
    - `EndCanvas(canvas, runtime, CanvasFrameOptions)` → draws grid/overlay/popups based on options
    - `BeginInTable(label, CanvasFrameOptions)` → table-aware begin returning `CanvasRuntime`
    - `EndInTable(runtime, CanvasFrameOptions)` → table-aware end with grid/overlay/popups
  - **CanvasFrameOptions fields:**
    - `canvas_size` - size of canvas content (0,0 = auto from config)
    - `draw_context_menu` - whether to call DrawContextMenu (default true)
    - `draw_grid` - whether to draw grid overlay (default true)
    - `grid_step` - optional grid step override
    - `draw_overlay` - whether to draw selection overlay (default true)
    - `render_popups` - whether to render persistent popups (default true)
    - `use_child_window` - wrap in ImGui::BeginChild (default **false** - important!)
    - `show_scrollbar` - show scrollbar when use_child_window is true (default false)
  - **Initialization (Phase 2):**
    - `Canvas()` → default constructor (preferred)
    - `Init(const CanvasConfig& config)` → post-construction initialization with full config
    - `Init(const std::string& id, ImVec2 canvas_size)` → simple initialization
    - Legacy constructors deprecated with `[[deprecated]]` attribute
  - **Extensions (Phase 4):**
    - `CanvasExtensions` struct holds optional modules: bpp_format_ui, bpp_conversion_dialog, bpp_comparison_tool, modals, palette_editor, automation_api
    - `EnsureExtensions()` → lazy accessor (private, creates extensions on first use)
    - Extensions only allocated when Show* or GetAutomationAPI() methods are called
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
- **Scroll & Zoom Helpers (Phase 0 - Infrastructure):**
  - `ClampScroll(scroll, content_px, canvas_px)` → clamps scroll to valid bounds `[-max_scroll, 0]`
  - `ComputeZoomToFit(content_px, canvas_px, padding_px)` → returns `ZoomToFitResult{scale, scroll}` to fit content
  - `IsMouseHoveringOverEntity(entity, rt)` → runtime-based entity hover detection for overworld

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
