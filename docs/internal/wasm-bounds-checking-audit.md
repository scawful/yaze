# WASM Bounds Checking Audit

This document tracks potentially unsafe array accesses that can cause "index out of bounds" RuntimeErrors in WASM builds with assertions enabled.

## Background

WASM builds with `-sASSERTIONS=1` perform runtime bounds checking on all memory accesses. Invalid accesses trigger a `RuntimeError: index out of bounds` that halts the module.

## Analysis Tools

Run the static analysis script to find potentially dangerous patterns:
```bash
./scripts/find-unsafe-array-access.sh
```

## Known Fixed Issues

### 1. object_drawer.cc - Tile Rendering (Fixed 2024-11)
**File:** `src/zelda3/dungeon/object_drawer.cc`
**Issue:** `tiledata[src_index]` access without bounds validation
**Fix:** Added `kMaxTileRow = 63` validation before access

### 2. background_buffer.cc - Background Tile Rendering (Fixed 2024-11)
**File:** `src/app/gfx/render/background_buffer.cc`
**Issue:** Same pattern as object_drawer.cc
**Fix:** Added same bounds checking

### 3. arena.h - Graphics Sheet Accessors (Fixed 2024-11)
**File:** `src/app/gfx/resource/arena.h`
**Issue:** `gfx_sheets_[i]` accessed without bounds check
**Fix:** Added bounds validation returning empty/null for invalid indices

### 4. bitmap.cc - Palette Application (Fixed 2024-11)
**File:** `src/app/gfx/core/bitmap.cc`
**Issue:** `palette[i]` accessed without checking palette size
**Fix:** Added bounds check against `sdl_palette->ncolors`

### 5. tilemap.cc - FetchTileDataFromGraphicsBuffer (Fixed 2024-11)
**File:** `src/app/gfx/render/tilemap.cc`
**Issue:** `data[src_index]` accessed without checking data vector size
**Fix:** Added `src_index >= 0 && src_index < data_size` validation

### 6. overworld.h - Map Accessors (Fixed 2025-11-26)
**File:** `src/zelda3/overworld/overworld.h`
**Issue:** `overworld_map(int i)` and `mutable_overworld_map(int i)` accessed vector without bounds check
**Fix:** Added bounds validation returning nullptr for invalid indices

### 7. overworld.h - Sprite Accessors (Fixed 2025-11-26)
**File:** `src/zelda3/overworld/overworld.h`
**Issue:** `sprites(int state)` accessed array without validating state (0-2)
**Fix:** Added bounds check returning empty vector for invalid state

### 8. overworld.h - Current Map Accessors (Fixed 2025-11-26)
**File:** `src/zelda3/overworld/overworld.h`
**Issue:** `current_graphics()`, `current_area_palette()`, etc. accessed `overworld_maps_[current_map_]` without validating `current_map_`
**Fix:** Added `is_current_map_valid()` helper and validation in all accessors

### 9. snes_palette.h - PaletteGroup Negative Index (Fixed 2025-11-26)
**File:** `src/app/gfx/types/snes_palette.h`
**Issue:** `operator[]` only checked upper bound, not negative indices
**Fix:** Added `i < 0` check to bounds validation

### 10. room.cc - LoadAnimatedGraphics sizeof vs size() (Fixed 2025-11-26)
**File:** `src/zelda3/dungeon/room.cc`
**Issue:** Used `sizeof(current_gfx16_)` instead of `.size()` for bounds checking
**Fix:** Changed to use `.size()` for clarity and maintainability

## Patterns Requiring Caution

### Critical Risk Patterns

These patterns directly access memory buffers and are most likely to crash:

1. **`tiledata[index]`** - Graphics buffer access
   - Buffer size: 0x10000 (65536 bytes)
   - Max tile row: 63 (rows 0-63)
   - Stride: 128 bytes per row
   - **Validation:** `tile_row <= 63` before computing `src_index`

2. **`buffer_[index]`** - Tile buffer access
   - Check: `index < buffer_.size()`

3. **`canvas[index]`** - Pixel canvas access
   - Check: `index < width * height`

4. **`.data()[index]`** - Vector data access
   - Check: `index < vector.size()`

### High Risk Patterns

These access ROM data or game structures that may contain corrupt values:

1. **`rom.data()[offset]`** - ROM data access
   - Check: `offset < rom.size()`

2. **`palette[index]`** - Palette color access
   - Check: `index < palette.size()`

3. **`overworld_maps_[i]`** - Map access
   - Check: `i < 160` (or appropriate constant)

4. **`rooms_[i]`** - Room access
   - Check: `i < 296`

### Medium Risk Patterns

Usually safe but worth verifying:

1. **`gfx_sheet(i)`** - Already has bounds check returning empty Bitmap
2. **`vram[index]`, `cgram[index]`, `oam[index]`** - Usually masked with `& 0x7fff`, `& 0xff`

## Bounds Checking Template

```cpp
// For tile data access
constexpr int kGfxBufferSize = 0x10000;
constexpr int kMaxTileRow = 63;

int tile_row = tile_id / 16;
if (tile_row > kMaxTileRow) {
  return;  // Skip invalid tile
}

int src_index = (src_row * 128) + src_col + tile_base_x + tile_base_y;
if (src_index < 0 || src_index >= kGfxBufferSize) {
  continue;  // Skip invalid access
}

// For destination canvas
int dest_index = dest_y * width + dest_x;
if (dest_index < 0 || dest_index >= static_cast<int>(canvas.size())) {
  continue;  // Skip invalid access
}
```

## Testing

1. Run WASM build with assertions: `-sASSERTIONS=1`
2. Load ROMs with varying data quality
3. Open each editor and interact with all features
4. Monitor browser console for `RuntimeError: index out of bounds`

## Error Reporting

The crash reporter (`src/web/core/crash_reporter.js`) provides:
- Stack trace parsing to identify WASM function indices
- Auto-diagnosis of known error patterns
- Problems panel for non-fatal errors
- Console log history capture

## Recovery

The recovery system (`src/web/core/wasm_recovery.js`) provides:
- Automatic crash detection
- Non-blocking recovery overlay
- Module reinitialization (up to 3 attempts)
- ROM data preservation via IndexedDB
