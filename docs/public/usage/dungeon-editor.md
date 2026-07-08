# Dungeon Editor Guide

The Dungeon Editor provides a Workbench-first workspace for editing Zelda 3
dungeon rooms. The default flow keeps room navigation, the canvas, selection
inspection, and local edit tools in one stable window so common edits do not
require chasing separate floating panels.

---

## Overview

### Key Features

- **512x512 canvas** per room with pan, zoom, grid, and collision overlays
- **Layer visualization** with BG1/BG2 toggles and colored object outlines
- **Dungeon Workbench** with room browser, canvas, inspector, and local tool drawer
- **Window workflow fallback** for users who still want standalone panels
- **Undo/Redo** shared across all panels
- **Overworld integration** - double-click entrances to open linked rooms

**Related Documentation:**
- [Architecture Overview](../developer/architecture.md)
- [Canvas System](../developer/canvas-system.md)

---

## Architecture

The editor uses a three-layer architecture:

| Layer | Components | Responsibility |
|-------|------------|----------------|
| **UI** | DungeonEditorV2 | Panels, canvas, menus, toolbar |
| **Backend** | DungeonEditorSystem | State management, undo/redo, persistence |
| **Data** | Room Model | Buffers, objects, palettes, blocksets |

### Rendering Pipeline

1. **Load** - Read room header, blockset pointers, and door/entrance metadata
2. **Decode** - Convert blockset into bitmaps; parse objects by layer
3. **Draw** - Build BG1/BG2 bitmaps with object overlays
4. **Queue** - Push bitmaps to texture queue for GPU upload
5. **Present** - Display layers with selection widgets and grid

Changes to tiles, palettes, or objects invalidate only the affected room's cache.

---

## Editing Workflow

### Opening Rooms

1. Launch YAZE with a ROM: `./scripts/yaze zelda3.sfc`
2. Select **Dungeon**.
3. Use the **Dungeon Workbench** room browser, recent-room tabs, or connected
   graph to navigate rooms.
4. Use split view or recent-room tabs for comparison instead of opening many
   separate room windows.

### Workbench Inspector

The right inspector has three primary modes:

| Mode | Purpose |
|------|---------|
| **Room** | Current room summary, room actions, header fields, apply scope, layer/compositing controls |
| **Selection** | Focused object/entity properties, copy/delete/clear actions, and entity-specific tools |
| **Tools** | Workbench-local edit tools embedded in the inspector drawer |

The **Tools** drawer includes Object Selector, Door tools, Sprite tools, Item
tools, Palette, Room Graphics, Room Tags, Custom Collision, Water Fill, and
Minecart tools. A compact 2x5 icon strip at the top of the drawer lets you hop
between tools in one click; the active tool is highlighted with the accent
color and hovering each icon shows a tooltip with the full tool name. Returning
to the room metadata view is one click on the inspector's primary segmented
selector. The drawer body fills the remaining inspector height so embedded
tools render as primary content rather than cramped popups. Standalone copies
of these tools remain available in the Window Browser/sidebar while Workbench
mode is active; entering Workbench mode only collapses navigation windows and
per-room windows by default.

The Object Selector renders room-context thumbnails by default when room
graphics are available. Entries that cannot render a tile layout fall back to a
typed symbol, and the hover tooltip shows whether the visible preview is a
rendered layout or a fallback.

### Available Panels

| Panel | Purpose |
|-------|---------|
| **Dungeon Workbench** | Default single-window dungeon editing surface |
| **Room List / Room Matrix** | Navigation/review surfaces; also available inside the Workbench flow |
| **Entrances List** | Navigate between overworld entrances and rooms |
| **Object Tile Editor** | Standalone 8x8 tile composition editor for object asset authoring |
| **Window Browser** | Manage standalone windows when using Window workflow |

Workbench-local edit tools are available both inside the Workbench drawer and as
standalone windows. Switch to **Window** workflow from the sidebar when you want
the older navigation-first panel layout.

### Canvas Controls

| Action | Control |
|--------|---------|
| Select object | Left-click |
| Add to selection | Shift + Left-click |
| Move object | Drag handles |
| Pan canvas | Hold Space + drag |
| Zoom | Mouse wheel or trackpad pinch |
| Context menu | Right-click |
| Cycle overlapping selections | Alt + click |

Enable **Object Labels** from the toolbar to display layer-colored labels.

### Saving

- **File > Save ROM** persists dungeon data in this order: dungeon maps (when **Save Dungeon Maps** is enabled), then per-room **objects**, **sprites**, **room headers** (14-byte header + message IDs), **door pointers** (with `0xF0 0xFF` marker), then **palettes**, **torches**, **pits**, **blocks**, **chests**, **pot items**, and dirty **entrance/spawn-point metadata**. No need to save from the Dungeon Editor separately for ROM file writes.
- Oversized dirty pot-item edits now fail the save and stay dirty instead of silently preserving stale ROM bytes.
- **Undo/Redo**: `Cmd/Ctrl+Z` and `Cmd/Ctrl+Shift+Z`
- Changes are tracked across all panels
- Keep backups enabled in `File > Options > Experiment Flags`
- To validate output against ZScream golden ROMs on macOS, see [ZScreamCLI Validation](zscream-validation.md).

---

## Troubleshooting

| Issue | Solution |
|-------|----------|
| Objects on wrong layer | Check BG toggles in the Workbench toolbar and the layer field in **Selection** or **Inspector → Tools** |
| Palette not saving | Open **Inspector → Tools → Palette** and ensure the palette edit is applied before switching rooms |
| Door alignment issues | Use clickable door-pair badges or **Inspector → Tools → Doors** to verify linked room IDs |
| Sluggish performance | Prefer Workbench mode and close unused standalone room windows |

---

## Quick Launch Examples

```bash
# Open specific room for testing
./scripts/yaze --rom_file=zelda3.sfc --editor=Dungeon --room=0 \
  --open_panels=dungeon.workbench

# Open Workbench directly with startup shells hidden
./scripts/yaze --rom_file=zelda3.sfc --editor=Dungeon --room=16 \
  --open_panels=dungeon.workbench \
  --startup_welcome=hide --startup_dashboard=hide

# Legacy full window workflow with standalone panels
./scripts/yaze --rom_file=zelda3.sfc --editor=Dungeon \
  --open_panels="dungeon.room_selector,dungeon.room_matrix,dungeon.object_selector,dungeon.palette_editor"
```

---

## Related Documentation

- [Architecture Overview](../developer/architecture.md) - Patterns shared across editors
- [Canvas System](../developer/canvas-system.md) - Canvas controls and context menus
- [Debugging Guide](../developer/debugging-guide.md) - Startup flags and logging
