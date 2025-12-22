# Object Rendering Fixes - Action Plan

**Date:** 2025-11-26
**Based on:** ZScream comparison analysis
**Status:** Ready for implementation

---

## Problem Summary

After fixing the layout loading issue (walls now render correctly), some dungeon objects still render incorrectly. Analysis of ZScream's implementation reveals yaze loads incorrect tile counts per object.

**Root Cause:** yaze hardcodes 8 tiles per object, while ZScream loads 1-242 tiles based on object type.

---

## Fix 1: Object Tile Count Lookup Table (CRITICAL)

### Files to Modify
- `src/zelda3/dungeon/object_parser.h`
- `src/zelda3/dungeon/object_parser.cc`

### Implementation

**Step 1:** Add tile count lookup table in `object_parser.h`:

```cpp
// Object-specific tile counts (from ZScream's RoomObjectTileLister)
// These specify how many 16-bit tile words to read from ROM per object
static const std::unordered_map<int16_t, int> kObjectTileCounts = {
    // Subtype 1 objects (0x000-0x0FF)
    {0x000, 4},   {0x001, 8},   {0x002, 8},   {0x003, 8},
    {0x004, 8},   {0x005, 8},   {0x006, 8},   {0x007, 8},
    {0x008, 4},   {0x009, 5},   {0x00A, 5},   {0x00B, 5},
    {0x00C, 5},   {0x00D, 5},   {0x00E, 5},   {0x00F, 5},
    {0x010, 5},   {0x011, 5},   {0x012, 5},   {0x013, 5},
    {0x014, 5},   {0x015, 5},   {0x016, 5},   {0x017, 5},
    {0x018, 5},   {0x019, 5},   {0x01A, 5},   {0x01B, 5},
    {0x01C, 5},   {0x01D, 5},   {0x01E, 5},   {0x01F, 5},
    {0x020, 5},   {0x021, 9},   {0x022, 3},   {0x023, 3},
    {0x024, 3},   {0x025, 3},   {0x026, 3},   {0x027, 3},
    {0x028, 3},   {0x029, 3},   {0x02A, 3},   {0x02B, 3},
    {0x02C, 3},   {0x02D, 3},   {0x02E, 3},   {0x02F, 6},
    {0x030, 6},   {0x031, 0},   {0x032, 0},   {0x033, 16},
    {0x034, 1},   {0x035, 1},   {0x036, 16},  {0x037, 16},
    {0x038, 6},   {0x039, 8},   {0x03A, 12},  {0x03B, 12},
    {0x03C, 4},   {0x03D, 8},   {0x03E, 4},   {0x03F, 3},
    {0x040, 3},   {0x041, 3},   {0x042, 3},   {0x043, 3},
    {0x044, 3},   {0x045, 3},   {0x046, 3},   {0x047, 0},
    {0x048, 0},   {0x049, 8},   {0x04A, 8},   {0x04B, 4},
    {0x04C, 9},   {0x04D, 16},  {0x04E, 16},  {0x04F, 16},
    {0x050, 1},   {0x051, 18},  {0x052, 18},  {0x053, 4},
    {0x054, 0},   {0x055, 8},   {0x056, 8},   {0x057, 0},
    {0x058, 0},   {0x059, 0},   {0x05A, 0},   {0x05B, 18},
    {0x05C, 18},  {0x05D, 15},  {0x05E, 4},   {0x05F, 3},
    {0x060, 4},   {0x061, 8},   {0x062, 8},   {0x063, 8},
    {0x064, 8},   {0x065, 8},   {0x066, 8},   {0x067, 4},
    {0x068, 4},   {0x069, 3},   {0x06A, 1},   {0x06B, 1},
    {0x06C, 6},   {0x06D, 6},   {0x06E, 0},   {0x06F, 0},
    {0x070, 16},  {0x071, 1},   {0x072, 0},   {0x073, 16},
    {0x074, 16},  {0x075, 8},   {0x076, 16},  {0x077, 16},
    {0x078, 4},   {0x079, 1},   {0x07A, 1},   {0x07B, 4},
    {0x07C, 1},   {0x07D, 4},   {0x07E, 0},   {0x07F, 8},
    {0x080, 8},   {0x081, 12},  {0x082, 12},  {0x083, 12},
    {0x084, 12},  {0x085, 18},  {0x086, 18},  {0x087, 8},
    {0x088, 12},  {0x089, 4},   {0x08A, 3},   {0x08B, 3},
    {0x08C, 3},   {0x08D, 1},   {0x08E, 1},   {0x08F, 6},
    {0x090, 8},   {0x091, 8},   {0x092, 4},   {0x093, 4},
    {0x094, 16},  {0x095, 4},   {0x096, 4},   {0x097, 0},
    {0x098, 0},   {0x099, 0},   {0x09A, 0},   {0x09B, 0},
    {0x09C, 0},   {0x09D, 0},   {0x09E, 0},   {0x09F, 0},
    {0x0A0, 1},   {0x0A1, 1},   {0x0A2, 1},   {0x0A3, 1},
    {0x0A4, 24},  {0x0A5, 1},   {0x0A6, 1},   {0x0A7, 1},
    {0x0A8, 1},   {0x0A9, 1},   {0x0AA, 1},   {0x0AB, 1},
    {0x0AC, 1},   {0x0AD, 0},   {0x0AE, 0},   {0x0AF, 0},
    {0x0B0, 1},   {0x0B1, 1},   {0x0B2, 16},  {0x0B3, 3},
    {0x0B4, 3},   {0x0B5, 8},   {0x0B6, 8},   {0x0B7, 8},
    {0x0B8, 4},   {0x0B9, 4},   {0x0BA, 16},  {0x0BB, 4},
    {0x0BC, 4},   {0x0BD, 4},   {0x0BE, 0},   {0x0BF, 0},
    {0x0C0, 1},   {0x0C1, 68},  {0x0C2, 1},   {0x0C3, 1},
    {0x0C4, 8},   {0x0C5, 8},   {0x0C6, 8},   {0x0C7, 8},
    {0x0C8, 8},   {0x0C9, 8},   {0x0CA, 8},   {0x0CB, 0},
    {0x0CC, 0},   {0x0CD, 28},  {0x0CE, 28},  {0x0CF, 0},
    {0x0D0, 0},   {0x0D1, 8},   {0x0D2, 8},   {0x0D3, 0},
    {0x0D4, 0},   {0x0D5, 0},   {0x0D6, 0},   {0x0D7, 1},
    {0x0D8, 8},   {0x0D9, 8},   {0x0DA, 8},   {0x0DB, 8},
    {0x0DC, 21},  {0x0DD, 16},  {0x0DE, 4},   {0x0DF, 8},
    {0x0E0, 8},   {0x0E1, 8},   {0x0E2, 8},   {0x0E3, 8},
    {0x0E4, 8},   {0x0E5, 8},   {0x0E6, 8},   {0x0E7, 8},
    {0x0E8, 8},   {0x0E9, 0},   {0x0EA, 0},   {0x0EB, 0},
    {0x0EC, 0},   {0x0ED, 0},   {0x0EE, 0},   {0x0EF, 0},
    {0x0F0, 0},   {0x0F1, 0},   {0x0F2, 0},   {0x0F3, 0},
    {0x0F4, 0},   {0x0F5, 0},   {0x0F6, 0},   {0x0F7, 0},

    // Subtype 2 objects (0x100-0x13F) - all 16 tiles for corners
    {0x100, 16},  {0x101, 16},  {0x102, 16},  {0x103, 16},
    {0x104, 16},  {0x105, 16},  {0x106, 16},  {0x107, 16},
    {0x108, 16},  {0x109, 16},  {0x10A, 16},  {0x10B, 16},
    {0x10C, 16},  {0x10D, 16},  {0x10E, 16},  {0x10F, 16},
    {0x110, 12},  {0x111, 12},  {0x112, 12},  {0x113, 12},
    {0x114, 12},  {0x115, 12},  {0x116, 12},  {0x117, 12},
    {0x118, 4},   {0x119, 4},   {0x11A, 4},   {0x11B, 4},
    {0x11C, 16},  {0x11D, 6},   {0x11E, 4},   {0x11F, 4},
    {0x120, 4},   {0x121, 6},   {0x122, 20},  {0x123, 12},
    {0x124, 16},  {0x125, 16},  {0x126, 6},   {0x127, 4},
    {0x128, 20},  {0x129, 16},  {0x12A, 8},   {0x12B, 4},
    {0x12C, 18},  {0x12D, 16},  {0x12E, 16},  {0x12F, 16},
    {0x130, 16},  {0x131, 16},  {0x132, 16},  {0x133, 16},
    {0x134, 4},   {0x135, 8},   {0x136, 8},   {0x137, 40},
    {0x138, 12},  {0x139, 12},  {0x13A, 12},  {0x13B, 12},
    {0x13C, 24},  {0x13D, 12},  {0x13E, 18},  {0x13F, 56},

    // Subtype 3 objects (0x200-0x27F)
    {0x200, 12},  {0x201, 20},  {0x202, 28},  {0x203, 1},
    {0x204, 1},   {0x205, 1},   {0x206, 1},   {0x207, 1},
    {0x208, 1},   {0x209, 1},   {0x20A, 1},   {0x20B, 1},
    {0x20C, 1},   {0x20D, 6},   {0x20E, 1},   {0x20F, 1},
    {0x210, 4},   {0x211, 4},   {0x212, 4},   {0x213, 4},
    {0x214, 12},  {0x215, 80},  {0x216, 4},   {0x217, 6},
    {0x218, 4},   {0x219, 4},   {0x21A, 4},   {0x21B, 16},
    {0x21C, 16},  {0x21D, 16},  {0x21E, 16},  {0x21F, 16},
    {0x220, 16},  {0x221, 16},  {0x222, 4},   {0x223, 4},
    {0x224, 4},   {0x225, 4},   {0x226, 16},  {0x227, 16},
    {0x228, 16},  {0x229, 16},  {0x22A, 16},  {0x22B, 4},
    {0x22C, 16},  {0x22D, 84},  {0x22E, 127}, {0x22F, 4},
    {0x230, 4},   {0x231, 12},  {0x232, 12},  {0x233, 16},
    {0x234, 6},   {0x235, 6},   {0x236, 18},  {0x237, 18},
    {0x238, 18},  {0x239, 18},  {0x23A, 24},  {0x23B, 24},
    {0x23C, 24},  {0x23D, 24},  {0x23E, 4},   {0x23F, 4},
    {0x240, 4},   {0x241, 4},   {0x242, 4},   {0x243, 4},
    {0x244, 4},   {0x245, 4},   {0x246, 4},   {0x247, 16},
    {0x248, 16},  {0x249, 4},   {0x24A, 4},   {0x24B, 24},
    {0x24C, 48},  {0x24D, 18},  {0x24E, 12},  {0x24F, 4},
    {0x250, 4},   {0x251, 4},   {0x252, 4},   {0x253, 4},
    {0x254, 26},  {0x255, 16},  {0x256, 4},   {0x257, 4},
    {0x258, 6},   {0x259, 4},   {0x25A, 8},   {0x25B, 32},
    {0x25C, 24},  {0x25D, 18},  {0x25E, 4},   {0x25F, 4},
    {0x260, 18},  {0x261, 18},  {0x262, 242}, {0x263, 4},
    {0x264, 4},   {0x265, 4},   {0x266, 16},  {0x267, 12},
    {0x268, 12},  {0x269, 12},  {0x26A, 12},  {0x26B, 16},
    {0x26C, 12},  {0x26D, 12},  {0x26E, 12},  {0x26F, 12},
    {0x270, 32},  {0x271, 64},  {0x272, 80},  {0x273, 1},
    {0x274, 64},  {0x275, 4},   {0x276, 64},  {0x277, 24},
    {0x278, 32},  {0x279, 12},  {0x27A, 16},  {0x27B, 8},
    {0x27C, 4},   {0x27D, 4},   {0x27E, 4},
};

// Helper function to get tile count for an object
inline int GetObjectTileCount(int16_t object_id) {
    auto it = kObjectTileCounts.find(object_id);
    return (it != kObjectTileCounts.end()) ? it->second : 8;  // Default 8 if not found
}
```

**Step 2:** Update `object_parser.cc` to use the lookup table:

```cpp
// Replace lines 141, 160, 178 with:
int tile_count = GetObjectTileCount(object_id);
return ReadTileData(tile_data_ptr, tile_count);
```

**Before:**
```cpp
absl::StatusOr<std::vector<gfx::TileInfo>> ObjectParser::ParseSubtype1(
    int16_t object_id) {
  // ...
  return ReadTileData(tile_data_ptr, 8);  // ❌ WRONG
}
```

**After:**
```cpp
absl::StatusOr<std::vector<gfx::TileInfo>> ObjectParser::ParseSubtype1(
    int16_t object_id) {
  // ...
  int tile_count = GetObjectTileCount(object_id);
  return ReadTileData(tile_data_ptr, tile_count);  // ✅ CORRECT
}
```

### Testing

Test with these specific objects after fix:

| Object | Tile Count | What It Is | Expected Result |
|--------|------------|-----------|-----------------|
| 0x033  | 16         | Carpet    | Full 4×4 pattern visible |
| 0x0C1  | 68         | Chest platform (tall) | Complete platform structure |
| 0x215  | 80         | Kholdstare prison cell | Full prison bars |
| 0x22D  | 84         | Agahnim's altar | Symmetrical 14-tile-wide altar |
| 0x22E  | 127        | Agahnim's boss room | Complete room structure |
| 0x262  | 242        | Fortune teller room | Full room layout |

---

## Fix 2: Tile Transformation Support (MEDIUM PRIORITY)

### Problem

Objects with horizontal/vertical mirroring (like Agahnim's altar) render incorrectly because tile transformations aren't applied.

### Files to Modify
- `src/zelda3/dungeon/object_drawer.h`
- `src/zelda3/dungeon/object_drawer.cc`

### Implementation

**Update `WriteTile8()` signature:**

```cpp
// In object_drawer.h
void WriteTile8(gfx::BackgroundBuffer& bg, uint8_t x_grid, uint8_t y_grid,
                const gfx::TileInfo& tile_info,
                bool h_flip = false, bool v_flip = false);

// In object_drawer.cc
void ObjectDrawer::WriteTile8(gfx::BackgroundBuffer& bg, uint8_t x_grid,
                               uint8_t y_grid, const gfx::TileInfo& tile_info,
                               bool h_flip, bool v_flip) {
  // ... existing code ...

  for (int py = 0; py < 8; py++) {
    for (int px = 0; px < 8; px++) {
      // Apply transformations
      int src_x = h_flip ? (7 - px) : px;
      int src_y = v_flip ? (7 - py) : py;

      int src_index = (src_y * 128) + src_x + tile_base_x + tile_base_y;

      // ... rest of pixel drawing code ...
    }
  }
}
```

**Then update draw routines to use transformations from TileInfo:**

```cpp
WriteTile8(bg, obj.x_ + (s * 2), obj.y_, tiles[0],
           tiles[0].horizontal_mirror_, tiles[0].vertical_mirror_);
```

---

## Fix 3: Update Draw Routines (OPTIONAL - For Complex Objects)

Some objects may need custom draw routines beyond pattern-based drawing. Consider implementing for:

- **0x22D (Agahnim's Altar)**: Symmetrical mirrored structure
- **0x22E (Agahnim's Boss Room)**: Complex multi-tile layout
- **0x262 (Fortune Teller Room)**: Extremely large 242-tile object

These could use a `DrawInfo`-based approach like ZScream:

```cpp
struct DrawInfo {
    int tile_index;
    int x_offset;  // In pixels
    int y_offset;  // In pixels
    bool h_flip;
    bool v_flip;
};

void DrawFromInstructions(const RoomObject& obj, gfx::BackgroundBuffer& bg,
                          std::span<const gfx::TileInfo> tiles,
                          const std::vector<DrawInfo>& instructions);
```

---

## Expected Outcomes

After implementing Fix 1:
- ✅ Carpets (0x33) render with full 4×4 tile pattern
- ✅ Chest platforms (0xC1) render with complete structure
- ✅ Large objects (0x215, 0x22D, 0x22E) appear (though possibly with wrong orientation)

After implementing Fix 2:
- ✅ Symmetrical objects render correctly with mirroring
- ✅ Agahnim's altar/room display properly

---

## Verification Steps

1. **Build and test:**
   ```bash
   cmake --build build --target yaze -j4
   ./build/bin/Debug/yaze.app/Contents/MacOS/yaze --rom_file=zelda3.sfc --editor=Dungeon
   ```

2. **Test rooms with specific objects:**
   - Room 0x0C (Eastern Palace): Basic walls and carpets
   - Room 0x20 (Agahnim's Tower): Agahnim's altar (0x22D)
   - Room 0x00 (Sanctuary): Basic objects

3. **Compare with ZScream:**
   - Open same room in ZScream
   - Verify tile-by-tile rendering matches

4. **Log verification:**
   ```cpp
   printf("[ObjectParser] Object %04X: Loading %d tiles\n", object_id, tile_count);
   ```

---

## Notes for Implementation

- The tile count lookup table comes directly from ZScream's `RoomObjectTileLister.cs:23-534`
- Each entry represents the number of **16-bit tile words** (2 bytes each) to read from ROM
- Objects with `0` tile count are empty placeholders or special objects (moving walls, etc.)
- Tile transformations (h_flip, v_flip) are stored in TileInfo from ROM data (bits in tile word)
- Some objects (0x0CD, 0x0CE) load tiles from multiple ROM addresses (not yet supported)

---

## References

- **ZScream Source:** `/Users/scawful/Code/ZScreamDungeon/ZeldaFullEditor/Data/Underworld/RoomObjectTileLister.cs`
- **Analysis Document:** `/Users/scawful/Code/yaze/docs/internal/zscream-comparison-object-rendering.md`
- **Rendering Analysis:** `/Users/scawful/Code/yaze/docs/internal/dungeon-rendering-analysis.md`
