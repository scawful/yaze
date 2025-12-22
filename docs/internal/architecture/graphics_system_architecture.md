# Graphics System Architecture

**Status**: Complete
**Last Updated**: 2025-11-21
**Related Code**: `src/app/gfx/`, `src/app/editor/graphics/`

This document outlines the architecture of the Graphics System in YAZE, including resource management, compression pipelines, and rendering workflows.

## Overview

The graphics system is designed to handle SNES-specific image formats (indexed color, 2BPP/3BPP) while efficiently rendering them using modern hardware acceleration via SDL2. It uses a centralized resource manager (`Arena`) to pool resources and manage lifecycle.

## Core Components

### 1. The Arena (`src/app/gfx/resource/arena.h`)

The `Arena` is a singleton class that acts as the central resource manager for all graphics.

**Responsibilities**:
*   **Resource Management**: Manages the lifecycle of `SDL_Texture` and `SDL_Surface` objects using RAII wrappers with custom deleters
*   **Graphics Sheets**: Holds a fixed array of 223 `Bitmap` objects representing the game's complete graphics space (indexed 0-222)
*   **Background Buffers**: Manages `BackgroundBuffer`s for SNES BG1 and BG2 layer rendering
*   **Deferred Rendering**: Implements a command queue (`QueueTextureCommand`) to batch texture creation/updates, preventing UI freezes during heavy loads
*   **Memory Pooling**: Reuses textures and surfaces to minimize allocation overhead

**Key Methods**:
*   `QueueTextureCommand(type, bitmap)`: Queue a texture operation for batch processing
*   `ProcessTextureQueue(renderer)`: Process all queued texture commands
*   `NotifySheetModified(sheet_index)`: Notify when a graphics sheet changes to synchronize editors
*   `gfx_sheets()`: Get all 223 graphics sheets
*   `mutable_gfx_sheet(index)`: Get mutable reference to a specific sheet

### 2. Bitmap (`src/app/gfx/core/bitmap.h`)

Represents a single graphics sheet or image optimized for SNES ROM editing.

**Key Features**:
*   **Data Storage**: Stores raw pixel data as `std::vector<uint8_t>` (indices into a palette)
*   **Palette**: Each bitmap owns a `SnesPalette` (256 colors maximum)
*   **Texture Management**: Manages an `SDL_Texture` handle and syncs CPU pixel data to GPU
*   **Dirty Tracking**: Tracks modified regions to minimize texture upload bandwidth
*   **Tile Extraction**: Provides methods like `Get8x8Tile()`, `Get16x16Tile()` for SNES tile operations

**Important Synchronization Rules**:
*   Never modify `SDL_Texture` directly - always modify the `Bitmap` data
*   Use `set_data()` for bulk updates to keep CPU and GPU in sync
*   Use `WriteToPixel()` for single-pixel modifications
*   Call `UpdateTexture()` to sync changes to GPU

### 3. Graphics Editor (`src/app/editor/graphics/graphics_editor.cc`)

The primary UI for viewing and modifying graphics.

*   **Sheet Editor**: Pixel-level editing of all 223 sheets
*   **Palette Integration**: Fetches palette groups from the ROM (Overworld, Dungeon, Sprites) to render sheets correctly
*   **Tools**: Pencil, Fill, Select, Zoom
*   **Real-time Display**: Uses `Canvas` class for drawing interface

### 4. IRenderer Interface (`src/app/gfx/backend/irenderer.h`)

Abstract interface for the rendering backend (currently implemented by `SdlRenderer`). This decouples graphics logic from SDL-specific calls, enabling:
*   Testing with mock renderers
*   Future backend swaps (e.g., Vulkan, Metal)

## Rendering Pipeline

### 1. Loading Phase

**Source**: ROM compressed data
**Process**:
1.  Iterates through all 223 sheet indices
2.  Determines format based on index range (2BPP, 3BPP compressed, 3BPP uncompressed)
3.  Calls decompression functions
4.  Converts to internal 8-bit indexed format
5.  Stores result in `Arena.gfx_sheets_`

**Performance**: Uses deferred loading via texture queue to avoid blocking

### 2. Composition Phase (Rooms/Overworld)

**Process**:
1.  Room/Overworld logic draws tiles from `gfx_sheets` into a `BackgroundBuffer` (wraps a `Bitmap`)
2.  This drawing happens on CPU, manipulating indexed pixel data
3.  Each room/map maintains its own `Bitmap` of rendered data

**Key Classes**:
*   `BackgroundBuffer`: Manages BG1 and BG2 layer rendering for a single room/area
*   Methods like `Room::RenderRoomGraphics()` handle composition

### 3. Texture Update Phase

**Process**:
1.  Editor checks if bitmaps are marked "dirty" (modified since last render)
2.  Modified bitmaps queue a `TextureCommand::UPDATE` to Arena
3.  Arena processes queue, uploading pixel data to SDL textures
4.  This batching avoids per-frame texture uploads

### 4. Display Phase

**Process**:
1.  `Canvas` or UI elements request the `SDL_Texture` from a `Bitmap`
2.  Texture is rendered to screen using ImGui or direct SDL calls
3.  Grid, overlays, and selection highlights are drawn on top

## Compression Pipeline

YAZE uses the **LC-LZ2** algorithm (often called "Hyrule Magic" compression) for ROM I/O.

### Supported Formats

| Format | Sheets | Bits Per Pixel | Usage | Location |
|--------|--------|--------|-------|----------|
| 3BPP (Compressed) | 0-112, 127-217 | 3 | Most graphics | Standard ROM |
| 2BPP (Compressed) | 113-114, 218-222 | 2 | HUD, Fonts, Effects | Standard ROM |
| 3BPP (Uncompressed) | 115-126 | 3 | Link Player Sprites | 0x080000 |

### Loading Process

**Entry Point**: `src/app/rom.cc:Rom::LoadFromFile()`

1.  Iterates through all 223 sheet indices
2.  Determines format based on index range
3.  Calls `gfx::lc_lz2::DecompressV2()` (or `DecompressV1()` for compatibility)
4.  For uncompressed sheets (115-126), copies raw data directly
5.  Converts result to internal 8-bit indexed format
6.  Stores in `Arena.gfx_sheets_[index]`

### Saving Process

**Process**:
1.  Get mutable reference: `auto& sheet = Arena::Get().mutable_gfx_sheet(index)`
2.  Make modifications to `sheet.mutable_data()`
3.  Notify Arena: `Arena::Get().NotifySheetModified(index)`
4.  When saving ROM:
    *   Convert 8-bit indexed data back to 2BPP/3BPP format
    *   Compress using `gfx::lc_lz2::CompressV3()`
    *   Write to ROM, handling pointer table updates if sizes change

## Link Graphics (Player Sprites)

**Location**: ROM offset `0x080000`
**Format**: Uncompressed 3BPP
**Sheet Indices**: 115-126
**Editor**: `GraphicsEditor` provides a "Player Animations" view
**Structure**: Sheets are assembled into poses using OAM (Object Attribute Memory) tables

## Canvas Interactions

The `Canvas` class (`src/app/gui/canvas/canvas.h`) is the primary rendering engine.

**Drawing Operations**:
*   `DrawBitmap()`: Renders a sheet texture to the canvas
*   `DrawSolidTilePainter()`: Preview of brush before commit
*   `DrawTileOnBitmap()`: Commits pixel changes to Bitmap data

**Selection and Tools**:
*   `DrawSelectRect()`: Rectangular region selection
*   Context Menu: Right-click for Zoom, Grid, view resets

**Coordinate Systems**:
*   Canvas Pixels: Unscaled (128-512 range depending on sheet)
*   Screen Pixels: Scaled by zoom level
*   Tile Coordinates: 8x8 or 16x16 tiles for SNES editing

## Best Practices

*   **Never modify `SDL_Texture` directly**: Always modify the `Bitmap` data and call `UpdateTexture()` or queue it
*   **Use `QueueTextureCommand`**: For bulk updates, queue commands to avoid stalling the main thread
*   **Respect Palettes**: Remember that `Bitmap` data is just indices. Visual result depends on the associated `SnesPalette`
*   **Sheet Modification**: When modifying a global graphics sheet, notify `Arena` via `NotifySheetModified()` to propagate changes to all editors
*   **Deferred Loading**: Always use the texture queue system for heavy operations to prevent UI freezes

## Future Improvements

*   **Vulkan/Metal Backend**: The `IRenderer` interface allows for potentially swapping SDL2 for a more modern API
*   **Compute Shaders**: Palette swapping could potentially be moved to GPU using shaders instead of CPU-side pixel manipulation
*   **Streaming Graphics**: Load/unload sheets on demand for very large ROM patches
