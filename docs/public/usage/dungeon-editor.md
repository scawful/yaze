# Dungeon Editor Guide

The Dungeon Editor provides a multi-panel workspace for editing Zelda 3 dungeon rooms. Each room has isolated graphics, objects, and palette data, allowing you to work on multiple rooms simultaneously.

---

## Overview

### Key Features

- **512x512 canvas** per room with pan, zoom, grid, and collision overlays
- **Layer visualization** with BG1/BG2 toggles and colored object outlines
- **Modular panels** for rooms, objects, palettes, and entrances
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

1. Launch YAZE with a ROM: `./build/bin/yaze --rom_file=zelda3.sfc`
2. Select a room from **Room Matrix** or **Rooms List** panel
3. Open multiple rooms in separate panels for comparison

### Available Panels

| Panel | Purpose |
|-------|---------|
| **Room Graphics** | Main canvas with BG toggles and grid options |
| **Object Editor** | Edit objects by type, layer, and coordinates |
| **Palette Editor** | Adjust room palettes with live preview |
| **Entrances List** | Navigate between overworld entrances and rooms |
| **Room Matrix** | Visual dungeon room grid for quick navigation |

Panels can be docked, detached, or saved as workspace presets.

### Canvas Controls

| Action | Control |
|--------|---------|
| Select object | Left-click |
| Add to selection | Shift + Left-click |
| Move object | Drag handles |
| Pan canvas | Hold Space + drag |
| Zoom | Mouse wheel or trackpad pinch |
| Context menu | Right-click |

Enable **Object Labels** from the toolbar to display layer-colored labels.

### Saving

- **Undo/Redo**: `Cmd/Ctrl+Z` and `Cmd/Ctrl+Shift+Z`
- Changes are tracked across all panels
- Keep backups enabled in `File > Options > Experiment Flags`

---

## Troubleshooting

| Issue | Solution |
|-------|----------|
| Objects on wrong layer | Check BG toggles in Room Graphics and layer filter in Object Editor |
| Palette not saving | Ensure Palette Editor writes values before switching rooms |
| Door alignment issues | Right-click door markers to verify leads-to IDs |
| Sluggish performance | Close unused room panels to release textures |

---

## Quick Launch Examples

```bash
# Open specific room for testing
./yaze --rom_file=zelda3.sfc --editor=Dungeon --open_panels="Room 0"

# Compare multiple rooms
./yaze --rom_file=zelda3.sfc --editor=Dungeon --open_panels="Room 0,Room 1,Room 105"

# Full workspace with all tools
./yaze --rom_file=zelda3.sfc --editor=Dungeon \
  --open_panels="Rooms List,Room Matrix,Object Editor,Palette Editor"
```

---

## Related Documentation

- [Architecture Overview](../developer/architecture.md) - Patterns shared across editors
- [Canvas System](../developer/canvas-system.md) - Canvas controls and context menus
- [Debugging Guide](../developer/debugging-guide.md) - Startup flags and logging
