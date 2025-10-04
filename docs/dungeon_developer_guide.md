# Dungeon Editor Developer Guide

## 1. Implementation Plan

**Goal**: Implement fully functional dungeon room editing based on ZScream's proven approach.

### 1.1. Current State Analysis

-   **Working**: Room header loading, basic class structures (`Room`, `RoomObject`), graphics loading, and UI framework.
-   **Not Working**: Object parsing, drawing, placement, and saving. The core issue is the incomplete implementation of the 3-byte object encoding/decoding scheme and the failure to render object tiles to the background buffer.

### 1.2. ZScream's Approach

ZScream's implementation relies on several key concepts that need to be ported:

-   **Object Encoding**: Three different 3-byte encoding formats for objects based on their ID range (Type 1, Type 2, Type 3).
-   **Layer Separation**: Objects are organized into three layers (BG1, BG2, BG3) separated by `0xFFFF` markers in the ROM.
-   **Object Loading**: A loop parses the object data, decodes each 3-byte entry based on its type, and handles layer and door markers.
-   **Object Drawing**: Each object's constituent tiles are drawn to the correct background buffer (`tilesBg1Buffer` or `tilesBg2Buffer`), which is then used to render the final room image.

### 1.3. Implementation Tasks

1.  **Core Object System (High Priority)**:
    *   Implement the three different object encoding and decoding schemes.
    *   Enhance the room object parser to correctly handle layers and door sections.
    *   Implement tile loading for objects from the ROM's subtype tables.
    *   Create an object drawing system that renders the loaded tiles to the correct background buffer.
2.  **Editor UI Integration (Medium Priority)**:
    *   Implement object placement, selection, and drag-and-drop on the canvas.
    *   Create UI panels for editing object properties (position, size, layer).
3.  **Save System (High Priority)**:
    *   Implement the serialization of room objects back into the 3-byte ROM format, respecting layers and markers.
    *   Implement the logic to write the serialized data back to the ROM, handling potential room size expansion.

---

## 2. Graphics Rendering Pipeline

This section provides a comprehensive analysis of how the dungeon editor renders room graphics.

### 2.1. Architecture Diagram

```
┌─────────────────────────────────────────────────────────────────┐
│                         ROM Data                                 │
│ ┌────────────────────────┐ ┌──────────────────────────────────┐│
│ │ Room Headers           │ │ Dungeon Palettes                 ││
│ │  - Palette ID          │ │  - dungeon_main[id]              ││
│ │  - Blockset ID         │ │  - sprites_aux1[id]              ││
│ └────────────────────────┘ └──────────────────────────────────┘│
└───────────────────────────┬─────────────────────────────────────┘
                            │
                            ▼
┌─────────────────────────────────────────────────────────────────┐
│                  Room Loading (room.cc)                          │
│    ├─ Load 16 blocks (graphics sheets)                          │
│    └─ Copy graphics to a 32KB buffer (`current_gfx16_`)         │
└───────────────────────────┬─────────────────────────────────────┘
                            │
                            ▼
┌─────────────────────────────────────────────────────────────────┐
│          Room::RenderRoomGraphics() Pipeline                     │
│                                                                  │
│  Step 1: DrawFloor() on BG1 and BG2 buffers                     │
│    └─ Populates buffers with repeating floor tile pattern.      │
│                                                                  │
│  Step 2: RenderObjectsToBackground()                            │
│    └─ Iterates room objects and draws their tiles into the BG1/BG2 buffers.│
│                                                                  │
│  Step 3: DrawBackground() on BG1 and BG2                        │
│    └─ Converts tile words in buffers to 8bpp indexed pixel data in a Bitmap.│
│                                                                  │
│  Step 4: Apply Palette & Create/Update Texture                  │
│    ├─ Get dungeon_main palette for the room.                    │
│    ├─ Apply palette to the Bitmap.                              │
│    └─ Create or update an SDL_Texture from the paletted Bitmap. │
└─────────────────────────────────────────────────────────────────┘
```

### 2.2. Blank Canvas Bug: Root Cause Analysis

A critical "blank canvas" bug was diagnosed and fixed. The root cause was multifaceted:

1.  **Missing Object Rendering**: The `RenderObjectsToBackground()` step was missing entirely. The background buffers only contained the floor pattern, so walls and objects were never drawn.
    *   **Fix**: Implemented `RenderObjectsToBackground()` to iterate through all loaded room objects and draw their constituent tiles into the correct background buffer (`bg1` or `bg2`).

2.  **Missing Palette Application**: The indexed-pixel `Bitmap` was being converted to a texture without its palette. SDL had no information to map the color indices to actual colors, resulting in a blank texture.
    *   **Fix**: Added a call to `bitmap.SetPaletteWithTransparent()` *before* the `CreateAndRenderBitmap()` or `UpdateBitmap()` call.

3.  **Incorrect Tile Word Format**: The floor drawing logic was writing only the tile ID to the buffer, discarding the necessary palette information within the 16-bit tile word.
    *   **Fix**: Modified the code to use `gfx::TileInfoToWord()` to ensure the full 16-bit value, including palette index, was written.

4.  **Incorrect Palette ID**: The system was hardcoded to use palette `0` for all rooms, causing most dungeons to render with incorrect, "gargoyle-y" colors.
    *   **Fix**: Modified the code to use the specific `palette` ID loaded from each room's header.

**Key Takeaway**: The entire rendering pipeline, from buffer population to palette application, must be executed in the correct order for graphics to appear.
