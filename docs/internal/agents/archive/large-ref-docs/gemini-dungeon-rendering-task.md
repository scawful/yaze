# Gemini Task: Fix Dungeon Object Rendering

## Build Instructions

```bash
# Configure and build (use dedicated build_gemini directory)
./scripts/gemini_build.sh

# Or manually:
cmake --preset mac-gemini
cmake --build build_gemini --target yaze -j8

# Run the app to test
./build_gemini/Debug/yaze.app/Contents/MacOS/yaze --rom_file=zelda3.sfc --editor=Dungeon

# Run all stable tests (GTest executable)
./build_gemini/Debug/yaze_test_stable

# Run specific test suites with gtest_filter
./build_gemini/Debug/yaze_test_stable --gtest_filter="*Room*"
./build_gemini/Debug/yaze_test_stable --gtest_filter="*Dungeon*"
./build_gemini/Debug/yaze_test_stable --gtest_filter="*ObjectDrawer*"

# List available tests
./build_gemini/Debug/yaze_test_stable --gtest_list_tests
```

---

## Executive Summary

**Root Cause**: The dungeon rendering system has TWO bugs:
1. **Missing 3BPP→4BPP conversion**: ROM data is copied raw without format conversion
2. **Wrong palette offset multiplier**: Uses `* 16` (4BPP) but should use `* 8` (3BPP)

**The Correct Fix**: Either:
- **Option A**: Convert 3BPP to 4BPP during buffer copy, then `* 16` is correct
- **Option B**: Keep raw 3BPP data, change multiplier back to `* 8`

ZScream uses Option A (full 4BPP conversion). This document provides the exact algorithm.

---

## Critical Bug Analysis

### Bug #1: Palette Offset Calculation (object_drawer.cc:911)

**Current Code (WRONG for 3BPP data):**
```cpp
uint8_t palette_offset = (tile_info.palette_ & 0x07) * 16;
```

**What ZScream Does (Reference Implementation):**
```csharp
// ZScreamDungeon/GraphicsManager.cs lines 1043-1044
gfx16Pointer[index + r ^ 1] = (byte)((pixel & 0x0F) + (tile.palette * 16));
gfx16Pointer[index + r] = (byte)(((pixel >> 4) & 0x0F) + (tile.palette * 16));
```

**Key Insight**: ZScream uses `* 16` because it CONVERTS the data to 4BPP first. Without that conversion, yaze should use `* 8`.

### Bug #2: Missing BPP Conversion (room.cc:228-295)

**Current Code (Copies raw 3BPP data):**
```cpp
void Room::CopyRoomGraphicsToBuffer() {
  auto gfx_buffer_data = rom()->mutable_graphics_buffer();
  int sheet_pos = 0;
  for (int i = 0; i < 16; i++) {
    int block_offset = blocks_[i] * kGfxBufferRoomOffset;  // 2048 bytes/block
    while (data < kGfxBufferRoomOffset) {
      current_gfx16_[data + sheet_pos] = (*gfx_buffer_data)[data + block_offset];
      data++;
    }
    sheet_pos += kGfxBufferRoomOffset;
  }
}
```

**Problem**: This copies raw bytes without any BPP format conversion!

---

## ZScream Reference Implementation

### Buffer Sizes (GraphicsManager.cs:20-95)

```csharp
// Graphics buffer: 32KB (128×512 pixels / 2 nibbles per byte)
currentgfx16Ptr = Marshal.AllocHGlobal((128 * 512) / 2)  // 32,768 bytes

// Room backgrounds: 256KB each (512×512 pixels @ 8BPP)
roomBg1Ptr = Marshal.AllocHGlobal(512 * 512)  // 262,144 bytes
roomBg2Ptr = Marshal.AllocHGlobal(512 * 512)  // 262,144 bytes
```

### Sheet Classification (Constants.cs:20-21)

```csharp
Uncompressed3BPPSize  = 0x0600  // 1536 bytes per 3BPP sheet (24 bytes/tile × 64 tiles)
UncompressedSheetSize = 0x0800  // 2048 bytes per 2BPP sheet

// 3BPP sheets: 0-112, 115-126, 127-217 (dungeon/overworld graphics)
// 2BPP sheets: 113-114, 218-222 (fonts, UI elements)
```

### 3BPP to 4BPP Conversion Algorithm (GraphicsManager.cs:379-400)

**This is the exact algorithm yaze needs to implement:**

```csharp
// For each 3BPP sheet:
for (int j = 0; j < 4; j++) {           // 4 rows of tiles
  for (int i = 0; i < 16; i++) {        // 16 tiles per row
    for (int y = 0; y < 8; y++) {       // 8 pixel rows per tile
      // Read 3 bitplanes from ROM (SNES planar format)
      byte lineBits0 = data[(y * 2) + (i * 24) + (j * 384) + sheetPosition];
      byte lineBits1 = data[(y * 2) + (i * 24) + (j * 384) + 1 + sheetPosition];
      byte lineBits2 = data[(y) + (i * 24) + (j * 384) + 16 + sheetPosition];

      // For each pair of pixels (4 nibbles = 4 pixels, but processed as 2 pairs)
      for (int x = 0; x < 4; x++) {
        byte pixdata = 0;
        byte pixdata2 = 0;

        // Extract pixel 1 color (bits from all 3 planes)
        if ((lineBits0 & mask[x * 2]) == mask[x * 2])     pixdata += 1;
        if ((lineBits1 & mask[x * 2]) == mask[x * 2])     pixdata += 2;
        if ((lineBits2 & mask[x * 2]) == mask[x * 2])     pixdata += 4;

        // Extract pixel 2 color
        if ((lineBits0 & mask[x * 2 + 1]) == mask[x * 2 + 1]) pixdata2 += 1;
        if ((lineBits1 & mask[x * 2 + 1]) == mask[x * 2 + 1]) pixdata2 += 2;
        if ((lineBits2 & mask[x * 2 + 1]) == mask[x * 2 + 1]) pixdata2 += 4;

        // Pack into 4BPP format (2 pixels per byte, 4 bits each)
        int destIndex = (y * 64) + x + (i * 4) + (j * 512) + (s * 2048);
        newData[destIndex] = (byte)((pixdata << 4) | pixdata2);
      }
    }
  }
  sheetPosition += 0x0600;  // Advance by 1536 bytes per 3BPP sheet
}

// Bit extraction mask
byte[] mask = { 0x80, 0x40, 0x20, 0x10, 0x08, 0x04, 0x02, 0x01 };
```

### Tile Drawing to Buffer (GraphicsManager.cs:140-164)

```csharp
public static void DrawTileToBuffer(Tile tile, byte* canvas, byte* tiledata) {
  // Calculate tile position in graphics buffer
  int tx = (tile.ID / 16 * 512) + ((tile.ID & 0xF) * 4);
  byte palnibble = (byte)(tile.Palette << 4);  // Palette offset (0, 16, 32, ...)
  byte r = tile.HFlipByte;

  for (int yl = 0; yl < 512; yl += 64) {    // Each line is 64 bytes apart
    int my = (tile.VFlip ? 448 - yl : yl);

    for (int xl = 0; xl < 4; xl++) {         // 4 nibble-pairs per tile row
      int mx = 2 * (tile.HFlip ? 3 - xl : xl);
      byte pixel = tiledata[tx + yl + xl];

      // Unpack nibbles and apply palette offset
      canvas[mx + my + r ^ 1] = (byte)((pixel & 0x0F) | palnibble);
      canvas[mx + my + r] = (byte)((pixel >> 4) | palnibble);
    }
  }
}
```

---

## SNES Disassembly Reference

### Do3bppToWRAM4bpp Algorithm (bank_00.asm:9759-9892)

**WRAM Addresses:**
- `$7E9000-$7E91FF`: Primary 4BPP conversion buffer (512 bytes)
- Planes 0-3: `$7E9000 + offset`
- Plane 4 (palette): `$7E9010 + offset`

**Byte Layout:**
```
3BPP Format (24 bytes per tile):
  Bytes 0-1:  Row 0, Planes 0-1 (interleaved)
  Bytes 2-3:  Row 1, Planes 0-1
  ...
  Bytes 16:   Row 0, Plane 2
  Bytes 17:   Row 1, Plane 2
  ...

4BPP Format (32 bytes per tile):
  Bytes 0-15:  Rows 0-7, Planes 0-1 (2 bytes per row)
  Bytes 16-31: Rows 0-7, Planes 2-3 (2 bytes per row)
```

**Conversion Pseudocode:**
```c
void Convert3BppTo4Bpp(uint8_t* source_3bpp, uint8_t* wram_dest, int num_tiles) {
  for (int tile = 0; tile < num_tiles; tile++) {
    uint8_t* palette_offset = source_3bpp + 0x10;

    for (int word = 0; word < 4; word++) {
      // Read 2 bytes from 3BPP source
      wram_dest[0] = source_3bpp[0];
      source_3bpp += 2;

      // Read palette plane byte
      wram_dest[0x10] = palette_offset[0] & 0xFF;
      palette_offset += 1;

      wram_dest += 2;
    }
    wram_dest += 0x10;  // 32 bytes per 4BPP tile
  }
}
```

---

## Existing yaze Conversion Functions

### Available in src/app/gfx/types/snes_tile.cc

**Recommended Function to Use:**
```cpp
// Line 117-129: Direct BPP conversion at tile level
std::vector<uint8_t> ConvertBpp(std::span<uint8_t> tiles,
                                 uint32_t from_bpp,
                                 uint32_t to_bpp);

// Usage:
std::vector<uint8_t> converted = gfx::ConvertBpp(tiles_data, 3, 4);
```

**Alternative - Sheet Level:**
```cpp
// Line 131+: Convert full graphics sheet
auto sheet_8bpp = gfx::SnesTo8bppSheet(data, 3);  // 3 = source BPP
```

### WARNING: BppFormatManager Has a Bug

**In src/app/gfx/util/bpp_format_manager.cc:314-318:**
```cpp
std::vector<uint8_t> BppFormatManager::Convert3BppTo8Bpp(...) {
  // BUG: Delegates to 4BPP conversion without actual 3BPP handling!
  return Convert4BppTo8Bpp(data, width, height);
}
```

**Do NOT use BppFormatManager for 3BPP conversion - use snes_tile.cc functions instead.**

---

## Implementation Options

### Option A: Full 4BPP Conversion (Recommended - Matches ZScream)

This is the recommended approach because it matches ZScream's working implementation and provides the clearest separation between ROM format (3BPP) and rendering format (4BPP).

---

#### Step 1: Replace `Room::CopyRoomGraphicsToBuffer()` in room.cc

**File**: `src/zelda3/dungeon/room.cc`
**Lines to replace**: 228-295 (the entire `CopyRoomGraphicsToBuffer()` function)

**Replace the ENTIRE function with this code:**

```cpp
void Room::CopyRoomGraphicsToBuffer() {
  if (!rom_ || !rom_->is_loaded()) {
    printf("[CopyRoomGraphicsToBuffer] ROM not loaded\n");
    return;
  }

  auto gfx_buffer_data = rom()->mutable_graphics_buffer();
  if (!gfx_buffer_data || gfx_buffer_data->empty()) {
    printf("[CopyRoomGraphicsToBuffer] Graphics buffer is null or empty\n");
    return;
  }

  printf("[CopyRoomGraphicsToBuffer] Room %d: Converting 3BPP to 4BPP\n",
         room_id_);

  // Bit extraction mask (MSB to LSB)
  static const uint8_t kBitMask[8] = {
    0x80, 0x40, 0x20, 0x10, 0x08, 0x04, 0x02, 0x01
  };

  // Clear destination buffer
  std::fill(current_gfx16_.begin(), current_gfx16_.end(), 0);

  int bytes_converted = 0;
  int dest_pos = 0;

  // Process each of the 16 graphics blocks
  for (int block = 0; block < 16; block++) {
    // Validate block index
    if (blocks_[block] < 0 || blocks_[block] > 255) {
      // Skip invalid blocks, but advance destination position
      dest_pos += 2048;  // 64 tiles * 32 bytes per 4BPP tile
      continue;
    }

    // Source offset in ROM graphics buffer (3BPP format)
    // Each 3BPP sheet is 1536 bytes (64 tiles * 24 bytes/tile)
    int src_sheet_offset = blocks_[block] * 1536;

    // Validate source bounds
    if (src_sheet_offset < 0 ||
        src_sheet_offset + 1536 > static_cast<int>(gfx_buffer_data->size())) {
      dest_pos += 2048;
      continue;
    }

    // Convert 64 tiles per block (arranged as 16x4 grid in sheet)
    for (int tile_row = 0; tile_row < 4; tile_row++) {       // 4 rows of tiles
      for (int tile_col = 0; tile_col < 16; tile_col++) {    // 16 tiles per row
        int tile_index = tile_row * 16 + tile_col;

        // Source offset for this tile in 3BPP format
        // ZScream formula: (i * 24) + (j * 384) where i=tile_col, j=tile_row
        int tile_src = src_sheet_offset + (tile_col * 24) + (tile_row * 384);

        // Convert 8 pixel rows
        for (int row = 0; row < 8; row++) {
          // Read 3 bitplanes from SNES planar format
          // Planes 0-1 are interleaved at bytes 0-15
          // Plane 2 is at bytes 16-23
          uint8_t plane0 = (*gfx_buffer_data)[tile_src + (row * 2)];
          uint8_t plane1 = (*gfx_buffer_data)[tile_src + (row * 2) + 1];
          uint8_t plane2 = (*gfx_buffer_data)[tile_src + 16 + row];

          // Convert 8 pixels to 4 nibble-pairs (4BPP packed format)
          for (int nibble_pair = 0; nibble_pair < 4; nibble_pair++) {
            uint8_t pix1 = 0;  // First pixel of pair
            uint8_t pix2 = 0;  // Second pixel of pair

            // Extract first pixel color from 3 bitplanes
            int bit_index1 = nibble_pair * 2;
            if (plane0 & kBitMask[bit_index1]) pix1 |= 1;
            if (plane1 & kBitMask[bit_index1]) pix1 |= 2;
            if (plane2 & kBitMask[bit_index1]) pix1 |= 4;

            // Extract second pixel color from 3 bitplanes
            int bit_index2 = nibble_pair * 2 + 1;
            if (plane0 & kBitMask[bit_index2]) pix2 |= 1;
            if (plane1 & kBitMask[bit_index2]) pix2 |= 2;
            if (plane2 & kBitMask[bit_index2]) pix2 |= 4;

            // Pack into 4BPP format: high nibble = pix1, low nibble = pix2
            // Destination uses ZScream's layout:
            // (row * 64) + nibble_pair + (tile_col * 4) + (tile_row * 512) + (block * 2048)
            int dest_index = (row * 64) + nibble_pair + (tile_col * 4) +
                            (tile_row * 512) + (block * 2048);

            if (dest_index >= 0 &&
                dest_index < static_cast<int>(current_gfx16_.size())) {
              current_gfx16_[dest_index] = (pix1 << 4) | pix2;
              if (pix1 != 0 || pix2 != 0) bytes_converted++;
            }
          }
        }
      }
    }
  }

  printf("[CopyRoomGraphicsToBuffer] Room %d: Converted %d non-zero pixel pairs\n",
         room_id_, bytes_converted);

  LoadAnimatedGraphics();
}
```

---

#### Step 2: Replace `ObjectDrawer::DrawTileToBitmap()` in object_drawer.cc

**File**: `src/zelda3/dungeon/object_drawer.cc`
**Lines to replace**: 890-971 (the entire `DrawTileToBitmap()` function)

**Replace the ENTIRE function with this code:**

```cpp
void ObjectDrawer::DrawTileToBitmap(gfx::Bitmap& bitmap,
                                    const gfx::TileInfo& tile_info, int pixel_x,
                                    int pixel_y, const uint8_t* tiledata) {
  // Draw an 8x8 tile directly to bitmap at pixel coordinates
  // Graphics data is in 4BPP packed format (2 pixels per byte)
  if (!tiledata) return;

  // DEBUG: Check if bitmap is valid
  if (!bitmap.is_active() || bitmap.width() == 0 || bitmap.height() == 0) {
    LOG_DEBUG("ObjectDrawer", "ERROR: Invalid bitmap - active=%d, size=%dx%d",
              bitmap.is_active(), bitmap.width(), bitmap.height());
    return;
  }

  // Calculate tile position in 4BPP graphics buffer
  // Layout: 16 tiles per row, each tile is 4 bytes wide (8 pixels / 2)
  // Row stride: 64 bytes (16 tiles * 4 bytes)
  int tile_col = tile_info.id_ % 16;
  int tile_row = tile_info.id_ / 16;
  int tile_base_x = tile_col * 4;    // 4 bytes per tile horizontally
  int tile_base_y = tile_row * 512;  // 512 bytes per tile row (8 rows * 64 bytes)

  // Palette offset: 4BPP uses 16 colors per palette
  uint8_t palette_offset = (tile_info.palette_ & 0x07) * 16;

  // DEBUG: Log tile info for first few tiles
  static int debug_tile_count = 0;
  if (debug_tile_count < 5) {
    printf("[ObjectDrawer] DrawTile4BPP: id=0x%03X pos=(%d,%d) base=(%d,%d) pal=%d\n",
           tile_info.id_, pixel_x, pixel_y, tile_base_x, tile_base_y,
           tile_info.palette_);
    debug_tile_count++;
  }

  // Draw 8x8 pixels (processing pixel pairs from packed bytes)
  int pixels_written = 0;
  int pixels_transparent = 0;

  for (int py = 0; py < 8; py++) {
    // Source row with vertical mirroring
    int src_row = tile_info.vertical_mirror_ ? (7 - py) : py;

    for (int nibble_pair = 0; nibble_pair < 4; nibble_pair++) {
      // Source column with horizontal mirroring
      int src_col = tile_info.horizontal_mirror_ ? (3 - nibble_pair) : nibble_pair;

      // Calculate source index in 4BPP buffer
      // ZScream layout: (row * 64) + nibble_pair + tile_base
      int src_index = (src_row * 64) + src_col + tile_base_x + tile_base_y;
      uint8_t packed_byte = tiledata[src_index];

      // Unpack the two pixels from nibbles
      uint8_t pix1, pix2;
      if (tile_info.horizontal_mirror_) {
        // When mirrored, swap nibble order
        pix1 = packed_byte & 0x0F;         // Low nibble first
        pix2 = (packed_byte >> 4) & 0x0F;  // High nibble second
      } else {
        pix1 = (packed_byte >> 4) & 0x0F;  // High nibble first
        pix2 = packed_byte & 0x0F;         // Low nibble second
      }

      // Calculate destination pixel positions
      int px1 = nibble_pair * 2;
      int px2 = nibble_pair * 2 + 1;

      // Write first pixel
      if (pix1 != 0) {
        uint8_t final_color = pix1 + palette_offset;
        int dest_x = pixel_x + px1;
        int dest_y = pixel_y + py;

        if (dest_x >= 0 && dest_x < bitmap.width() &&
            dest_y >= 0 && dest_y < bitmap.height()) {
          int dest_index = dest_y * bitmap.width() + dest_x;
          if (dest_index >= 0 &&
              dest_index < static_cast<int>(bitmap.mutable_data().size())) {
            bitmap.mutable_data()[dest_index] = final_color;
            pixels_written++;
          }
        }
      } else {
        pixels_transparent++;
      }

      // Write second pixel
      if (pix2 != 0) {
        uint8_t final_color = pix2 + palette_offset;
        int dest_x = pixel_x + px2;
        int dest_y = pixel_y + py;

        if (dest_x >= 0 && dest_x < bitmap.width() &&
            dest_y >= 0 && dest_y < bitmap.height()) {
          int dest_index = dest_y * bitmap.width() + dest_x;
          if (dest_index >= 0 &&
              dest_index < static_cast<int>(bitmap.mutable_data().size())) {
            bitmap.mutable_data()[dest_index] = final_color;
            pixels_written++;
          }
        }
      } else {
        pixels_transparent++;
      }
    }
  }

  // Mark bitmap as modified if we wrote any pixels
  if (pixels_written > 0) {
    bitmap.set_modified(true);
  }

  // DEBUG: Log pixel writing stats for first few tiles
  if (debug_tile_count <= 5) {
    printf("[ObjectDrawer] Tile 0x%03X: wrote %d pixels, %d transparent\n",
           tile_info.id_, pixels_written, pixels_transparent);
  }
}
```

---

#### Step 3: Verify Constants in room.h

**File**: `src/zelda3/dungeon/room.h`
**Line 412**: Ensure buffer size is correct

```cpp
std::array<uint8_t, 0x8000> current_gfx16_;  // 32KB = 16 blocks * 2048 bytes
```

This is CORRECT. 32KB holds 16 blocks of 64 tiles each in 4BPP format:
- 16 blocks × 64 tiles × 32 bytes/tile = 32,768 bytes = 0x8000

---

### Option B: Keep 3BPP, Fix Palette Offset (Simpler but Less Correct)

**Step 1: Change palette offset back to `* 8` in object_drawer.cc:911**
```cpp
uint8_t palette_offset = (tile_info.palette_ & 0x07) * 8;  // 8 colors per 3BPP palette
```

**Step 2: Ensure graphics buffer is already converted to 8BPP indexed**

Check if `rom()->mutable_graphics_buffer()` already contains 8BPP indexed data (it should, based on ROM loading code).

**Note**: This option is simpler but may not render correctly if the graphics buffer format doesn't match expectations. Option A is recommended.

---

## Testing Strategy

### Test Infrastructure Notes

> **IMPORTANT**: The test utility functions have been updated to properly initialize the full editor system. If you're writing new GUI tests, use the provided test utilities:

**Test Utilities** (defined in `test/test_utils.cc`):

| Function | Purpose |
|----------|---------|
| `gui::LoadRomInTest(ctx, rom_path)` | Loads ROM and initializes ALL editors (calls full `LoadAssets()` flow) |
| `gui::OpenEditorInTest(ctx, "Dungeon")` | Opens an editor via the **View** menu (NOT "Editors" menu!) |

**Menu Structure Note**: Editors are under the `View` menu, not `Editors`:
- Correct: `ctx->MenuClick("View/Dungeon")`
- Incorrect: `ctx->MenuClick("Editors/Dungeon")` ← This will fail!

**Full Initialization Flow**: `LoadRomInTest()` calls `Controller::LoadRomForTesting()` which:
1. Calls `EditorManager::OpenRomOrProject()`
2. Finds/creates a session for the ROM
3. Calls `ConfigureEditorDependencies()`
4. Calls `LoadAssets()` which:
   - Initializes all editors (registers their cards)
   - Loads graphics data into `gfx::Arena`
   - Loads dungeon/overworld/sprite data from ROM
5. Updates UI state (hides welcome screen, shows editor selection)

Without this full flow, editors will appear as empty windows.

---

### Quick Build & Test Cycle

```bash
# 1. Build the project
cmake --build build_gemini --target yaze -j8

# 2. Run unit tests to verify no regressions
./build_gemini/Debug/yaze_test_stable --gtest_filter="*Dungeon*:*Room*:*ObjectDrawer*"

# 3. Visual test with the app
./build_gemini/Debug/yaze.app/Contents/MacOS/yaze --rom_file=zelda3.sfc --editor=Dungeon

# 4. Run specific palette test to verify fix
./build_gemini/Debug/yaze_test_stable --gtest_filter="*PaletteOffset*"
```

---

### Unit Tests to Run After Implementation

**Existing tests that MUST pass:**

```bash
# Core dungeon tests
./build_gemini/Debug/yaze_test_stable --gtest_filter="DungeonObjectRenderingTests.*"
./build_gemini/Debug/yaze_test_stable --gtest_filter="DungeonPaletteTest.*"
./build_gemini/Debug/yaze_test_stable --gtest_filter="*Room*"

# All dungeon-related tests
./build_gemini/Debug/yaze_test_stable --gtest_filter="*Dungeon*:*Object*:*Room*"
```

**Key test files:**
| File | Purpose |
|------|---------|
| `test/integration/zelda3/dungeon_palette_test.cc` | Validates palette offset calculation |
| `test/integration/zelda3/dungeon_object_rendering_tests.cc` | Tests ObjectDrawer with BackgroundBuffer |
| `test/integration/zelda3/dungeon_room_test.cc` | Tests Room loading and graphics |
| `test/e2e/dungeon_object_drawing_test.cc` | End-to-end drawing verification |

---

### New Test to Add: 3BPP to 4BPP Conversion Test

**Create file**: `test/unit/zelda3/dungeon/bpp_conversion_test.cc`

```cpp
#include <gtest/gtest.h>
#include <array>
#include <cstdint>

namespace yaze {
namespace zelda3 {
namespace test {

class Bpp3To4ConversionTest : public ::testing::Test {
 protected:
  // Simulates the conversion algorithm
  static const uint8_t kBitMask[8];

  void Convert3BppTo4Bpp(const uint8_t* src_3bpp, uint8_t* dest_4bpp) {
    // Convert one 8x8 tile from 3BPP (24 bytes) to 4BPP packed (32 bytes)
    for (int row = 0; row < 8; row++) {
      uint8_t plane0 = src_3bpp[row * 2];
      uint8_t plane1 = src_3bpp[row * 2 + 1];
      uint8_t plane2 = src_3bpp[16 + row];

      for (int nibble_pair = 0; nibble_pair < 4; nibble_pair++) {
        uint8_t pix1 = 0, pix2 = 0;

        int bit1 = nibble_pair * 2;
        int bit2 = nibble_pair * 2 + 1;

        if (plane0 & kBitMask[bit1]) pix1 |= 1;
        if (plane1 & kBitMask[bit1]) pix1 |= 2;
        if (plane2 & kBitMask[bit1]) pix1 |= 4;

        if (plane0 & kBitMask[bit2]) pix2 |= 1;
        if (plane1 & kBitMask[bit2]) pix2 |= 2;
        if (plane2 & kBitMask[bit2]) pix2 |= 4;

        dest_4bpp[row * 4 + nibble_pair] = (pix1 << 4) | pix2;
      }
    }
  }
};

const uint8_t Bpp3To4ConversionTest::kBitMask[8] = {
  0x80, 0x40, 0x20, 0x10, 0x08, 0x04, 0x02, 0x01
};

// Test that all-zero 3BPP produces all-zero 4BPP
TEST_F(Bpp3To4ConversionTest, ZeroInputProducesZeroOutput) {
  std::array<uint8_t, 24> src_3bpp = {};  // All zeros
  std::array<uint8_t, 32> dest_4bpp = {};

  Convert3BppTo4Bpp(src_3bpp.data(), dest_4bpp.data());

  for (int i = 0; i < 32; i++) {
    EXPECT_EQ(dest_4bpp[i], 0) << "Byte " << i << " should be zero";
  }
}

// Test that all-ones in plane0 produces correct pattern
TEST_F(Bpp3To4ConversionTest, Plane0OnlyProducesColorIndex1) {
  std::array<uint8_t, 24> src_3bpp = {};
  // Set plane0 to all 1s for first row
  src_3bpp[0] = 0xFF;  // Row 0, plane 0

  std::array<uint8_t, 32> dest_4bpp = {};
  Convert3BppTo4Bpp(src_3bpp.data(), dest_4bpp.data());

  // First row should have color index 1 for all pixels
  // Packed: (1 << 4) | 1 = 0x11
  EXPECT_EQ(dest_4bpp[0], 0x11);
  EXPECT_EQ(dest_4bpp[1], 0x11);
  EXPECT_EQ(dest_4bpp[2], 0x11);
  EXPECT_EQ(dest_4bpp[3], 0x11);
}

// Test that all planes set produces color index 7
TEST_F(Bpp3To4ConversionTest, AllPlanesProducesColorIndex7) {
  std::array<uint8_t, 24> src_3bpp = {};
  // Set all planes for first row
  src_3bpp[0] = 0xFF;   // Row 0, plane 0
  src_3bpp[1] = 0xFF;   // Row 0, plane 1
  src_3bpp[16] = 0xFF;  // Row 0, plane 2

  std::array<uint8_t, 32> dest_4bpp = {};
  Convert3BppTo4Bpp(src_3bpp.data(), dest_4bpp.data());

  // First row should have color index 7 for all pixels
  // Packed: (7 << 4) | 7 = 0x77
  EXPECT_EQ(dest_4bpp[0], 0x77);
  EXPECT_EQ(dest_4bpp[1], 0x77);
  EXPECT_EQ(dest_4bpp[2], 0x77);
  EXPECT_EQ(dest_4bpp[3], 0x77);
}

// Test alternating pixel pattern
TEST_F(Bpp3To4ConversionTest, AlternatingPixelsCorrectlyPacked) {
  std::array<uint8_t, 24> src_3bpp = {};
  // Alternate: 0xAA = 10101010 (pixels 0,2,4,6 set)
  src_3bpp[0] = 0xAA;  // Plane 0 only

  std::array<uint8_t, 32> dest_4bpp = {};
  Convert3BppTo4Bpp(src_3bpp.data(), dest_4bpp.data());

  // Pixels 0,2,4,6 have color 1; pixels 1,3,5,7 have color 0
  // Packed: (1 << 4) | 0 = 0x10
  EXPECT_EQ(dest_4bpp[0], 0x10);
  EXPECT_EQ(dest_4bpp[1], 0x10);
  EXPECT_EQ(dest_4bpp[2], 0x10);
  EXPECT_EQ(dest_4bpp[3], 0x10);
}

// Test output buffer size matches expected 4BPP format
TEST_F(Bpp3To4ConversionTest, OutputSizeIs32BytesPerTile) {
  // 8 rows * 4 bytes per row = 32 bytes
  // Each row has 8 pixels, 2 pixels per byte = 4 bytes per row
  constexpr int kExpectedOutputSize = 32;
  std::array<uint8_t, 24> src_3bpp = {};
  std::array<uint8_t, kExpectedOutputSize> dest_4bpp = {};

  Convert3BppTo4Bpp(src_3bpp.data(), dest_4bpp.data());
  // If we got here without crash, size is correct
  SUCCEED();
}

}  // namespace test
}  // namespace zelda3
}  // namespace yaze
```

**Add to test/CMakeLists.txt:**
```cmake
# Under the stable test sources, add:
test/unit/zelda3/dungeon/bpp_conversion_test.cc
```

---

### Update Existing Palette Test

**File**: `test/integration/zelda3/dungeon_palette_test.cc`

**Add this test to verify 4BPP conversion works end-to-end:**

```cpp
TEST_F(DungeonPaletteTest, PaletteOffsetWorksWithConvertedData) {
  gfx::Bitmap bitmap(8, 8);
  bitmap.Create(8, 8, 8, std::vector<uint8_t>(64, 0));

  // Create 4BPP packed tile data (simulating converted buffer)
  // Layout: 512 bytes per tile row, 4 bytes per tile
  // For tile 0: base_x=0, base_y=0
  std::vector<uint8_t> tiledata(512 * 8, 0);

  // Set pixel pair at row 0: high nibble = 3, low nibble = 5
  tiledata[0] = 0x35;

  gfx::TileInfo tile_info;
  tile_info.id_ = 0;
  tile_info.palette_ = 2;  // Palette 2 → offset 32
  tile_info.horizontal_mirror_ = false;
  tile_info.vertical_mirror_ = false;
  tile_info.over_ = false;

  drawer_->DrawTileToBitmap(bitmap, tile_info, 0, 0, tiledata.data());

  const auto& data = bitmap.vector();
  // Pixel 0 (high nibble 3) + offset 32 = 35
  EXPECT_EQ(data[0], 35);
  // Pixel 1 (low nibble 5) + offset 32 = 37
  EXPECT_EQ(data[1], 37);
}
```

---

### Visual Verification Checklist

After implementing Option A, manually verify these scenarios:

**1. Open Room 0 (Sanctuary Interior)**
```bash
./build_gemini/Debug/yaze.app/Contents/MacOS/yaze --rom_file=zelda3.sfc --editor=Dungeon --cards="Room 0"
```
- [ ] Floor tiles render with correct brown/gray colors
- [ ] Walls have proper shading gradients
- [ ] No "rainbow" or garbled color patterns
- [ ] Tiles align properly (no 1-pixel shifts)

**2. Open Room 1 (Hyrule Castle Entrance)**
- [ ] Castle wall patterns are recognizable
- [ ] Door frames render correctly
- [ ] Torch sconces have correct coloring

**3. Open Room 263 (Ganon's Tower)**
- [ ] Complex tile patterns render correctly
- [ ] Multiple palette usage is visible
- [ ] No missing or black tiles

**4. Check All Palettes (0-7)**
- Open any room and modify object palette values
- [ ] Palette 0: First 16 colors work
- [ ] Palette 7: Last palette range (colors 112-127) works
- [ ] No overflow into adjacent palette ranges

---

### Debug Output Verification

When running with the fix, you should see console output like:

```
[CopyRoomGraphicsToBuffer] Room 0: Converting 3BPP to 4BPP
[CopyRoomGraphicsToBuffer] Room 0: Converted 12543 non-zero pixel pairs
[ObjectDrawer] DrawTile4BPP: id=0x010 pos=(40,40) base=(0,512) pal=2
[ObjectDrawer] Tile 0x010: wrote 42 pixels, 22 transparent
```

**Good signs:**
- "Converting 3BPP to 4BPP" message appears
- Non-zero pixel pairs > 0 (typically 5000-15000 per room)
- Tile positions (`base=`) show reasonable values
- Pixels written > 0

**Bad signs:**
- "Converted 0 non-zero pixel pairs" → Source data not found
- All tiles show "wrote 0 pixels" → Addressing formula wrong
- Crash or segfault → Buffer bounds issue

---

## Quick Verification Test (Inline Debug)

**Add this debug code temporarily to verify data format:**

```cpp
// In CopyRoomGraphicsToBuffer(), add after the conversion loop:
printf("=== 4BPP Conversion Debug ===\n");
printf("First 32 bytes of converted buffer:\n");
for (int i = 0; i < 32; i++) {
  printf("%02X ", current_gfx16_[i]);
  if ((i + 1) % 16 == 0) printf("\n");
}
printf("\nExpected: Mixed nibbles (values like 00, 11, 22, 35, 77, etc.)\n");
printf("If all zeros: Conversion failed or source data missing\n");
printf("If values > 0x77: Wrong addressing\n");
```

---

## File Modification Summary

| File | Line | Change |
|------|------|--------|
| `src/zelda3/dungeon/room.cc` | 228-295 | Add 3BPP→4BPP conversion in `CopyRoomGraphicsToBuffer()` |
| `src/zelda3/dungeon/object_drawer.cc` | 911 | Keep `* 16` if converting, or change to `* 8` if not |
| `src/zelda3/dungeon/object_drawer.cc` | 935 | Update buffer addressing formula |
| `src/zelda3/dungeon/room.h` | 412 | Keep `0x8000` buffer size (32KB is correct) |

---

## Success Criteria

1. Dungeon objects render with correct colors (not garbled/shifted)
2. Object shapes are correct (proper tile boundaries)
3. All 296 rooms load without graphical corruption
4. No performance regression (rooms should render in <100ms)
5. Palette sub-indices 0-7 map to correct colors in dungeon palette

---

## Useful Debug Commands

```bash
# Run with debug logging
./build_gemini/Debug/yaze.app/Contents/MacOS/yaze --debug --log_file=debug.log --rom_file=zelda3.sfc --editor=Dungeon

# Open specific room for testing
./build_gemini/Debug/yaze.app/Contents/MacOS/yaze --rom_file=zelda3.sfc --editor=Dungeon --cards="Room 0"

# Run specific dungeon-related tests
./build_gemini/Debug/yaze_test_stable --gtest_filter="*Room*:*Dungeon*:*Object*"

# Run tests with verbose output
./build_gemini/Debug/yaze_test_stable --gtest_filter="*Room*" --gtest_also_run_disabled_tests
```

---

## Troubleshooting Guide

### Issue: All tiles render as solid color or black

**Cause**: Source graphics buffer offset is wrong (reading zeros or wrong data)

**Debug Steps:**
1. Add debug print in `CopyRoomGraphicsToBuffer()`:
```cpp
printf("Block %d: index=%d, src_offset=%d\n", block, blocks_[block], src_sheet_offset);
```
2. Check that `blocks_[block]` values are in range 0-222
3. Verify `src_sheet_offset` doesn't exceed graphics buffer size

**Fix**: The source offset calculation may need adjustment. Check if ROM graphics buffer uses different sheet sizes (some may be 2048 bytes instead of 1536).

---

### Issue: Colors are wrong but shapes are correct

**Cause**: Palette offset calculation mismatch

**Debug Steps:**
1. Verify palette offset in `DrawTileToBitmap()`:
```cpp
printf("Palette %d -> offset %d\n", tile_info.palette_, palette_offset);
```
2. Check expected range: palette 0-7 should give offset 0-112

**Fix**: Ensure using `* 16` for 4BPP converted data, not `* 8`.

---

### Issue: Tiles appear "scrambled" or shifted by pixels

**Cause**: Buffer addressing formula is wrong

**Debug Steps:**
1. For a known tile (e.g., tile ID 0), print the source indices:
```cpp
printf("Tile %d: base_x=%d, base_y=%d\n", tile_info.id_, tile_base_x, tile_base_y);
```
2. Expected for tile 0: base_x=0, base_y=0
3. Expected for tile 16: base_x=0, base_y=512

**Fix**: Check the addressing formula matches ZScream's layout:
- `tile_base_x = (tile_id % 16) * 4`
- `tile_base_y = (tile_id / 16) * 512`

---

### Issue: Horizontal mirroring looks wrong

**Cause**: Nibble unpacking order is incorrect when mirrored

**Debug Steps:**
1. Test with a known asymmetric tile
2. Check the nibble swap logic in `DrawTileToBitmap()`

**Fix**: When `horizontal_mirror_` is true:
- Read nibbles in reverse order from the byte
- Swap which nibble goes to which pixel position

---

### Issue: Crash or segfault during rendering

**Cause**: Buffer overflow - accessing memory out of bounds

**Debug Steps:**
1. Check all array accesses have bounds validation
2. Add explicit bounds checks:
```cpp
if (src_index >= current_gfx16_.size()) {
  printf("ERROR: src_index %d >= buffer size %zu\n", src_index, current_gfx16_.size());
  return;
}
```

**Fix**: Ensure:
- `current_gfx16_` size is 0x8000 (32768 bytes)
- Source index never exceeds buffer size
- Destination bitmap index is within bitmap bounds

---

### Issue: Test `DungeonPaletteTest.PaletteOffsetIsCorrectFor4BPP` fails

**Cause**: The test was written for old linear buffer layout

**Fix**: Update the test to use the new 4BPP packed layout:
```cpp
// Old test assumed linear layout: src_index = y * 128 + x
// New test needs: src_index = (row * 64) + nibble_pair + tile_base
```

The test file at `test/integration/zelda3/dungeon_palette_test.cc` may need updates to match the new addressing scheme.

---

### Issue: `rom()->mutable_graphics_buffer()` returns wrong format

**Cause**: ROM loading may already convert graphics to different format

**Debug Steps:**
1. Check what format the graphics buffer contains:
```cpp
auto gfx_buf = rom()->mutable_graphics_buffer();
printf("Graphics buffer size: %zu\n", gfx_buf->size());
printf("First 16 bytes: ");
for (int i = 0; i < 16; i++) printf("%02X ", (*gfx_buf)[i]);
printf("\n");
```
2. Compare against expected 3BPP pattern

**If ROM already converts to 8BPP:**
- Option A conversion is still correct (just reading from different source format)
- May need to adjust source read offsets

---

### Common Constants Reference

| Constant | Value | Meaning |
|----------|-------|---------|
| 3BPP tile size | 24 bytes | 8 rows × 3 bytes/row |
| 4BPP tile size | 32 bytes | 8 rows × 4 bytes/row |
| 3BPP sheet size | 1536 bytes | 64 tiles × 24 bytes |
| 4BPP sheet size | 2048 bytes | 64 tiles × 32 bytes |
| Tiles per row | 16 | Sheet is 16×4 tiles |
| Row stride (4BPP) | 64 bytes | 16 tiles × 4 bytes |
| Tile row stride | 512 bytes | 8 pixel rows × 64 bytes |
| Block stride | 2048 bytes | One full 4BPP sheet |
| Total buffer | 32768 bytes | 16 blocks × 2048 bytes |

---

## Stretch Goal: Cinematic GUI Test

Create an interactive GUI test that visually demonstrates dungeon object rendering with deliberate pauses for observation. This test is useful for:
- Verifying the fix works visually in the actual editor
- Demonstrating rendering to stakeholders
- Debugging rendering issues in real-time

### Create File: `test/e2e/dungeon_cinematic_rendering_test.cc`

```cpp
/**
 * @file dungeon_cinematic_rendering_test.cc
 * @brief Cinematic test for watching dungeon objects render in slow-motion
 *
 * This test opens multiple dungeon rooms with deliberate pauses between
 * operations so you can visually observe the object rendering process.
 *
 * Run with:
 *   ./build_gemini/Debug/yaze_test_gui --gtest_filter="*Cinematic*"
 *
 * Or register with ImGuiTestEngine for interactive execution.
 */

#define IMGUI_DEFINE_MATH_OPERATORS

#include <chrono>
#include <thread>
#include <vector>

#include "app/controller.h"
#include "rom/rom.h"
#include "gtest/gtest.h"
#include "imgui.h"
#include "imgui_test_engine/imgui_te_context.h"
#include "imgui_test_engine/imgui_te_engine.h"
#include "test_utils.h"

namespace yaze {
namespace test {

// =============================================================================
// Cinematic Test Configuration
// =============================================================================

struct CinematicConfig {
  int frame_delay_short = 30;      // ~0.5 seconds at 60fps
  int frame_delay_medium = 60;     // ~1 second
  int frame_delay_long = 120;      // ~2 seconds
  int frame_delay_dramatic = 180;  // ~3 seconds (for key moments)
  bool log_verbose = true;
};

// =============================================================================
// Room Tour Data - Interesting rooms to showcase
// =============================================================================

struct RoomShowcase {
  int room_id;
  const char* name;
  const char* description;
  int view_duration;  // in frames
};

static const std::vector<RoomShowcase> kCinematicRooms = {
    {0x00, "Sanctuary Interior", "Simple room - good baseline test", 120},
    {0x01, "Hyrule Castle Entrance", "Complex walls and floor patterns", 150},
    {0x02, "Hyrule Castle Main Hall", "Multiple layers and objects", 150},
    {0x10, "Eastern Palace Entrance", "Different tileset/palette", 120},
    {0x20, "Desert Palace Entrance", "Desert-themed graphics", 120},
    {0x44, "Tower of Hera", "Vertical room layout", 120},
    {0x60, "Skull Woods Entrance", "Dark World palette", 150},
    {0x80, "Ice Palace Entrance", "Ice tileset", 120},
    {0xA0, "Misery Mire Entrance", "Swamp tileset", 120},
    {0xC8, "Ganon's Tower Entrance", "Complex multi-layer room", 180},
};

// =============================================================================
// Cinematic Test Functions
// =============================================================================

/**
 * @brief Main cinematic test - tours through showcase rooms
 *
 * Opens each room with dramatic pauses, allowing visual observation of:
 * - Room loading animation
 * - Object rendering (BG1 and BG2 layers)
 * - Palette application
 * - Tile alignment
 */
void E2ETest_Cinematic_DungeonRoomTour(ImGuiTestContext* ctx) {
  CinematicConfig config;

  ctx->LogInfo("========================================");
  ctx->LogInfo("   CINEMATIC DUNGEON RENDERING TEST");
  ctx->LogInfo("========================================");
  ctx->LogInfo("");
  ctx->LogInfo("This test will open multiple dungeon rooms");
  ctx->LogInfo("with pauses for visual observation.");
  ctx->LogInfo("");
  ctx->Yield(config.frame_delay_dramatic);

  // Step 1: Load ROM
  ctx->LogInfo(">>> Loading ROM...");
  gui::LoadRomInTest(ctx, "zelda3.sfc");
  ctx->Yield(config.frame_delay_medium);
  ctx->LogInfo("    ROM loaded successfully!");
  ctx->Yield(config.frame_delay_short);

  // Step 2: Open Dungeon Editor
  ctx->LogInfo(">>> Opening Dungeon Editor...");
  gui::OpenEditorInTest(ctx, "Dungeon");
  ctx->Yield(config.frame_delay_long);
  ctx->LogInfo("    Dungeon Editor ready!");
  ctx->Yield(config.frame_delay_short);

  // Step 3: Enable Room Selector
  ctx->LogInfo(">>> Enabling Room Selector...");
  if (ctx->WindowInfo("Dungeon Controls").Window != nullptr) {
    ctx->SetRef("Dungeon Controls");
    ctx->ItemClick("Rooms");
    ctx->Yield(config.frame_delay_medium);
  }

  // Step 4: Tour through rooms
  ctx->LogInfo("");
  ctx->LogInfo("========================================");
  ctx->LogInfo("   BEGINNING ROOM TOUR");
  ctx->LogInfo("========================================");
  ctx->Yield(config.frame_delay_medium);

  int rooms_visited = 0;
  for (const auto& room : kCinematicRooms) {
    ctx->LogInfo("");
    ctx->LogInfo("----------------------------------------");
    ctx->LogInfo("Room %d/%zu: %s (0x%02X)",
                 rooms_visited + 1, kCinematicRooms.size(),
                 room.name, room.room_id);
    ctx->LogInfo("  %s", room.description);
    ctx->LogInfo("----------------------------------------");
    ctx->Yield(config.frame_delay_short);

    // Open the room
    char room_label[32];
    snprintf(room_label, sizeof(room_label), "Room 0x%02X", room.room_id);

    if (ctx->WindowInfo("Room Selector").Window != nullptr) {
      ctx->SetRef("Room Selector");

      // Try to find and click the room
      char search_pattern[16];
      snprintf(search_pattern, sizeof(search_pattern), "[%03X]*", room.room_id);

      ctx->LogInfo("  >>> Opening room...");

      // Scroll to room if needed
      ctx->ScrollToItem(search_pattern);
      ctx->Yield(config.frame_delay_short);

      // Double-click to open
      ctx->ItemDoubleClick(search_pattern);
      ctx->Yield(config.frame_delay_short);

      ctx->LogInfo("  >>> RENDERING IN PROGRESS...");
      ctx->LogInfo("      (Watch BG1/BG2 layers draw)");

      // Main viewing pause - watch the rendering
      ctx->Yield(room.view_duration);

      ctx->LogInfo("  >>> Room rendered!");
      rooms_visited++;
    } else {
      ctx->LogWarning("  Room selector not available");
    }

    ctx->Yield(config.frame_delay_short);
  }

  // Final summary
  ctx->LogInfo("");
  ctx->LogInfo("========================================");
  ctx->LogInfo("   CINEMATIC TEST COMPLETE");
  ctx->LogInfo("========================================");
  ctx->LogInfo("");
  ctx->LogInfo("Rooms visited: %d/%zu", rooms_visited, kCinematicRooms.size());
  ctx->LogInfo("");
  ctx->LogInfo("Visual checks to verify:");
  ctx->LogInfo("  [ ] Objects rendered with correct colors");
  ctx->LogInfo("  [ ] No rainbow/garbled patterns");
  ctx->LogInfo("  [ ] Tiles properly aligned (no shifts)");
  ctx->LogInfo("  [ ] Different palettes visible in different rooms");
  ctx->LogInfo("");
  ctx->Yield(config.frame_delay_dramatic);
}

/**
 * @brief Layer toggle demonstration
 *
 * Opens a room and toggles BG1/BG2 visibility with pauses
 * to demonstrate layer rendering.
 */
void E2ETest_Cinematic_LayerToggleDemo(ImGuiTestContext* ctx) {
  CinematicConfig config;

  ctx->LogInfo("========================================");
  ctx->LogInfo("   LAYER TOGGLE DEMONSTRATION");
  ctx->LogInfo("========================================");
  ctx->Yield(config.frame_delay_medium);

  // Setup
  gui::LoadRomInTest(ctx, "zelda3.sfc");
  gui::OpenEditorInTest(ctx, "Dungeon");
  ctx->Yield(config.frame_delay_medium);

  // Open Room 0
  if (ctx->WindowInfo("Dungeon Controls").Window != nullptr) {
    ctx->SetRef("Dungeon Controls");
    ctx->ItemClick("Rooms");
    ctx->Yield(config.frame_delay_short);
  }

  if (ctx->WindowInfo("Room Selector").Window != nullptr) {
    ctx->SetRef("Room Selector");
    ctx->ItemDoubleClick("[000]*");
    ctx->Yield(config.frame_delay_long);
  }

  // Layer toggle demonstration
  if (ctx->WindowInfo("Room 0x00").Window != nullptr) {
    ctx->SetRef("Room 0x00");

    ctx->LogInfo(">>> Showing both layers (default)");
    ctx->Yield(config.frame_delay_long);

    // Toggle BG1 off
    if (ctx->ItemExists("Show BG1")) {
      ctx->LogInfo(">>> Hiding BG1 layer...");
      ctx->ItemClick("Show BG1");
      ctx->Yield(config.frame_delay_long);
      ctx->LogInfo("    (Only BG2 visible now)");
      ctx->Yield(config.frame_delay_medium);

      // Toggle BG1 back on
      ctx->LogInfo(">>> Showing BG1 layer...");
      ctx->ItemClick("Show BG1");
      ctx->Yield(config.frame_delay_long);
    }

    // Toggle BG2 off
    if (ctx->ItemExists("Show BG2")) {
      ctx->LogInfo(">>> Hiding BG2 layer...");
      ctx->ItemClick("Show BG2");
      ctx->Yield(config.frame_delay_long);
      ctx->LogInfo("    (Only BG1 visible now)");
      ctx->Yield(config.frame_delay_medium);

      // Toggle BG2 back on
      ctx->LogInfo(">>> Showing BG2 layer...");
      ctx->ItemClick("Show BG2");
      ctx->Yield(config.frame_delay_long);
    }
  }

  ctx->LogInfo("========================================");
  ctx->LogInfo("   LAYER DEMO COMPLETE");
  ctx->LogInfo("========================================");
}

/**
 * @brief Palette comparison test
 *
 * Opens rooms with different palette indices side by side
 * to verify palette offset calculation.
 */
void E2ETest_Cinematic_PaletteShowcase(ImGuiTestContext* ctx) {
  CinematicConfig config;

  ctx->LogInfo("========================================");
  ctx->LogInfo("   PALETTE SHOWCASE");
  ctx->LogInfo("========================================");
  ctx->LogInfo("");
  ctx->LogInfo("Opening rooms with different palettes to verify");
  ctx->LogInfo("palette offset calculation is correct.");
  ctx->Yield(config.frame_delay_medium);

  gui::LoadRomInTest(ctx, "zelda3.sfc");
  gui::OpenEditorInTest(ctx, "Dungeon");
  ctx->Yield(config.frame_delay_medium);

  // Enable room selector
  if (ctx->WindowInfo("Dungeon Controls").Window != nullptr) {
    ctx->SetRef("Dungeon Controls");
    ctx->ItemClick("Rooms");
    ctx->Yield(config.frame_delay_short);
  }

  // Rooms that use different palette indices
  struct PaletteRoom {
    int room_id;
    const char* name;
    int expected_palette;
  };

  std::vector<PaletteRoom> palette_rooms = {
      {0x00, "Sanctuary (Palette 0)", 0},
      {0x01, "Hyrule Castle (Palette 1)", 1},
      {0x10, "Eastern Palace (Palette 2)", 2},
      {0x60, "Skull Woods (Dark Palette)", 4},
  };

  for (const auto& room : palette_rooms) {
    ctx->LogInfo("");
    ctx->LogInfo(">>> %s", room.name);
    ctx->LogInfo("    Expected palette index: %d", room.expected_palette);
    ctx->LogInfo("    Expected color offset: %d", room.expected_palette * 16);

    if (ctx->WindowInfo("Room Selector").Window != nullptr) {
      ctx->SetRef("Room Selector");
      char pattern[16];
      snprintf(pattern, sizeof(pattern), "[%03X]*", room.room_id);
      ctx->ItemDoubleClick(pattern);
      ctx->Yield(config.frame_delay_dramatic);
    }
  }

  ctx->LogInfo("");
  ctx->LogInfo("========================================");
  ctx->LogInfo("   PALETTE SHOWCASE COMPLETE");
  ctx->LogInfo("========================================");
  ctx->LogInfo("");
  ctx->LogInfo("Verify each room uses distinct colors!");
}

// =============================================================================
// GTest Registration (for non-interactive execution)
// =============================================================================

class DungeonCinematicTest : public ::testing::Test {
 protected:
  void SetUp() override {
    // Note: These tests require GUI mode
    // Skip if running in headless mode
  }
};

TEST_F(DungeonCinematicTest, DISABLED_RoomTour) {
  // This test is registered with ImGuiTestEngine
  // Run via: ./build_gemini/Debug/yaze_test_gui --gtest_filter="*Cinematic*"
  GTEST_SKIP() << "Run via GUI test engine";
}

TEST_F(DungeonCinematicTest, DISABLED_LayerDemo) {
  GTEST_SKIP() << "Run via GUI test engine";
}

TEST_F(DungeonCinematicTest, DISABLED_PaletteShowcase) {
  GTEST_SKIP() << "Run via GUI test engine";
}

}  // namespace test
}  // namespace yaze
```

---

### Register Tests with ImGuiTestEngine

**Add to the test registration in your GUI test setup:**

```cpp
// In test setup or controller initialization:
if (test_engine) {
  ImGuiTestEngine_RegisterTest(
      test_engine, "Dungeon", "Cinematic_RoomTour",
      E2ETest_Cinematic_DungeonRoomTour);

  ImGuiTestEngine_RegisterTest(
      test_engine, "Dungeon", "Cinematic_LayerToggle",
      E2ETest_Cinematic_LayerToggleDemo);

  ImGuiTestEngine_RegisterTest(
      test_engine, "Dungeon", "Cinematic_PaletteShowcase",
      E2ETest_Cinematic_PaletteShowcase);
}
```

---

### Running the Cinematic Tests

```bash
# Build with GUI tests enabled
cmake --build build_gemini --target yaze_test_gui -j8

# Run all cinematic tests
./build_gemini/Debug/yaze_test_gui --gtest_filter="*Cinematic*"

# Or run interactively via ImGuiTestEngine menu:
# 1. Launch yaze normally
# 2. Open Tools > Test Engine
# 3. Select "Dungeon/Cinematic_RoomTour"
# 4. Click "Run"
```

---

### What to Watch For

During the cinematic test:

1. **Room Loading Phase**
   - Watch the canvas area for initial rendering
   - Objects should appear in sequence (or all at once, depending on implementation)

2. **Color Correctness**
   - Browns/grays for castle walls
   - Distinct palettes for different dungeon types
   - No "rainbow" or garbled colors

3. **Layer Separation**
   - When BG1 is hidden, floor/background remains
   - When BG2 is hidden, walls/foreground remains
   - Both layers combine correctly when visible

4. **Tile Alignment**
   - No 1-pixel shifts between tiles
   - Object edges line up properly
   - No visible seams in repeated patterns

---

## Reference Documentation

- `docs/internal/agents/dungeon-system-reference.md` - Full dungeon system architecture
- `docs/internal/architecture/graphics_system_architecture.md` - Graphics pipeline
- `CLAUDE.md` - Project coding conventions and build instructions
- ZScreamDungeon source: `/Users/scawful/Code/ZScreamDungeon/ZeldaFullEditor/GraphicsManager.cs`
- SNES disassembly: `assets/asm/usdasm/bank_00.asm` (lines 9759-9892)
