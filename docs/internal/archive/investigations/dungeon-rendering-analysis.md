# Dungeon Rendering System Analysis

This document analyzes the dungeon object and background rendering pipeline, identifying potential issues with palette indexing, graphics buffer access, and memory safety.

## Graphics Pipeline Overview

```
ROM Data (3BPP compressed)
    ↓
DecompressV2() → SnesTo8bppSheet()
    ↓
graphics_buffer_ (8BPP linear, values 0-7)
    ↓
Room::CopyRoomGraphicsToBuffer()
    ↓
current_gfx16_ (room-specific graphics buffer)
    ↓
ObjectDrawer::DrawTileToBitmap() / BackgroundBuffer::DrawTile()
    ↓
Bitmap pixel data (indexed 8BPP)
    ↓
SetPalette() → SDL Surface → Texture
```

## Palette Structure

### ROM Storage (kDungeonMainPalettes = 0xDD734)
- 20 dungeon palette sets
- 90 colors per set (180 bytes)
- Colors packed without transparent entries

### SNES Hardware Layout
The SNES expects 16-color rows with transparent at indices 0, 16, 32...

### Current yaze Implementation
- 90 colors loaded as linear array (indices 0-89)
- 6 groups of 15 colors each
- Palette stride: `* 15`

## Palette Offset Analysis

### Current Implementation
```cpp
// object_drawer.cc:916
uint8_t palette_offset = (tile_info.palette_ & 0x07) * 15;

// background_buffer.cc:64
uint8_t palette_offset = palette_idx * 15;
```

### The Math
For 3BPP graphics (pixel values 0-7):
- Pixel 0 = transparent (skipped)
- Pixels 1-7 → `(pixel - 1) + palette_offset`

With `* 15` stride:
| Palette | Pixel 1 | Pixel 7 | Colors Used |
|---------|---------|---------|-------------|
| 0       | 0       | 6       | 0-6         |
| 1       | 15      | 21      | 15-21       |
| 2       | 30      | 36      | 30-36       |
| 3       | 45      | 51      | 45-51       |
| 4       | 60      | 66      | 60-66       |
| 5       | 75      | 81      | 75-81       |

**Unused colors**: 7-14, 22-29, 37-44, 52-59, 67-74, 82-89 (8 colors per group)

### Verdict
The `* 15` stride is **correct** for the 90-color packed format. The "wasted" colors are an artifact of:
- ROM storing 15 colors per group (4BPP capacity)
- Graphics using only 8 values (3BPP)

## Current Status & Findings (2025-11-26)

### 1. 8BPP Conversion Mismatch (Fixed)
- **Issue:** `LoadAllGraphicsData` converted 3BPP to **8BPP linear** (1 byte/pixel), but `Room::CopyRoomGraphicsToBuffer` was treating it as 3BPP planar and trying to convert it to 4BPP packed. This caused double conversion and data corruption.
- **Fix:** Updated `CopyRoomGraphicsToBuffer` to copy 8BPP data directly (4096 bytes per sheet). Updated draw routines to read 1 byte per pixel.

### 2. Palette Stride (Fixed)
- **Issue:** Previous code used `* 16` stride, which skipped colors in the packed 90-color palette.
- **Fix:** Updated to `* 15` stride and `pixel - 1` indexing.

### 3. Buffer vs. Arena (Investigation)
- **Issue:** We attempted to switch to `gfx::Arena::Get().gfx_sheets()` for safer access, but this resulted in an empty frame (likely due to initialization order or empty Arena).
- **Status:** Reverted to `rom()->graphics_buffer()` but added strict 8BPP offset calculations (4096 bytes/sheet).
- **Artifacts:** We observed "number-like" tiles (5, 7, 8) instead of dungeon walls. This suggests we might be reading from a font sheet or an incorrect offset in the buffer.
- **Next Step:** Debug logging added to `CopyRoomGraphicsToBuffer` to print block IDs and raw bytes. This will confirm if we are reading valid graphics data or garbage.

### 4. LoadAnimatedGraphics sizeof vs size() (Fixed 2025-11-26)
- **Issue:** `room.cc:821,836` used `sizeof(current_gfx16_)` instead of `.size()` for bounds checking.
- **Context:** For `std::array<uint8_t, N>`, sizeof equals N (works), but this pattern is confusing and fragile.
- **Fix:** Updated to use `current_gfx16_.size()` for clarity and maintainability.

### 5. LoadRoomGraphics Entrance Blockset Condition (Not a Bug - 2025-11-26)
- **File:** `src/zelda3/dungeon/room.cc:352`
- **Observation:** Condition `if (i == 6)` applies entrance graphics only to block 6
- **Status:** This is intentional behavior. The misleading "3-6" comment was removed.
- **Note:** Changing to `i >= 3 && i <= 6` caused tiling artifacts - reverted.

### 7. Layout Not Being Loaded (Fixed 2025-11-26) - MAJOR BREAKTHROUGH
- **File:** `src/zelda3/dungeon/room.cc` - `LoadLayoutTilesToBuffer()`
- **Issue:** `layout_.LoadLayout(layout)` was never called, so `layout_.GetObjects()` always returned empty
- **Impact:** Only floor tiles were drawn, no layout tiles appeared
- **Fix:** Added `layout_.set_rom(rom_)` and `layout_.LoadLayout(layout)` call before accessing layout objects
- **Result:** **WALLS NOW RENDER CORRECTLY!** Left/right walls display properly.
- **Remaining:** Some objects still don't look right - needs further investigation

## Breakthrough Status (2025-11-26)

### What's Working Now
- ✅ Floor tiles render correctly
- ✅ Layout tiles load from ROM
- ✅ Left/right walls display correctly
- ✅ Basic room structure visible

### What Still Needs Work
- ⚠️ Some objects don't render correctly
- ⚠️ Need to verify object tile IDs and graphics lookup
- ⚠️ May be palette or graphics sheet issues for specific object types
- ⚠️ Floor rendering (screenshot shows grid, need to confirm if floor tiles are actually rendering or if the grid is obscuring them)

### Next Investigation Steps
1. **Verify Floor Rendering:** Check if the floor tiles are actually rendering underneath the grid or if they are missing.
2. **Check Object Types:** Identify which specific objects are rendering incorrectly (e.g., chests, pots, enemies).
3. **Verify Tile IDs:** Check `RoomObject::DecodeObjectFromBytes()` and `GetTile()` to ensure correct tile IDs are being calculated.
4. **Debug Logging:** Use the added logging to verify that the correct graphics sheets are being loaded for the objects.

## Detailed Context for Next Session

### Architecture Overview
The dungeon rendering has TWO separate tile systems:

1. **Layout Tiles** (`RoomLayout` class) - Pre-defined room templates (8 layouts total)
   - Loaded from ROM via `kRoomLayoutPointers[]` in `dungeon_rom_addresses.h`
   - Rendered by `LoadLayoutTilesToBuffer()` → `bg1_buffer_.SetTileAt()` / `bg2_buffer_.SetTileAt()`
   - **NOW WORKING** after the LoadLayout fix

2. **Object Tiles** (`RoomObject` class) - Placed objects (walls, doors, decorations, etc.)
   - Loaded from ROM via `LoadObjects()` → `ParseObjectsFromLocation()`
   - Rendered by `RenderObjectsToBackground()` → `ObjectDrawer::DrawObject()`
   - **PARTIALLY WORKING** - walls visible but some objects look wrong

### Key Files for Object Rendering
| File | Purpose |
|------|---------|
| `room.cc:LoadObjects()` | Parses object data from ROM |
| `room.cc:RenderObjectsToBackground()` | Iterates objects, calls ObjectDrawer |
| `object_drawer.cc` | Main object rendering logic |
| `object_drawer.cc:DrawTileToBitmap()` | Draws individual 8x8 tiles |
| `room_object.cc:DecodeObjectFromBytes()` | Decodes 3-byte object format |
| `room_object.cc:GetTile()` | Returns TileInfo for object tiles |

### Object Encoding Format (3 bytes)
```
Byte 1: YYYYY XXX  (Y = tile Y position bits 4-0, X = tile X position bits 2-0)
Byte 2: S XXX YYYY (S = size bit, X = tile X position bits 5-3, Y = tile Y position bits 8-5)
Byte 3: OOOOOOOO  (Object ID)
```

### Potential Object Rendering Issues to Investigate

1. **Tile ID Calculation**
   - Objects use `GetTile(index)` to get TileInfo for each sub-tile
   - The tile ID might be calculated incorrectly for some object types
   - Check `RoomObject::EnsureTilesLoaded()` and tile lookup tables

2. **Graphics Sheet Selection**
   - Objects should use tiles from `current_gfx16_` (room-specific buffer)
   - Different object types may need tiles from different sheet ranges:
     - Blocks 0-7: Main dungeon graphics
     - Blocks 8-11: Static sprites (pots, fairies, etc.)
     - Blocks 12-15: Enemy sprites

3. **Palette Assignment**
   - Objects have a `palette_` field in TileInfo
   - Dungeon palette has 6 groups × 15 colors = 90 colors
   - Palette offset = `(palette_ & 0x07) * 15`
   - Some objects might have wrong palette index

4. **Object Type Handlers**
   - `ObjectDrawer` has different draw methods for different object sizes
   - `DrawSingle()`, `Draw2x2()`, `DrawVertical()`, `DrawHorizontal()`, etc.
   - Some handlers might have bugs in tile placement

### Debug Logging Currently Active
- `[CopyRoomGraphicsToBuffer]` - Logs block/sheet IDs and first bytes
- `[RenderRoomGraphics]` - Logs dirty flags and floor graphics
- `[LoadLayoutTilesToBuffer]` - Logs layout object count
- `[ObjectDrawer]` - Logs first 5 tile draws with position/palette info

### Files Modified in This Session
1. `src/zelda3/dungeon/room.cc:LoadLayoutTilesToBuffer()` - Added layout loading call
2. `src/zelda3/dungeon/room.cc:LoadRoomGraphics()` - Fixed comment, kept i==6 condition
3. `src/app/app.cmake` - Added z3ed WASM exports
4. `src/app/editor/ui/ui_coordinator.cc` - Fixed menu bar right panel positioning
5. `src/app/rom.cc` - Added debug logging for graphics loading

### Quick Test Commands
```bash
# Build
cmake --build build --target yaze -j4

# Run with dungeon editor
./build/bin/Debug/yaze.app/Contents/MacOS/yaze --rom_file=zelda3.sfc --editor=Dungeon
```

### 6. 2BPP Placeholder Sheets (Verified 2025-11-26)
- **Sheets 113-114:** These are 2BPP font/title sheets loaded separately via Load2BppGraphics()
- **In graphics_buffer:** They contain 0xFF placeholder data (4096 bytes each)
- **Impact:** If blockset IDs accidentally point to 113-114, tiles render as solid color
- **Status:** This is expected behavior, not a bug

## Debugging the "Number-Like" Artifacts

The observed "5, 7, 8" number patterns could indicate:

1. **Font Sheet Access:** Blockset IDs pointing to font sheets (but sheets 113-114 have 0xFF, not font data)
2. **Debug Rendering:** Tile IDs or coordinates rendered as text (check for printf to canvas)
3. **Corrupted Offset:** Wrong src_index calculation causing read from arbitrary memory
4. **Uninitialized blocks_:** If LoadRoomGraphics() not called before CopyRoomGraphicsToBuffer()

### Debug Logging Added (room.cc)
```cpp
printf("[CopyRoomGraphicsToBuffer] Block %d (Sheet %d): Offset %d\n", block, sheet_id, src_sheet_offset);
printf("  Bytes: %02X %02X %02X %02X %02X %02X %02X %02X\n", ...);
```

### Next Steps
1. Run the application and check console output for:
   - Sheet IDs for each block (should be 0-112 or 115-126 for valid dungeon graphics)
   - First bytes of each sheet (should NOT be 0xFF for valid graphics)
2. If sheet IDs are valid but graphics are wrong, check:
   - LoadGfxGroups() output for blockset 0 (verify main_blockset_ids)
   - GetGraphicsAddress() returning correct ROM offsets

## Graphics Buffer Layout

### Per-Sheet (4096 bytes each)
- Width: 128 pixels (16 tiles × 8 pixels)
- Height: 32 pixels (4 tiles × 8 pixels)
- Format: 8BPP linear (1 byte per pixel)
- Values: 0-7 for 3BPP graphics

### Room Graphics Buffer (current_gfx16_)
- Size: 64KB (0x10000 bytes)
- Layout: 16 blocks × 4096 bytes
- Contains: Room-specific graphics from blocks_[0..15]

### Index Calculation
```cpp
int tile_col = tile_id % 16;
int tile_row = tile_id / 16;
int tile_base_x = tile_col * 8;
int tile_base_y = tile_row * 1024;  // 8 rows * 128 bytes stride
int src_index = (py * 128) + px + tile_base_x + tile_base_y;
```

## Files Involved

| File | Function | Purpose |
|------|----------|---------|
| `rom.cc` | `LoadAllGraphicsData()` | Decompresses and converts 3BPP→8BPP |
| `room.cc` | `CopyRoomGraphicsToBuffer()` | Copies sheet data to room buffer |
| `room.cc` | `LoadAnimatedGraphics()` | Loads animated tile data |
| `room.cc` | `RenderRoomGraphics()` | Renders room with palette |
| `object_drawer.cc` | `DrawTileToBitmap()` | Draws object tiles |
| `background_buffer.cc` | `DrawTile()` | Draws background tiles |
| `snes_palette.cc` | `LoadDungeonMainPalettes()` | Loads 90-color palettes |
| `snes_tile.cc` | `SnesTo8bppSheet()` | Converts 3BPP→8BPP |
