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
   - **Save ROM** (EditorManager::SaveRom) now calls `GetDungeonEditor()->Save()`, so File > Save ROM persists dungeon objects, sprites, door pointers, room headers (14-byte header + message IDs), and palettes in addition to dungeon maps (when kSaveDungeonMaps is on).  
   - `DungeonEditorV2::Save` saves palettes via `PaletteManager`, then per room: `Room::SaveObjects()`, `Room::SaveSprites()`, `Room::SaveRoomHeader()`, then `DungeonEditorSystem::SaveDungeon()`.  
   - `Room::EncodeObjects()` emits the door marker `0xF0 0xFF` and the door list (per ZScreamDungeon); `Room::SaveObjects()` writes the door pointer table at `kDoorPointers` so the pointer points to the first byte after the marker.  
   - **Dungeon layout vs ZScream**: yaze writes room object data in-place at each room’s existing object pointer. ZScreamDungeon repacks room data into five fixed sections (DungeonSection1–5). So `validate-yaze --feature=dungeon` may report byte differences in object layout regions even when object and door content are equivalent; semantic comparison or “in-place” documentation applies.  
   - Other entities (chests, pots, collision, torches, pits, blocks) have load paths; save paths are added per plan (Phase B/C).

## Recent Additions

### Object Tile Editor (February 2026)

New subsystem for visual editing of the 8x8 tile composition of dungeon objects.

**Architecture:**
- **ObjectTileEditor** (`zelda3/dungeon/object_tile_editor.{h,cc}`) — Backend that captures tile layout via `ObjectDrawer` trace_only mode, renders preview bitmaps, and writes back to ROM or custom `.bin` files.
- **ObjectTileLayout** — Editable model: vector of `Cell` structs (rel_x, rel_y, TileInfo, original_word, modified flag) built from `ObjectDrawer::TileTrace` entries.
- **ObjectTileEditorPanel** (`app/editor/dungeon/panels/object_tile_editor_panel.{h,cc}`) — ImGui panel with two-column layout: interactive tile grid (4x scale) on the left, source tile8 atlas (2x scale) on the right, per-tile property editing (palette, flip, priority), and Apply/Revert/Close actions.

**Integration:**
- Registered as EditorPanel in `DungeonEditorV2::Load()` via PanelManager.
- Opened from ObjectEditorPanel's static object editor via "Edit Tiles" button.
- Callback wired through `ObjectEditorPanel::set_tile_editor_callback()`.
- Palette synced via `SyncPanelsToRoom()`.

**Write-back paths:**
- Standard objects: patches ROM at `tile_data_address + i*2` with `TileInfoToWord()`.
- Custom objects: re-serializes to binary format matching `CustomObjectManager::ParseBinaryData()`, writes `.bin` file, calls `ReloadAll()`.

**Status:** Core implementation complete (Phases 1–3). Keyboard shortcuts, object selector preview enhancement, and custom object creation remain (Phases 4–5).

## Current Limitations / Gaps

- **Persistence coverage**: Tile objects, sprites, doors (marker + pointer table), room headers (14 bytes + message IDs), and palettes are written back. Chests, pots, collision, torches, pits, and blocks have load paths; save paths are in progress or stubs.
- **Object tile editor**: Core pipeline works but needs keyboard shortcuts, room re-render after apply, and shared tile data confirmation dialog. No unit tests yet for ObjectTileLayout/ObjectTileEditor.
- **Object previews**: Selector uses primitive rectangles; enhancing with `ObjectTileEditor::RenderLayoutToBitmap()` is planned (Phase 4).
- **Tests**: Integration/E2E cover loading and card plumbing but not ROM writes for doors/chests/entrances. Object tile editor needs roundtrip tests.

## Suggested Next Steps

1. **Object Tile Editor Polish (Phase 4–5)**:
   - Keyboard shortcuts in tile editor panel (arrow keys, palette numbers, H/V/P toggles).
   - Room re-render after Apply: invalidate canvas viewer render cache after `WriteBack()`.
   - Shared tile data warning dialog before applying standard object changes.
   - "New Custom Object" workflow for creating `.bin` files from scratch.
   - Enhance `DungeonObjectSelector::GetOrCreatePreview()` with `RenderLayoutToBitmap()` for higher-quality previews cached per `(object_id, blockset, palette_hash)`.
2. **Object Tile Editor Tests**: Unit tests for `ObjectTileLayout::FromTraces`, `CaptureObjectLayout`, `WriteBack` roundtrip, and custom object binary serialize/deserialize.
3. **Save Pipeline**: Extend `DungeonEditorV2::Save` to call DungeonEditorSystem save hooks and verify round-trips in tests.
4. **Test Coverage**: Add integration tests that:
   - Place/delete objects and verify `Room::EncodeObjects` output changes in ROM.
   - Add doors/chests/entrances and assert persistence once implemented.
   - Exercise undo/redo on object placement and palette edits.
5. **Live Emulator Preview (optional)**: Keep `DungeonObjectEmulatorPreview` as a hook for live patching when the emulator integration lands.
