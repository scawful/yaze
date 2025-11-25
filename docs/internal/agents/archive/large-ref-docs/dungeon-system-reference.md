# YAZE Dungeon System - Complete Technical Reference

Comprehensive reference for AI agents working on the YAZE Dungeon editing system.

---

## 1. Architecture Overview

### File Structure
```
src/zelda3/dungeon/
├── dungeon.h/cc                # Main Dungeon class
├── room.h/cc                   # Room class (1,337 lines)
├── room_object.h/cc            # RoomObject encoding (633+249 lines)
├── object_drawer.h/cc          # Object rendering (210+972 lines)
├── object_parser.h/cc          # ROM tile parsing (172+387 lines)
├── room_entrance.h             # Entrance data (367 lines)
├── dungeon_rom_addresses.h     # ROM address constants (108 lines)

src/app/editor/dungeon/
├── dungeon_editor_v2.h/cc      # Main editor (card-based)
├── dungeon_room_loader.h/cc    # ROM data loading
├── dungeon_room_selector.h/cc  # Room selection UI
├── dungeon_canvas_viewer.h/cc  # Canvas rendering
├── dungeon_object_selector.h/cc # Object palette
├── dungeon_object_interaction.h/cc # Mouse interactions
```

### Data Model
```
Dungeon
  └── rooms_[296]
        └── Room
              ├── tile_objects_[] (RoomObject instances)
              ├── sprites_[]
              ├── chests_in_room_[]
              ├── z3_staircases_[]
              ├── bg1_buffer_ (512x512 pixels)
              ├── bg2_buffer_ (512x512 pixels)
              └── current_gfx16_[] (16KB graphics)
```

---

## 2. Room Structure

### Room Count & Organization
- **Total Rooms:** 296 (indices 0x00-0x127)
- **Canvas Size:** 512x512 pixels (64x64 tiles)
- **Layers:** BG1, BG2, BG3

### Room Properties
```cpp
// room.h
int room_id_;                    // Room index (0-295)
uint8_t blockset;                // Graphics blockset ID
uint8_t spriteset;               // Sprite set ID
uint8_t palette;                 // Palette ID (0-63)
uint8_t layout;                  // Layout template (0-7)
uint8_t floor1, floor2;          // Floor graphics (nibbles)
uint16_t message_id_;            // Associated message

// Behavioral
CollisionKey collision_type;     // Collision enum
EffectKey effect_type;           // Visual effect enum
TagKey tag1, tag2;               // Special condition tags
LayerMergeType layer_merge;      // BG1/BG2 blend mode
```

### Layer Merge Types
```cpp
enum LayerMergeType {
  LayerMerge00 = 0x00,  // Off - Layer 2 invisible
  LayerMerge01 = 0x01,  // Parallax scrolling
  LayerMerge02 = 0x02,  // Dark overlay
  LayerMerge03 = 0x03,  // On top (translucent)
  LayerMerge04 = 0x04,  // Translucent blend
  LayerMerge05 = 0x05,  // Addition blend
  LayerMerge06 = 0x06,  // Normal overlay
  LayerMerge07 = 0x07,  // Transparent
  LayerMerge08 = 0x08,  // Dark room effect
};
```

---

## 3. Object Encoding System

### 3-Byte Object Format

Objects are stored as 3 bytes in ROM with three distinct encoding types:

#### Type 1: Standard Objects (ID 0x00-0xFF)
```
Byte format: xxxxxxss | yyyyyyss | iiiiiiii
             b1         b2         b3

Decoding:
  x = (b1 & 0xFC) >> 2        // 6 bits (0-63)
  y = (b2 & 0xFC) >> 2        // 6 bits (0-63)
  size = ((b1 & 0x03) << 2) | (b2 & 0x03)  // 4 bits (0-15)
  id = b3                      // 8 bits
```

#### Type 2: Extended Objects (ID 0x100-0x1FF)
```
Indicator: b1 >= 0xFC

Byte format: 111111xx | xxxxyyyy | yyiiiiii
             b1         b2         b3

Decoding:
  id = (b3 & 0x3F) | 0x100
  x = ((b2 & 0xF0) >> 4) | ((b1 & 0x03) << 4)
  y = ((b2 & 0x0F) << 2) | ((b3 & 0xC0) >> 6)
  size = 0  // No size parameter
```

#### Type 3: Rare Objects (ID 0xF00-0xFFF)
```
Indicator: b3 >= 0xF8

Byte format: xxxxxxii | yyyyyyii | 11111iii
             b1         b2         b3

Decoding:
  id = (b3 << 4) | 0x80 | ((b2 & 0x03) << 2) | (b1 & 0x03)
  x = (b1 & 0xFC) >> 2
  y = (b2 & 0xFC) >> 2
  size = ((b1 & 0x03) << 2) | (b2 & 0x03)
```

### Object Categories

| Type | ID Range | Examples |
|------|----------|----------|
| Type 1 | 0x00-0xFF | Walls, floors, decorations |
| Type 2 | 0x100-0x1FF | Corners, stairs, furniture |
| Type 3 | 0xF00-0xFFF | Chests, pipes, special objects |

---

## 4. ObjectDrawer Rendering System

### Class Structure
```cpp
// object_drawer.h
class ObjectDrawer {
  // Entry point
  absl::Status DrawObject(const RoomObject& object,
                          gfx::BackgroundBuffer& bg1,
                          gfx::BackgroundBuffer& bg2,
                          const gfx::PaletteGroup& palette_group);

  // Data
  Rom* rom_;
  const uint8_t* room_gfx_buffer_;  // current_gfx16_
  std::unordered_map<int16_t, int> object_to_routine_map_;
  std::vector<DrawRoutine> draw_routines_;
};
```

### Draw Routine Status

| # | Routine Name | Status | Lines |
|---|--------------|--------|-------|
| 0 | DrawRightwards2x2_1to15or32 | COMPLETE | 302-321 |
| 1 | DrawRightwards2x4_1to15or26 | COMPLETE | 323-348 |
| 2 | DrawRightwards2x4spaced4_1to16 | COMPLETE | 350-373 |
| 3 | DrawRightwards2x4spaced4_BothBG | **STUB** | 375-381 |
| 4 | DrawRightwards2x2_1to16 | COMPLETE | 383-399 |
| 5 | DrawDiagonalAcute_1to16 | **UNCLEAR** | 401-417 |
| 6 | DrawDiagonalGrave_1to16 | **UNCLEAR** | 419-435 |
| 7 | DrawDownwards2x2_1to15or32 | COMPLETE | 689-708 |
| 8 | DrawDownwards4x2_1to15or26 | COMPLETE | 710-753 |
| 9 | DrawDownwards4x2_BothBG | **STUB** | 755-761 |
| 10 | DrawDownwardsDecor4x2spaced4 | COMPLETE | 763-782 |
| 11 | DrawDownwards2x2_1to16 | COMPLETE | 784-799 |
| 12 | DrawDownwardsHasEdge1x1 | COMPLETE | 801-813 |
| 13 | DrawDownwardsEdge1x1 | COMPLETE | 815-827 |
| 14 | DrawDownwardsLeftCorners | COMPLETE | 829-842 |
| 15 | DrawDownwardsRightCorners | COMPLETE | 844-857 |
| 16 | DrawRightwards4x4_1to16 | COMPLETE | 534-550 |
| - | CustomDraw | **STUB** | 524-532 |
| - | DrawDoorSwitcherer | **STUB** | 566-575 |

### INCOMPLETE: BothBG Routines (4 locations)

These routines should draw to BOTH BG1 and BG2 but currently only call single-layer version:

```cpp
// Line 375-381: DrawRightwards2x4spaced4_1to16_BothBG
// Line 437-442: DrawDiagonalAcute_1to16_BothBG
// Line 444-449: DrawDiagonalGrave_1to16_BothBG
// Line 755-761: DrawDownwards4x2_1to16_BothBG

// Current (WRONG):
void DrawRightwards2x4spaced4_1to16_BothBG(
    const RoomObject& obj, gfx::BackgroundBuffer& bg,  // Only 1 buffer!
    std::span<const gfx::TileInfo> tiles) {
  // Just calls single-layer version - misses BG2
  DrawRightwards2x4spaced4_1to16(obj, bg, tiles);
}

// Should be:
void DrawRightwards2x4spaced4_1to16_BothBG(
    const RoomObject& obj,
    gfx::BackgroundBuffer& bg1,  // Both buffers
    gfx::BackgroundBuffer& bg2,
    std::span<const gfx::TileInfo> tiles) {
  DrawRightwards2x4spaced4_1to16(obj, bg1, tiles);
  DrawRightwards2x4spaced4_1to16(obj, bg2, tiles);
}
```

### UNCLEAR: Diagonal Routines

```cpp
// Lines 401-417, 419-435
// Issues:
// - Hardcoded +6 and 5 iterations (why?)
// - Coordinate formula may produce negative Y
// - Only uses 4 tiles from larger span
// - No bounds checking

for (int s = 0; s < size + 6; s++) {  // Why +6?
  for (int i = 0; i < 5; i++) {       // Why 5?
    WriteTile8(bg, obj.x_ + s, obj.y_ + (i - s), tiles[i % 4]);
    // ^^ (i - s) can be negative when s > i
  }
}
```

---

## 5. Tile Rendering Pipeline

### WriteTile8() - Tile to Pixel Conversion
```cpp
// object_drawer.cc lines 863-883
void WriteTile8(gfx::BackgroundBuffer& bg, int tile_x, int tile_y,
                const gfx::TileInfo& tile_info) {
  // tile coords → pixel coords: tile_x * 8, tile_y * 8
  DrawTileToBitmap(bitmap, tile_info, tile_x * 8, tile_y * 8, room_gfx_buffer_);
}
```

### DrawTileToBitmap() - Pixel Rendering
```cpp
// object_drawer.cc lines 890-970
// Key steps:
// 1. Graphics sheet lookup: tile_info.id_ → (sheet_x, sheet_y)
// 2. Palette offset: (palette & 0x0F) * 8
// 3. Per-pixel with mirroring support
// 4. Color 0 = transparent (skipped)

int tile_sheet_x = (tile_info.id_ % 16) * 8;   // 0-127 pixels
int tile_sheet_y = (tile_info.id_ / 16) * 8;   // 0-127 pixels
uint8_t palette_offset = (tile_info.palette_ & 0x0F) * 8;

for (int py = 0; py < 8; py++) {
  for (int px = 0; px < 8; px++) {
    int src_x = tile_info.horizontal_mirror_ ? (7 - px) : px;
    int src_y = tile_info.vertical_mirror_ ? (7 - py) : py;
    // Read pixel, apply palette, write to bitmap
  }
}
```

### Palette Application (CRITICAL)
```cpp
// object_drawer.cc lines 71-115
// Palette must be applied AFTER drawing, BEFORE SDL sync

// 1. Draw all objects (writes palette indices 0-255)
for (auto& obj : objects) {
  DrawObject(obj, bg1, bg2, palette_group);
}

// 2. Apply dungeon palette to convert indices → RGB
bg1_bmp.SetPalette(dungeon_palette);
bg2_bmp.SetPalette(dungeon_palette);

// 3. Sync to SDL surfaces
SDL_LockSurface(bg1_bmp.surface());
memcpy(bg1_bmp.surface()->pixels, bg1_bmp.mutable_data().data(), ...);
SDL_UnlockSurface(bg1_bmp.surface());
```

---

## 6. ROM Addresses

### Room Data
```cpp
kRoomObjectLayoutPointer = 0x882D   // Layout pointer table
kRoomObjectPointer = 0x874C         // Object data pointer
kRoomHeaderPointer = 0xB5DD         // Room headers (3-byte long)
kRoomHeaderPointerBank = 0xB5E7     // Bank byte
```

### Palette & Graphics
```cpp
kDungeonsMainBgPalettePointers = 0xDEC4B
kDungeonsPalettes = 0xDD734
kGfxGroupsPointer = 0x6237
kTileAddress = 0x1B52               // Main tile graphics
kTileAddressFloor = 0x1B5A          // Floor tile graphics
```

### Object Subtypes
```cpp
kRoomObjectSubtype1 = 0x0F8000      // Standard objects
kRoomObjectSubtype2 = 0x0F83F0      // Extended objects
kRoomObjectSubtype3 = 0x0F84F0      // Rare objects
kRoomObjectTileAddress = 0x091B52   // Tile data
```

### Special Objects
```cpp
kBlocksPointer[1-4] = 0x15AFA-0x15B0F
kChestsDataPointer1 = 0xEBFB
kTorchData = 0x2736A
kPitPointer = 0x394AB
kDoorPointers = 0xF83C0
```

---

## 7. TODOs in room_object.h (30+ items)

### Unknown Objects Needing Verification

| Line | ID | Description |
|------|-----|-------------|
| 234 | 0x35 | "WEIRD DOOR" - needs investigation |
| 252-255 | 0x49-0x4C | "Unknown" Type 1 objects |
| 350-353 | 0xC4-0xC7 | "Diagonal layer 2 mask B" - needs verify |
| 392-395 | 0xDE-0xE1 | "Moving wall flag" - WTF IS THIS? |
| 466-476 | Type 2 | Multiple "Unknown" objects |
| 480 | 0x30 | "Intraroom stairs north B" - verify layer |
| 486 | 0x36 | "Water ladder (south)" - needs verify |
| 512-584 | Type 3 | Multiple "Unknown" objects |

---

## 8. DungeonEditorV2 Architecture

### Card-Based Component System
```cpp
DungeonEditorV2 (Coordinator)
├── DungeonRoomLoader       // ROM data loading
├── DungeonRoomSelector     // Room list/selection
├── DungeonCanvasViewer     // 512x512 canvas
├── DungeonObjectSelector   // Object palette
├── DungeonObjectInteraction // Mouse handling
├── ObjectEditorCard        // Property editing
└── PaletteEditorWidget     // Color editing
```

### Card Types
```cpp
show_control_panel_   // Room/entrance selection
show_room_selector_   // Room list
show_room_matrix_     // 16x19 visual layout
show_entrances_list_  // Entrance/spawn list
show_room_graphics_   // Blockset/palette
show_object_editor_   // Object placement
show_palette_editor_  // Palette colors
show_debug_controls_  // Debug options
```

### Undo/Redo System
```cpp
// Per-room object snapshots
std::unordered_map<int, std::vector<std::vector<RoomObject>>> undo_history_;
std::unordered_map<int, std::vector<std::vector<RoomObject>>> redo_history_;
```

---

## 9. Room Loading Flow

```
LoadRoomFromRom(room_id)
  │
  ├── Resolve room header pointer (0xB5DD + room_id * 3)
  │
  ├── Parse header bytes:
  │   ├── BG2 type, collision, light flag
  │   ├── Palette, blockset, spriteset
  │   ├── Effect type, tags
  │   └── Staircase data
  │
  ├── Load graphics sheets (16 blocks)
  │
  └── LoadObjects()
        │
        ├── Read floor/layout header (2 bytes)
        │
        ├── Parse object stream:
        │   ├── 3 bytes per object
        │   ├── 0xFF 0xFF = layer boundary
        │   └── 0xF0 0xFF = door section
        │
        └── Handle special objects:
            ├── Staircases
            ├── Chests
            ├── Doors
            ├── Torches
            └── Blocks
```

---

## 10. Rendering Pipeline

```
1. LoadRoomGraphics()
   └── Build graphics sheet list from blockset

2. CopyRoomGraphicsToBuffer()
   └── Copy ROM sheets → current_gfx16_[]

3. RenderRoomGraphics()
   ├── Check dirty flags
   ├── LoadLayoutTilesToBuffer()
   ├── Draw floor to bg1/bg2 buffers
   └── RenderObjectsToBackground()
         └── ObjectDrawer::DrawObjectList()

4. Present (Canvas Viewer)
   ├── Process deferred texture queue
   ├── Create/update GPU textures
   └── Render to ImGui canvas
```

---

## 11. Known Issues Summary

### BothBG Support (4 stubs)
- Line 380: `DrawRightwards2x4spaced4_1to16_BothBG`
- Line 441: `DrawDiagonalAcute_1to16_BothBG`
- Line 448: `DrawDiagonalGrave_1to16_BothBG`
- Line 760: `DrawDownwards4x2_1to16_BothBG`

**Fix:** Change signature to accept both `bg1` and `bg2` buffers.

### Diagonal Logic (2 routines)
- Lines 401-435: Hardcoded constants, potential negative coords
- **Needs:** Game verification or ZScream reference

### Custom/Door Stubs (2 routines)
- Line 524-532: `CustomDraw` - only draws first tile
- Line 566-575: `DrawDoorSwitcherer` - only draws first tile

### Object Names (30+ unknowns)
- Multiple objects need in-game verification
- See section 7 for full list

---

## 12. Testing

### Run Dungeon Tests
```bash
# Unit tests
ctest --test-dir build -R "dungeon\|Dungeon" -V

# E2E tests
ctest --test-dir build -R "DungeonEditor" -V
```

### E2E Test Files
- `test/e2e/dungeon_editor_smoke_test.cc`
- `test/e2e/dungeon_canvas_interaction_test.cc`
- `test/e2e/dungeon_layer_rendering_test.cc`
- `test/e2e/dungeon_object_drawing_test.cc`

### Test Design Doc
`docs/internal/testing/dungeon-gui-test-design.md` (1000+ lines)
