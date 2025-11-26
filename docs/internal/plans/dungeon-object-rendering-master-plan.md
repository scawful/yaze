# Dungeon Object Rendering - Master Plan

## Completed Phases

### Phase 1: BG Layer Draw Order
**Completed:** 2025-11-26
- Swapped draw order: BG2 first (background), then BG1 (foreground)
- Objects on BG1 no longer covered by BG2

### Phase 2: Wall Rendering Investigation
**Completed:** 2025-11-26
- Root cause: Bitmap initialization bug (1 byte instead of 262144)
- Created ROM-dependent integration tests confirming fix

### Phase 3: Subtype1 Tile Count Lookup Table
**Completed:** 2025-11-26
- Added `kSubtype1TileLengths[0xF8]` from ZScream
- Objects now load correct tile counts (e.g., 0xC1 = 68 tiles)

### Phase 4a: Type Detection Fixes
**Completed:** 2025-11-26
- Fix 1: Type 2/Type 3 decode order (check b1 >= 0xFC before b3 >= 0xF8)
- Fix 2: Type 2 index mask 0x7F -> 0xFF
- Fix 3: Type 3 threshold 0x200 -> 0xF80

---

## Pending Phases

### Phase 4b: North/South Wall Draw Routines
**Status:** Not started
**Priority:** High

**Problem:** North/south walls (Rightwards routines 0x01-0x04) don't render correctly. East/west walls (Downwards routines 0x60+) work fine.

**ROOT CAUSE FOUND (ZScream Analysis):**
The tile arrangement is **COLUMN-MAJOR**, not row-major!

**ZScream Rightwards2x4 Pattern:**
```
Column 0    Column 1
[tile 0]    [tile 4]   <- Row 0
[tile 1]    [tile 5]   <- Row 1
[tile 2]    [tile 6]   <- Row 2
[tile 3]    [tile 7]   <- Row 3
```

**Current yaze implementation (WRONG):**
```
[tile 0] [tile 1]   <- Row 0
[tile 2] [tile 3]   <- Row 1
[tile 0] [tile 1]   <- Row 2 (repeat)
[tile 2] [tile 3]   <- Row 3 (repeat)
```

**Fix Required:**
```cpp
// Column 0 (left), top to bottom
WriteTile8(bg, obj.x_ + (s * 2), obj.y_, tiles[0]);
WriteTile8(bg, obj.x_ + (s * 2), obj.y_ + 1, tiles[1]);
WriteTile8(bg, obj.x_ + (s * 2), obj.y_ + 2, tiles[2]);
WriteTile8(bg, obj.x_ + (s * 2), obj.y_ + 3, tiles[3]);
// Column 1 (right), top to bottom
WriteTile8(bg, obj.x_ + (s * 2) + 1, obj.y_, tiles[4]);
WriteTile8(bg, obj.x_ + (s * 2) + 1, obj.y_ + 1, tiles[5]);
WriteTile8(bg, obj.x_ + (s * 2) + 1, obj.y_ + 2, tiles[6]);
WriteTile8(bg, obj.x_ + (s * 2) + 1, obj.y_ + 3, tiles[7]);
```

**Files:**
- `src/zelda3/dungeon/object_drawer.cc` - DrawRightwards* routines
- ZScream: `ZeldaFullEditor/Data/Types/DungeonObjectDraw.cs`

**Objects affected:**
- 0x01-0x02: North walls (routine 1) - 8 tiles
- 0x03-0x04: South walls (routine 2) - 8 tiles
- 0x05-0x06: Both-layer walls (routine 3) - 8 tiles

---

### Phase 4c: Corner Wall Objects
**Status:** Not started
**Priority:** Medium

**Problem:** Corner wall objects not yet implemented.

**ZScream Analysis - Corner Object IDs:**

**Type 2 Corners (Subtype 2) - Grid-based 4x4 drawing:**
| ID Range | Type | Tiles | Description |
|----------|------|-------|-------------|
| 0x40-0x43 | Concave Top | 16 (4x4) | Inner corners |
| 0x44-0x47 | Convex Top | 16 (4x4) | Outer corners |
| 0x48-0x4B | Concave Bottom | 16 (4x4) | Inner corners (layer 2) |
| 0x4C-0x4F | Convex Bottom | 16 (4x4) | Outer corners (layer 2) |
| 0x10-0x13 | Kinked N/S | 12 (3x4) | Direction change corners |
| 0x14-0x17 | Kinked E/W | 12 (4x3) | Direction change corners |
| 0x18-0x1B | Deep Concave | 4 (2x2) | Depth effect corners |

**Type 1 Diagonal Walls (0x09-0x20):**
| ID Range | Direction | Draw Method |
|----------|-----------|-------------|
| 0x09, 0x0C, 0x0D, 0x10, 0x11, 0x14, 0x15, 0x18, 0x19, 0x1C, 0x1D, 0x20 | Rising | `draw_diagonal_up()` |
| 0x0A, 0x0B, 0x0E, 0x0F, 0x12, 0x13, 0x16, 0x17, 0x1A, 0x1B, 0x1E, 0x1F | Falling | `draw_diagonal_down()` |

**Diagonal Draw Pattern (5 tiles per column):**
```cpp
// draw_diagonal_up: Y decreases as X increases
for (int s = 0; s < Size + 6; s++) {
    draw_tile(tiles[0], s * 8, (0 - s) * 8);
    draw_tile(tiles[1], s * 8, (1 - s) * 8);
    draw_tile(tiles[2], s * 8, (2 - s) * 8);
    draw_tile(tiles[3], s * 8, (3 - s) * 8);
    draw_tile(tiles[4], s * 8, (4 - s) * 8);
}

// draw_diagonal_down: Y increases as X increases
for (int s = 0; s < Size + 6; s++) {
    draw_tile(tiles[0], s * 8, (0 + s) * 8);
    draw_tile(tiles[1], s * 8, (1 + s) * 8);
    // ... etc
}
```

**Corner Draw Routine (Grid-based):**
```cpp
// Standard grid for 4x4 corners
for (int xx = 0; xx < 4; xx++) {
    for (int yy = 0; yy < 4; yy++) {
        draw_tile(tiles[tid++], xx * 8, yy * 8);
    }
}
```

**Files:**
- ZScream: `ZeldaFullEditor/Rooms/Object_Draw/Subtype2_Multiple.cs`
- ZScream: `ZeldaFullEditor/Rooms/Room_Object.cs` (diagonal methods)

---

### Phase 4d: Floor Objects
**Status:** Not started
**Priority:** Medium

**Problem:** Floor objects not rendering.

**ZScream Analysis - Floor Object IDs:**

**Horizontal Floors (Rightwards):**
| ID | Tiles | Draw Routine | Description |
|----|-------|--------------|-------------|
| 0x033 | 16 | RoomDraw_Rightwards4x4_1to16 | Standard 4x4 floor |
| 0x034 | 1 | RoomDraw_Rightwards1x1Solid_1to16_plus3 | Solid floor with perimeter |
| 0x0B2 | 16 | RoomDraw_Rightwards4x4_1to16 | 4x4 floor variant |
| 0x0B3-0x0B4 | 1 | RoomDraw_RightwardsHasEdge1x1_1to16_plus2 | Edge floors |
| 0x0BA | 16 | RoomDraw_Rightwards4x4_1to16 | 4x4 floor variant |

**Vertical Floors (Downwards):**
| ID | Tiles | Draw Routine | Description |
|----|-------|--------------|-------------|
| 0x070 | 16 | RoomDraw_DownwardsFloor4x4_1to16 | Standard 4x4 floor |
| 0x071 | 1 | RoomDraw_Downwards1x1Solid_1to16_plus3 | Solid floor with perimeter |
| 0x094 | 16 | RoomDraw_DownwardsFloor4x4_1to16 | 4x4 floor variant |
| 0x08D-0x08E | 1 | RoomDraw_DownwardsEdge1x1_1to16 | Edge floors |

**Special Floors (SuperSquare patterns):**
| ID | Tiles | Draw Routine | Category |
|----|-------|--------------|----------|
| 0x0C3 | 9 | RoomDraw_3x3FloorIn4x4SuperSquare | Pits, MetaLayer |
| 0x0C4-0x0CA | 16 | RoomDraw_4x4FloorIn4x4SuperSquare | Various |
| 0x0DF | 16 | RoomDraw_4x4FloorIn4x4SuperSquare | Spikes |
| 0x212 | 9 | RoomDraw_RupeeFloor | Secrets |
| 0x247 | 16 | RoomDraw_BombableFloor | Pits, Manipulable |

**Layer Assignment:**
- **Single-layer floors:** Drawn to object's assigned layer only
- **Both-layer floors (MetaLayer):** Objects 0x0C3-0x0E8 drawn to BOTH BG1 and BG2
- Floors typically on BG1 (background), walls on BG2 (foreground)

**4x4 Floor Tile Pattern:**
```
[0 ] [1 ] [2 ] [3 ]
[4 ] [5 ] [6 ] [7 ]
[8 ] [9 ] [10] [11]
[12] [13] [14] [15]
```

**BombableFloor Special Pattern (irregular):**
```
[0 ] [4 ] [2 ] [6 ]
[8 ] [12] [10] [14]
[1 ] [5 ] [3 ] [7 ]
[9 ] [13] [11] [15]
```

**Files:**
- ZScream: `ZeldaFullEditor/Data/Types/DungeonObjectDraw.cs`
- ZScream: `ZeldaFullEditor/Data/Types/DungeonObjectTypes.cs`

---

### Phase 5: Validation & Lifecycle Fixes
**Status:** Deferred
**Priority:** Low

From original analysis:
- Object ID validation range (0x3FF -> 0xFFF)
- `tile_objects_` not cleared on reload

---

### Phase 6: Draw Routine Completion
**Status:** Deferred
**Priority:** Low

From original analysis:
- Complete draw routine registry (routines 17-34 uninitialized)
- Currently reserves 35 routines but only initializes 17

---

## Testing Strategy

**Per-phase testing:**
1. Test specific object types affected by the phase
2. Compare against ZScream visual output
3. Verify no regressions in previously working objects

**Key test rooms:**
- Room 0x00: Basic walls
- Rooms with complex wall arrangements
- Agahnim's tower rooms (complex objects)

---

## Reference Files

**yaze:**
- `src/zelda3/dungeon/object_drawer.cc` - Draw routines
- `src/zelda3/dungeon/object_parser.cc` - Tile loading
- `src/zelda3/dungeon/room_object.cc` - Object decoding
- `docs/internal/plans/dungeon-object-rendering-fix-plan.md` - Original analysis

**ZScream (authoritative reference):**
- `ZeldaFullEditor/Data/Types/DungeonObjectDraw.cs` - All draw routines
- `ZeldaFullEditor/Data/Types/DungeonObjectTypes.cs` - Object definitions
- `ZeldaFullEditor/Data/DungeonObjectData.cs` - Tile counts, routine mappings
- `ZeldaFullEditor/Rooms/Room_Object.cs` - Object class, diagonal methods
- `ZeldaFullEditor/Rooms/Object_Draw/Subtype2_Multiple.cs` - Type 2 corners

---

## Key ZScream Insights

### Critical Finding: Column-Major Tile Ordering
ZScream's `RoomDraw_RightwardsXbyY` and `RoomDraw_DownwardsXbyY` use **column-major** tile iteration:
```csharp
for (int x = 0; x < sizex; x++) {      // Outer loop: columns
    for (int y = 0; y < sizey; y++) {  // Inner loop: rows
        draw_tile(tiles[t++], x * 8, y * 8);
    }
}
```

This means for a 2x4 wall:
- Tiles 0-3 go in column 0 (left)
- Tiles 4-7 go in column 1 (right)

### Rightwards vs Downwards Difference
The ONLY difference between horizontal and vertical routines is the increment axis:
- **Rightwards:** `inc = sizex * 8` applied to X (repeats horizontally)
- **Downwards:** `inc = sizey * 8` applied to Y (repeats vertically)

### Layer System
- BG1 = Layer 1 (background/floor)
- BG2 = Layer 2 (foreground/walls)
- Some objects draw to BOTH via `allbg` flag
