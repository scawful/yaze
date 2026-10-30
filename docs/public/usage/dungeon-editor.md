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
- **Dungeon Workbench** with room browser, canvas, and a three-mode inspector
- **Window workflow fallback** for users who still want standalone panels
- **Undo/Redo** shared across all panels
- **Overworld integration** - double-click entrances to open linked rooms

**Related Documentation:**
- [Architecture Overview](../developer/architecture.md)
- [Canvas System](../developer/canvas-system.md)
- [Spatial explanation and pattern composition](dungeon-spatial-contract.md)

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
| **Tools** | Workbench-local edit tools embedded as primary inspector content |

The **Tools** mode includes Object Selector, Door tools, Sprite tools, Item
tools, Palette, Room Graphics, Room Tags, Custom Collision, Water Fill, and
Minecart tools. Choose the active tool from one grouped menu; the tool fills the
remaining inspector height. Use **Room** or **Selection** to return to metadata,
or **Pop out** to move the active tool into its traditional floating window.
Standalone tool entries remain available in the Window Browser/sidebar while
Workbench mode is active. If a tool is already floating, opening it from the
Workbench focuses that window instead of drawing the same tool twice.

The Object Selector renders room-context thumbnails by default when room
graphics are available. The compact cards preserve each object's aspect ratio,
show its hexadecimal ID in a stable footer, and put the full name and diagnostic
detail in the hover tooltip. Search and the two primary filters stay visible;
thumbnail, card-size, reset, and custom-object actions live under **More**.
Entries that cannot render a tile layout fall back to a typed symbol.

### Available Panels

| Panel | Purpose |
|-------|---------|
| **Dungeon Workbench** | Default single-window dungeon editing surface |
| **Room List / Room Matrix** | Navigation/review surfaces; also available inside the Workbench flow |
| **Entrances List** | Navigate between overworld entrances and rooms |
| **Object Tile Editor** | Standalone 8x8 tile composition editor for object asset authoring |
| **Window Browser** | Manage standalone windows when using Window workflow |

Workbench-local edit tools can run inside the Tools inspector or in standalone
windows. Switch to **Window** workflow from the sidebar when you want the older
navigation-first panel layout.

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

To record a rendering problem, right-click the canvas and open the room or
selection issue report. The report stays local until you choose **Save to Issue
Log**. Its form scrolls independently on small windows, while save, copy,
screenshot, and close actions remain available in the footer. File paths and
raw renderer diagnostics are collapsed by default so they do not crowd the
normal reporting flow. Screenshot capture keeps the action buttons in place.
Long status messages stay on one line; hover over the status to read the full
message, including any error details.

### Previewing source-authored wall patterns

When a project assembles standard dungeon object patterns from a separate data
file, the editor can preview those tile words while keeping the base ROM as the
edit target. Add the source path to the project's existing `[files]` section:

```ini
dungeon_tile_patterns=Dungeons/house_walls.asm
```

Paths resolve relative to the project. **Room Graphics** shows the source, the
number of tile words, and any load error. After changing the file, choose
**Reload Pattern Source** to refresh room objects, layouts, and thumbnails.
Each open project and background room scan retains its own source context.
The preview works independently of the custom `.bin` object feature flag.

This reader accepts comments, labels, `lorom`, literal `org $00xxxx` addresses,
and literal `dw $xxxx, ...` lists in the bank-00 object-data range. It rejects
includes, expressions, conditionals, instructions, and overlapping declarations.
It does not execute ASM or replace draw routines. Door routines and other
special renderers that read literal ROM addresses directly are outside this
preview path.

ROM Save and Save As retain the ROM's original pattern bytes. Object Tile Editor
refuses ROM composition writes that overlap source-owned patterns; edit the
source file instead. Ordinary room placement and graphics-sheet editing retain
their existing save paths. To test the source changes in-game, build the game
normally and use its assembled ROM in the emulator.

`z3ed dungeon-render --project-context=/path/to/project.yaze` uses the same
preview. Without a configured source, rendering continues to use ROM patterns.

### Minecart rails and nodes

1. Choose **Minecart** in the Workbench toolbar. In **Rails**, choose a floor
   or pit rail and place it in the room. **Auto collision** pairs supported
   rail behavior with placement and Undo while preserving other authored
   collision.
2. In **Nodes**, choose **Cart spawn**, **End stop**, **Switch**, or **Switch
   track**. Set the cart ID or switch channel and departure/orientation before
   placing. The selected node stays highlighted; **Cancel node** clears it.
3. Use **Route rooms** to choose a cart and jump to its start or other rooms
   containing that cart ID. The room buttons retain the selected placement
   tool. **Scan project rooms** includes unopened rooms; otherwise the list
   covers loaded rooms and includes their unsaved edits.
4. In **Overview**, enable **Minecart nodes** to see cart starts, placed carts,
   stops, switches, and rotating tracks across the displayed rooms. Markers
   follow live room and source drafts, including unsaved edits and Undo.
   The selected cart ID or switch channel is highlighted. Hover a marker for
   its identity, position, layer, and available collision details.
5. Click a room in **Overview** to return to **Room** view and continue
   placement with the chosen tool and brush. Reopening the active Minecart
   tool also keeps the selected node or behavior tool.

Cart placements identify matching route IDs. They do not establish that the
rails connect or that a cart can complete the route in the game. Advanced
source publishing, collision generation, and detailed checks remain under
**Source and audit**.

### Editing custom objects

Choose **Custom assets** in the object browser to preview and place an existing
asset. The available variants and names follow the project's admitted catalog,
including expanded minecart T junctions and relocated props.

After selecting a placed custom object, use **Selection → Variant** to change
its asset. Missing or invalid assets are unavailable and show the reason. A
room's existing unavailable variant stays unchanged until you choose a valid
replacement.

In **Selection**, use **Edit tiles…** to open the selected object's exact asset
in the existing Object Tile Editor. The custom filename and availability appear
beside the action; unavailable assets keep the button disabled and show the
reason. The button uses the same guarded operation as right-click **Edit Object
Tiles…**. The asset browser also opens the Object Tile Editor. Tile edits affect
every placement using that asset; object position, layer, and variant remain
room-instance properties. Apply or revert an unfinished tile draft before
opening another asset. Attempting to open another asset preserves the draft.

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
