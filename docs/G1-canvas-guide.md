# Canvas System Overview

## Canvas Architecture
- **Canvas States**: track `canvas`, `content`, and `draw` rectangles independently; expose size/scale through `CanvasState` inspection panel
- **Layer Stack**: background ➝ bitmaps ➝ entity overlays ➝ selection/tooltip layers
- **Interaction Modes**: Tile Paint, Tile Select, Rectangle Select, Entity Manipulation, Palette Editing, Diagnostics
- **Context Menu**: persistent menu with material icon sections (Mode, View, Info, Bitmap, Palette, BPP, Performance, Layout, Custom)

## Core API Patterns
- Modern usage: `Begin/End` (auto grid/overlay, persistent context menu)
- Legacy helpers still available (`DrawBackground`, `DrawGrid`, `DrawSelectRect`, etc.)
- Unified state snapshot: `CanvasState` exposes geometry, zoom, scroll
- Interaction handler manages mode-specific tools (tile brush, select rect, entity gizmo)

## Context Menu Sections
- **Mode Selector**: switch modes with icons (Brush, Select, Rect, Bitmap, Palette, BPP, Perf)
- **View & Grid**: reset/zoom, toggle grid/labels, advanced/scaling dialogs
- **Canvas Info**: real-time canvas/content size, scale, scroll, mouse position
- **Bitmap/Palette/BPP**: format conversion, palette analysis, BPP workflows with persistent modals
- **Performance**: profiler metrics, dashboard, usage report
- **Layout**: draggable toggle, auto resize, grid step
- **Custom Actions**: consumer-provided menu items

## Interaction Modes & Capabilities
- **Tile Painting**: tile16 painter, brush size, finish stroke callbacks
  - Operations: finish_paint, reset_view, zoom, grid, scaling
- **Tile Selection**: multi-select rectangle, copy/paste selection
  - Operations: select_all, clear_selection, reset_view, zoom, grid, scaling
- **Rectangle Selection**: drag-select area, clear selection
  - Operations: clear_selection, reset_view, zoom, grid, scaling
- **Bitmap Editing**: format conversion, bitmap manipulation
  - Operations: bitmap_convert, palette_edit, bpp_analysis, reset_view, zoom, grid, scaling
- **Palette Editing**: inline palette editor, ROM palette picker, color analysis
  - Operations: palette_edit, palette_analysis, reset_palette, reset_view, zoom, grid, scaling
- **BPP Conversion**: format analysis, conversion workflows
  - Operations: bpp_analysis, bpp_conversion, bitmap_convert, reset_view, zoom, grid, scaling
- **Performance Mode**: diagnostics, texture queue, performance overlays
  - Operations: performance, usage_report, copy_metrics, reset_view, zoom, grid, scaling

## Debug & Diagnostics
- Persistent modals (`View→Advanced`, `View→Scaling`, `Palette`, `BPP`) stay open until closed
- Texture inspector shows current bitmap, VRAM sheet, palette group, usage stats
- State overlay: canvas size, content size, global scale, scroll, highlight entity
- Performance HUD: operation counts, timing graphs, usage recommendations

## Automation API
- CanvasAutomationAPI: tile operations (`SetTileAt`, `SelectRect`), view control (`ScrollToTile`, `SetZoom`), entity manipulation hooks
- Exposed through CLI (`z3ed`) and gRPC service, matching UI modes

## Integration Steps for Editors
1. Construct `Canvas`, set renderer (optional) and ID
2. Call `InitializePaletteEditor` and `SetUsageMode`
3. Configure available modes: `SetAvailableModes({kTilePainting, kTileSelecting})`
4. Register mode callbacks (tile paint finish, selection clear, etc.)
5. During frame: `canvas.Begin(size)` → draw bitmaps/entities → `canvas.End()`
6. Provide custom menu items via `AddMenuItem`/`AddMenuItem(item, usage)`
7. Use `GetConfig()`/`GetSelection()` for state; respond to context menu commands via callback lambda in `Render`

## Migration Checklist
- Replace direct `DrawContextMenu` logic with new render callback signature
- Move palette/BPP helpers into `canvas/` module; update includes
- Ensure persistent modals wired (advanced/scaling/palette/bpp/perf)
- Update usage tracker integrations to record mode switches
- Validate overworld/tile16/dungeon editors in tile paint, select, entity modes

## Testing Notes
- Manual regression: overworld paint/select, tile16 painter, dungeon entity drag
- Verify context menu persists and modals remain until closed
- Ensure palette/BPP modals populate with correct bitmap/palette data
- Automation: run CanvasAutomation API tests/end-to-end scripts for overworld edits
