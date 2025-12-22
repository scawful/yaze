# Zelda 3 (Link to the Past) Dungeon System Specification

**Purpose:** This document provides a comprehensive specification for the ALTTP dungeon/underworld system based on analysis of:
- **yaze** - C++ dungeon editor implementation (partial)
- **ZScream** - C# reference implementation (complete)
- **usdasm** - Assembly disassembly of bank_01.asm (ground truth)

**Goal:** Close the gaps in yaze's dungeon editor to achieve correct room rendering.

---

## Table of Contents

1. [Room Data Structure](#1-room-data-structure)
2. [Room Header Format](#2-room-header-format)
3. [Object System](#3-object-system)
4. [Layer System](#4-layer-system)
5. [Door System](#5-door-system)
6. [Sprites](#6-sprites)
7. [Items and Chests](#7-items-and-chests)
8. [Graphics and Tilesets](#8-graphics-and-tilesets)
9. [Implementation Gaps in yaze](#9-implementation-gaps-in-yaze)
10. [ROM Address Reference](#10-rom-address-reference)

---

## 1. Room Data Structure

### 1.1 Overview

A dungeon room in ALTTP consists of:
- **Room Header** (14 bytes) - Metadata, properties, warp destinations
- **Object Data** - Tile objects for walls, floors, decorations
- **Sprite Data** - Enemies, NPCs, interactive entities
- **Item Data** - Chest contents, pot items, key drops
- **Door Data** - Connections between rooms

### 1.2 Total Rooms

```
Total Rooms: 296 (0x00 - 0x127)
Room Size: 512x512 pixels (64x64 tiles at 8x8 pixels each)
```

### 1.3 Room Memory Layout

```
Room Object Data:
+0x00: Floor nibbles (floor2 << 4 | floor1)
+0x01: Layout byte ((value >> 2) & 0x07)
+0x02+: Object stream (3 bytes per object)

Layer Separators:
- 0xFF 0xFF: Marks transition to next layer
- 0xF0 0xFF: Begin door list
- Layer 3 reached or 0xFF 0xFF at boundary: End of data
```

---

## 2. Room Header Format

### 2.1 Header Structure (14 bytes)

| Offset | Bits | Field | Description |
|--------|------|-------|-------------|
| 0x00 | 7-5 | bg2 | Background 2 / Layer merge type (0-7) |
| 0x00 | 4-2 | collision | Collision type (0-4) |
| 0x00 | 0 | light | Light flag (if set, bg2 = 0x08 Dark Room) |
| 0x01 | 5-0 | palette | Palette ID (0-63, masked 0x3F) |
| 0x02 | 7-0 | blockset | Tileset/Blockset ID (0-23) |
| 0x03 | 7-0 | spriteset | Spriteset ID (0-64) |
| 0x04 | 7-0 | effect | Room effect (0-7) |
| 0x05 | 7-0 | tag1 | Tag 1 trigger (0-64) |
| 0x06 | 7-0 | tag2 | Tag 2 trigger (0-64) |
| 0x07 | 1-0 | holewarp_plane | Pit warp plane (0-3) |
| 0x07 | 3-2 | staircase_plane[0] | Staircase 1 plane (0-3) |
| 0x07 | 5-4 | staircase_plane[1] | Staircase 2 plane (0-3) |
| 0x07 | 7-6 | staircase_plane[2] | Staircase 3 plane (0-3) |
| 0x08 | 1-0 | staircase_plane[3] | Staircase 4 plane (0-3) |
| 0x09 | 7-0 | holewarp | Hole warp destination room (0-255) |
| 0x0A | 7-0 | staircase_rooms[0] | Staircase 1 destination room |
| 0x0B | 7-0 | staircase_rooms[1] | Staircase 2 destination room |
| 0x0C | 7-0 | staircase_rooms[2] | Staircase 3 destination room |
| 0x0D | 7-0 | staircase_rooms[3] | Staircase 4 destination room |

### 2.2 Header Address Calculation

```cpp
// Step 1: Get master pointer
int header_pointer = ROM[kRoomHeaderPointer, 3];  // 24-bit
header_pointer = SNEStoPC(header_pointer);

// Step 2: Index into pointer table
int table_offset = header_pointer + (room_id * 2);
int address = (ROM[kRoomHeaderPointerBank] << 16) | ROM[table_offset, 2];

// Step 3: Convert to PC address
int header_location = SNEStoPC(address);
```

### 2.3 Layer Merge Types (bg2 field)

| ID | Name | Layer2OnTop | Layer2Translucent | Layer2Visible |
|----|------|-------------|-------------------|---------------|
| 0x00 | Off | true | false | false |
| 0x01 | Parallax | true | false | false |
| 0x02 | Dark | true | true | true |
| 0x03 | On top | true | true | false |
| 0x04 | Translucent | true | true | true |
| 0x05 | Addition | true | true | true |
| 0x06 | Normal | true | false | false |
| 0x07 | Transparent | true | true | true |
| 0x08 | Dark room | true | true | true |

**Note:** When `light` flag is set, bg2 is overridden to 0x08 (Dark room).

### 2.4 Collision Types

| ID | Name |
|----|------|
| 0 | One_Collision |
| 1 | Both |
| 2 | Both_With_Scroll |
| 3 | Moving_Floor_Collision |
| 4 | Moving_Water_Collision |

### 2.5 Room Effects

| ID | Name | Description |
|----|------|-------------|
| 0 | Nothing | No effect |
| 1 | One | Effect 1 |
| 2 | Moving_Floor | Animated floor tiles |
| 3 | Moving_Water | Animated water tiles |
| 4 | Four | Effect 4 |
| 5 | Red_Flashes | Red screen flashes |
| 6 | Torch_Show_Floor | Floor revealed by torches |
| 7 | Ganon_Room | Final boss room effect |

### 2.6 Room Tags (65 types)

Key tags include:
- `NW_Kill_Enemy_to_Open` (1) - Kill all enemies in NW quadrant
- `Clear_Quadrant_to_Open` (various) - Quadrant-based triggers
- `Push_Block_to_Open` (various) - Block puzzle triggers
- `Water_Gate` - Water level control
- `Agahnim_Room` - Boss room setup
- `Holes_0` through `Holes_2` - Pit configurations

---

## 3. Object System

### 3.1 Object Subtypes

ALTTP uses three object subtypes with different encoding and capabilities:

| Subtype | ID Range | Count | Scalable | Description |
|---------|----------|-------|----------|-------------|
| 1 | 0x00-0xF7 | 248 | Yes | Standard room objects |
| 2 | 0x100-0x13F | 64 | No | Fixed-size objects |
| 3 | 0xF80-0xFFF | 128 | No | Special/complex objects |

### 3.2 Object Encoding (3 bytes)

**Subtype 1 (b3 < 0xF8):**
```
Byte 1: xxxxxxss  (x = X position bits 7-2, s = size X bits 1-0)
Byte 2: yyyyyyss  (y = Y position bits 7-2, s = size Y bits 1-0)
Byte 3: iiiiiiii  (object ID 0x00-0xF7)

Decoding:
  posX = (b1 & 0xFC) >> 2
  posY = (b2 & 0xFC) >> 2
  sizeX = b1 & 0x03
  sizeY = b2 & 0x03
  sizeXY = (sizeX << 2) + sizeY
  object_id = b3
```

**Subtype 2 (b1 >= 0xFC):**
```
Byte 1: 111111xx  (x = X position bits 5-4)
Byte 2: xxxxyyyy  (x = X position bits 3-0, y = Y position bits 5-2)
Byte 3: yyiiiiii  (y = Y position bits 1-0, i = object ID low 6 bits)

Decoding:
  posX = ((b2 & 0xF0) >> 4) + ((b1 & 0x03) << 4)
  posY = ((b2 & 0x0F) << 2) + ((b3 & 0xC0) >> 6)
  object_id = (b3 & 0x3F) | 0x100
```

**Subtype 3 (b3 >= 0xF8):**
```
Byte 1: xxxxxxii  (x = X position bits 7-2, i = size/ID bits 1-0)
Byte 2: yyyyyyii  (y = Y position bits 7-2, i = size/ID bits 3-2)
Byte 3: 11111iii  (i = ID bits 6-4, marker 0xF8-0xFF)

Decoding:
  posX = (b1 & 0xFC) >> 2
  posY = (b2 & 0xFC) >> 2
  object_id = ((b3 << 4) | 0x80 + (((b2 & 0x03) << 2) + (b1 & 0x03))) - 0xD80
  // Results in 0x200-0x27E range
```

### 3.3 Object Data Tables (bank_01.asm)

```
Address        | Size  | Content
---------------|-------|------------------------------------------
$018000-$0181FE| 512B  | Subtype 1 data offsets (256 entries x 2)
$018200-$0183EE| 494B  | Subtype 1 routine pointers (256 entries)
$0183F0-$01846E| 128B  | Subtype 2 data offsets (64 entries x 2)
$018470-$0184EE| 128B  | Subtype 2 routine pointers (64 entries)
$0184F0-$0185EE| 256B  | Subtype 3 data offsets (128 entries x 2)
$0185F0-$0186EE| 256B  | Subtype 3 routine pointers (128 entries)
```

### 3.4 Drawing Routines

There are 24+ unique drawing patterns used by objects:

| Routine | Pattern | Multiplier | Objects Using |
|---------|---------|------------|---------------|
| Rightwards2x2_1to15or32 | 2x2 horizontal | s*2 | Walls, floors |
| Rightwards2x4_1to15or26 | 2x4 horizontal | s*2 | Taller walls |
| Downwards2x2_1to15or32 | 2x2 vertical | s*2 | Vertical features |
| Downwards4x2_1to15or26 | 4x2 vertical | s*2 | Wide columns |
| DiagonalAcute_1to16 | Diagonal / | +1,+1 | Stairs up-right |
| DiagonalGrave_1to16 | Diagonal \ | +1,-1 | Stairs down-right |
| 4x4 | Fixed 4x4 | none | Type 2 objects |
| 3x4 | Fixed 3x4 | none | Doors, decorations |
| Single2x2 | Single 2x2 | none | Small decorations |
| Single2x3Pillar | Single 2x3 | none | Pillars |

### 3.5 Size Handling

```cpp
// Default size when size byte is 0
if (size == 0) {
    size = 32;  // For most subtype 1 objects
}

// Some objects use size + 1 for iteration count
for (int s = 0; s < size + 1; s++) { ... }

// Size masking for some routines
int effective_size = size & 0x0F;  // Use lower 4 bits only
```

### 3.6 Layer Assignment

Objects are assigned to layers via the object stream:
- Objects before first `0xFF 0xFF` marker: Layer 0 (BG1)
- Objects after first `0xFF 0xFF` marker: Layer 1 (BG2)
- Objects after second `0xFF 0xFF` marker: Layer 2 (BG3/Sprites)

Some objects have `allBgs = true` flag and draw to both BG1 and BG2.

### 3.7 Common Object IDs

**Subtype 1 (Scalable):**
| ID | Name |
|----|------|
| 0x00-0x07 | Wall segments (N/S/E/W) |
| 0x08-0x0B | Pit edges |
| 0x0C-0x20 | Diagonal walls |
| 0x21-0x30 | Rails and supports |
| 0x31-0x40 | Carpets and trim |
| 0xD0-0xD7 | Floor types (8 patterns) |
| 0xE0-0xE7 | Ceiling types |
| 0xF0-0xF3 | Conveyor belts (4 directions) |

**Subtype 2 (Fixed):**
| ID | Name |
|----|------|
| 0x100-0x107 | Corners (concave/convex) |
| 0x108-0x10F | Braziers and statues |
| 0x110-0x117 | Star tiles |
| 0x118-0x11F | Torches and furniture |
| 0x120-0x127 | Stairs (inter/intra room) |
| 0x128-0x12F | Blocks and platforms |

**Subtype 3 (Special):**
| ID | Name |
|----|------|
| 0x200-0x207 | Waterfall faces |
| 0x208-0x20F | Somaria paths |
| 0x210-0x217 | Item piles (rupees) |
| 0x218-0x21F | Chests (various) |
| 0x220-0x227 | Pipes and conveyors |
| 0x228-0x22F | Pegs and switches |

---

## 4. Layer System

### 4.1 Layer Architecture

ALTTP uses a 3-layer system matching SNES hardware:

| Layer | SNES Name | Memory | Purpose |
|-------|-----------|--------|---------|
| BG1 | Background 1 | $7E2000 | Main room layout |
| BG2 | Background 2 | $7E4000 | Overlay layer |
| BG3 | Background 3 | Sprites | Sprites/effects |

### 4.2 Tilemap Memory Layout

```
Upper Layer (BG1/BG2): $7E2000-$7E27FF (2KB per layer)
Lower Layer (BG3):     $7E4000-$7E47FF (2KB per layer)

Grid Structure: 4 columns x 3 rows of 32x32 blocks
Offset: $100 bytes per block horizontally
        $1C0 bytes per block vertically

Each tilemap entry: 16-bit word
  Bits [9:0]:   Tile index (0-1023)
  Bit  [10]:    Priority bit
  Bits [13:11]: Palette select (0-7)
  Bit  [14]:    Horizontal flip
  Bit  [15]:    Vertical flip
```

### 4.3 Layer Merge Behavior

The `bg2` field controls how layers are composited:

```cpp
switch (bg2) {
    case 0x00: // Off - BG2 hidden
    case 0x01: // Parallax - BG2 scrolls differently
    case 0x02: // Dark - Dark room effect
    case 0x03: // On top - BG2 overlays BG1
    case 0x04: // Translucent - BG2 semi-transparent
    case 0x05: // Addition - Additive blending
    case 0x06: // Normal - Standard display
    case 0x07: // Transparent - BG2 transparent
    case 0x08: // Dark room - Special dark room
}
```

### 4.4 Floor Rendering

Floor tiles are specified by the first byte of room object data:
```cpp
uint8_t floor_byte = ROM[object_data_ptr];
uint8_t floor1 = floor_byte & 0x0F;  // Low nibble
uint8_t floor2 = floor_byte >> 4;    // High nibble
```

Floor patterns (0-15) reference tile graphics at `tile_address_floor`.

---

## 5. Door System

### 5.1 Door Data Format

Doors are encoded as 2-byte entries after the `0xF0 0xFF` marker:

```
Byte 1: Door position (0-255)
Byte 2: Door type and direction
  Bits [3:0]: Direction (0=North, 1=South, 2=West, 3=East)
  Bits [7:4]: Door type/subtype
```

### 5.2 Door Types

| Type | Name | Description |
|------|------|-------------|
| 0x00 | Regular | Standard door |
| 0x02 | Regular2 | Standard door variant |
| 0x06 | EntranceDoor | Entrance from overworld |
| 0x08 | WaterfallTunnel | Behind waterfall |
| 0x0A | EntranceLarge | Large entrance |
| 0x0C | EntranceLarge2 | Large entrance variant |
| 0x0E | EntranceCave | Cave entrance |
| 0x12 | ExitToOW | Exit to overworld |
| 0x14 | ThroneRoom | Throne room door |
| 0x16 | PlayerBgChange | Layer toggle door |
| 0x18 | ShuttersTwoWay | Two-way shutter |
| 0x1A | InvisibleDoor | Hidden door |
| 0x1C | SmallKeyDoor | Requires small key |
| 0x20-0x26 | StairMaskLocked | Locked stair doors |
| 0x28 | BreakableWall | Bomb-able wall |
| 0x30 | LgExplosion | Large explosion door |
| 0x32 | Slashable | Sword-cuttable |
| 0x40 | RegularDoor33 | Regular variant |
| 0x44 | Shutter | One-way shutter |
| 0x46 | WarpRoomDoor | Warp tile door |
| 0x48-0x4A | ShutterTrap | Trap shutter doors |

### 5.3 Door Graphics Addresses

```
Direction | Graphics Pointer
----------|------------------
North     | $014D9E (kDoorGfxUp)
South     | $014E06 (kDoorGfxDown)
West      | $014E66 (kDoorGfxLeft)
East      | $014EC6 (kDoorGfxRight)
```

### 5.4 Door Rendering Dimensions

| Direction | Width | Height |
|-----------|-------|--------|
| North/South | 4 tiles | 3 tiles |
| East/West | 3 tiles | 4 tiles |

---

## 6. Sprites

### 6.1 Sprite Data Structure

```cpp
struct Sprite {
    uint8_t id;        // Sprite type (0x00-0xF3)
    uint8_t x;         // X position (0-63)
    uint8_t y;         // Y position (0-63)
    uint8_t subtype;   // Subtype flags
    uint8_t layer;     // Layer (0-2)
    uint8_t key_drop;  // Key drop (0=none, 1=small, 2=big)
    bool overlord;     // Is overlord sprite
};
```

### 6.2 Sprite Encoding

Sprites are stored as 3-byte entries:
```
Byte 1: Y position (bits 7-1), Layer flag (bit 0)
Byte 2: X position (bits 7-1), Subtype high (bit 0)
Byte 3: Sprite ID (0x00-0xFF)

Special handling:
- Overlord check: (subtype & 0x07) == 0x07
- Key drop at sprite ID 0xE4:
  - Position (0x00, 0x1E) = small key drop
  - Position (0x00, 0x1D) = big key drop
```

### 6.3 Overlord Sprites

When `(subtype & 7) == 7`, the sprite is an "overlord" with special behavior.
Overlord IDs 0x01-0x1A have separate name tables.

### 6.4 Key Drop Mechanics

```cpp
// Detection during sprite loading
if (sprite_id == 0xE4) {
    if (x == 0x00 && y == 0x1E) {
        key_drop = 1;  // Small key
    } else if (x == 0x00 && y == 0x1D) {
        key_drop = 2;  // Big key
    }
}
```

---

## 7. Items and Chests

### 7.1 Chest Data

Chests are stored separately from room objects:

```cpp
struct ChestData {
    uint16_t room_id;   // Room containing chest
    uint8_t x;          // X position
    uint8_t y;          // Y position
    uint8_t item_id;    // Item contained
    bool is_big;        // Big chest flag
};
```

### 7.2 Item Types (Pot Items)

| ID | Item | ID | Item |
|----|------|----|------|
| 0 | Nothing | 14 | Small magic |
| 1 | Rupee (green) | 15 | Big magic |
| 2 | Rock crab | 16 | Bomb refill |
| 3 | Bee | 17 | Arrow refill |
| 4 | Random | 18 | Fairy |
| 5 | Bomb | 19 | Key |
| 6 | Heart | 20 | Fairy*8 |
| 7 | Blue rupee | 21-22 | Various |
| 8 | Key*8 | 23 | Hole |
| 9 | Arrow | 24 | Warp |
| 10 | 1 bomb | 25 | Staircase |
| 11 | Heart | 26 | Bombable |
| 12 | Rupee (blue) | 27 | Switch |
| 13 | Heart variant | | |

### 7.3 Item Encoding

Items with ID >= 0x80 use special encoding:
```cpp
if (id & 0x80) {
    int actual_id = ((id - 0x80) / 2) + 23;
}
```

### 7.4 Item ROM Addresses

```
Chest Pointers:     $01EBF6 (kChestsLengthPointer)
Chest Data:         $01EBFB (kChestsDataPointer1)
Room Items:         $01DB69 (kRoomItemsPointers)
```

---

## 8. Graphics and Tilesets

### 8.1 Graphics Organization

```
Sheet Count:          223 sheets
Uncompressed Size:    2048 bytes (0x800) per sheet
3BPP Size:            1536 bytes (0x600) per sheet
```

### 8.2 Key Graphics Addresses

```
Tile Address:         $009B52 (kTileAddress)
Tile Address Floor:   $009B5A (kTileAddressFloor)
Subtype 1 Tiles:      $018000 (kRoomObjectSubtype1)
Subtype 2 Tiles:      $0183F0 (kRoomObjectSubtype2)
Subtype 3 Tiles:      $0184F0 (kRoomObjectSubtype3)
GFX Groups:           $006237 (kGfxGroupsPointer)
```

### 8.3 Palette Configuration

```
Palettes Per Group:   16
Colors Per Palette:   16
Total Palette Size:   256 colors
Half Palette Size:    8 colors

Dungeon Main BG:      $0DEC4B (kDungeonsMainBgPalettePointers)
Dungeon Palettes:     $0DD734 (kDungeonsPalettes)
```

### 8.4 Tile Info Format

```cpp
struct TileInfo {
    uint16_t id;           // Tile index (10 bits)
    uint8_t palette;       // Palette (3 bits)
    bool h_flip;           // Horizontal mirror
    bool v_flip;           // Vertical mirror
    bool priority;         // Priority bit
};

// Decode from 16-bit word
TileInfo decode(uint16_t word) {
    TileInfo t;
    t.id = word & 0x03FF;
    t.priority = (word >> 10) & 1;
    t.palette = (word >> 11) & 0x07;
    t.h_flip = (word >> 14) & 1;
    t.v_flip = (word >> 15) & 1;
    return t;
}
```

---

## 9. Implementation Gaps in yaze

### 9.1 Critical Gaps (Must Fix for Correct Rendering)

| Gap | Severity | Description | ZScream Reference |
|-----|----------|-------------|-------------------|
| **Type 3 Objects** | Critical | Stub implementation, simplified drawing | Subtype3_Draw.cs |
| **Door Rendering** | Critical | LoadDoors() is stub, no type handling | Doors_Draw.cs |
| **all_bgs Flag Ignored** | Critical | Uses hardcoded routine IDs (3,9,17,18) instead | Room_Object.cs allBgs |
| **Floor Rendering** | High | Floor values loaded but not rendered | Room.cs floor1/floor2 |
| **Type 2 Complex Layouts** | High | Missing column, bed, spiral stair handling | Subtype2_Multiple.cs |
| **Layer Merge Effects** | High | Flags exist but not applied during render | LayerMergeType.cs |

### 9.2 Missing Systems

| System | Status in yaze | ZScream Implementation |
|--------|----------------|----------------------|
| Pot Items | Not implemented | Items_Draw.cs (28 types) |
| Key Drop Visualization | Detection only, no draw | Sprite.cs DrawKey() |
| Door Graphics | Generic object render | Doors_Draw.cs (40+ types) |
| Item-Sprite Linking | Not implemented | PotItem.cs |
| Selection State | Not tracked | Room_Object.cs selected |
| Unique Sprite IDs | Not tracked | ROM.uniqueSpriteID |

### 9.3 Architectural Differences

| Aspect | yaze Approach | ZScream Approach | Recommendation |
|--------|---------------|------------------|----------------|
| Object Classes | Single RoomObject class | Per-ID classes (object_00, etc.) | Keep unified, add type handlers |
| Draw Routines | 38 shared lambdas | 256+ override methods | Keep yaze approach |
| Tile Loading | On-demand parser | Pre-loaded static arrays | Keep yaze approach |
| Layer Selection | Binary choice (BG1/BG2) | Enum with BG3 | Add BG3 support |

### 9.4 Fix Priority List

**Phase 1: Core Rendering**
1. Fix `all_bgs_` flag usage instead of hardcoded routine IDs
2. Implement proper floor rendering from floor1/floor2 values
3. Complete Type 3 object drawing (Somaria paths, etc.)
4. Add missing Type 2 object patterns

**Phase 2: Doors**
5. Implement door type classification system
6. Add special door graphics (caves, holes, hidden walls)
7. Mirror effect for bidirectional doors
8. Layer-specific door rendering

**Phase 3: Items & Sprites**
9. Implement PotItem system (28 types)
10. Add key drop visualization
11. Link items to sprites for drops
12. Selection state tracking

**Phase 4: Polish**
13. Layer merge effect application
14. BG3 layer support
15. Complete bounds checking
16. Dimension calculation for complex objects

---

## 10. ROM Address Reference

### 10.1 Room Data

```cpp
constexpr int kRoomObjectLayoutPointer = 0x882D;
constexpr int kRoomObjectPointer = 0x874C;
constexpr int kRoomHeaderPointer = 0xB5DD;
constexpr int kRoomHeaderPointerBank = 0xB5E7;
constexpr int kNumberOfRooms = 296;
```

### 10.2 Graphics

```cpp
constexpr int kTileAddress = 0x001B52;
constexpr int kTileAddressFloor = 0x001B5A;
constexpr int kRoomObjectSubtype1 = 0x8000;
constexpr int kRoomObjectSubtype2 = 0x83F0;
constexpr int kRoomObjectSubtype3 = 0x84F0;
constexpr int kGfxGroupsPointer = 0x6237;
```

### 10.3 Palettes

```cpp
constexpr int kDungeonsMainBgPalettePointers = 0xDEC4B;
constexpr int kDungeonsPalettes = 0xDD734;
```

### 10.4 Sprites & Items

```cpp
constexpr int kRoomItemsPointers = 0xDB69;
constexpr int kRoomsSpritePointer = 0x4C298;
constexpr int kSpriteBlocksetPointer = 0x5B57;
constexpr int kSpritesData = 0x4D8B0;
constexpr int kDungeonSpritePointers = 0x090000;
```

### 10.5 Blocks & Features

```cpp
constexpr int kBlocksLength = 0x8896;
constexpr int kBlocksPointer1 = 0x15AFA;
constexpr int kBlocksPointer2 = 0x15B01;
constexpr int kBlocksPointer3 = 0x15B08;
constexpr int kBlocksPointer4 = 0x15B0F;
```

### 10.6 Chests & Torches

```cpp
constexpr int kChestsLengthPointer = 0xEBF6;
constexpr int kChestsDataPointer1 = 0xEBFB;
constexpr int kTorchData = 0x2736A;
constexpr int kTorchesLengthPointer = 0x88C1;
```

### 10.7 Pits & Doors

```cpp
constexpr int kPitPointer = 0x394AB;
constexpr int kPitCount = 0x394A6;
constexpr int kDoorPointers = 0xF83C0;
constexpr int kDoorGfxUp = 0x4D9E;
constexpr int kDoorGfxDown = 0x4E06;
constexpr int kDoorGfxLeft = 0x4E66;
constexpr int kDoorGfxRight = 0x4EC6;
```

---

## Appendix A: Object ID Quick Reference

### Subtype 1 (0x00-0xF7) - Scalable

| Range | Category |
|-------|----------|
| 0x00-0x07 | Wall segments |
| 0x08-0x0B | Pit edges |
| 0x0C-0x20 | Diagonal walls (allBgs=true) |
| 0x21-0x30 | Rails and supports |
| 0x31-0x40 | Carpets and trim |
| 0x41-0x50 | Decorations |
| 0xD0-0xD7 | Floor patterns |
| 0xE0-0xE7 | Ceiling patterns |
| 0xF0-0xF3 | Conveyor belts |

### Subtype 2 (0x100-0x13F) - Fixed Size

| Range | Category |
|-------|----------|
| 0x100-0x107 | Corners |
| 0x108-0x10F | Braziers/statues |
| 0x110-0x117 | Star tiles |
| 0x118-0x11F | Torches/furniture |
| 0x120-0x127 | Stairs |
| 0x128-0x12F | Blocks/platforms |
| 0x130-0x13F | Misc decorations |

### Subtype 3 (0x200-0x27E) - Special

| Range | Category |
|-------|----------|
| 0x200-0x207 | Waterfall faces |
| 0x208-0x20F | Somaria paths |
| 0x210-0x217 | Item piles |
| 0x218-0x21F | Chests |
| 0x220-0x227 | Pipes/conveyors |
| 0x228-0x22F | Pegs/switches |
| 0x230-0x23F | Boss objects |
| 0x240-0x27E | Misc special |

---

## Appendix B: Drawing Routine Reference

| ID | Routine Name | Pattern | Objects |
|----|--------------|---------|---------|
| 0 | Rightwards2x2_1to15or32 | 2x2 horizontal | 0x00, walls |
| 1 | Rightwards2x4_1to15or26 | 2x4 horizontal | 0x01-0x02 |
| 2 | Downwards2x2_1to15or32 | 2x2 vertical | vertical walls |
| 3 | Rightwards2x2_BothBG | 2x2 both layers | 0x03-0x04 |
| 4 | Rightwards2x4spaced4_1to16 | 2x4 spaced | 0x05-0x06 |
| 5 | DiagonalAcute_1to16 | Diagonal / | stairs |
| 6 | DiagonalGrave_1to16 | Diagonal \ | stairs |
| 7 | Downwards4x2_1to15or26 | 4x2 vertical | wide columns |
| 8 | 4x4 | Fixed 4x4 | Type 2 objects |
| 9 | Downwards4x2_BothBG | 4x2 both layers | special walls |
| 17 | DiagonalAcute_BothBG | Diagonal both | diagonal walls |
| 18 | DiagonalGrave_BothBG | Diagonal both | diagonal walls |

---

*Document generated from analysis of yaze, ZScream, and usdasm codebases.*
*Last updated: 2025-12-01*
