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
