# ZScream vs yaze: Dungeon Object Rendering Comparison

**Date:** 2025-11-26
**Author:** Claude (Sonnet 4.5)
**Purpose:** Identify discrepancies between ZScream's proven object rendering and yaze's implementation

---

## Executive Summary

This analysis compares yaze's dungeon object rendering system with ZScream's reference implementation. The goal is to identify bugs causing incorrect object rendering, particularly issues observed where "some objects don't look right" despite walls rendering correctly after the LoadLayout fix.

### Key Findings

1. **Tile Count Mismatch**: yaze loads only 4-8 tiles per object, while ZScream loads variable counts (1-242 tiles) based on object type
2. **Drawing Method Difference**: ZScream uses tile-by-tile DrawInfo instructions, yaze uses pattern-based draw routines
3. **Graphics Sheet Access**: Different approaches to accessing tile graphics data
4. **Palette Handling**: Both use similar palette offset calculations (correct in yaze)

---

## 1. Tile Loading Architecture

### ZScream's Approach (Reference Implementation)

**File:** `/ZScreamDungeon/ZeldaFullEditor/Data/Underworld/RoomObjectTileLister.cs`

```csharp
// Initialization specifies exact tile counts per object
AutoFindTiles(0x000, 4);   // Object 0x00: 4 tiles
AutoFindTiles(0x001, 8);   // Object 0x01: 8 tiles
AutoFindTiles(0x033, 16);  // Object 0x33: 16 tiles
AutoFindTiles(0x0C1, 68);  // Object 0xC1: 68 tiles (Chest platform)
AutoFindTiles(0x215, 80);  // Object 0x215: 80 tiles (Kholdstare prison)
AutoFindTiles(0x262, 242); // Object 0x262: 242 tiles (Fortune teller room!)
SetTilesFromKnownOffset(0x22D, 0x1B4A, 84);  // Agahnim's altar: 84 tiles
SetTilesFromKnownOffset(0x22E, 0x1BF2, 127); // Agahnim's boss room: 127 tiles
```

**Key Method:**
```csharp
public static TilesList CreateNewDefinition(ZScreamer ZS, int position, int count)
{
    Tile[] list = new Tile[count];
    for (int i = 0; i < count; i++)
    {
        list[i] = new Tile(ZS.ROM.Read16(position + i * 2)); // 2 bytes per tile
    }
    return new TilesList(list);
}
```

**Critical Insight:** ZScream reads **exactly 2 bytes per tile** (one 16-bit word per tile) and loads **object-specific counts** (not fixed 8 tiles for all).

### yaze's Approach (Current Implementation)

**File:** `/yaze/src/zelda3/dungeon/object_parser.cc`

```cpp
absl::StatusOr<std::vector<gfx::TileInfo>> ObjectParser::ParseSubtype1(
    int16_t object_id) {
  int index = object_id & 0xFF;
  int tile_ptr = kRoomObjectSubtype1 + (index * 2);

  uint8_t low = rom_->data()[tile_ptr];
  uint8_t high = rom_->data()[tile_ptr + 1];
  int tile_data_ptr = kRoomObjectTileAddress + ((high << 8) | low);

  // Read 8 tiles (most subtype 1 objects use 8 tiles) ❌
  return ReadTileData(tile_data_ptr, 8);  // HARDCODED to 8!
}

absl::StatusOr<std::vector<gfx::TileInfo>> ObjectParser::ReadTileData(
    int address, int tile_count) {
  for (int i = 0; i < tile_count; i++) {
    int tile_offset = address + (i * 2);  // ✅ Correct: 2 bytes per tile
    uint16_t tile_word =
        rom_->data()[tile_offset] | (rom_->data()[tile_offset + 1] << 8);
    tiles.push_back(gfx::WordToTileInfo(tile_word));
  }
  return tiles;
}
```

**Problems Identified:**
1. ❌ **Hardcoded tile count**: Always reads 8 tiles, regardless of object type
2. ❌ **Missing object-specific counts**: No lookup table for actual tile requirements
3. ✅ **Correct byte stride**: 2 bytes per tile (matches ZScream)
4. ✅ **Correct pointer resolution**: Matches ZScream's tile address calculation

---

## 2. Object Drawing Methods

### ZScream's Drawing Architecture

**File:** `/ZScreamDungeon/ZeldaFullEditor/Data/Types/DungeonObjectDraw.cs`

ZScream uses explicit `DrawInfo` instructions that specify:
- Which tile index to draw
- X/Y pixel offset from object origin
- Whether to flip horizontally/vertically

**Example: Agahnim's Altar (Object 0x22D)**
```csharp
public static void RoomDraw_AgahnimsAltar(ZScreamer ZS, RoomObject obj)
{
    int tid = 0;
    for (int y = 0; y < 14 * 8; y += 8)
    {
        DrawTiles(ZS, obj, false,
            new DrawInfo(tid, 0, y, hflip: false),
            new DrawInfo(tid + 14, 8, y, hflip: false),
            new DrawInfo(tid + 14, 16, y, hflip: false),
            new DrawInfo(tid + 28, 24, y, hflip: false),
            new DrawInfo(tid + 42, 32, y, hflip: false),
            new DrawInfo(tid + 56, 40, y, hflip: false),
            new DrawInfo(tid + 70, 48, y, hflip: false),

            new DrawInfo(tid + 70, 56, y, hflip: true),
            new DrawInfo(tid + 56, 64, y, hflip: true),
            new DrawInfo(tid + 42, 72, y, hflip: true),
            new DrawInfo(tid + 28, 80, y, hflip: true),
            new DrawInfo(tid + 14, 88, y, hflip: true),
            new DrawInfo(tid + 14, 96, y, hflip: true),
            new DrawInfo(tid, 104, y, hflip: true)
        );
        tid++;
    }
}
```

This creates a **14-tile-high, 14-tile-wide symmetrical structure** using 84 total tile placements with mirroring.

**Core Drawing Method:**
```csharp
public static unsafe void DrawTiles(ZScreamer ZS, RoomObject obj, bool allbg,
    params DrawInfo[] instructions)
{
    foreach (DrawInfo d in instructions)
    {
        if (obj.Width < d.XOff + 8) obj.Width = d.XOff + 8;
        if (obj.Height < d.YOff + 8) obj.Height = d.YOff + 8;

        int tm = (d.XOff / 8) + obj.GridX + ((obj.GridY + (d.YOff / 8)) * 64);

        if (tm < Constants.TilesPerUnderworldRoom && tm >= 0)
        {
            ushort td = obj.Tiles[d.TileIndex].GetModifiedUnsignedShort(
                hflip: d.HFlip, vflip: d.VFlip);

            ZS.GFXManager.tilesBg1Buffer[tm] = td;  // Direct tile buffer write
        }
    }
}
```

**Key Points:**
- ✅ Uses tile index (`d.TileIndex`) to access specific tiles from the object's tile array
- ✅ Calculates linear buffer index: `(x_tile) + (y_tile * 64)`
- ✅ Applies tile transformations (hflip, vflip) before writing
- ✅ Dynamically updates object bounds based on drawn tiles

### yaze's Drawing Architecture

**File:** `/yaze/src/zelda3/dungeon/object_drawer.cc`

yaze uses **pattern-based draw routines** that assume tile arrangements:

```cpp
void ObjectDrawer::DrawRightwards2x2_1to15or32(
    const RoomObject& obj, gfx::BackgroundBuffer& bg,
    std::span<const gfx::TileInfo> tiles) {
  int size = obj.size_;
  if (size == 0) size = 32;  // Special case for object 0x00

  for (int s = 0; s < size; s++) {
    if (tiles.size() >= 4) {
      WriteTile8(bg, obj.x_ + (s * 2), obj.y_, tiles[0]);      // Top-left
      WriteTile8(bg, obj.x_ + (s * 2) + 1, obj.y_, tiles[1]);  // Top-right
      WriteTile8(bg, obj.x_ + (s * 2), obj.y_ + 1, tiles[2]);  // Bottom-left
      WriteTile8(bg, obj.x_ + (s * 2) + 1, obj.y_ + 1, tiles[3]); // Bottom-right
    }
  }
}
```

**Problems:**
- ❌ **Assumes 2x2 pattern works for all size values** (but ZScream uses explicit tile indices)
- ❌ **Only uses first 4 tiles** (`tiles[0-3]`) even if more tiles are loaded
- ❌ **No tile transformation support** (hflip, vflip not implemented in draw routines)
- ⚠️ **Pattern might not match actual ROM data** for complex objects

**Comparison:**

| Feature | ZScream | yaze |
|---------|---------|------|
| Drawing Style | Explicit tile indices + offsets | Pattern-based (2x2, 2x4, etc.) |
| Tile Selection | `obj.Tiles[d.TileIndex]` | `tiles[0..3]` |
| Tile Transforms | ✅ hflip, vflip per tile | ❌ Not implemented |
| Object Bounds | ✅ Dynamic, updated per tile | ❌ Fixed by pattern |
| Large Objects | ✅ 84+ tile instructions | ❌ Limited by pattern size |

---

## 3. Graphics Sheet Access

### ZScream's Graphics Manager

```csharp
// ZScream accesses graphics via GFXManager
byte* ptr = (byte*) ZS.GFXManager.currentgfx16Ptr.ToPointer();
byte* alltilesData = (byte*) ZS.GFXManager.currentgfx16Ptr.ToPointer();

// For preview rendering:
byte* previewPtr = (byte*) ZS.GFXManager.previewObjectsPtr[pre.ObjectType.FullID].ToPointer();
```

**Structure:**
- `currentgfx16`: Room-specific graphics buffer (16 blocks × 4096 bytes = 64KB)
- Tiles accessed as **4BPP packed data** (2 bytes per pixel row for 8 pixels)

### yaze's Graphics Buffer

```cpp
// File: src/zelda3/dungeon/object_drawer.cc
void ObjectDrawer::WriteTile8(gfx::BackgroundBuffer& bg, uint8_t x_grid,
                               uint8_t y_grid, const gfx::TileInfo& tile_info) {
  int tile_index = tile_info.id_;
  int blockset_index = tile_index / 0x200;
  int sheet_tile_id = tile_index % 0x200;

  // Access from room_gfx_buffer_ (set during Room initialization)
  uint8_t* gfx_sheet = const_cast<uint8_t*>(room_gfx_buffer_) + (blockset_index * 0x1000);

  for (int py = 0; py < 8; py++) {
    for (int px = 0; px < 8; px++) {
      int tile_col = sheet_tile_id % 16;
      int tile_row = sheet_tile_id / 16;
      int tile_base_x = tile_col * 8;
      int tile_base_y = tile_row * 1024;  // 8 rows × 128 bytes
      int src_index = (py * 128) + px + tile_base_x + tile_base_y;

      if (src_index < 0 || src_index >= 0x1000) continue;  // Bounds check

      uint8_t pixel_value = gfx_sheet[src_index];
      if (pixel_value == 0) continue;  // Skip transparent

      uint8_t palette_offset = (tile_info.palette_ & 0x07) * 15;
      uint8_t color_index = (pixel_value - 1) + palette_offset;

      bg.SetPixel(x_pixel, y_pixel, color_index);
    }
  }
}
```

**Structure:**
- `room_gfx_buffer_`: 8BPP linear pixel data (1 byte per pixel, values 0-7)
- Sheet size: 128×32 pixels = 4096 bytes
- Tile layout: 16 columns × 32 rows (512 tiles per sheet)

**Differences:**

| Aspect | ZScream | yaze |
|--------|---------|------|
| Format | 4BPP packed (planar) | 8BPP linear (indexed) |
| Access | Pointer arithmetic on packed data | Array indexing on 8BPP buffer |
| Tile Stride | 16 bytes per tile | 64 bytes per tile (8×8 pixels) |
| Palette Offset | `* 16` (SNES standard) | `* 15` (packed 90-color format) |

---

## 4. Specific Object Rendering Comparison

### Example: Object 0x33 (Carpet)

**ZScream:**
```csharp
AutoFindTiles(0x033, 16);  // Loads 16 tiles from ROM

public static readonly RoomObjectType Object033 = new RoomObjectType(0x033,
    RoomDraw_4x4FloorIn4x4SuperSquare, Horizontal, ...);

public static void RoomDraw_4x4FloorIn4x4SuperSquare(ZScreamer ZS, RoomObject obj)
{
    RoomDraw_Arbtrary4x4in4x4SuperSquares(ZS, obj);
}

private static void RoomDraw_Arbtrary4x4in4x4SuperSquares(ZScreamer ZS, RoomObject obj,
    bool bothbg = false, int sizebonus = 1)
{
    int sizex = 32 * (sizebonus + ((obj.Size >> 2) & 0x03));
    int sizey = 32 * (sizebonus + ((obj.Size) & 0x03));

    for (int x = 0; x < sizex; x += 32)
    {
        for (int y = 0; y < sizey; y += 32)
        {
            DrawTiles(ZS, obj, bothbg,
                new DrawInfo(0, x, y),
                new DrawInfo(1, x + 8, y),
                new DrawInfo(2, x + 16, y),
                new DrawInfo(3, x + 24, y),

                new DrawInfo(4, x, y + 8),
                new DrawInfo(5, x + 8, y + 8),
                new DrawInfo(6, x + 16, y + 8),
                new DrawInfo(7, x + 24, y + 8),

                // ... continues with tiles 0-15 in 4x4 pattern
            );
        }
    }
}
```

**yaze:**
```cpp
// Object 0x33 maps to routine 16 in InitializeDrawRoutines()
object_to_routine_map_[0x33] = 16;

// Routine 16 calls:
void ObjectDrawer::DrawRightwards4x4_1to16(
    const RoomObject& obj, gfx::BackgroundBuffer& bg,
    std::span<const gfx::TileInfo> tiles) {
  int size = obj.size_ & 0x0F;

  for (int s = 0; s < size; s++) {
    if (tiles.size() >= 16) {  // ⚠️ Requires 16 tiles
      for (int ty = 0; ty < 4; ty++) {
        for (int tx = 0; tx < 4; tx++) {
          int tile_idx = ty * 4 + tx;  // 0-15
          WriteTile8(bg, obj.x_ + tx + (s * 4), obj.y_ + ty, tiles[tile_idx]);
        }
      }
    }
  }
}
```

**Analysis:**
- ✅ Both use 16 tiles
- ✅ Both use 4×4 grid pattern
- ⚠️ yaze's pattern assumes linear tile ordering (0-15), but actual ROM data might be different
- ⚠️ ZScream explicitly places each tile with DrawInfo, yaze assumes pattern

---

## 5. Critical Bugs in yaze

### Bug 1: Hardcoded Tile Count (HIGH PRIORITY)

**Location:** `src/zelda3/dungeon/object_parser.cc:141,160,178`

```cpp
// Current code (WRONG):
return ReadTileData(tile_data_ptr, 8);  // Always 8 tiles!

// Should be:
return ReadTileData(tile_data_ptr, GetObjectTileCount(object_id));
```

**Impact:**
- Objects requiring 16+ tiles (carpets, chests, altars) only get first 8 tiles
- Complex objects (Agahnim's room: 127 tiles) rendered with only 8 tiles
- Results in incomplete/incorrect object rendering

**Fix Required:**
Create lookup table based on ZScream's `RoomObjectTileLister.InitializeTilesFromROM()`:

```cpp
static const std::unordered_map<int16_t, int> kObjectTileCounts = {
    {0x000, 4},
    {0x001, 8},
    {0x002, 8},
    {0x033, 16},  // Carpet
    {0x0C1, 68},  // Chest platform
    {0x215, 80},  // Kholdstare prison
    {0x22D, 84},  // Agahnim's altar
    {0x22E, 127}, // Agahnim's boss room
    {0x262, 242}, // Fortune teller room
    // ... complete table from ZScream
};

int ObjectParser::GetObjectTileCount(int16_t object_id) {
    auto it = kObjectTileCounts.find(object_id);
    return (it != kObjectTileCounts.end()) ? it->second : 8;  // Default 8
}
```

### Bug 2: Pattern-Based Drawing Limitations (MEDIUM PRIORITY)

**Location:** `src/zelda3/dungeon/object_drawer.cc`

**Problem:** Pattern-based routines don't match ZScream's explicit tile placement for complex objects.

**Example:** Agahnim's altar uses symmetrical mirroring:
```csharp
// ZScream places tiles explicitly with transformations
new DrawInfo(tid + 70, 56, y, hflip: true),  // Mirror tile 70
new DrawInfo(tid + 56, 64, y, hflip: true),  // Mirror tile 56
```

yaze's `DrawRightwards4x4_1to16()` can't replicate this behavior.

**Fix Options:**
1. **Option A:** Implement ZScream-style `DrawInfo` instructions per object
2. **Option B:** Pre-bake tile transformations into tile arrays during loading
3. **Option C:** Add tile transformation support to draw routines

**Recommended:** Option A (most accurate, matches ZScream)

### Bug 3: Missing Tile Transformation (MEDIUM PRIORITY)

**Location:** `src/zelda3/dungeon/object_drawer.cc:WriteTile8()`

**Current code:**
```cpp
void ObjectDrawer::WriteTile8(gfx::BackgroundBuffer& bg, uint8_t x_grid,
                               uint8_t y_grid, const gfx::TileInfo& tile_info) {
    // No handling of tile_info.horizontal_mirror_ or vertical_mirror_
    // Pixels always drawn in normal orientation
}
```

**ZScream code:**
```csharp
ushort td = obj.Tiles[d.TileIndex].GetModifiedUnsignedShort(
    hflip: d.HFlip, vflip: d.VFlip);
```

**Impact:**
- Symmetrical objects (altars, rooms with mirrors) render incorrectly
- Diagonal walls may have wrong orientation

**Fix Required:**
```cpp
void ObjectDrawer::WriteTile8(gfx::BackgroundBuffer& bg, uint8_t x_grid,
                               uint8_t y_grid, const gfx::TileInfo& tile_info,
                               bool h_flip = false, bool v_flip = false) {
    for (int py = 0; py < 8; py++) {
        for (int px = 0; px < 8; px++) {
            // Apply transformations
            int src_x = h_flip ? (7 - px) : px;
            int src_y = v_flip ? (7 - py) : py;

            // Use src_x, src_y for pixel access
            // ...
        }
    }
}
```

### Bug 4: Graphics Buffer Format Mismatch (RESOLVED)

**Status:** ✅ Fixed in previous session (2025-11-26)

The 8BPP linear format is correct. Palette stride of `* 15` is correct for 90-color packed palettes.

---

## 6. Recommended Fixes (Priority Order)

### Priority 1: Fix Tile Count Loading

**File:** `src/zelda3/dungeon/object_parser.cc`

1. Add complete tile count lookup table from ZScream
2. Replace hardcoded `8` with `GetObjectTileCount(object_id)`
3. Test with objects requiring 16+ tiles (0x33, 0xC1, 0x22D)

**Expected Result:** Complex objects render with all tiles present

### Priority 2: Add Tile Transformation Support

**File:** `src/zelda3/dungeon/object_drawer.cc`

1. Add `h_flip` and `v_flip` parameters to `WriteTile8()`
2. Implement pixel coordinate transformation
3. Pass transformation flags from draw routines

**Expected Result:** Symmetrical objects render correctly

### Priority 3: Implement Object-Specific Draw Instructions

**File:** `src/zelda3/dungeon/object_drawer.cc`

Consider refactoring to support ZScream-style DrawInfo:

```cpp
struct DrawInstruction {
    int tile_index;
    int x_offset;
    int y_offset;
    bool h_flip;
    bool v_flip;
};

void ObjectDrawer::DrawFromInstructions(
    const RoomObject& obj,
    gfx::BackgroundBuffer& bg,
    std::span<const gfx::TileInfo> tiles,
    const std::vector<DrawInstruction>& instructions) {

    for (const auto& inst : instructions) {
        if (inst.tile_index >= tiles.size()) continue;
        WriteTile8(bg, obj.x_ + (inst.x_offset / 8), obj.y_ + (inst.y_offset / 8),
                   tiles[inst.tile_index], inst.h_flip, inst.v_flip);
    }
}
```

**Expected Result:** Ability to replicate ZScream's complex object rendering exactly

---

## 7. Testing Strategy

### Test Cases

1. **Simple Objects (0x00-0x08)**: Walls, ceilings - should work with current code
2. **Medium Objects (0x33)**: 16-tile carpet - currently broken due to tile count
3. **Complex Objects (0x22D, 0x22E)**: Agahnim's altar/room - broken, needs transformations
4. **Special Objects (0xC1, 0x215)**: Large platforms - broken due to tile count

### Verification Method

Compare rendered output with:
1. ZScream's dungeon editor rendering
2. In-game ALTTP screenshots
3. ZSNES/bsnes emulator tile viewers

---

## 8. Reference: ZScream Object Tile Counts (Partial List)

```
0x000: 4    | 0x001: 8    | 0x002: 8    | 0x003: 8
0x033: 16   | 0x036: 16   | 0x037: 16   | 0x03A: 12
0x0C1: 68   | 0x0CD: 28   | 0x0CE: 28   | 0x0DC: 21
0x100-0x13F: 16 (all subtype2 corners use 16 tiles)
0x200: 12   | 0x201: 20   | 0x202: 28   | 0x214: 12
0x215: 80   | 0x22D: 84   | 0x22E: 127  | 0x262: 242
```

Full list available in ZScream's `RoomObjectTileLister.cs:23-534`.

---

## Conclusion

The primary issue causing "some objects don't look right" is yaze's **hardcoded 8-tile limit** per object. ZScream loads object-specific tile counts ranging from 1 to 242 tiles, while yaze loads a fixed 8 tiles regardless of object type. This causes complex objects (carpets, chests, altars) to render with incomplete graphics.

Secondary issues include:
- Missing tile transformation support (h_flip, v_flip)
- Pattern-based drawing doesn't match ROM data for complex objects

**Immediate Action:** Implement the tile count lookup table from ZScream (Priority 1 fix above) to restore correct rendering for most objects.
