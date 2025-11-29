# ZScreamDungeon Codebase Analysis for YAZE Feature Parity

**Date:** November 22, 2025
**Source Codebase:** `ZScreamDungeon` (C#)
**Target Project:** `yaze` (C++)

## 1. Executive Summary
The ZScreamDungeon codebase provides a comprehensive roadmap for implementing feature parity in the 'yaze' project. The core architecture revolves around direct manipulation of a byte array containing the game's ROM data. Data is read from the ROM in a structured way, with `Constants.cs` providing the necessary memory addresses.

The system uses a custom object factory pattern to parse variable-length object streams from the ROM and instantiate specific C# classes for rendering. Graphics are decompressed on startup and cached, then composited into a final image buffer during the room loading process.

## 2. Core Architecture

-   **ROM Handling:** The entire ROM is loaded into a static `byte[]` array in `ROM.cs`. All reads/writes happen directly on this buffer.
-   **Address Mapping:** `Constants.cs` serves as the "Rosetta Stone," mapping high-level concepts (headers, sprite pointers, etc.) to physical ROM addresses.
-   **Room Loading:** Rooms are not contiguous blocks. `Room.cs` orchestrates loading from multiple scattered tables (headers, object data, sprites) based on the room index.

## 3. Key Data Structures

### Rooms (`Room.cs`)
A room is composed of:
-   **Header (14 bytes):** Contains palettes, blocksets, effect flags, and collision modes.
-   **Object Data:** A variable-length stream of bytes. This is **not** a fixed array. It uses a custom packing format where the first byte determines the object type (Type 1, 2, or 3) and subsequent bytes define position and size.
-   **Sprite Data:** A separate list of sprites, typically 3 bytes per entry (ID, X, Y), loaded via `addSprites()`.

### Room Objects (`Room_Object.cs`)
-   Base class for all interactive elements.
-   **Factory Pattern:** The `loadTilesObjects()` method in `Room.cs` parses the byte stream and calls `addObject()`, which switches on the object ID to instantiate the correct subclass (e.g., `Room_Object_Door`, `Room_Object_Chest`).

### Graphics (`GFX.cs`)
-   **Storage:** Graphics are stored compressed in the ROM.
-   **Decompression:** Uses a custom LZ-style algorithm (`std_nintendo_decompress`).
-   **Decoding:** Raw bitplanes (2bpp/3bpp) are decoded into a 4bpp format usable by the editor's renderer.

## 4. Rendering Pipeline

1.  **Initialization:** On startup, all graphics packs are decompressed and decoded into a master `allgfxBitmap`.
2.  **Room Load:** When entering a room, the specific graphics needed for that room (based on the header) are copied into `currentgfx16Bitmap`.
3.  **Object Rasterization:**
    -   `Room.cs` iterates through all loaded `Room_Object` instances.
    -   It calls `obj.Draw()`.
    -   Crucially, `Draw()` **does not** render to the screen. It writes tile IDs, palette indices, and flip flags into two 2D arrays: `GFX.tilesBg1Buffer` and `GFX.tilesBg2Buffer` (representing the SNES background layers).
4.  **Final Composition:**
    -   `GFX.DrawBG1()` and `GFX.DrawBG2()` iterate through the buffers.
    -   They look up the actual pixel data in `currentgfx16Bitmap` based on the tile IDs.
    -   The pixels are written to the final `Bitmap` displayed in the UI.

## 5. Implementation Recommendations for YAZE

To achieve feature parity, `yaze` should:

1.  **Replicate the Parsing Logic:** The `loadTilesObjects()` method in `Room.cs` is the critical path. Porting this logic to C++ is essential for correctly interpreting the room object data stream.
2.  **Port the Decompressor:** The `std_nintendo_decompress` algorithm in `Decompress.cs` must be ported to C++ to read graphics and map data.
3.  **Adopt the Buffer-Based Rendering:** Instead of trying to render objects directly to a texture, use an intermediate "Tilemap Buffer" (similar to `tilesBg1Buffer`). This accurately simulates the SNES PPU architecture where objects are just collections of tilemap entries.
4.  **Constants Mirroring:** Create a C++ header that mirrors `Constants.cs`. Do not try to derive these addresses algorithmically; the hardcoded values are necessary for compatibility.

## 6. Key File References

| File Path (Relative to ZScreamDungeon) | Key Responsibility | Important Symbols |
| :--- | :--- | :--- |
| `ZeldaFullEditor/ROM.cs` | Raw ROM access | `ROM.DATA`, `ReadByte`, `WriteByte` |
| `ZeldaFullEditor/Constants.cs` | Address definitions | `room_header_pointer`, `room_object_pointer` |
| `ZeldaFullEditor/Rooms/Room.cs` | Room parsing & orchestration | `loadTilesObjects()`, `addSprites()`, `addObject()` |
| `ZeldaFullEditor/Rooms/Room_Object.cs` | Object base class | `Draw()`, `draw_tile()` |
| `ZeldaFullEditor/GFX.cs` | Graphics pipeline | `CreateAllGfxData()`, `DrawBG1()`, `tilesBg1Buffer` |
| `ZeldaFullEditor/ZCompressLibrary/Decompress.cs` | Data decompression | `std_nintendo_decompress()` |
