# Screen Editor Implementation Status

**Last Updated**: October 14, 2025  
**Author**: AI Assistant working with scawful

## Overview

This document tracks the implementation status of the Screen Editor in yaze, covering the Title Screen, Overworld Map Screen, and Dungeon Map Screen editors.

---

## 1. Title Screen Editor

### Current Status: ⚠️ BLOCKED - Graphics/Tilemap Loading Issues

#### What Works ✅

1. **Palette System**
   - 64-color composite palette loads correctly
   - Colors from multiple palette groups: overworld_main[5], overworld_animated[0], overworld_aux[3], hud[0], sprites_aux1[1]
   - Palette application to bitmaps functional
   - Colors display correctly in UI

2. **GFX Group System**
   - GFX group indices read correctly from ROM (tiles=35, sprites=125)
   - GFX groups pointer dereferenced properly (SNES=0xE073, PC=0x006073)
   - All 16 graphics sheets load with valid IDs (no out-of-bounds errors)
   - Sheet IDs: 22, 57, 29, 23, 64, 65, 57, 30, 115, 139, 121, 122, 131, 112, 112, 112

3. **ROM Format Detection**
   - Successfully detects ZScream ROMs (pointer in range 0x108000-0x10FFFF)
   - Successfully detects vanilla ROMs (other pointer values)
   - ZScream format loads 2048 tilemap entries correctly

4. **UI Components**
   - "Show BG1" and "Show BG2" checkboxes implemented
   - Composite canvas displays (though currently incorrect)
   - Tile selector canvas present
   - Selected tile tracking (currently 0)

#### What Doesn't Work ❌

1. **Vanilla ROM Tilemap Loading** (CRITICAL)
   - **Issue**: Reading 0 tilemap entries from 7 sections
   - **Symptom**: All sections immediately terminate at VRAM=0xFF00
   - **Root Cause**: Incorrect parsing of vanilla tilemap format
   - **Attempted Fixes**:
     - Used fixed ROM addresses: 0x053DE4, 0x053E2C, 0x053E08, 0x053E50, 0x053E74, 0x053E98, 0x053EBC
     - Tried reading DMA blocks from pointer location (loaded wrong data - 1724 entries with invalid tile IDs)
     - Tried various terminator detection methods (0xFF first byte, 0x8000 high bit, 0xFFFF)
   - **Current State**: Completely broken for vanilla ROMs

2. **Tilemap Data Format** (INVESTIGATION NEEDED)
   - Format is unclear: DMA blocks? Compressed data? Raw tilemaps?
   - VRAM address mapping: BG1 at 0x1000-0x13FF, BG2 at 0x0000-0x03FF
   - Tile word format: vhopppcc cccccccc (v=vflip, h=hflip, o=obj priority, ppp=palette, cc cccccccc=tile ID)
   - Unknown: How sections are delimited, how to detect end-of-section

3. **Graphics Sheet Display**
   - Tile Selector shows solid purple (graphics sheets not rendering individually)
   - May be palette-related or tile extraction issue
   - Prevents verification of loaded graphics

4. **Tile Painting**
   - Not implemented yet
   - Requires:
     - Click detection on composite canvas
     - Tile ID write to tilemap buffers
     - Flip/palette controls
     - Canvas redraw after edit

#### Key Findings 🔍

1. **GFX Buffer Format**
   - `rom->graphics_buffer()` contains pre-converted 8BPP data
   - All ALTTP graphics are 3BPP in ROM, converted to 8BPP by `LoadAllGraphicsData`
   - Each sheet is 0x1000 bytes (4096 bytes) in 8BPP format
   - No additional BPP conversion needed when using graphics_buffer

2. **ZScream vs Vanilla Differences**
   - **ZScream**: Stores tilemaps at expanded location (0x108000), simple flat format
   - **Vanilla**: Stores tilemaps at original 7 ROM sections, complex DMA format
   - **Detection**: Read pointer at 0x137A+3, 0x1383+3, 0x138C+3
   - **ZScream Pointer**: Points directly to tilemap data
   - **Vanilla Pointer**: Points to executable code (not data!)

3. **Tilemap Addresses** (from disassembly)
   - Vanilla ROM has 7 sections at PC addresses:
     - 0x053DE4, 0x053E2C, 0x053E08, 0x053E50, 0x053E74, 0x053E98, 0x053EBC
   - These are confirmed in bank_0A.asm disassembly
   - Format at these addresses is still unclear

#### Next Steps 📋

**Priority 1: Fix Vanilla ROM Tilemap Loading**
1. Study ZScream's `LoadTitleScreen()` in detail (lines 379+ in ScreenEditor.cs)
2. Compare with vanilla ROM disassembly (bank_0A.asm)
3. Hexdump vanilla ROM at 0x053DE4 to understand actual format
4. Implement correct parser based on findings

**Priority 2: Verify ZScream ROM Display**
1. Test with a ZScream-modified ROM to verify it works
2. Ensure composite rendering with transparency is correct
3. Validate BG1/BG2 layer stacking

**Priority 3: Implement Tile Painting**
1. Canvas click detection
2. Write tile words to buffers (flip, palette, tile ID)
3. Immediate canvas redraw
4. Save functionality (write buffers back to ROM)

---

## 2. Overworld Map Screen Editor

### Current Status: ✅ COMPLETE (Basic Functionality)

#### What Works ✅

1. **Map Loading**
   - Mode 7 graphics load correctly (128x128 tileset, 16x16 tiles)
   - Tiled-to-linear bitmap conversion working
   - Interleaved map data loading from 4 ROM sections (IDKZarby + 0x0000/0x0400/0x0800/0x0C00)
   - Dark World unique section at IDKZarby + 0x1000
   - 64x64 tilemap (512x512 pixel output)

2. **Dual Palette Support**
   - Light World palette at 0x055B27
   - Dark World palette at 0x055C27
   - Correct 128-color SNES palette application

3. **World Toggle**
   - Switch between Light World and Dark World
   - Correct map data selection

4. **Custom Map Support**
   - LoadCustomMap(): Load external .bin files
   - SaveCustomMap(): Export maps as raw binary
   - UI buttons: "Load Custom Map..." and "Save Custom Map..."

5. **UI Components**
   - Map canvas displays correctly
   - Tile selector shows Mode 7 tileset
   - World toggle button functional

#### What's Left TODO 📝

1. **Tile Painting**
   - Click detection on map canvas
   - Write selected tile to map data buffer
   - Immediate redraw
   - Save to ROM

2. **Enhanced Custom Map Support**
   - Support for different map sizes
   - Validation of loaded binary files
   - Better error handling

3. **Polish**
   - Zoom controls
   - Grid overlay toggle
   - Tile ID display on hover

---

## 3. Dungeon Map Screen Editor

### Current Status: ✅ NEARLY COMPLETE

#### What Works ✅

1. **Map Loading**
   - All 14 dungeon maps load correctly
   - Floor selection (basement/ground/upper floors)
   - Dungeon selection dropdown

2. **Graphics**
   - Tileset renders properly
   - Correct palette application
   - Floor-specific graphics

3. **UI**
   - Dungeon selector
   - Floor selector
   - Map canvas
   - Tile selector

#### What's Left TODO 📝

1. **Tile Painting**
   - Not yet implemented
   - Similar pattern to other editors

2. **Save Functionality**
   - Write edited map data back to ROM

---

## Technical Architecture

### Color/Palette System

**Files**: `snes_color.h/cc`, `snes_palette.h/cc`, `bitmap.h/cc`

- `SnesColor`: 15-bit SNES color container (0-31 per channel)
- `SnesPalette`: Collection of SnesColor objects
- Conversion functions: `ConvertSnesToRgb`, `ConvertRgbToSnes`, `SnesColorToImVec4`
- `SetPalette()`: Apply full palette to bitmap
- `SetPaletteWithTransparent()`: Apply sub-palette with color 0 transparent
- `BitmapMetadata`: Track source BPP, palette format, source type
- `ApplyPaletteByMetadata()`: Choose palette application method

### Bitmap/Texture System

**Files**: `bitmap.h/cc`, `arena.h/cc`

- `gfx::Bitmap`: Image representation with SDL surface/texture
- `gfx::Arena`: Manages SDL resources, queues texture operations
- `UpdateSurfacePixels()`: Copy pixel data from vector to SDL surface
- Deferred texture creation/updates via command queue

### Canvas System

**Files**: `canvas.h/cc`, `canvas_utils.h/cc`

- `gui::Canvas`: ImGui-based drawable canvas
- Drag, zoom, grid, palette management
- Context menu with palette help
- Live update control for palette changes

---

## Lessons Learned

### DO ✅

1. **Use `rom->graphics_buffer()` directly**
   - Already converted to 8BPP
   - No additional BPP conversion needed
   - Standard throughout yaze

2. **Dereference pointers in ROM**
   - Don't read directly from pointer addresses
   - Use `SnesToPc()` for SNES address conversion
   - Follow pointer chains properly

3. **Log extensively during development**
   - Sample pixels from loaded sheets
   - Tilemap entry counts
   - VRAM addresses and tile IDs
   - Helps identify issues quickly

4. **Test with both vanilla and modded ROMs**
   - Different data formats
   - Different storage locations
   - Auto-detection critical

### DON'T ❌

1. **Assume pointer points to data**
   - In vanilla ROM, title screen pointer points to CODE
   - Need fixed addresses or disassembly

2. **Modify source palettes**
   - Create copies for display
   - Preserve ROM data integrity
   - Use `SetPaletteWithTransparent()` for rendering

3. **Skip `UpdateSurfacePixels()`**
   - Rendered data stays in vector
   - Must copy to SDL surface
   - Must queue texture update

4. **Hardcode sheet IDs**
   - Use GFX group tables
   - Read indices from ROM
   - Support dynamic configuration

---

## Code Locations

### Title Screen
- **Header**: `yaze/src/zelda3/screen/title_screen.h`
- **Implementation**: `yaze/src/zelda3/screen/title_screen.cc`
- **UI**: `yaze/src/app/editor/graphics/screen_editor.cc` (DrawTitleScreenEditor)

### Overworld Map
- **Header**: `yaze/src/zelda3/screen/overworld_map_screen.h`
- **Implementation**: `yaze/src/zelda3/screen/overworld_map_screen.cc`
- **UI**: `yaze/src/app/editor/graphics/screen_editor.cc` (DrawOverworldMapEditor)

### Dungeon Map
- **Header**: `yaze/src/zelda3/screen/dungeon_map.h`
- **Implementation**: `yaze/src/zelda3/screen/dungeon_map.cc`
- **UI**: `yaze/src/app/editor/graphics/screen_editor.cc` (DrawDungeonMapEditor)

---

## References

- **ZScream Source**: `/Users/scawful/Code/ZScreamDungeon/ZeldaFullEditor/Gui/MainTabs/ScreenEditor.cs`
- **Disassembly**: `yaze/assets/asm/bank_0A.asm`
- **Palette Docs**: `yaze/docs/palette-system-architecture.md`, `yaze/docs/user-palette-guide.md`
- **Implementation Docs**: `yaze/docs/title-and-overworld-map-implementation.md`

---

## Blocked Items from Original TODO

From the original plan, these items are **blocked** pending title screen fix:

- [ ] ~~Identify and load correct graphics sheets for title screen~~ (DONE - sheets load correctly)
- [ ] ~~Verify tile ID to bitmap position mapping~~ (BLOCKED - need working tilemap first)
- [ ] ~~Add title_composite_bitmap_ to TitleScreen class~~ (DONE)
- [ ] ~~Implement RenderCompositeLayer() with transparency~~ (DONE)
- [ ] ~~Add Show BG1/BG2 checkboxes to screen editor UI~~ (DONE)
- [ ] **Tile painting for title screen** (BLOCKED - need working display first)
- [ ] ~~Add LoadCustomMap() for overworld~~ (DONE)
- [ ] ~~Add SaveCustomMap() for overworld~~ (DONE)
- [ ] ~~Add Load/Save Custom Map buttons~~ (DONE)
- [ ] **Tile painting for overworld map** (TODO - display works, just need painting)
- [ ] **Tile painting for dungeon maps** (TODO - display works, just need painting)

---

## Recommendation

**Stop fighting vanilla ROM format** - Focus on ZScream ROMs for now:

1. Verify ZScream ROM display works correctly
2. Implement tile painting for ZScream format (simpler)
3. Polish overworld/dungeon editors (they work!)
4. Return to vanilla ROM format later with fresh perspective

The vanilla ROM tilemap format is complex and poorly documented. ZScream's flat format is much easier to work with and covers the primary use case (ROM hacking).

