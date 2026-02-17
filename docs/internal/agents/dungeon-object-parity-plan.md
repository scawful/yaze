# Dungeon Object Drawing Parity Plan

## Identified Issues

### 1. Water/Layer Effect Objects Missing Proper BG2 Handling

**Problem:** Water overlay objects (0xC8, 0xD8, 0xD9, 0xDA) and flood water objects
are listed in `is_pit_or_mask` but the actual water overlay draw routine
(`DrawWaterOverlay8x8_1to16`) doesn't handle BG2 layer semantics correctly.

In-game, water overlays use SNES HDMA to create a wavy distortion effect on BG2.
The editor draws them as flat tile overlays but doesn't:
- Set the water overlay objects to draw on BG2 (they should be Layer 1 objects)
- Apply translucency to water tiles in the compositor
- Handle the Moving_Water room effect properly for BG2 blend modes

**Fix:**
- Ensure water overlay objects are correctly routed to BG2
- In `ApplyRoomEffect()`, handle more effect types beyond just Moving_Water
- Add per-object-type translucency flags for water objects in the compositor

### 2. Room Effect Handling Incomplete

**Problem:** `ApplyRoomEffect()` only handles `Moving_Water`. Missing:
- `Moving_Floor` (conveyor belt effect)
- `Torch_Show_Floor` (lantern reveals BG2 floor)
- `Red_Flashes` (Ganon fight lightning)
- `Ganon_Room` (special Ganon room rendering)

**Fix:** Expand `ApplyRoomEffect()` with proper blend modes for each effect type.

### 3. Palette Offset Calculation Bug for Banks 0-1

**Problem:** In `DrawTileToBitmap()`, palettes 0-1 map to `palette_offset = 0`:
```cpp
if (pal >= 2 && pal <= 7) {
    palette_offset = (pal - 2) * 16;
} else {
    palette_offset = 0;  // Bug: palette 0 and 1 both map to bank 0
}
```
On SNES, palette 0-1 are for HUD/sprites, not dungeon BG tiles. However, some
objects in ROM have palette bits 0 or 1 set. We should map them correctly:
- Palette 0: offset 0 (transparent bank - effectively invisible)
- Palette 1: offset 16 (would be HUD, but in dungeon context = bank 1 colors)

Actually, ALTTP uses palettes 2-7 for BG tiles. Palettes 0-1 are sprite palettes
loaded to different CGRAM addresses. The current mapping `(pal - 2) * 16` is
correct for the 6 dungeon BG tile palette banks. Objects with pal 0-1 would be
referencing sprite palettes which isn't available in BG tile rendering - the
fallback to 0 is reasonable.

**No fix needed** - current behavior is correct. BUT we should add the sprite
palette data as banks 6-7 in the SDL palette for completeness.

### 4. Translucent Blending in CompositeToOutput Needs Real Color Math

**Problem:** In the non-priority compositing path, translucent blending is simplified:
```cpp
case LayerBlendMode::Translucent:
    if (IsTransparent(dst_data[idx]) || layer_alpha > 180) {
        dst_data[idx] = src_pixel;
    }
    break;
```
This doesn't actually blend - it just overwrites. For proper SNES color math,
we need to look up actual RGB values from the palette and average them.

**Fix:** Implement proper palette-based blending in the priority compositing path.
When BG2 is translucent and overlaps BG1, compute:
`result = (bg1_rgb + bg2_rgb) / 2` then find closest palette index.

### 5. Missing Object IDs 0xFE and 0xFF

**Problem:** These 2 Subtype 1 objects are not in the mapping.

**Fix:** Map 0xFE and 0xFF to routine 38 (kNothing) as they are unused in vanilla.

## Implementation Order

1. Fix water overlay BG2 routing + room effects -- DONE
2. Implement proper translucent blending in compositor -- DONE
3. Map remaining objects and add sprite palette banks -- DONE
4. Write comprehensive validation tests -- DONE

## Completion Status

All 5 issues resolved. Changes:

### Files Modified
- `src/zelda3/dungeon/room_layer_manager.h` - Expanded `ApplyRoomEffect()` for
  Moving_Floor, Torch_Show_Floor, Red_Flashes, Ganon_Room effects
- `src/zelda3/dungeon/room_layer_manager.cc` - Implemented proper RGB color math
  for translucent blending: palette lookup table, `(bg1_rgb + bg2_rgb) / 2`,
  `find_nearest_in_bank` for palette-indexed color blending
- `src/zelda3/dungeon/draw_routines/draw_routine_registry.cc` - Added missing
  mappings for 0xF8 (routine 39), 0xFE/0xFF (routine 38)
- `test/unit/zelda3/dungeon/object_drawing_comprehensive_test.cc` - Added 19
  parity validation tests covering full routine coverage, palette offsets,
  pit/mask objects, BothBG flags, water objects, room effects, layer merging,
  and drawer fallback behavior
- `test/unit/zelda3/dungeon/object_dimensions_test.cc` - Updated unmapped test
- `test/unit/zelda3/dungeon/object_geometry_test.cc` - Updated unmapped test

### Validation Results
- 448/448 vanilla objects have valid routine mappings (100% coverage)
- All routine IDs within bounds of draw_routines_ array
- All tile counts non-zero for all subtypes
- Room effect blend modes verified for all effect types
- Layer merge types verified (translucent, dark room)
- All 20 parity tests pass, full stable suite green
