# F2: Dungeon Editor v2 Guide

**Scope**: DungeonEditorV2 (card-based UI), DungeonEditorSystem, dungeon canvases  
**Related**: [Architecture Overview](../developer/architecture.md), [Canvas System](../developer/canvas-system.md)

---

## 1. Overview

The Dungeon Editor ships with the multi-card workspace introduced in the 0.3.x releases.
Self-contained room buffers keep graphics, objects, and palettes isolated so you can switch between
rooms without invalidating the entire renderer.

### Key Features
- 512×512 canvas per room with pan/zoom, grid, and collision overlays.
- Layer-specific visualization (BG1/BG2 toggles, colored object outlines, slot labels).
- Modular cards for rooms, objects, palettes, entrances, and toolsets.
- Undo/Redo shared across cards via `DungeonEditorSystem`.
- Tight overworld integration: double-click an entrance to open the linked dungeon room.

---

## 2. Architecture Snapshot

```
DungeonEditorV2 (UI)
├─ Cards & docking
├─ Canvas presenter
└─ Menu + toolbar actions

DungeonEditorSystem (Backend)
├─ Room/session state
├─ Undo/Redo stack
├─ Sprite/entrance/item helpers
└─ Persistence + ROM writes

Room Model (Data)
├─ bg1_buffer_, bg2_buffer_
├─ tile_objects_, door data, metadata
└─ Palette + blockset caches
```

### Room Rendering Pipeline
1. **Load** – `DungeonRoomLoader` reads the room header, blockset pointers, and door/entrance
   metadata, producing a `Room` instance with immutable layout info.
2. **Decode** – The requested blockset is converted into `current_gfx16_` bitmaps; objects are parsed
   into `tile_objects_` grouped by layer and palette slot.
3. **Draw** – `DungeonCanvasViewer` builds BG1/BG2 bitmaps, then overlays each object layer via
   `ObjectDrawer`. Palette state comes from the room’s 90-color dungeon palette.
4. **Queue** – The finished bitmaps are pushed into the graphics `Arena`, which uploads a bounded
   number of textures per frame so UI latency stays flat.
5. **Present** – When textures become available, the canvas displays the layers, draws interaction
   widgets (selection rectangles, door gizmos, entity labels), and applies zoom/grid settings.

Changing tiles, palettes, or objects invalidates the affected room cache so steps 2–5 rerun only for
that room.

---

## 3. Editing Workflow

### Opening Rooms
1. Launch `yaze` with a ROM (`./build/bin/yaze --rom_file=zelda3.sfc`).
2. Use the **Room Matrix** or **Rooms List** card to choose a room. The toolbar “+” button also opens
   the selector.
3. Pin multiple rooms by opening them in separate cards; each card maintains its own canvas state.

### Working with Cards

| Card | Purpose |
|------|---------|
| **Room Graphics** | Primary canvas, BG toggles, collision/grid switches. |
| **Object Editor** | Filter by type/layer, edit coordinates, duplicate/delete objects. |
| **Palette Editor** | Adjust per-room palette slots and preview results immediately. |
| **Entrances List** | Jump between overworld entrances and their mapped rooms. |
| **Room Matrix** | Visual grid of all rooms grouped per dungeon for quick navigation. |

Cards can be docked, detached, or saved as workspace presets; use the sidebar to store favorite
layouts (e.g., Room Graphics + Object Editor + Palette).

### Canvas Interactions
- Left-click to select an object; Shift-click to add to the selection.
- Drag handles to move objects or use the property grid for precise coordinates.
- Right-click to open the context menu, which includes quick inserts for common objects and a “jump
  to entrance” helper.
- Hold Space to pan, use mouse wheel (or trackpad pinch) to zoom. The status footer shows current
  zoom and cursor coordinates.
- Enable **Object Labels** from the toolbar to show layer-colored labels (e.g., `L1 Chest 0x23`).

### Saving & Undo
- The editor queues every change through `DungeonEditorSystem`. Use `Cmd/Ctrl+Z` and `Cmd/Ctrl+Shift+Z`
  to undo/redo across cards.
- Saving writes back the room buffers, door metadata, and palettes for the active session. Keep
  backups enabled (`File → Options → Experiment Flags`) for safety.

---

## 4. Tips & Troubleshooting

- **Layer sanity**: If objects appear on the wrong layer, check the BG toggles in Room Graphics and
  the layer filter in Object Editor—they operate independently.
- **Palette issues**: Palettes are per room. After editing, ensure `Palette Editor` writes the new
  values before switching rooms; the status footer confirms pending writes.
- **Door alignment**: Use the entrance/door inspector popup (right-click a door marker) to verify
  leads-to IDs without leaving the canvas.
- **Performance**: Large ROMs with many rooms can accumulate textures. If the editor feels sluggish,
  close unused room cards; each card releases its textures when closed.

---

## 5. Related Docs
- [Developer Architecture Overview](../developer/architecture.md) – patterns shared across editors.
- [Canvas System Guide](../developer/canvas-system.md) – detailed explanation of canvas usage,
  context menus, and popups.
- [Debugging Guide](../developer/debugging-guide.md) – startup flags and logging tips (e.g.,
  `--editor=Dungeon --cards="Room 0"` for focused debugging).
