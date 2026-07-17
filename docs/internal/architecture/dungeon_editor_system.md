# Dungeon Editor System Architecture

**Status**: Active
**Last Updated**: 2026-04-20
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
   - **Save ROM** (EditorManager::SaveRom) calls `GetDungeonEditor()->Save()`, so File > Save ROM persists dungeon objects, sprites, door pointers, room headers (14-byte header + message IDs), palettes, and the supported room-scoped tables in addition to dungeon maps (when `kSaveDungeonMaps` is on).
   - `DungeonEditorV2::Save` saves palettes via `PaletteManager`, then writes per-room data according to the dungeon save flags. Current focused ROM-safety coverage exists for room headers, chests, and pot items in `DungeonEditorV2RomSafetyTest`.
   - `DungeonEditorSystem::SaveRoom()` / `SaveDungeon()` now persist full managed-room state for objects, sprites, room headers, torches, pushable blocks, custom collision, chests, pot items, and dungeon entrances. `SaveDungeon()` first writes room-local streams for every loaded room, then runs the global aggregators once across the managed-room set so later rooms do not clobber earlier chest/pot/collision writes.
   - `Room::EncodeObjects()` emits the door marker `0xF0 0xFF` and the door list (per ZScreamDungeon); `Room::SaveObjects()` writes the door pointer table at `kDoorPointers` so the pointer points to the first byte after the marker.  
   - **Dungeon layout vs ZScream**: yaze writes room object data in-place at each room’s existing object pointer. ZScreamDungeon repacks room data into five fixed sections (DungeonSection1–5). So `validate-yaze --feature=dungeon` may report byte differences in object layout regions even when object and door content are equivalent; semantic comparison or “in-place” documentation applies.  
   - The pit-damage table preserves its protected pointer region unless the
     fixed-capacity workbench membership editor marks `PitDamageTable` dirty;
     pushable blocks use the room-aware encoder and keep unmaterialized rooms
     byte-identical.

## Recent Additions

### Object Tile Editor (February 2026)

New subsystem for visual editing of the 8x8 tile composition of dungeon objects.

**Architecture:**
- **ObjectTileEditor** (`zelda3/dungeon/object_tile_editor.{h,cc}`) — Backend that captures tile layout via `ObjectDrawer` trace_only mode, renders preview bitmaps, and writes back to ROM or custom `.bin` files.
- **ObjectTileLayout** — Editable model: vector of `Cell` structs (rel_x, rel_y, TileInfo, original_word, modified flag) built from `ObjectDrawer::TileTrace` entries.
- **ObjectTileEditorPanel** (`app/editor/dungeon/ui/window/object_tile_editor_panel.{h,cc}`) — ImGui panel with two-column layout: interactive tile grid (4x scale) on the left, source tile8 atlas (2x scale) on the right, per-tile property editing (palette, flip, priority), and Apply/Revert/Close actions.

**Integration:**
- Registered as `WindowContent` in `DungeonEditorV2::Load()` via `WorkspaceWindowManager`.
- Opened from `DungeonObjectSelector` (static object editor) via "Edit Tiles"; `DungeonEditorV2` wires `SetTileEditorPanel()` on the selector.
- Palette synced when the active room changes (`DungeonEditorV2` calls `ObjectTileEditorPanel::SetCurrentPaletteGroup`).

**Write-back paths:**
- Standard objects: patches ROM at `tile_data_address + i*2` with `TileInfoToWord()`.
- Custom objects: re-serializes to binary format matching `CustomObjectManager::ParseBinaryData()`, writes `.bin` file, calls `ReloadAll()`.

**Status:** Core implementation plus the first polish pass are in place. The panel now has keyboard shortcuts, room re-render after apply, shared-tile confirmation, palette-change invalidation, reopen/reset protection, backend coverage in `object_tile_editor_test.cc`, and panel-state coverage in `object_tile_editor_panel_test.cc`. The main remaining feature gap is a first-class "new custom object" workflow.

### Room Layer Manager & Compositing (February 2026)

Subsystem for accurate SNES-style layer compositing of dungeon room renders.

**Architecture:**
- **RoomLayerManager** (`zelda3/dungeon/room_layer_manager.{h,cc}`) — Manages per-layer blend modes and composites BG1/BG2 layout + object buffers into the final output bitmap.
- **LayerBlendMode** — Per-layer enum: `Normal`, `Translucent`, `Dark`. Stored per logical layer (BG1_Layout, BG1_Objects, BG2_Layout, BG2_Objects).
- **ApplyRoomEffect()** — Configures blend modes based on the room's `EffectKey`:
  - `Moving_Water`: Sets BG2 layers to Translucent.
  - `Moving_Floor`: No blend change (conveyor belt effect is positional, not visual).
  - `Torch_Show_Floor`: Sets BG1 layers to Dark (lantern reveals BG2 floor underneath).
  - `Red_Flashes`: No persistent blend change (Ganon fight lightning is temporal).
  - `Ganon_Room`: Sets BG2 layout to Translucent.
- **CompositeToOutput()** — Priority-aware pixel compositing:
  - Builds a palette RGB lookup table from the room's SDL surface palette.
  - For translucent layers: computes `(bg1_rgb + bg2_rgb) / 2` per channel, then finds the nearest palette index within the same palette bank via `find_nearest_in_bank`.
  - For dark layers: dims BG1 pixels to simulate unlit rooms.
  - Respects SNES priority bits: BG2 priority=1 tiles render above BG1 priority=0 tiles.

**Key files:**
- `src/zelda3/dungeon/room_layer_manager.h` — Blend mode API, `ApplyRoomEffect()`.
- `src/zelda3/dungeon/room_layer_manager.cc` — `CompositeToOutput()` with RGB color math.
- `src/zelda3/dungeon/room.h` — `EffectKey` enum, `LayerMergeType` constants.

**Draw Routine Registry:**
- **DrawRoutineRegistry** (`zelda3/dungeon/draw_routines/draw_routine_registry.{h,cc}`) — Singleton mapping all 448 vanilla object IDs to routine IDs (0–130). 100% coverage: 256 subtype 1, 64 subtype 2, 128 subtype 3.
- Per-routine metadata includes `draws_to_both_bgs` flag for routines that explicitly write both tilemaps (e.g., routine 2/kRightwards2x4, routine 19/Corner4x4, routine 97/PrisonCell).

**Validation:**
- 19 parity tests in `test/unit/zelda3/dungeon/object_drawing_comprehensive_test.cc` validate routine coverage, palette offsets, pit/mask identification, BothBG flags, water layer semantics, room effects, and layer merge behavior.

## Current Limitations / Gaps

- **Persistence coverage**: Tile objects, sprites, doors (marker + pointer
  table), room headers (14 bytes + message IDs), palettes, torches, pushable
  blocks, custom collision, chests, pot items, dungeon entrances, and edited
  pit-damage membership are written back and have focused regression coverage.
  Pit/block tables remain fixed to their existing vanilla capacities.
- **Object tile editor**: Core editing, preview/atlas rendering, keyboard shortcuts, room re-render after apply, shared tile confirmation, palette invalidation, and reopen/reset behavior are implemented. Remaining gaps are the "new custom object" flow, deeper editor/integration coverage, and any future preview-quality polish after the selector/browser churn settles.
- **Tests**: Focused unit coverage now exists for `ObjectTileLayout`, standard/custom object tile writeback, palette-sensitive preview generation, panel reset behavior, `DungeonEditorSystem`, `DungeonSaveTest`, and `DungeonEditorV2RomSafetyTest`. Broader integration/E2E coverage for ROM-write workflows is still lighter than the unit surface.

## Suggested Next Steps

1. **Object Tile Editor Completion**:
   - Add a guided "New Custom Object" workflow for creating `.bin` files from scratch.
   - Add broader editor/integration coverage around apply/reload/save flows.
   - Revisit selector/browser preview quality after the current object-selector refactor settles.
2. **Save Pipeline Follow-up**:
   - Keep pit-damage edits within the fixed-capacity membership table; treat repointing or capacity expansion as a separate ROM-layout feature.
   - Treat pushable-block table repointing/expansion as a separate ROM-layout feature; the current encoder intentionally stays within the vanilla four-region cap. It fails closed for dirty-but-unloaded room state and when an edit would empty the global table: `LoadAndBuildRoom` scans at least one entry before comparing the byte-length immediate, so zero is not a safe runtime limit without an engine patch. Successful compaction rebases loaded block slot identities only after all ROM writes commit, and the editor transaction snapshot restores those identities if a later save stage rolls back.
   - Add broader integration coverage for door/chest/pot/collision/entrance/write flows beyond the current unit and ROM-safety tests.
3. **Test Coverage**: Add integration tests that:
   - Place/delete objects and verify `Room::EncodeObjects` output changes in ROM.
   - Add doors/chests/entrances and assert persistence across full editor save/reload flows.
   - Exercise undo/redo on object placement and palette edits.
4. **Live Emulator Preview (optional)**: Keep `DungeonObjectEmulatorPreview` as a hook for live patching when the emulator integration lands.
