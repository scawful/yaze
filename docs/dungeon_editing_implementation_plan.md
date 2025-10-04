# Dungeon Editing Implementation Plan for Yaze

**Status**: Planning Phase  
**Goal**: Implement fully functional dungeon room editing based on ZScream's proven approach  
**Created**: October 4, 2025

## Executive Summary

The dungeon object system in yaze currently does not work properly. ZScream's dungeon editing is fully functional and provides a proven reference implementation. This document outlines a comprehensive plan to implement working dungeon editing in yaze by analyzing ZScream's approach and adapting it to yaze's C++ architecture.

## Table of Contents

1. [Current State Analysis](#current-state-analysis)
2. [ZScream's Working Approach](#zscreams-working-approach)
3. [ROM Data Structure Reference](#rom-data-structure-reference)
4. [Implementation Tasks](#implementation-tasks)
5. [Technical Architecture](#technical-architecture)
6. [Testing Strategy](#testing-strategy)
7. [References](#references)

---

## Current State Analysis

### What Works in Yaze

- ✅ Room header loading (basic properties: blockset, palette, spriteset)
- ✅ Room class structure exists
- ✅ RoomObject class with basic properties
- ✅ Graphics loading system (blocks, palettes)
- ✅ Room layout system for structural elements
- ✅ Sprite loading (separate from objects)
- ✅ UI framework with canvas, selectors, and viewers

### What Doesn't Work

- ❌ **Object Parsing**: Objects are loaded but not properly decoded
- ❌ **Object Drawing**: Objects don't render correctly on canvas
- ❌ **Object Placement**: No working system to place/edit objects
- ❌ **Object Encoding**: Can't save objects back to ROM format
- ❌ **Object Selection**: Can't select/move/delete objects properly
- ❌ **Layer Management**: Layer separation not working correctly
- ❌ **Door Objects**: Door section (0xF0 FF) not handled
- ❌ **Object Types**: Type1, Type2, Type3 distinctions not properly implemented
- ❌ **Tile Loading**: Objects don't load their tile data correctly
- ❌ **Collision/Properties**: Object-specific properties not tracked

### Key Problems Identified

1. **Object Data Format**: The 3-byte object encoding/decoding is incomplete
2. **Layer Separation**: Objects on BG1, BG2, BG3 not properly separated
3. **Object Rendering**: Object tile data not being loaded and drawn
4. **Editor Integration**: No working UI flow for object placement
5. **Save System**: No encoding of edited objects back to ROM format

---

## ZScream's Working Approach

### Room Data Structure (ZScream)

```csharp
public class Room
{
    // Header Data (14 bytes at header_location)
    public byte layout;           // Byte 1 (bits 2-4)
    public byte floor1;           // Byte 0 (bits 0-3)
    public byte floor2;           // Byte 0 (bits 4-7)
    public byte blockset;         // Byte 2
    public byte spriteset;        // Byte 3
    public byte palette;          // Byte 1 (bits 0-5)
    public Background2 bg2;       // Byte 0 (bits 5-7)
    public CollisionKey collision; // Byte 0 (bits 2-4)
    public EffectKey effect;      // Byte 4
    public TagKey tag1;           // Byte 5
    public TagKey tag2;           // Byte 6
    public byte holewarp;         // Byte 9
    public byte[] staircase_rooms; // Bytes 10-13 (4 stairs)
    public byte[] staircase_plane; // Byte 7-8 (plane info)
    
    // Object Data
    public List<Room_Object> tilesObjects;  // Objects in room
    public List<Room_Object> tilesLayoutObjects; // Layout/structural objects
    public List<Sprite> sprites;            // Sprites (separate system)
    public List<Chest> chest_list;          // Chests in room
    public byte[] blocks;                    // 16 GFX blocks for room
}
```

### Object Data Structure (ZScream)

```csharp
public class Room_Object
{
    // ROM Data (encoded in 3 bytes)
    public byte X;          // Position X (0-63)
    public byte Y;          // Position Y (0-63)  
    public byte Size;       // Size parameter (0-15)
    public ushort id;       // Object ID (0x000-0xFFF)
    public LayerType Layer; // BG1, BG2, or BG3
    
    // Rendering Data
    public List<Tile> tiles;     // Tile data for drawing
    public int width, height;     // Computed object size
    public int offsetX, offsetY;  // Drawing offset
    
    // Editor Data
    public string name;           // Object name for UI
    public ObjectOption options;  // Flags: Door, Chest, Block, Torch, Stairs
    public DungeonLimits LimitClass; // Object limits tracking
    public int uniqueID;          // Unique ID for undo/redo
}
```

### Object Encoding Format (Critical!)

ZScream uses **three different encoding formats** based on object ID range:

#### Type 1 Objects (ID 0x000-0x0FF) - Standard Objects

**Encoding**: `xxxxxxss yyyyyyss iiiiiiii`

```
Byte 1: (X << 2) | ((Size >> 2) & 0x03)
Byte 2: (Y << 2) | (Size & 0x03)
Byte 3: Object ID (0x00-0xFF)
```

**Decoding**:
```
X = (Byte1 & 0xFC) >> 2          // Bits 2-7 = X position
Y = (Byte2 & 0xFC) >> 2          // Bits 2-7 = Y position
Size = ((Byte1 & 0x03) << 2) | (Byte2 & 0x03)  // Size 0-15
ID = Byte3                        // Object ID
```

#### Type 2 Objects (ID 0x100-0x1FF) - Special Objects

**Encoding**: `111111xx xxxxyyyy yyiiiiii`

```
Byte 1: 0xFC | ((X & 0x30) >> 4)        // 0xFC-0xFF
Byte 2: ((X & 0x0F) << 4) | ((Y & 0x3C) >> 2)
Byte 3: ((Y & 0x03) << 6) | (ID & 0x3F)
```

**Decoding**:
```
X = ((Byte1 & 0x03) << 4) | ((Byte2 & 0xF0) >> 4)  // X position
Y = ((Byte2 & 0x0F) << 2) | ((Byte3 & 0xC0) >> 6)  // Y position
ID = (Byte3 & 0x3F) | 0x100                         // Object ID
Size = 0  // No size parameter
```

#### Type 3 Objects (ID 0xF00-0xFFF) - Extended Objects

**Encoding**: `xxxxxxii yyyyyyii 11111iii`

```
Byte 1: (X << 2) | (ID & 0x03)
Byte 2: (Y << 2) | ((ID >> 2) & 0x03)
Byte 3: (ID >> 4) & 0xFF              // 0xF0-0xFF
```

**Decoding**:
```
X = (Byte1 & 0xFC) >> 2
Y = (Byte2 & 0xFC) >> 2
ID = ((Byte3 << 4) & 0xF00) | ((Byte2 & 0x03) << 2) | (Byte1 & 0x03) | 0x80
```

### Layer and Door Markers

Objects are organized in **layers** separated by special markers:

```
[Floor/Layout byte]
[Layout configuration byte]
<Layer 1 Objects>
0xFF 0xFF  <- Layer 1 End
<Layer 2 Objects>
0xFF 0xFF  <- Layer 2 End
<Layer 3 Objects>
[Optional Door Section]
0xF0 0xFF  <- Door Section Marker
<Door Objects (2 bytes each)>
0xFF 0xFF  <- End of Room
```

### Object Loading Process (ZScream)

```csharp
// 1. Get room object pointer (3 bytes)
int object_pointer = ROM at (room_object_pointer + room_id * 3)
int objects_location = SnesToPc(object_pointer)

// 2. Read floor/layout
byte floor1 = data[objects_location] & 0x0F
byte floor2 = (data[objects_location] >> 4) & 0x0F
byte layout = (data[objects_location + 1] >> 2) & 0x07

// 3. Parse objects
int pos = objects_location + 2
int layer = 0
bool door = false

while (true) {
    byte b1 = data[pos]
    byte b2 = data[pos + 1]
    
    // Check for layer end
    if (b1 == 0xFF && b2 == 0xFF) {
        layer++
        pos += 2
        if (layer == 3) break
        continue
    }
    
    // Check for door section
    if (b1 == 0xF0 && b2 == 0xFF) {
        door = true
        pos += 2
        continue
    }
    
    byte b3 = data[pos + 2]
    
    if (!door) {
        // Decode object based on type
        RoomObject obj = DecodeObject(b1, b2, b3, layer)
        tilesObjects.Add(obj)
        pos += 3
    } else {
        // Decode door (2 bytes)
        door_pos = b1 >> 3
        door_dir = b1 & 0x07
        door_type = b2
        // Create door object
        pos += 2
    }
}
```

### Object Saving Process (ZScream)

```csharp
public byte[] getTilesBytes()
{
    List<byte> objectsBytes = new List<byte>();
    List<byte> doorsBytes = new List<byte>();
    
    // 1. Write floor/layout
    byte floorbyte = (byte)((floor2 << 4) + floor1);
    byte layoutbyte = (byte)(layout << 2);
    objectsBytes.Add(floorbyte);
    objectsBytes.Add(layoutbyte);
    
    // 2. Write Layer 1
    foreach (object in layer0_objects) {
        byte[] encoded = EncodeObject(object);
        objectsBytes.AddRange(encoded);
    }
    objectsBytes.Add(0xFF);
    objectsBytes.Add(0xFF);
    
    // 3. Write Layer 2
    foreach (object in layer1_objects) {
        byte[] encoded = EncodeObject(object);
        objectsBytes.AddRange(encoded);
    }
    objectsBytes.Add(0xFF);
    objectsBytes.Add(0xFF);
    
    // 4. Write Layer 3
    foreach (object in layer2_objects) {
        byte[] encoded = EncodeObject(object);
        objectsBytes.AddRange(encoded);
    }
    
    // 5. Write Doors if any
    if (has_doors) {
        objectsBytes.Add(0xF0);
        objectsBytes.Add(0xFF);
        objectsBytes.AddRange(doorsBytes);
    }
    
    objectsBytes.Add(0xFF);
    objectsBytes.Add(0xFF);
    
    return objectsBytes.ToArray();
}
```

### Object Tile Loading (ZScream)

Each object references tile data from ROM tables:

```csharp
// Get tile pointer from subtype table
int tile_ptr = GetSubtypeTablePointer(object_id);
int tile_pos = ROM[tile_ptr] + ROM[tile_ptr+1] << 8;

// Load tiles (each tile is 2 bytes: ID and properties)
for (int i = 0; i < tile_count; i++) {
    byte tile_id = ROM[tile_pos + i*2];
    byte tile_prop = ROM[tile_pos + i*2 + 1];
    
    Tile tile = new Tile(tile_id, tile_prop);
    object.tiles.Add(tile);
}
```

### Object Drawing (ZScream)

```csharp
public void Draw()
{
    // Calculate object dimensions based on size
    width = basewidth + (sizewidth * Size);
    height = baseheight + (sizeheight * Size);
    
    // Draw each tile at computed position
    foreach (Tile tile in tiles) {
        draw_tile(tile, tile_x * 8, tile_y * 8);
    }
}

// Tile drawing writes to background buffers
public void draw_tile(Tile t, int xx, int yy)
{
    int buffer_index = ((xx / 8) + offsetX + nx) + 
                       ((ny + offsetY + (yy / 8)) * 64);
    
    if (Layer == BG1) {
        tilesBg1Buffer[buffer_index] = t.GetTileInfo();
    }
    if (Layer == BG2) {
        tilesBg2Buffer[buffer_index] = t.GetTileInfo();
    }
}
```

---

## ROM Data Structure Reference

### Key ROM Addresses (from ALTTP Disassembly)

```
# Room Headers
room_object_pointer         = 0x874C   # Long pointer to room object data
room_object_layout_pointer  = 0x882D   # Pointer to layout data
kRoomHeaderPointer          = 0xB5DD   # Long pointer to room headers
kRoomHeaderPointerBank      = 0xB5E7   # Bank byte

# Graphics
gfx_groups_pointer          = 0x6237   # Graphics groups
blocks_pointer1-4           = 0x15AFA-0x15B0F  # Block data pointers
dungeons_palettes           = 0xDD734  # Palette data
sprite_blockset_pointer     = 0x5B57   # Sprite blocksets

# Sprites
rooms_sprite_pointer        = 0x4C298  # 2-byte bank pointer
sprites_data                = 0x4D8B0  # Sprite data
sprites_end_data            = 0x4EC9E  # End of sprite data

# Chests
chests_length_pointer       = 0xEBF6   # Chest count
chests_data_pointer1        = 0xEBFB   # Chest data pointer

# Doors
door_gfx_up/down/left/right = 0x4D9E-0x4EC6  # Door graphics
door_pos_up/down/left/right = 0x197E-0x19C6  # Door positions

# Torches, Blocks, Pits
torch_data                  = 0x2736A  # Torch data
torches_length_pointer      = 0x88C1   # Torch count
blocks_length               = 0x8896   # Block count
pit_pointer                 = 0x394AB  # Pit data
pit_count                   = 0x394A6  # Pit count

# Messages
messages_id_dungeon         = 0x3F61D  # Message IDs

# Object Subtypes (Tile Data Tables)
kRoomObjectSubtype1         = 0x8000   # Type 1 table
kRoomObjectSubtype2         = 0x83F0   # Type 2 table
kRoomObjectSubtype3         = 0x84F0   # Type 3 table
kRoomObjectTileAddress      = 0x1B52   # Base tile address
```

### Room Header Format (14 bytes)

```
Offset | Bits      | Description
-------|-----------|------------------------------------------
0      | 0-0       | Is Light (1 = light room, 0 = dark)
0      | 2-4       | Collision type
0      | 5-7       | Background2 type
1      | 0-5       | Palette ID
2      | 0-7       | Blockset ID
3      | 0-7       | Spriteset ID
4      | 0-7       | Effect
5      | 0-7       | Tag 1
6      | 0-7       | Tag 2
7      | 2-3       | Staircase 1 plane
7      | 4-5       | Staircase 2 plane
7      | 6-7       | Staircase 3 plane
8      | 0-1       | Staircase 4 plane
9      | 0-7       | Holewarp room
10     | 0-7       | Staircase 1 room
11     | 0-7       | Staircase 2 room
12     | 0-7       | Staircase 3 room
13     | 0-7       | Staircase 4 room
```

### Object Data Format

```
[Floor/Layout - 2 bytes]
  Byte 0: floor1 (bits 0-3), floor2 (bits 4-7)
  Byte 1: layout (bits 2-4)

[Layer 1 Objects]
  For each object: 3 bytes (Type1/Type2/Type3 encoding)

[Layer 1 End Marker]
  0xFF 0xFF

[Layer 2 Objects]
  For each object: 3 bytes (Type1/Type2/Type3 encoding)

[Layer 2 End Marker]
  0xFF 0xFF

[Layer 3 Objects]
  For each object: 3 bytes (Type1/Type2/Type3 encoding)

[Optional: Door Section]
  0xF0 0xFF (Door section marker)
  For each door: 2 bytes
    Byte 1: door_pos (bits 3-7), door_dir (bits 0-2)
    Byte 2: door_type

[End Marker]
  0xFF 0xFF
```

### Tile Data Format

```
Each Tile16 (metatile) consists of 4 Tile8s (8x8 tiles):
  Word 0: Top-left tile
  Word 1: Top-right tile
  Word 2: Bottom-left tile
  Word 3: Bottom-right tile

Each Word encodes:
  Bits 0-9:   Tile ID (0-1023)
  Bit 10:     Horizontal flip
  Bit 11:     Vertical flip
  Bits 12-14: Palette (0-7)
  Bit 15:     Priority
```

---

## Implementation Tasks

### Phase 1: Core Object System (HIGH PRIORITY)

#### Task 1.1: Object Encoding/Decoding ✅ CRITICAL

**Files**: `src/app/zelda3/dungeon/room_object.cc`, `room_object.h`

**Objectives**:
- Implement `DecodeObjectFromBytes(byte b1, byte b2, byte b3, layer)` 
- Implement `EncodeObjectToBytes(RoomObject) -> [byte, byte, byte]`
- Handle Type1, Type2, Type3 encoding schemes correctly
- Add unit tests for encoding/decoding round-trips

**Dependencies**: None

**Estimated Time**: 3-4 hours

**Implementation Details**:

```cpp
// Add to room_object.h
struct ObjectBytes {
  uint8_t b1;
  uint8_t b2;
  uint8_t b3;
};

// Decode object from 3-byte ROM format
static RoomObject DecodeObjectFromBytes(uint8_t b1, uint8_t b2, uint8_t b3, 
                                         uint8_t layer);

// Encode object to 3-byte ROM format  
ObjectBytes EncodeObjectToBytes() const;

// Determine object type from bytes
static int DetermineObjectType(uint8_t b1, uint8_t b3);
```

#### Task 1.2: Enhanced Object Parsing ✅ CRITICAL

**Files**: `src/app/zelda3/dungeon/room.cc`, `room.h`

**Objectives**:
- Refactor `ParseObjectsFromLocation()` to handle all 3 layers
- Implement door section parsing (0xF0 FF marker)
- Track objects by layer properly
- Handle layer end markers (0xFF FF)

**Dependencies**: Task 1.1

**Estimated Time**: 4-5 hours

**Implementation Details**:

```cpp
// Add to room.h
struct LayeredObjects {
  std::vector<RoomObject> layer1;
  std::vector<RoomObject> layer2;
  std::vector<RoomObject> layer3;
  std::vector<DoorObject> doors;
};

// Enhanced parsing with layer separation
absl::Status ParseLayeredObjects(int objects_location, 
                                  LayeredObjects& out_objects);
```

#### Task 1.3: Object Tile Loading ✅ CRITICAL

**Files**: `src/app/zelda3/dungeon/room_object.cc`, `object_parser.cc`

**Objectives**:
- Implement proper tile loading from subtype tables
- Load tiles based on object ID and subtype
- Cache tile data for performance
- Handle animated tiles

**Dependencies**: Task 1.1

**Estimated Time**: 4-5 hours

**Implementation Details**:

```cpp
// Enhance RoomObject::EnsureTilesLoaded()
absl::Status LoadTilesFromSubtypeTable(int object_id);

// Get subtype table info
SubtypeTableInfo GetSubtypeTable(int object_id);

// Load tile data from ROM
absl::StatusOr<std::vector<gfx::Tile16>> LoadTileData(int tile_ptr);
```

#### Task 1.4: Object Drawing System ⚠️ HIGH PRIORITY

**Files**: `src/app/zelda3/dungeon/object_renderer.cc`

**Objectives**:
- Implement proper object rendering with tiles
- Draw objects at correct positions with proper offsets
- Handle object sizing based on Size parameter
- Render to correct background layer (BG1/BG2/BG3)

**Dependencies**: Task 1.3

**Estimated Time**: 5-6 hours

**Implementation Details**:

```cpp
// Enhanced rendering
absl::Status RenderObjectWithTiles(const RoomObject& object,
                                     const gfx::SnesPalette& palette,
                                     std::vector<uint8_t>& bg1_buffer,
                                     std::vector<uint8_t>& bg2_buffer);

// Compute object dimensions
void ComputeObjectDimensions(RoomObject& object);

// Draw individual tile
void DrawTileToBuffer(const gfx::Tile16& tile, int x, int y,
                      std::vector<uint8_t>& buffer);
```

---

### Phase 2: Editor UI Integration (MEDIUM PRIORITY)

#### Task 2.1: Object Placement System

**Files**: `src/app/editor/dungeon/dungeon_object_interaction.cc`

**Objectives**:
- Implement click-to-place object system
- Snap objects to grid
- Show preview of object before placement
- Validate object placement (limits, collisions)

**Dependencies**: Phase 1 complete

**Estimated Time**: 4-5 hours

#### Task 2.2: Object Selection System

**Files**: `src/app/editor/dungeon/dungeon_object_interaction.cc`

**Objectives**:
- Click to select objects
- Drag to move objects
- Multi-select with box selection
- Show selection highlight

**Dependencies**: Task 2.1

**Estimated Time**: 3-4 hours

#### Task 2.3: Object Properties Editor

**Files**: `src/app/editor/dungeon/dungeon_object_selector.cc`

**Objectives**:
- Show object properties in sidebar
- Edit object properties (X, Y, Size, Layer)
- Change object type
- Real-time preview of changes

**Dependencies**: Task 2.2

**Estimated Time**: 3-4 hours

#### Task 2.4: Layer Management UI

**Files**: `src/app/editor/dungeon/dungeon_editor.cc`

**Objectives**:
- Layer visibility toggles
- Active layer selection
- Layer opacity controls
- Show layer indicators

**Dependencies**: Task 2.1

**Estimated Time**: 2-3 hours

---

### Phase 3: Save System (HIGH PRIORITY)

#### Task 3.1: Room Object Encoding ✅ CRITICAL

**Files**: `src/app/zelda3/dungeon/room.cc`

**Objectives**:
- Implement `SerializeRoomObjects() -> byte[]`
- Encode objects by layer with proper markers
- Handle door section separately
- Maintain floor/layout bytes

**Dependencies**: Task 1.1

**Estimated Time**: 4-5 hours

**Implementation Details**:

```cpp
// Add to room.h
struct SerializedRoomData {
  std::vector<uint8_t> data;
  int size;
  int original_size;
};

// Serialize all room objects to ROM format
absl::StatusOr<SerializedRoomData> SerializeRoomObjects();

// Encode objects for one layer
std::vector<uint8_t> EncodeLayer(const std::vector<RoomObject>& objects);

// Encode door section
std::vector<uint8_t> EncodeDoors(const std::vector<DoorObject>& doors);
```

#### Task 3.2: ROM Writing System

**Files**: `src/app/rom.cc`, `src/app/zelda3/dungeon/room.cc`

**Objectives**:
- Write serialized object data back to ROM
- Update room pointers if size changed
- Handle room size expansion
- Validate written data

**Dependencies**: Task 3.1

**Estimated Time**: 5-6 hours

#### Task 3.3: Save Validation

**Files**: `src/app/zelda3/dungeon/room.cc`

**Objectives**:
- Verify encoded data matches original format
- Check object limits (sprites, doors, chests)
- Validate room size constraints
- Round-trip testing (load -> save -> load)

**Dependencies**: Task 3.2

**Estimated Time**: 3-4 hours

---

### Phase 4: Advanced Features (LOW PRIORITY)

#### Task 4.1: Undo/Redo System

**Files**: `src/app/zelda3/dungeon/dungeon_editor_system.cc`

**Objectives**:
- Track object changes
- Implement undo stack
- Implement redo stack
- Handle multiple operations

**Dependencies**: Phase 2, Phase 3

**Estimated Time**: 5-6 hours

#### Task 4.2: Copy/Paste Objects

**Files**: `src/app/editor/dungeon/dungeon_object_interaction.cc`

**Objectives**:
- Copy selected objects
- Paste with offset
- Duplicate objects
- Paste across rooms

**Dependencies**: Task 2.2

**Estimated Time**: 2-3 hours

#### Task 4.3: Object Library/Templates

**Files**: `src/app/editor/dungeon/dungeon_object_selector.cc`

**Objectives**:
- Save common object configurations
- Load object templates
- Share templates across rooms
- Import/export templates

**Dependencies**: Phase 2

**Estimated Time**: 4-5 hours

#### Task 4.4: Room Import/Export

**Files**: `src/app/zelda3/dungeon/room.cc`

**Objectives**:
- Export room to JSON/binary
- Import room from file
- Batch export/import
- Room comparison tools

**Dependencies**: Phase 3

**Estimated Time**: 5-6 hours

---

## Technical Architecture

### Class Hierarchy

```
Rom
├── Room (296 rooms)
│   ├── RoomHeader (14 bytes)
│   ├── RoomLayout (structural elements)
│   ├── std::vector<RoomObject> (tile objects)
│   │   ├── RoomObject (base)
│   │   ├── Subtype1 (Type1 objects)
│   │   ├── Subtype2 (Type2 objects)
│   │   └── Subtype3 (Type3 objects)
│   ├── std::vector<DoorObject> (doors)
│   ├── std::vector<Sprite> (sprites)
│   └── std::vector<chest_data> (chests)
└── DungeonEditor
    ├── DungeonEditorSystem (editing logic)
    ├── DungeonObjectEditor (object manipulation)
    ├── ObjectRenderer (rendering)
    ├── ObjectParser (parsing)
    ├── DungeonCanvasViewer (display)
    ├── DungeonObjectSelector (object palette)
    └── DungeonObjectInteraction (mouse/keyboard)
```

### Data Flow

```
ROM File
    ↓
[LoadRoomFromRom]
    ↓
Room Header → Room Properties
    ↓
[ParseObjectsFromLocation]
    ↓
Object Bytes (3-byte format)
    ↓
[DecodeObjectFromBytes]
    ↓
RoomObject instances
    ↓
[LoadTilesFromSubtypeTable]
    ↓
Tile data loaded
    ↓
[RenderObjectWithTiles]
    ↓
Display on Canvas
    ↓
User Edits
    ↓
[SerializeRoomObjects]
    ↓
Object Bytes (3-byte format)
    ↓
[WriteToRom]
    ↓
ROM File Updated
```

### Key Algorithms

#### Object Type Detection

```cpp
int DetermineObjectType(uint8_t b1, uint8_t b3) {
  // Type 3: b3 >= 0xF8 or b1 >= 0xFC with b3 >= 0xF0
  if (b3 >= 0xF8) return 3;
  
  // Type 2: b1 >= 0xFC
  if (b1 >= 0xFC) return 2;
  
  // Type 1: default
  return 1;
}
```

#### Object Decoding

```cpp
RoomObject DecodeObjectFromBytes(uint8_t b1, uint8_t b2, uint8_t b3, uint8_t layer) {
  int type = DetermineObjectType(b1, b3);
  
  uint8_t x, y, size;
  uint16_t id;
  
  switch (type) {
    case 1:  // Type1: xxxxxxss yyyyyyss iiiiiiii
      x = (b1 & 0xFC) >> 2;
      y = (b2 & 0xFC) >> 2;
      size = ((b1 & 0x03) << 2) | (b2 & 0x03);
      id = b3;
      break;
      
    case 2:  // Type2: 111111xx xxxxyyyy yyiiiiii
      x = ((b1 & 0x03) << 4) | ((b2 & 0xF0) >> 4);
      y = ((b2 & 0x0F) << 2) | ((b3 & 0xC0) >> 6);
      size = 0;
      id = (b3 & 0x3F) | 0x100;
      break;
      
    case 3:  // Type3: xxxxxxii yyyyyyii 11111iii
      x = (b1 & 0xFC) >> 2;
      y = (b2 & 0xFC) >> 2;
      size = 0;
      id = ((b3 & 0xFF) << 4) | ((b2 & 0x03) << 2) | (b1 & 0x03) | 0x80;
      break;
  }
  
  return RoomObject(id, x, y, size, layer);
}
```

#### Object Encoding

```cpp
ObjectBytes EncodeObjectToBytes(const RoomObject& obj) {
  ObjectBytes bytes;
  
  if (obj.id_ >= 0xF00) {  // Type 3
    bytes.b1 = (obj.x_ << 2) | (obj.id_ & 0x03);
    bytes.b2 = (obj.y_ << 2) | ((obj.id_ >> 2) & 0x03);
    bytes.b3 = (obj.id_ >> 4) & 0xFF;
  }
  else if (obj.id_ >= 0x100) {  // Type 2
    bytes.b1 = 0xFC | ((obj.x_ & 0x30) >> 4);
    bytes.b2 = ((obj.x_ & 0x0F) << 4) | ((obj.y_ & 0x3C) >> 2);
    bytes.b3 = ((obj.y_ & 0x03) << 6) | (obj.id_ & 0x3F);
  }
  else {  // Type 1
    bytes.b1 = (obj.x_ << 2) | ((obj.size_ >> 2) & 0x03);
    bytes.b2 = (obj.y_ << 2) | (obj.size_ & 0x03);
    bytes.b3 = obj.id_;
  }
  
  return bytes;
}
```

---

## Testing Strategy

### Unit Tests

#### Encoding/Decoding Tests

```cpp
TEST(RoomObjectTest, EncodeDecodeType1) {
  // Create Type1 object
  RoomObject obj(0x42, 10, 20, 3, 0);
  
  // Encode
  auto bytes = obj.EncodeObjectToBytes();
  
  // Decode
  auto decoded = RoomObject::DecodeObjectFromBytes(bytes.b1, bytes.b2, bytes.b3, 0);
  
  // Verify
  EXPECT_EQ(decoded.id_, obj.id_);
  EXPECT_EQ(decoded.x_, obj.x_);
  EXPECT_EQ(decoded.y_, obj.y_);
  EXPECT_EQ(decoded.size_, obj.size_);
}

TEST(RoomObjectTest, EncodeDecodeType2) {
  // Type2 object
  RoomObject obj(0x125, 15, 30, 0, 1);
  
  auto bytes = obj.EncodeObjectToBytes();
  auto decoded = RoomObject::DecodeObjectFromBytes(bytes.b1, bytes.b2, bytes.b3, 1);
  
  EXPECT_EQ(decoded.id_, obj.id_);
  EXPECT_EQ(decoded.x_, obj.x_);
  EXPECT_EQ(decoded.y_, obj.y_);
}

TEST(RoomObjectTest, EncodeDecodeType3) {
  // Type3 object (chest)
  RoomObject obj(0xF99, 5, 10, 0, 0);
  
  auto bytes = obj.EncodeObjectToBytes();
  auto decoded = RoomObject::DecodeObjectFromBytes(bytes.b1, bytes.b2, bytes.b3, 0);
  
  EXPECT_EQ(decoded.id_, obj.id_);
  EXPECT_EQ(decoded.x_, obj.x_);
  EXPECT_EQ(decoded.y_, obj.y_);
}
```

#### Room Parsing Tests

```cpp
TEST(RoomTest, ParseObjectsWithLayers) {
  // Create test data with 3 layers
  std::vector<uint8_t> data = {
    0x12,        // floor1/floor2
    0x04,        // layout
    // Layer 1
    0x28, 0x50, 0x10,  // Object 1
    0xFF, 0xFF,        // Layer 1 end
    // Layer 2
    0x30, 0x60, 0x20,  // Object 2
    0xFF, 0xFF,        // Layer 2 end
    0xFF, 0xFF         // End
  };
  
  Room room;
  auto status = room.ParseObjectsFromData(data);
  EXPECT_TRUE(status.ok());
  EXPECT_EQ(room.GetTileObjects().size(), 2);
}
```

### Integration Tests

#### Round-Trip Tests

```cpp
TEST(IntegrationTest, LoadSaveLoadRoom) {
  Rom rom("test_rom.sfc");
  
  // Load room
  Room room1 = LoadRoomFromRom(&rom, 0x001);
  size_t original_objects = room1.GetTileObjects().size();
  
  // Save room
  auto serialized = room1.SerializeRoomObjects();
  EXPECT_TRUE(serialized.ok());
  
  // Write to ROM
  auto write_status = rom.WriteRoomObjects(0x001, serialized.value());
  EXPECT_TRUE(write_status.ok());
  
  // Load again
  Room room2 = LoadRoomFromRom(&rom, 0x001);
  
  // Verify same objects
  EXPECT_EQ(room2.GetTileObjects().size(), original_objects);
  for (size_t i = 0; i < original_objects; i++) {
    EXPECT_EQ(room2.GetTileObject(i).id_, room1.GetTileObject(i).id_);
    EXPECT_EQ(room2.GetTileObject(i).x_, room1.GetTileObject(i).x_);
    EXPECT_EQ(room2.GetTileObject(i).y_, room1.GetTileObject(i).y_);
  }
}
```

### Manual Testing

#### Test Cases

1. **Basic Object Loading**
   - Load room with various object types
   - Verify all objects appear
   - Check object positions match ZScream

2. **Object Placement**
   - Place new objects in empty room
   - Move existing objects
   - Delete objects
   - Verify changes persist

3. **Layer Management**
   - Place objects on different layers
   - Toggle layer visibility
   - Verify layer separation

4. **Save/Load**
   - Edit multiple rooms
   - Save changes
   - Reload ROM
   - Verify all changes present

5. **Edge Cases**
   - Full rooms (many objects)
   - Empty rooms
   - Rooms with only doors
   - Large objects (max size)

---

## References

### Documentation

- **ZScream Source Code**: `/Users/scawful/Code/ZScreamDungeon/ZeldaFullEditor/Rooms/`
- **Yaze Source Code**: `/Users/scawful/Code/yaze/src/app/zelda3/dungeon/`
- **ALTTP Disassembly**: `/Users/scawful/Code/yaze/assets/asm/usdasm/`
- **This Document**: `/Users/scawful/Code/yaze/docs/dungeon_editing_implementation_plan.md`

### Key Files

#### ZScream (Reference)
- `Room.cs` - Main room class with object loading/saving
- `Room_Object.cs` - Object class with encoding/decoding
- `DungeonObjectData.cs` - Object tile data tables
- `GFX.cs` - Graphics management

#### Yaze (Implementation)
- `src/app/zelda3/dungeon/room.h/cc` - Room class
- `src/app/zelda3/dungeon/room_object.h/cc` - RoomObject class
- `src/app/zelda3/dungeon/object_parser.h/cc` - Object parsing
- `src/app/zelda3/dungeon/object_renderer.h/cc` - Object rendering
- `src/app/editor/dungeon/dungeon_editor.h/cc` - Main editor
- `src/app/editor/dungeon/dungeon_object_interaction.h/cc` - Object interaction

### External Resources

- **ALTTP Hacking Resources**: https://github.com/spannerisms/z3doc
- **ROM Map**: https://www.zeldacodes.org/
- **Disassembly**: https://github.com/camthesaxman/zelda3

---

## Appendix: Object ID Reference

### Type 1 Objects (0x00-0xFF)

Common objects with size parameter:
- 0x00-0x09: Walls (ceiling, top, bottom)
- 0x0A-0x20: Diagonal walls
- 0x21: Platform stairs
- 0x22: Rail
- 0x23-0x2E: Pit edges
- 0x2F-0x30: Rail walls
- 0x33: Carpet
- 0x34: Carpet trim
- 0x36-0x3E: Drapes, statues, columns
- 0x3F-0x45: Water edges
- 0x50-0x5F: Dungeon-specific decorations

### Type 2 Objects (0x100-0x1FF)

Special fixed-size objects:
- 0x100-0x10F: Corners (concave, convex)
- 0x110-0x11F: Kinked corners
- 0x120-0x12F: Decorative objects (braziers, statues)
- 0x130-0x13F: Functional objects (blocks, stairs, chests)
- 0x140-0x14F: Interactive objects (switches, torches)

### Type 3 Objects (0xF00-0xFFF)

Extended special objects:
- 0xF99: Small chest
- 0xFB1: Large chest
- 0xF80-0xF8F: Various stairs
- 0xF90-0xF9F: Special effects
- 0xFA0-0xFAF: Interactive elements

---

## Status Tracking

### Current Progress

- [x] Document created
- [ ] Phase 1: Core Object System
  - [ ] Task 1.1: Object Encoding/Decoding
  - [ ] Task 1.2: Enhanced Object Parsing
  - [ ] Task 1.3: Object Tile Loading
  - [ ] Task 1.4: Object Drawing System
- [ ] Phase 2: Editor UI Integration
- [ ] Phase 3: Save System
- [ ] Phase 4: Advanced Features

### Next Steps

1. **Start with Task 1.1** - Implement object encoding/decoding
2. **Create unit tests** - Test all three object types
3. **Verify with ZScream** - Compare results with known working implementation
4. **Proceed to Task 1.2** - Once encoding/decoding is solid

### Timeline Estimate

- **Phase 1**: 16-20 hours (CRITICAL - Must be done first)
- **Phase 2**: 12-16 hours (Required for usability)
- **Phase 3**: 12-15 hours (Required for saving)
- **Phase 4**: 16-20 hours (Nice to have)

**Total**: 56-71 hours (7-9 full working days)

---

*Document Version: 1.0*  
*Last Updated: October 4, 2025*  
*Author: AI Assistant with scawful*

