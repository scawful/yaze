# Room Data Persistence & Loading

**Status**: Complete
**Last Updated**: 2025-11-21
**Related Code**: `src/app/editor/dungeon/dungeon_room_loader.cc`, `src/app/editor/dungeon/dungeon_room_loader.h`, `src/zelda3/dungeon/room.cc`, `src/zelda3/dungeon/dungeon_rom_addresses.h`

This document details how dungeon rooms are loaded from and saved to the SNES ROM in YAZE, including the pointer table system, room size calculations, and thread safety considerations.

## Loading Process

The `DungeonRoomLoader` component is responsible for the heavy lifting of reading room data from the ROM. It handles:
*   Decompression of ROM data
*   Pointer table lookups
*   Object parsing
*   Room size calculations for safe editing

### Single Room Loading

**Method**: `DungeonRoomLoader::LoadRoom(int room_id, zelda3::Room& room)`

**Process**:
1.  **Validation**: Checks if ROM is loaded and room ID is in valid range (0x000-0x127)
2.  **ROM Lookup**: Uses pointer table at `kRoomObjectLayoutPointer` to find the room data offset
3.  **Decompression**: Decompresses the room data using SNES compression format
4.  **Object Parsing**: Calls `room.LoadObjects()` to parse the object byte stream into structured `RoomObject` vectors
5.  **Metadata Loading**: Loads room properties (graphics, palette, music) from ROM headers

### Bulk Loading (Multithreaded)

```cpp
absl::Status LoadAllRooms(std::array<zelda3::Room, 0x128>& rooms);
```

To improve startup performance, YAZE loads all 296 rooms in parallel using `std::async`.
*   **Concurrency**: Determines optimal thread count (up to 8).
*   **Batching**: Divides rooms into batches for each thread.
*   **Thread Safety**: Uses `std::mutex` when collecting results (sizes, palettes) into shared vectors.
*   **Performance**: This significantly reduces the initial load time compared to serial loading.

## Data Structure & Size Calculation

ALttP stores dungeon rooms in a compressed format packed into ROM banks. Because rooms vary in size, editing them can change their length, potentially overwriting adjacent data.

### Size Calculation

The loader calculates the size of each room to ensure safe editing:
1.  **Pointers**: Reads the pointer table to find the start address of each room.
2.  **Bank Sorting**: Groups rooms by their ROM bank.
3.  **Delta Calculation**: Sorts pointers within a bank and calculates the difference between adjacent room pointers to determine the available space for each room.
4.  **End of Bank**: The last room in a bank is limited by the bank boundary (0xFFFF).

### Graphics Loading

1.  **Graphics Loading**: `LoadRoomGraphics` reads the blockset configuration from the room header.
2.  **Rendering**: `RenderRoomGraphics` draws the room's tiles into `bg1` and `bg2` buffers.
3.  **Palette**: The loader resolves the palette ID from the header and loads the corresponding SNES palette colors.

## Saving Strategy (Planned/In-Progress)

When saving a room:
1.  **Serialization**: Convert `RoomObject`s back into the game's byte format.
2.  **Size Check**: Compare the new size against the calculated `room_size`.
3.  **Repointing**:
    *   If the new data fits, overwrite in place.
    *   If it exceeds the space, the room must be moved to free space (expanded ROM area), and the pointer table updated.
    *   *Note: Repointing logic is a critical safety feature to prevent ROM corruption.*

## Key Challenges

*   **Bank Boundaries**: SNES addressing is bank-based. Data cannot easily cross bank boundaries.
*   **Shared Data**: Some graphics and palettes are shared between rooms. Modifying a shared resource requires care (or un-sharing/forking the data).
*   **Pointer Tables**: There are multiple pointer tables (headers, objects, sprites, chests) that must be kept in sync.
