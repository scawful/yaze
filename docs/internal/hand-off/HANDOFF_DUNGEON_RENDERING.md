# Dungeon Tile Rendering - Debugging Strategy & Next Steps

**Date**: 2025-12-01
**Status**: Partially Fixed - Floor/Wall tiles working, object sizes need attention

## Summary

Fixed a critical graphics loading order bug that caused dungeon tiles to display incorrect graphics. Floor tiles, most walls, and special tiles now render correctly. However, several objects still have incorrect sizes or excessive transparent tiles.

## What Was Fixed

### Root Cause: Graphics Sheet Loading Order
The `blocks_[]` array (which maps slots 0-15 to graphics sheet IDs) was being read BEFORE it was initialized by `LoadRoomGraphics()`.

**Before (broken)**:
```cpp
void Room::RenderRoomGraphics() {
  if (graphics_dirty_) {
    CopyRoomGraphicsToBuffer();  // Uses stale blocks_[] values!
    graphics_dirty_ = false;
  }
  // LoadRoomGraphics was called elsewhere, too late
}
```

**After (fixed)**:
```cpp
void Room::RenderRoomGraphics() {
  if (graphics_dirty_) {
    LoadRoomGraphics(blockset);  // Initialize blocks_[] FIRST
    CopyRoomGraphicsToBuffer();  // Now uses correct sheet IDs
    graphics_dirty_ = false;
  }
}
```

### Files Modified
1. **`src/zelda3/dungeon/room.cc`** - Main fix + debug logging
2. **`src/zelda3/dungeon/object_drawer.cc`** - Enhanced draw routines
3. **`src/zelda3/dungeon/object_parser.cc`** - Debug logging for tile lookups
4. **`src/app/editor/music/music_editor.cc`** - Fixed SIGSEGV crash (unrelated)

## Debugging Strategy Used

### 1. Trace the Data Flow
Added printf logging at key points to trace the tile rendering pipeline:

```
[LoadRoomGraphics] Room 0: blockset=19
[LoadRoomGraphics] Sheet IDs: 0, 1, 16, 9, 14, 42, 23, 15
[CopyRoomGraphicsToBuffer] Block 0=Sheet 0, Block 1=Sheet 1...
[ParseSubtype1] obj=0x00: tile_ptr=0x8000, offset=...
[DrawTile] id=200 (col=8,row=12) -> gfx offset=...
```

### 2. Verify ROM Address Calculations
Cross-referenced with ALTTP disassembly:
- `kRoomObjectSubtype1` = 0x8000 (SNES $01:8000) - subtype 1 object table
- `kRoomObjectTileAddress` = 0x1B52 (SNES $00:9B52) - RoomDrawObjectData

### 3. Compare Execution Order
The critical insight came from comparing the order of debug messages:
- **Broken**: CopyRoomGraphicsToBuffer ran BEFORE LoadRoomGraphics
- **Fixed**: LoadRoomGraphics initializes blocks_[] BEFORE copy

### 4. Key Debugging Locations

| File | Function | What to Log |
|------|----------|-------------|
| room.cc | LoadRoomGraphics() | Blockset ID, sheet IDs for blocks_[0-15] |
| room.cc | CopyRoomGraphicsToBuffer() | Which sheets being copied to which slots |
| object_parser.cc | ParseSubtype1() | Tile pointer addresses, offsets, tile words |
| object_drawer.cc | DrawTileToBitmap() | Tile ID, palette, pixel data |

## Remaining Issues to Fix

### 1. Type 2 Object Size Bug (CRITICAL)
**Location**: `room_object.cc:193`

Type 2 objects (0x100-0x13F, e.g., chests, stairs) have `size = 0` hardcoded!

```cpp
// Type 2: 111111xx xxxxyyyy yyiiiiii
if (b1 >= 0xFC) {
    id = (b3 & 0x3F) | 0x100;
    x = ((b2 & 0xF0) >> 4) | ((b1 & 0x03) << 4);
    y = ((b2 & 0x0F) << 2) | ((b3 & 0xC0) >> 6);
    size = 0;  // <-- BUG: Size is always 0!
}
```

**Impact**: Type 2 objects only draw 1 repetition regardless of actual encoded size.

**Fix Options**:
1. Research original ALTTP assembly to determine if Type 2 objects have encoded size
2. If Type 2 objects don't use size encoding, update draw routines to not rely on size
3. Cross-reference with ZScream's handling of Type 2 objects

### 2. Inconsistent Size Masking
Draw routines inconsistently handle size values:

| Pattern | Issue |
|---------|-------|
| `int size = obj.size_;` | Uses raw value (can be 0-255) |
| `int size = obj.size_ & 0x0F;` | Masks to 0-15 (more correct) |
| `if (size == 0) size = 32;` | Special case for specific objects |

**Example inconsistencies**:
```cpp
// DrawRightwards2x2_1to15or32 - uses raw obj.size_
int size = obj.size_;
if (size == 0) size = 32;

// DrawRightwards2x2_1to16 - masks to 4 bits
int size = obj.size_ & 0x0F;
int count = size + 1;
```

**Fix**: Standardize on `obj.size_ & 0x0F` for all routines unless specific object needs special handling.

### 3. Object Size Decoding for Type 1 Objects
Type 1 decoding extracts size correctly but may have edge cases:

```cpp
// Type 1: xxxxxxss yyyyyyss iiiiiiii
size = ((b1 & 0x03) << 2) | (b2 & 0x03);  // 4 bits = 0-15
```

This gives size 0-15, but some objects expect size 0 to mean "draw 32 times".
**Verify**: Cross-reference with game assembly to confirm which objects have special size=0 handling.

### 4. Subtype 2/3 Tile Count Issues
Subtype 2 (0x100-0x13F) and Subtype 3 (0x200-0x2FF) objects may have different tile lookup logic.

**Investigate**:
- `ParseSubtype2()` and `ParseSubtype3()` in object_parser.cc
- These use different ROM table addresses
- May need different tile count tables (currently hardcoded to 8)

### 3. Tile Count Table Accuracy
The `kSubtype1TileLengths[0xF8]` table was derived from ZScream but may have errors.

**Verify**: Cross-reference with original game assembly for each object type.

### 4. Routine-to-Object Mapping Gaps
Some objects fall through to the fallback 1x1 drawing routine.

```cpp
// object_drawer.cc lines 53-62
if (routine_id < 0 || routine_id >= ...) {
    // Fallback to simple 1x1 drawing
    WriteTile8(target_bg, object.x_, object.y_, tiles[0]);
}
```

**Fix**: Add missing entries to `object_to_routine_map_`.

## Architecture Reference

### Graphics Buffer Pipeline
```
ROM (3BPP compressed sheets)
    ↓ SnesTo8bppSheet()
gfx_sheets_ (8BPP, 16 base sheets × 128×32)
    ↓ CopyRoomGraphicsToBuffer()
current_gfx16_ (room-specific 64KB buffer)
    ↓ ObjectDrawer::DrawTileToBitmap()
bg1_bitmap / bg2_bitmap (512×512 room canvas)
```

### Tile Info Format (SNES Tilemap Word)
```
vhopppcccccccccc
v = vertical flip
h = horizontal flip
o = priority
ppp = palette (0-7, but dungeon uses 0-5)
cccccccccc = tile ID (0-1023)
```

### Object Subtypes
| Subtype | ID Range | ROM Table | Description |
|---------|----------|-----------|-------------|
| 1 | 0x00-0xF7 | $01:8000 | Most dungeon objects (walls, floors, etc.) |
| 2 | 0x100-0x13F | $01:83F0 | Special objects (chests, stairs) |
| 3 | 0x200-0x2FF | $01:84F0 | Complex objects (water faces, Somaria paths) |

## Testing Commands

```bash
# Run with dungeon editor, check specific room
./yaze --rom_file=zelda3.sfc --editor=Dungeon

# Debug output (add to code temporarily)
printf("[Object] ID=0x%03X at (%d,%d) size=%d tiles=%zu\n",
       object.id_, object.x_, object.y_, object.size_, object.tiles().size());
```

## Next Steps Priority

1. **High**: Fix object size decoding in `room_object.cc`
2. **High**: Verify tile counts for subtype 2/3 objects
3. **Medium**: Complete `object_to_routine_map_` entries
4. **Medium**: Add size-aware draw routines
5. **Low**: Remove debug printf statements after verification

## Files to Study

- `/src/zelda3/dungeon/room_object.cc` - Object data decoding
- `/src/zelda3/dungeon/object_parser.cc` - Tile lookup logic
- `/src/zelda3/dungeon/object_drawer.cc` - Draw routine implementations
- ZScream's `DungeonObjectData.cs` - Reference for object mappings
- ALTTP disassembly `bank_00.asm` - RoomDrawObjectData at $00:9B52
