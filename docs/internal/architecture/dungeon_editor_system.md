# Dungeon Editor System Architecture

**Status**: Active  
**Last Updated**: 2025-11-26  
**Related Code**: `src/app/editor/dungeon/`, `src/zelda3/dungeon/`, `test/integration/dungeon_editor_v2_test.cc`, `test/e2e/dungeon_editor_smoke_test.cc`

## Overview

DungeonEditorV2 is the ImGui-based dungeon editor for *A Link to the Past*. It uses a card/docking
layout and delegates most logic to small components:

- **DungeonRoomLoader** (`dungeon_room_loader.{h,cc}`): Reads rooms/entrances from the ROM, caches per-room palette metadata, and (optionally) loads all rooms in parallel.
- **DungeonRoomSelector** (`dungeon_room_selector.{h,cc}`): Lists rooms, matrix navigation, and entrance jump-to.
- **DungeonCanvasViewer** (`dungeon_canvas_viewer.{h,cc}`): Renders BG1/BG2 bitmaps per room, manages per-room layer visibility, and drives mouse interaction.
- **DungeonObjectInteraction** (`dungeon_object_interaction.{h,cc}`): Selection, multi-select, drag/move, copy/paste, and ghost previews on the canvas.
- **DungeonObjectSelector** (`dungeon_object_selector.{h,cc}`): Asset-browser style object picker and compact editors for sprites/items/doors/chests/properties (UI only).
- **ObjectEditorCard** (`object_editor_card.{h,cc}`): Unified object editor card.
- **DungeonEditorSystem** (`zelda3/dungeon/dungeon_editor_system.{h,cc}`): Orchestration layer for sprites/items/doors/chests/room properties.
- **Room Model** (`zelda3/dungeon/room.{h,cc}`): Holds room metadata, objects, sprites, background buffers, and encodes objects back to ROM.

The editor acts as a coordinator: it wires callbacks between selector/interaction/canvas, tracks
tabbed room cards, and queues texture uploads through `gfx::Arena`.

## Important ImGui Patterns

**Critical**: The dungeon editor uses many `BeginChild`/`EndChild` pairs. Always ensure `EndChild()` is called OUTSIDE the if block:

```cpp
// ✅ CORRECT
if (ImGui::BeginChild("##RoomsList", ImVec2(0, 0), true)) {
  // Draw content
}
ImGui::EndChild();  // ALWAYS called

// ❌ WRONG - causes ImGui state corruption
if (ImGui::BeginChild("##RoomsList", ImVec2(0, 0), true)) {
  // Draw content
  ImGui::EndChild();  // BUG: Not called when BeginChild returns false!
}
```

**Avoid duplicate rendering**: Don't call `RenderRoomGraphics()` in `DrawRoomGraphicsCard()` - it's already called in `DrawRoomTab()` when the room loads. The graphics card should only display already-rendered data.

## Data Flow (intended)

1. **Load**  
   - `DungeonRoomLoader::LoadRoom` loads room headers/objects/sprites for a room on demand.  
   - `DungeonRoomLoader::LoadRoomEntrances` fills `entrances_` for navigation.  
   - Palettes are pulled from `Rom::palette_group().dungeon_main`.

2. **Render**  
   - `Room::LoadRoomGraphics` pulls blockset tiles into the room’s private BG1/BG2 buffers.  
   - `Room::RenderRoomGraphics` renders objects into BG buffers; `DungeonCanvasViewer` queues textures and draws with grid/overlays.

3. **Interact**  
   - `DungeonObjectSelector` emits a preview object; `DungeonCanvasViewer` hands it to `DungeonObjectInteraction` for ghosting and placement.  
   - Selection/drag/copy/paste adjust `RoomObject` instances directly, then invalidate room graphics to trigger re-render.

4. **Save**  
   - `DungeonEditorV2::Save` currently saves palettes via `PaletteManager` then calls `Room::SaveObjects()` for all rooms.  
   - Other entities (sprites, doors, chests, entrances, items, room metadata) are not persisted yet.

## Current Limitations / Gaps

- **Undo/Redo**: `DungeonEditorV2` methods return `Unimplemented`; no command history is wired.  
- **Persistence coverage**: Only tile objects (and palettes) are written back. Sprites, doors, chests, entrances, collision, pot drops, and room metadata are UI-only stubs through `DungeonEditorSystem`.  
- **DungeonEditorSystem**: Exists as API scaffolding but does not load/save or render; panels in `DungeonObjectSelector` cannot commit changes to the ROM.  
- **Object previews**: Selector uses primitive rectangles; no `ObjectDrawer`/real tiles are shown.  
- **Tests**: Integration/E2E cover loading and card plumbing but not ROM writes for doors/chests/entrances or undo/redo flows.

## Suggested Next Steps

1. **Wire DungeonEditorSystem**: Initialize it in `DungeonEditorV2::Load`, back it with real ROM I/O for sprites/doors/chests/entrances/items/room properties, and sync UI panels to it.  
2. **Undo/Redo**: Add a command stack (objects/sprites/palettes/metadata) and route `Ctrl+Z/Ctrl+Shift+Z`; re-use patterns from overworld editor if available.  
3. **Save Pipeline**: Extend `DungeonEditorV2::Save` to call DungeonEditorSystem save hooks and verify round-trips in tests.  
4. **Object Rendering**: Replace rectangle previews in `DungeonObjectSelector` with `ObjectDrawer`-based thumbnails to match in-canvas visuals.  
5. **Test Coverage**: Add integration tests that:  
   - Place/delete objects and verify `Room::EncodeObjects` output changes in ROM.  
   - Add doors/chests/entrances and assert persistence once implemented.  
   - Exercise undo/redo on object placement and palette edits.  
6. **Live Emulator Preview (optional)**: Keep `DungeonObjectEmulatorPreview` as a hook for live patching when the emulator integration lands.
