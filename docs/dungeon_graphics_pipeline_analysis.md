# Dungeon Graphics Rendering Pipeline Analysis

## Overview

This document provides a comprehensive analysis of how the YAZE dungeon editor renders room graphics, including the interaction between bitmaps, arena buffers, palettes, and palette groups.

## Architecture Diagram

```
┌─────────────────────────────────────────────────────────────────┐
│                         ROM Data                                 │
│ ┌────────────────────────┐ ┌──────────────────────────────────┐│
│ │ Room Headers           │ │ Dungeon Palettes                 ││
│ │  - Palette ID          │ │  - dungeon_main[id][180 colors] ││
│ │  - Blockset ID         │ │  - sprites_aux1[id][colors]      ││
│ │  - Spriteset ID        │ │  - Palette Groups                ││
│ │  - Background ID       │ │                                  ││
│ └────────────────────────┘ └──────────────────────────────────┘│
└───────────────────────────┬─────────────────────────────────────┘
                            │
                            ▼
┌─────────────────────────────────────────────────────────────────┐
│                  Room Loading (room.cc)                          │
│                                                                  │
│  LoadRoomFromRom() →  LoadRoomGraphics() → Copy RoomGraphics   │
│    ├─ Load 16 blocks (graphics sheets)                          │
│    ├─ blocks[0-7]: Main blockset                                │
│    ├─ blocks[8-11]: Static sprites (fairies, pots, etc.)        │
│    └─ blocks[12-15]: Spriteset sprites                          │
└───────────────────────────┬─────────────────────────────────────┘
                            │
                            ▼
┌─────────────────────────────────────────────────────────────────┐
│                Graphics Arena (arena.h/.cc)                      │
│                                                                  │
│  ┌──────────────────┐  ┌──────────────────┐                    │
│  │ gfx_sheets_[223] │  │ Background       │                    │
│  │ (Bitmap objects) │  │ Buffers          │                    │
│  │                  │  │  - bg1_          │                    │
│  │  Each holds:     │  │  - bg2_          │                    │
│  │  - Pixel data    │  │                  │                    │
│  │  - SDL Surface   │  │  layer1_buffer_  │                    │
│  │  - SDL Texture   │  │  layer2_buffer_  │                    │
│  │  - Palette       │  │  [64x64 = 4096   │                    │
│  └──────────────────┘  │   tile words]    │                    │
│                        └──────────────────┘                    │
└───────────────────────────┬─────────────────────────────────────┘
                            │
                            ▼
┌─────────────────────────────────────────────────────────────────┐
│          Room::RenderRoomGraphics() Pipeline                     │
│                                                                  │
│  Step 1: CopyRoomGraphicsToBuffer()                             │
│    └─ Copy 16 blocks to current_gfx16_[32768] buffer           │
│                                                                  │
│  Step 2: DrawFloor() on both BG1 and BG2                        │
│    ├─ Read floor tile IDs from ROM                              │
│    ├─ Create TileInfo objects (id, palette, mirror flags)       │
│    └─ SetTileAt() in background buffers (repeating pattern)     │
│                                                                  │
│  Step 3: RenderObjectsToBackground() ⚠️ NEW                     │
│    ├─ Iterate through tile_objects_                             │
│    ├─ For each object, get its Tile16 array                     │
│    ├─ Each Tile16 contains 4 TileInfo (8x8 tiles)               │
│    ├─ Convert TileInfo → 16-bit word:                           │
│    │    (vflip<<15) | (hflip<<14) | (palette<<10) | tile_id    │
│    └─ SetTileAt() in correct layer (BG1 or BG2)                 │
│                                                                  │
│  Step 4: DrawBackground() on both BG1 and BG2                   │
│    ├─ BackgroundBuffer::DrawBackground(current_gfx16_)          │
│    ├─ For each tile in buffer_[4096]:                           │
│    │    ├─ Extract 16-bit word                                  │
│    │    ├─ WordToTileInfo() → TileInfo                          │
│    │    └─ DrawTile() → Write 8x8 pixels to bitmap_.data_      │
│    └─ bitmap_.Create(512, 512, 8, pixel_data)                   │
│                                                                  │
│  Step 5: Apply Palette & Create/Update Texture                  │
│    ├─ Get dungeon_main palette for this room                    │
│    ├─ bitmap_.SetPaletteWithTransparent(palette, 0)             │
│    ├─ If first time:                                             │
│    │    └─ CreateAndRenderBitmap() → Create SDL_Texture         │
│    └─ Else:                                                      │
│         └─ UpdateBitmap() → Update existing SDL_Texture         │
└─────────────────────────────────────────────────────────────────┘
```

## Component Breakdown

### 1. Arena (gfx/arena.h/.cc)

**Purpose**: Global graphics resource manager using singleton pattern.

**Key Members**:
- `gfx_sheets_[223]`: Array of Bitmap objects (one for each graphics sheet in ROM)
- `bg1_`, `bg2_`: BackgroundBuffer objects for SNES layer 1 and layer 2
- `layer1_buffer_[4096]`, `layer2_buffer_[4096]`: Raw tile word arrays

**Responsibilities**:
- Resource pooling for SDL textures and surfaces
- Batch texture updates for performance
- Centralized access to graphics sheets

### 2. BackgroundBuffer (gfx/background_buffer.h/.cc)

**Purpose**: Manages a single SNES background layer (512x512 pixels = 64x64 tiles).

**Key Members**:
- `buffer_[4096]`: Array of 16-bit tile words (vflip|hflip|palette|tile_id)
- `bitmap_`: The Bitmap object that holds the rendered pixel data
- `width_`, `height_`: Dimensions in pixels (typically 512x512)

**Key Methods**:

#### `SetTileAt(int x, int y, uint16_t value)`
```cpp
// Sets a tile word at tile coordinates (x, y)
// x, y are in tile units (0-63)
buffer_[y * tiles_w + x] = value;
```

#### `DrawBackground(std::span<uint8_t> gfx16_data)`
```cpp
// Renders all tiles in buffer_ to bitmap_
1. Create bitmap (512x512, 8bpp)
2. For each tile (64x64 grid):
   - Get tile word from buffer_[xx + yy * 64]
   - WordToTileInfo() to extract: id, palette, hflip, vflip
   - DrawTile() writes 64 pixels to bitmap at correct position
```

#### `DrawFloor()`
```cpp
// Special case: Draws floor pattern from ROM data
1. Read 8 floor tile IDs from ROM (2 rows of 4)
2. Repeat pattern across entire 64x64 grid
3. SetTileAt() for each position
```

### 3. Bitmap (gfx/bitmap.h/.cc)

**Purpose**: Represents a 2D image with SNES-specific features.

**Key Members**:
- `data_[width * height]`: Raw indexed pixel data (palette indices)
- `palette_`: SnesPalette object (15-bit RGB colors)
- `surface_`: SDL_Surface for pixel manipulation
- `texture_`: SDL_Texture for rendering to screen
- `active_`, `modified_`: State flags

**Key Methods**:

#### `Create(int w, int h, int depth, vector<uint8_t> data)`
```cpp
// Initialize bitmap with pixel data
width_ = w;
height_ = h;
depth_ = depth;  // Usually 8 (bits per pixel)
data_ = data;
active_ = true;
```

#### `SetPaletteWithTransparent(SnesPalette palette, size_t index)`
```cpp
// Apply palette and make color[index] transparent
palette_ = palette;
// Update surface_->format->palette with SDL_Colors
// Set color[index] alpha to 0 for transparency
```

#### `CreateTexture(SDL_Renderer* renderer)` / `UpdateTexture()`
```cpp
// Convert surface_ to hardware-accelerated texture_
texture_ = SDL_CreateTextureFromSurface(renderer, surface_);
// or
SDL_UpdateTexture(texture_, nullptr, surface_->pixels, surface_->pitch);
```

### 4. Room (zelda3/dungeon/room.h/.cc)

**Purpose**: Represents a single dungeon room with all its data.

**Key Members**:
- `room_id_`: Room index (0-295)
- `palette`, `blockset`, `spriteset`: IDs from ROM header
- `blocks_[16]`: Graphics sheet indices for this room
- `current_gfx16_[32768]`: Raw graphics data for this room
- `tile_objects_`: Vector of RoomObject instances
- `rom_`: Pointer to ROM data

**Key Methods**:

#### `LoadRoomGraphics(uint8_t entrance_blockset)`
```cpp
// Load 16 graphics sheets for this room
blocks_[0-7]:   Main blockset sheets
blocks_[8-11]:  Static sprites (fairies, pots, etc.)
blocks_[12-15]: Spriteset sprites
```

#### `CopyRoomGraphicsToBuffer()`
```cpp
// Copy 16 blocks of 2KB each into current_gfx16_[32KB]
for (int i = 0; i < 16; i++) {
  int block = blocks_[i];
  memcpy(current_gfx16_ + i*2048, 
         graphics_buffer[block*2048], 
         2048);
}
LoadAnimatedGraphics();  // Overlay animated frames
```

#### `RenderRoomGraphics()` ⭐ **Main Rendering Method**
```cpp
void Room::RenderRoomGraphics() {
  // Step 1: Copy graphics data from ROM
  CopyRoomGraphicsToBuffer();  
  
  // Step 2: Draw floor pattern
  Arena::Get().bg1().DrawFloor(rom->vector(), tile_address, 
                                tile_address_floor, floor1_graphics_);
  Arena::Get().bg2().DrawFloor(rom->vector(), tile_address, 
                                tile_address_floor, floor2_graphics_);
  
  // Step 3: ⚠️ NEW - Render room objects to buffers
  RenderObjectsToBackground();
  
  // Step 4: Convert tile buffers to bitmaps
  Arena::Get().bg1().DrawBackground(span<uint8_t>(current_gfx16_));
  Arena::Get().bg2().DrawBackground(span<uint8_t>(current_gfx16_));
  
  // Step 5: Apply palette and create/update textures
  auto palette = rom->palette_group().dungeon_main[palette_id][0];
  if (!Arena::Get().bg1().bitmap().is_active()) {
    Renderer::Get().CreateAndRenderBitmap(..., Arena::Get().bg1().bitmap(), palette);
    Renderer::Get().CreateAndRenderBitmap(..., Arena::Get().bg2().bitmap(), palette);
  } else {
    Renderer::Get().UpdateBitmap(&Arena::Get().bg1().bitmap());
    Renderer::Get().UpdateBitmap(&Arena::Get().bg2().bitmap());
  }
}
```

#### `RenderObjectsToBackground()` ⚠️ **Critical New Method** ✅ **Fixed**
```cpp
void Room::RenderObjectsToBackground() {
  auto& bg1 = Arena::Get().bg1();
  auto& bg2 = Arena::Get().bg2();
  
  for (const auto& obj : tile_objects_) {
    // Ensure object has tiles loaded
    obj.EnsureTilesLoaded();
    auto tiles_result = obj.GetTiles();  // Returns span<const Tile16>
    
    // Calculate the width of the object in Tile16 units
    // Most objects are arranged in a grid, typically 1-8 tiles wide
    int tiles_wide = 1;
    if (tiles.size() > 1) {
      // Try to determine optimal layout based on tile count
      // Common patterns: 1x1, 2x2, 4x1, 2x4, 4x4, 8x1, etc.
      int sq = static_cast<int>(std::sqrt(tiles.size()));
      if (sq * sq == tiles.size()) {
        tiles_wide = sq;  // Perfect square (4, 9, 16, etc.)
      } else if (tiles.size() <= 4) {
        tiles_wide = tiles.size();  // Small objects laid out horizontally
      } else {
        // For larger objects, try common widths (4 or 8)
        tiles_wide = (tiles.size() >= 8) ? 8 : 4;
      }
    }
    
    // Each Tile16 is 16x16 (4 TileInfo of 8x8)
    for (size_t i = 0; i < tiles.size(); i++) {
      const auto& tile16 = tiles[i];
      
      // Calculate base position using calculated width (in 8x8 units)
      int base_x = obj.x_ + ((i % tiles_wide) * 2);
      int base_y = obj.y_ + ((i / tiles_wide) * 2);
      
      // Tile16.tiles_info[4] contains the 4 sub-tiles:
      // [0][1]  (top-left, top-right)
      // [2][3]  (bottom-left, bottom-right)
      for (int sub = 0; sub < 4; sub++) {
        int tile_x = base_x + (sub % 2);
        int tile_y = base_y + (sub / 2);
        
        // Bounds check
        if (tile_x < 0 || tile_x >= 64 || tile_y < 0 || tile_y >= 64) {
          continue;
        }
        
        // Convert TileInfo to 16-bit word
        uint16_t word = TileInfoToWord(tile16.tiles_info[sub]);
        
        // Set in correct layer
        bool is_bg2 = (obj.layer_ == RoomObject::LayerType::BG2);
        auto& buffer = is_bg2 ? bg2 : bg1;
        buffer.SetTileAt(tile_x, tile_y, word);
      }
    }
  }
}
```

## Palette System

### Palette Hierarchy

```
ROM Palette Data
│
├─ dungeon_main[palette_group_id]
│   └─ Large palette (180 colors)
│       └─ Split into PaletteGroup:
│           ├─ palette(0): Main dungeon palette
│           ├─ palette(1): Alternate palette 1
│           └─ palette(2-n): More palettes
│
└─ sprites_aux1[palette_id]
    └─ Sprite auxiliary palettes
```

### Palette Loading Flow

```cpp
// In DungeonEditor::Load()
auto dungeon_pal_group = rom->palette_group().dungeon_main;
full_palette_ = dungeon_pal_group[current_palette_group_id_];
ASSIGN_OR_RETURN(current_palette_group_,
                 CreatePaletteGroupFromLargePalette(full_palette_));

// In DungeonCanvasViewer::LoadAndRenderRoomGraphics()
auto dungeon_palette_ptr = rom->paletteset_ids[room.palette][0];
auto palette_id = rom->ReadWord(0xDEC4B + dungeon_palette_ptr);
current_palette_group_id_ = palette_id.value() / 180;
full_palette = rom->palette_group().dungeon_main[current_palette_group_id_];

// Apply to graphics sheets
for (int i = 0; i < 8; i++) {  // BG1 layers
  int block = room.blocks()[i];
  Arena::Get().gfx_sheets()[block].SetPaletteWithTransparent(
      current_palette_group_[current_palette_id_], 0);
}

for (int i = 8; i < 16; i++) {  // BG2 layers (sprites)
  int block = room.blocks()[i];
  Arena::Get().gfx_sheets()[block].SetPaletteWithTransparent(
      sprites_aux1_pal_group[current_palette_id_], 0);
}
```

---

# Appendix: Blank Canvas Bug - Detailed Root Cause Analysis

This section, merged from `dungeon_canvas_blank_fix.md`, details the investigation and resolution of the blank canvas bug.

## Problem

The DungeonEditor canvas displayed as blank white despite the rendering pipeline appearing to execute correctly.

## Diagnostic Output Analysis

Using a comprehensive diagnostic system, the data flow was traced through 8 steps. Steps 1-6 (ROM loading, buffer population, bitmap creation, texture creation) were all passing. The failure was in Step 7.

### ❌ Step 7: PALETTE MISSING

```
=== Step 7: Palette ===
  Palette size: 0 colors  ❌❌❌ ROOT CAUSE!
```

## Root Cause Analysis

Three distinct issues were identified and fixed in sequence:

### Cause 1: Missing Palette Application

**The palette was never applied to the bitmap objects!** The bitmaps contained indexed pixel data (e.g., color indices 8, 9, 12), but without a color palette, SDL couldn't map these indices to actual colors, resulting in a blank texture.

**The Fix (Location: `src/app/zelda3/dungeon/room.cc:319-322`)**:
Added `SetPaletteWithTransparent()` calls to the bitmaps *before* creating the SDL textures. This ensures the renderer has the color information it needs.

```cpp
// CRITICAL: Apply palette to bitmaps BEFORE creating/updating textures
bg1_bmp.SetPaletteWithTransparent(bg1_palette, 0);
bg2_bmp.SetPaletteWithTransparent(bg1_palette, 0);

// Now create/update textures (palette is already set in bitmap)
Renderer::Get().CreateAndRenderBitmap(..., bg1_bmp, bg1_palette);
```

### Cause 2: All Black Canvas (Incorrect Tile Word)

After the first fix, the canvas was all black. This was because `DrawFloor()` was only passing the tile ID to the background buffer, losing the crucial palette information.

**The Fix**:
Converted the `TileInfo` struct to a full 16-bit word (which includes palette bits) before writing it to the buffer.

```cpp
// CORRECT: Convert TileInfo to word with all metadata
uint16_t word1 = gfx::TileInfoToWord(floorTile1);
SetTileAt((xx * 4), (yy * 2), word1);  // ✅ Now includes palette!
```

### Cause 3: Wrong Palette (All Rooms Look "Gargoyle-y")

After the second fix, all rooms rendered, but with the same incorrect palette (from the first dungeon).

**The Fix**:
Used the room's specific `palette` ID loaded from the ROM header instead of hardcoding palette index `0`.

```cpp
// ✅ CORRECT: Use the room's palette ID
auto bg1_palette =
    rom()->mutable_palette_group()->get_group("dungeon_main")[0].palette(palette);
```

## Key Takeaway

**Always apply a palette to indexed-color bitmaps before creating SDL textures.** The rendering pipeline requires this step to translate color indices into visible pixels. Each subsequent fix ensured the *correct* palette information was being passed at each stage.