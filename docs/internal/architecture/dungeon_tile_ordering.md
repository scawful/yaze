# Dungeon Object Tile Ordering Reference

This document describes the tile ordering patterns used in ALTTP dungeon object rendering, based on analysis of the ZScream reference implementation.

## Key Finding

**There is NO simple global rule** for when to use ROW-MAJOR vs COLUMN-MAJOR ordering. The choice is made on a **per-object basis** based on the visual appearance and extension direction of each object.

## Core Draw Patterns

ZScream uses five primary draw routine patterns:

### 1. RightwardsXbyY (Horizontal Extension)

- **Direction**: Extends rightward
- **Tile ordering**: Tiles fill each vertical slice, then move right
- **Usage**: Horizontal walls, rails, decorations (objects 0x00-0x5F range)
- **Pattern**: For 2x4, column 0 gets tiles 0-3, column 1 gets tiles 4-7

### 2. DownwardsXbyY (Vertical Extension)

- **Direction**: Extends downward
- **Tile ordering**: Tiles fill each horizontal slice, then move down
- **Usage**: Vertical walls, pillars, decorations (objects 0x60-0x98 range)
- **Pattern**: For 4x2, row 0 gets tiles 0-3, row 1 gets tiles 4-7

### 3. ArbitraryXByY (Generic Grid)

- **Direction**: No extension, fixed grid
- **Tile ordering**: Row-first (X outer loop, Y inner loop)
- **Usage**: Floors, generic rectangular objects

### 4. ArbitraryYByX (Column-First Grid)

- **Direction**: No extension, fixed grid
- **Tile ordering**: Column-first (Y outer loop, X inner loop)
- **Usage**: Beds, pillars, furnaces

### 5. Arbitrary4x4in4x4SuperSquares (Tiled Blocks)

- **Direction**: Both, repeating pattern
- **Tile ordering**: 4x4 blocks in 32x32 super-squares
- **Usage**: Floors, conveyor belts, ceiling blocks

## Object Groups and Their Patterns

| Object Range | Description | Pattern |
|--------------|-------------|---------|
| 0x00 | Rightwards 2x2 wall | RightwardsXbyY (COLUMN-MAJOR per slice) |
| 0x01-0x02 | Rightwards 2x4 walls | RightwardsXbyY (COLUMN-MAJOR per slice) |
| 0x03-0x06 | Rightwards 2x4 spaced | RightwardsXbyY (COLUMN-MAJOR per slice) |
| 0x60 | Downwards 2x2 wall | DownwardsXbyY (interleaved) |
| 0x61-0x62 | Downwards 4x2 walls | DownwardsXbyY (ROW-MAJOR per slice) |
| 0x63-0x64 | Downwards 4x2 both BG | DownwardsXbyY (ROW-MAJOR per slice) |
| 0x65-0x66 | Downwards 4x2 spaced | DownwardsXbyY (needs verification) |

## Verified Fixes

### Objects 0x61-0x62 (Left/Right Walls)

These use `DrawDownwards4x2_1to15or26` and require **ROW-MAJOR** ordering:

```
Row 0: tiles[0], tiles[1], tiles[2], tiles[3] at x+0, x+1, x+2, x+3
Row 1: tiles[4], tiles[5], tiles[6], tiles[7] at x+0, x+1, x+2, x+3
```

This was verified by comparing yaze output with ZScream and confirmed working.

## Objects Needing Verification

Before changing any other routines, verify against ZScream by:

1. Loading the same room in both editors
2. Comparing the visual output
3. Checking the specific object IDs in question
4. Only then updating the code

Objects that may need review:
- 0x65-0x66 (DrawDownwardsDecor4x2spaced4_1to16)
- Other 4x2/2x4 patterns

## Implementation Notes

### 2x2 Patterns

The 2x2 patterns use an interleaved ordering that produces identical visual results whether interpreted as row-major or column-major:

```
ZScream order:  tiles[0]@(0,0), tiles[2]@(1,0), tiles[1]@(0,1), tiles[3]@(1,1)
yaze order:     tiles[0]@(0,0), tiles[1]@(0,1), tiles[2]@(1,0), tiles[3]@(1,1)
Result:         Same positions for same tiles
```

### Why This Matters

The tile data in ROM contains the actual graphics. If tiles are placed in wrong positions, objects will appear scrambled, inverted, or wrong. The h_flip/v_flip flags in tile data handle mirroring - the draw routine just needs to place tiles at correct positions.

## References

- ZScream source: `ZeldaFullEditor/Rooms/Object_Draw/Subtype1_Draw.cs`
- ZScream types: `ZeldaFullEditor/Data/Types/DungeonObjectDraw.cs`
- yaze implementation: `src/zelda3/dungeon/object_drawer.cc`
