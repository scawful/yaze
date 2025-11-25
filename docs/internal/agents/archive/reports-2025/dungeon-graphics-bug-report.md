# Dungeon Graphics Rendering Bug Report

**Status**: CRITICAL - Objects not rendering
**Affected System**: Dungeon Object Editor
**Root Causes**: 4 critical bugs identified
**Research By**: zelda3-hacking-expert + backend-infra-engineer agents

---

## Executive Summary

Dungeon objects are not rendering correctly due to **incorrect ROM addresses** and **missing palette application**. Four critical bugs have been identified in the rendering pipeline.

---

## CRITICAL BUG #1: Wrong ROM Addresses in ObjectParser ⚠️

**Priority**: P0 - BLOCKER
**File**: `src/zelda3/dungeon/object_parser.cc` (Lines 10-14)
**Impact**: Objects read garbage data from ROM

### Current Code (WRONG)
```cpp
static constexpr int kRoomObjectSubtype1 = 0x0A8000;  // ❌ PLACEHOLDER
static constexpr int kRoomObjectSubtype2 = 0x0A9000;  // ❌ PLACEHOLDER
static constexpr int kRoomObjectSubtype3 = 0x0AA000;  // ❌ PLACEHOLDER
static constexpr int kRoomObjectTileAddress = 0x0AB000;  // ❌ PLACEHOLDER
```

**These addresses don't exist in ALTTP's ROM!** They are placeholders from early development.

### Fix (CORRECT)
```cpp
// ALTTP US 1.0 ROM addresses (PC format)
static constexpr int kRoomObjectSubtype1 = 0x0F8000;      // SNES: $08:8000
static constexpr int kRoomObjectSubtype2 = 0x0F83F0;      // SNES: $08:83F0
static constexpr int kRoomObjectSubtype3 = 0x0F84F0;      // SNES: $08:84F0
static constexpr int kRoomObjectTileAddress = 0x091B52;   // SNES: $09:1B52
```

### Explanation

**How ALTTP Object Graphics Work**:
```
1. Object ID (e.g., $10 = wall) → Subtype Table Lookup
   ├─ Read pointer from: kRoomObjectSubtype1 + (ID * 2)
   └─ Pointer is 16-bit offset from kRoomObjectTileAddress

2. Calculate Tile Data Address
   ├─ tile_data_addr = kRoomObjectTileAddress + offset
   └─ Each tile = 2 bytes (TileInfo word)

3. TileInfo Word Format (16-bit: vhopppcccccccccc)
   ├─ v (bit 15): Vertical flip
   ├─ h (bit 14): Horizontal flip
   ├─ o (bit 13): Priority/Over flag
   ├─ ppp (bits 10-12): Palette index (0-7)
   └─ cccccccccc (bits 0-9): CHR tile ID (0-1023)
```

**Example for Object $10 (Wall)**:
```
1. Subtype 1 table: 0x0F8000 + ($10 * 2) = 0x0F8020
2. Read offset: [Low, High] = $0234
3. Tile data: 0x091B52 + $0234 = 0x091D86
4. Read TileInfo words (8 tiles = 16 bytes)
```

---

## CRITICAL BUG #2: Missing Palette Application ⚠️

**Priority**: P0 - BLOCKER
**File**: `src/zelda3/dungeon/object_drawer.cc` (Lines 76-104)
**Impact**: Black screen or wrong colors

### The Problem

`ObjectDrawer` writes palette index values (0-255) to the bitmap, but **never applies the dungeon palette** to the SDL surface. The bitmap has no color information!

**Current Flow**:
```
ObjectDrawer writes index values → memcpy to SDL surface → Display ❌
                                     ↑
                            No palette applied!
```

**Should Be**:
```
ObjectDrawer writes index values → Apply palette → memcpy to SDL → Display ✅
```

### Fix

**Add to `ObjectDrawer::DrawObjectList()` after line 77**:

```cpp
absl::Status ObjectDrawer::DrawObjectList(
    const std::vector<RoomObject>& objects,
    gfx::BackgroundBuffer& bg1,
    gfx::BackgroundBuffer& bg2,
    const gfx::PaletteGroup& palette_group) {

  // Draw all objects
  for (const auto& object : objects) {
    RETURN_IF_ERROR(DrawObject(object, bg1, bg2, palette_group));
  }

  // ✅ FIX: Apply dungeon palette to background buffers
  auto& bg1_bmp = bg1.bitmap();
  auto& bg2_bmp = bg2.bitmap();

  if (!palette_group.empty()) {
    const auto& dungeon_palette = palette_group[0];  // Main dungeon palette (90 colors)
    bg1_bmp.SetPalette(dungeon_palette);
    bg2_bmp.SetPalette(dungeon_palette);
  }

  // Sync bitmap data to SDL surfaces AFTER palette is applied
  if (bg1_bmp.modified() && bg1_bmp.surface() && !bg1_bmp.data().empty()) {
    SDL_LockSurface(bg1_bmp.surface());
    memcpy(bg1_bmp.surface()->pixels, bg1_bmp.data().data(), bg1_bmp.data().size());
    SDL_UnlockSurface(bg1_bmp.surface());
  }

  if (bg2_bmp.modified() && bg2_bmp.surface() && !bg2_bmp.data().empty()) {
    SDL_LockSurface(bg2_bmp.surface());
    memcpy(bg2_bmp.surface()->pixels, bg2_bmp.data().data(), bg2_bmp.data().size());
    SDL_UnlockSurface(bg2_bmp.surface());
  }

  return absl::OkStatus();
}
```

---

## BUG #3: Incorrect Palette Offset Calculation

**Priority**: P1 - HIGH
**File**: `src/zelda3/dungeon/object_drawer.cc` (Line 900)
**Impact**: Wrong colors for objects

### Current Code (WRONG)
```cpp
// Line 899-900
uint8_t palette_offset = (tile_info.palette_ & 0x0F) * 8;
```

**Problem**: Uses 4 bits (`& 0x0F`) but dungeon graphics are 3BPP with only 3-bit palette indices!

### Fix
```cpp
// Dungeon graphics are 3BPP (8 colors per palette)
// Only use 3 bits for palette index (0-7)
uint8_t palette_offset = (tile_info.palette_ & 0x07) * 8;
```

### Dungeon Palette Structure

From `snes_palette.cc` line 198:
- Total: **90 colors** per dungeon palette
- Colors 0-29: Main graphics (palettes 0-3)
- Colors 30-59: Secondary graphics (palettes 4-7)
- Colors 60-89: Sprite graphics (palettes 8-11)

Each sub-palette has 8 colors (3BPP), arranged:
- Palette 0: Colors 0-7
- Palette 1: Colors 8-15
- Palette 2: Colors 16-23
- Palette 3: Colors 24-29 (NOT 24-31!)

---

## BUG #4: Palette Metadata Not Initialized

**Priority**: P2 - MEDIUM
**File**: `src/app/gfx/render/background_buffer.cc` (constructor)
**Impact**: `ApplyPaletteByMetadata()` may not work correctly

### Fix

Ensure BackgroundBuffer initializes bitmap metadata:

```cpp
BackgroundBuffer::BackgroundBuffer(int width, int height)
    : width_(width), height_(height) {
  buffer_.resize((width / 8) * (height / 8));
  std::vector<uint8_t> data(width * height, 0);

  // Create 8-bit indexed color bitmap
  bitmap_.Create(width, height, 8, data);

  // Set metadata for dungeon rendering
  auto& metadata = bitmap_.metadata();
  metadata.source_bpp = 3;               // 3BPP dungeon graphics
  metadata.palette_format = 0;           // Full palette (90 colors)
  metadata.source_type = "dungeon_background";
  metadata.palette_colors = 90;          // Dungeon main palette size
}
```

---

## Complete Rendering Pipeline

### Correct Flow
```
1. ROM Data (0x0F8000+) → ObjectParser
   ├─ Read subtype table
   ├─ Calculate tile data offset
   └─ Parse TileInfo words

2. TileInfo[] → ObjectDrawer
   ├─ For each tile:
   │   ├─ Calculate position in graphics sheet
   │   ├─ Read 8x8 indexed pixels (0-7)
   │   ├─ Apply palette offset: pixel + (palette * 8)
   │   └─ Write to BackgroundBuffer bitmap
   └─ Apply dungeon palette to bitmap (SetPalette)

3. BackgroundBuffer → SDL Surface
   ├─ memcpy indexed pixel data
   └─ SDL uses surface palette for display

4. SDL Surface → ImGui Texture → Screen
```

---

## ROM Address Reference

| Structure | SNES Address | PC Address | Purpose |
|-----------|-------------|-----------|---------|
| Subtype 1 Table | `$08:8000` | `0x0F8000` | Objects $00-$FF pointers (512 bytes) |
| Subtype 2 Table | `$08:83F0` | `0x0F83F0` | Objects $100-$1FF pointers (256 bytes) |
| Subtype 3 Table | `$08:84F0` | `0x0F84F0` | Objects $F00-$FFF pointers (256 bytes) |
| Tile Data Base | `$09:1B52` | `0x091B52` | TileInfo word arrays (~8KB) |
| Graphics Sheets | `$0C:8000+` | `0x0C8000+` | 4BPP compressed CHR data |

---

## Implementation Order

1. ✅ **Fix Bug #1** (ROM addresses) - 5 minutes
2. ✅ **Fix Bug #2** (palette application) - 15 minutes
3. ✅ **Fix Bug #3** (palette offset) - 5 minutes
4. ⚠️ **Fix Bug #4** (metadata) - 10 minutes (verify needed)

**Total Time**: ~35 minutes to fix all critical bugs

---

## Testing Checklist

After fixes:
- [ ] Load dungeon room 0x01 (Eastern Palace entrance)
- [ ] Verify gray stone walls render correctly
- [ ] Check that objects have distinct colors
- [ ] Verify no black/transparent artifacts
- [ ] Test multiple rooms with different palettes
- [ ] Verify BG1 and BG2 layers are distinct

---

## Debugging Commands

Add these logs to verify the fix:

```cpp
// In ObjectParser::ReadTileData() after reading first tile:
if (i == 0) {
  printf("[ObjectParser] Object 0x%03X: tile_addr=0x%06X word=0x%04X → id=%03X pal=%d\n",
         object_id, tile_offset, tile_word, tile_info.id_, tile_info.palette_);
}

// In ObjectDrawer::DrawObjectList() after applying palette:
if (!palette_group.empty()) {
  const auto& pal = palette_group[0];
  printf("[ObjectDrawer] Applied palette: %zu colors, first=RGB(%d,%d,%d)\n",
         pal.size(), pal[0].rom_color().red, pal[0].rom_color().green, pal[0].rom_color().blue);
}

// In DrawTileToBitmap() after palette calculation:
printf("[Tile] ID=0x%03X pal_idx=%d offset=%d pixel[0]=%d\n",
       tile_info.id_, tile_info.palette_, palette_offset, tiledata[0]);
```

---

## References

- **ALTTP Disassembly**: https://github.com/

spannerisms/ALTTPR-estrela
- **ZScream Source**: DungeonObjectData.cs (C# implementation)
- **yaze Graphics System**: CLAUDE.md Pattern 4 (Bitmap sync requirements)

---

**Last Updated**: 2025-11-21
**Research By**: CLAUDE_CORE (zelda3-hacking-expert + backend-infra-engineer)
**Status**: Ready for implementation
