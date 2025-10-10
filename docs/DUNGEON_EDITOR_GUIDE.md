# YAZE Dungeon Editor: Complete Guide

**Last Updated**: October 9, 2025  
**Status**: PRODUCTION READY - Core features stable, tested, and functional

---

## Table of Contents
- [Overview](#overview)
- [Current Status](#current-status)
- [Architecture](#architecture)
- [Quick Start](#quick-start)
- [Core Features](#core-features)
- [Technical Details](#technical-details)
- [Testing](#testing)
- [Troubleshooting](#troubleshooting)
- [ROM Internals](#rom-internals)
- [Reference](#reference)

---

## Overview

The Dungeon Editor uses a modern card-based architecture for editing dungeon rooms in The Legend of Zelda: A Link to the Past. The editor features lazy loading, per-room settings, and a component-based design for maximum flexibility.

### Key Capabilities
- **Visual room editing** with 512x512 canvas
- **Object placement** with pattern-based rendering
- **Live palette editing** with instant preview
- **Independent dockable UI cards**
- **Multi-room editing** support
- **Automatic graphics loading**
- **Per-room layer visibility** settings
- **Command-line quick testing** support

---

## Current Status

### ✅ Production Ready Features
- Core rendering pipeline (floor, walls, objects, sprites)
- Object drawing via ObjectDrawer with pattern-based rendering
- Live palette editing with HSV picker
- Per-room background buffers (no shared state corruption)
- Independent dockable card system
- Cross-editor navigation (overworld ↔ dungeon)
- Error recovery system
- Test suite (29/29 tests passing - 100%)

### 🔧 Recently Fixed Issues
1. **Object Visibility** ✅ FIXED
   - **Problem**: Objects drawn to bitmaps but not visible on canvas
   - **Root Cause**: Textures not updated after `RenderObjectsToBackground()`
   - **Fix**: Added texture UPDATE commands after object rendering

2. **Property Change Re-rendering** ✅ FIXED
   - **Problem**: Changing blockset/palette didn't trigger re-render
   - **Fix**: Added change detection and automatic re-rendering

3. **One-Time Rendering** ✅ FIXED
   - **Problem**: Objects only rendered once, never updated
   - **Fix**: Removed restrictive rendering checks

4. **Per-Room Layer Settings** ✅ IMPLEMENTED
   - Each room now has independent BG1/BG2 visibility settings
   - Layer type controls (Normal, Translucent, Addition, Dark, Off)

5. **Canvas Context Menu** ✅ IMPLEMENTED
   - Dungeon-specific options (Place Object, Delete Selected, Toggle Layers, Re-render)
   - Dynamic menu based on current selection

---

## Architecture

### Component Hierarchy
```
DungeonEditorV2 (Coordinator)
│
├── Dungeon Controls (Collapsible panel)
│   └── Card visibility toggles
│
├── Independent Cards (all fully dockable)
│   ├── Rooms List Card (filterable, searchable)
│   ├── Room Matrix Card (16x19 grid, 296 rooms)
│   ├── Entrances List Card (entrance configuration)
│   ├── Room Graphics Card (blockset graphics display)
│   ├── Object Editor Card (unified object placement)
│   ├── Palette Editor Card (90-color palette editing)
│   └── Room Cards (dynamic, auto-dock together)
│
└── Per-Room Rendering
    └── Room
        ├── bg1_buffer_ (BackgroundBuffer)
        ├── bg2_buffer_ (BackgroundBuffer)
        └── DungeonCanvasViewer
```

### Card-Based Architecture Benefits
- ✅ Full freedom to drag, dock, resize
- ✅ No layout constraints or inheritance
- ✅ Can be arranged however user wants
- ✅ Session-aware card titles
- ✅ ImGui handles all docking logic
- ✅ Independent lifetime (close Dungeon Controls, rooms stay open)

---

## Quick Start

### Launch from Command Line
```bash
# Open specific room
./yaze --rom_file=zelda3.sfc --editor=Dungeon --cards="Room 0"

# Compare multiple rooms
./yaze --rom_file=zelda3.sfc --editor=Dungeon --cards="Room 0,Room 1,Room 105"

# Full workspace
./yaze --rom_file=zelda3.sfc --editor=Dungeon \
  --cards="Rooms List,Room Matrix,Object Editor,Palette Editor"

# Debug mode with logging
./yaze --debug --log_file=debug.log --rom_file=zelda3.sfc --editor=Dungeon
```

### From GUI
1. Launch YAZE
2. Load ROM (File → Open ROM or drag & drop)
3. Open Dungeon Editor (Tools → Dungeon Editor)
4. Toggle cards via "Dungeon Controls" checkboxes
5. Click room in list/matrix to open

---

## Core Features

### 1. Rooms List Card
```
Features:
- Filter/search functionality (ICON_MD_SEARCH)
- Format: [HEX_ID] Room Name
- Click to open room card
- Double-click for instant focus
- Shows all 296 rooms (0x00-0x127)
```

### 2. Room Matrix Card (Visual Navigation)
```
Layout:
- 16 columns × 19 rows = 304 cells
- Displays all 296 rooms (0x00-0x127)
- 24px cells with 1px spacing (optimized)
- Window size: 440x520

Visual Features:
- Deterministic HSV colors (no loading needed)
- Light green outline: Currently selected room
- Green outline: Open rooms
- Gray outline: Inactive rooms

Performance:
- Before: 2-4 seconds (lazy loading 296 rooms)
- After: < 50ms (pure math, no I/O)
```

### 3. Entrances List Card
```
Configuration UI (ZScream Parity):
- Entrance ID, Room ID, Dungeon ID
- Blockset, Music, Floor
- Player Position (X, Y)
- Camera Trigger (X, Y)
- Scroll Position (X, Y)
- Exit value
- Camera Boundaries (quadrant & full room)

List Features:
- Format: [HEX_ID] Entrance Name -> Room Name
- Shows entrance-to-room relationship
- Click to select and open associated room
```

### 4. Object Editor Card (Unified)
```
Improved UX:
- Mode controls at top: None | Place | Select | Delete
- Current object info always visible
- 2 tabs:
  - Browser: Object selection with previews
  - Preview: Emulator rendering with controls

Object Browser:
- Categorized objects (Floor/Wall/Special)
- 32x32 preview icons
- Filter/search functionality
- Shows object ID and type
```

### 5. Palette Editor Card
```
Features:
- Palette selector dropdown (20 dungeon palettes)
- 90-color grid (15 per row)
- Visual selection with yellow border
- Tooltips: color index, SNES BGR555, RGB values
- HSV color wheel picker
- Live RGB display (0-255)
- SNES format display (15-bit BGR555)
- Reset button

Live Updates:
- Edit palette → all open rooms re-render automatically
- Callback system decouples palette editor from rooms
```

### 6. Room Cards (Auto-Loading)
```
Features:
- Auto-loads graphics when properties change
- Simple status indicator (✓ Loaded / ⏳ Not Loaded)
- Auto-saves with main Save command
- Per-room layer controls (BG1/BG2 visibility, BG2 layer type)

Docking Behavior:
- ImGuiWindowClass for automatic tab grouping
- New room cards auto-dock with existing rooms
- Can be undocked independently
- Maintains session state
```

### 7. Canvas Context Menu (NEW)
```
Dungeon-Specific Options:
- Place Object (Ctrl+P)
- Delete Selected (Del) - conditional on selection
- Toggle BG1 (1)
- Toggle BG2 (2)
- Re-render Room (Ctrl+R)

Integration:
- Dynamic menu based on current state
- Consistent with overworld editor UX
```

---

## Technical Details

### Rendering Pipeline

```
1. Room::CopyRoomGraphicsToBuffer()
   → Loads tile graphics into current_gfx16_ [128×N indexed pixels]

2. BackgroundBuffer::DrawFloor()
   → Fills tilemap buffer with floor tile IDs

3. BackgroundBuffer::DrawBackground()
   → Renders floor tiles to bitmap (512×512 indexed surface)

4. Room::SetPalette()
   → Apply 90-color dungeon palette to SDL surface

5. Room::RenderObjectsToBackground()
   → ObjectDrawer writes wall/object tiles to BG1/BG2 buffers

6. gfx::Arena::QueueTextureCommand(UPDATE, &bitmap)
   → CRITICAL: Update textures after object rendering

7. gfx::Arena::ProcessTextureQueue(renderer)
   → Process queued texture operations

8. DungeonCanvasViewer::DrawRoomBackgroundLayers()
   → Draw textures to canvas with ImGui::Image()
```

### Critical Fix: Texture Update After Object Rendering

**Problem**: Objects were drawn to bitmaps but textures were never updated.

**Solution** (in `room.cc`):
```cpp
void Room::RenderRoomGraphics() {
  // 1. Draw floor and background
  bg1_buffer_.DrawFloor(...);
  bg1_buffer_.DrawBackground(...);
  
  // 2. Apply palette and create initial textures
  bg1_bmp.SetPalette(bg1_palette);
  gfx::Arena::Get().QueueTextureCommand(CREATE, &bg1_bmp);
  
  // 3. Render objects to bitmaps
  RenderObjectsToBackground();
  
  // 4. CRITICAL FIX: Update textures with new bitmap data
  gfx::Arena::Get().QueueTextureCommand(UPDATE, &bg1_bmp);
  gfx::Arena::Get().QueueTextureCommand(UPDATE, &bg2_bmp);
}
```

### SNES Graphics Format

**8-bit Indexed Color (3BPP for dungeons)**:
```cpp
// Each pixel is a palette index (0-7)
// RGB color comes from applying dungeon palette
bg1_bmp.SetPalette(dungeon_pal_group[palette_id]);  // 90 colors
Renderer::Get().RenderBitmap(&bitmap);  // indexed → RGB
```

**Color Format: 15-bit BGR555**
```
Bits: 0BBB BBGG GGGR RRRR
       ││││ ││││ ││││ ││││
       │└──┴─┘└──┴─┘└──┴─┘
       │ Blue  Green  Red
       └─ Unused (always 0)

Each channel: 0-31 (5 bits)
Total colors: 32,768 (2^15)
```

**Palette Organization**:
- 20 total palettes (one per dungeon color scheme)
- 90 colors per palette (full SNES BG palette)
- ROM address: `kDungeonMainPalettes` (0xDD734)

### Critical Math Formulas

**Tile Position in Tilesheet (128px wide)**:
```cpp
int tile_x = (tile_id % 16) * 8;
int tile_y = (tile_id / 16) * 8;
int pixel_offset = (tile_y * 128) + tile_x;
```

**Tile Position in Canvas (512×512)**:
```cpp
int canvas_x = (tile_col * 8);
int canvas_y = (tile_row * 8);
int pixel_offset = (canvas_y * 512) + canvas_x;

// CRITICAL: For NxN tiles, advance by (tile_row * 8 * width)
int dest_offset = (yy * 8 * 512) + (xx * 8);  // NOT just (yy * 512)!
```

**Palette Index Calculation**:
```cpp
// 3BPP: 8 colors per subpalette
int final_index = pixel_value + (palette_id * 8);

// NOT 4BPP (× 16)!
// int final_index = pixel_value + (palette_id << 4);  // WRONG
```

### Per-Room Buffers

**Problem**: Multiple rooms shared `gfx::Arena::Get().bg1()` and corrupted each other.

**Solution**: Each `Room` has its own buffers:
```cpp
// In room.h
gfx::BackgroundBuffer bg1_buffer_;
gfx::BackgroundBuffer bg2_buffer_;

// In room.cc
bg1_buffer_.DrawFloor(...);
bg1_buffer_.DrawBackground(std::span<uint8_t>(current_gfx16_));
Renderer::Get().RenderBitmap(&bg1_buffer_.bitmap());
```

---

## Testing

### Test Suite Status

| Test Type         | Total | Passing | Pass Rate |
| ----------------- | ----- | ------- | --------- |
| **Unit Tests**    | 14    | 14      | 100% ✅    |
| **Integration**   | 14    | 14      | 100% ✅    |
| **E2E Tests**     | 1     | 1       | 100% ✅    |
| **TOTAL**         | **29**| **29**  | **100%** ✅ |

### Running Tests

```bash
# Build tests (mac-ai preset)
cmake --preset mac-ai -B build_ai
cmake --build build_ai --target yaze_test

# Run all dungeon tests
./build_ai/bin/yaze_test --gtest_filter="*Dungeon*"

# Run E2E tests with GUI (normal speed)
./build_ai/bin/yaze_test --ui --show-gui --normal --gtest_filter="*DungeonEditorSmokeTest*"

# Run E2E tests in slow-motion (cinematic mode)
./build_ai/bin/yaze_test --ui --show-gui --cinematic --gtest_filter="*DungeonEditorSmokeTest*"

# Run all tests with fast execution
./build_ai/bin/yaze_test --ui --fast
```

### Test Speed Modes (NEW)

```bash
--fast       # Run tests as fast as possible (teleport mouse, skip delays)
--normal     # Run tests at human watchable speed (for debugging)
--cinematic  # Run tests in slow-motion with pauses (for demos/tutorials)
```

---

## Troubleshooting

### Common Issues & Fixes

#### Issue 1: Objects Not Visible
**Symptom**: Floor/walls render but objects invisible  
**Fix**: ✅ RESOLVED - Texture update after object rendering now working

#### Issue 2: Wrong Colors
**Symptom**: Colors don't match expected palette  
**Fix**: Use `SetPalette()` not `SetPaletteWithTransparent()` for dungeons

**Reason**:
```cpp
// WRONG (extracts only 8 colors):
bitmap.SetPaletteWithTransparent(palette);

// CORRECT (applies full 90-color palette):
bitmap.SetPalette(palette);
```

#### Issue 3: Bitmap Stretched/Corrupted
**Symptom**: Graphics only in top portion, repeated/stretched  
**Fix**: Wrong offset in DrawBackground()

```cpp
// WRONG:
int offset = (yy * 512) + (xx * 8);  // Only advances 512 per row

// CORRECT:
int offset = (yy * 8 * 512) + (xx * 8);  // Advances 4096 per row
```

#### Issue 4: Room Properties Don't Update
**Symptom**: Changing blockset/palette has no effect  
**Fix**: ✅ RESOLVED - Property change detection now working

---

## ROM Internals

### Object Encoding

Dungeon objects are stored in one of three formats:

#### Type 1: Standard Objects (ID 0x00-0xFF)
```
Format: xxxxxxss yyyyyyss iiiiiiii
Use: Common geometry like walls and floors
```

#### Type 2: Large Coordinate Objects (ID 0x100-0x1FF)
```
Format: 111111xx xxxxyyyy yyiiiiii
Use: More complex or interactive structures
```

#### Type 3: Special Objects (ID 0x200-0x27F)
```
Format: xxxxxxii yyyyyyii 11111iii
Use: Critical gameplay elements (chests, switches, bosses)
```

### Core Data Tables in ROM

- **`bank_01.asm`**:
  - **`DrawObjects` (0x018000)**: Master tables mapping object ID → drawing routine
  - **`LoadAndBuildRoom` (0x01873A)**: Primary routine that reads and draws a room

- **`rooms.asm`**:
  - **`RoomData_ObjectDataPointers` (0x1F8000)**: Table of 3-byte pointers to object data for each of 296 rooms

### Key ROM Addresses
```cpp
constexpr int dungeons_palettes = 0xDD734;
constexpr int room_object_pointer = 0x874C;  // Long pointer
constexpr int kRoomHeaderPointer = 0xB5DD;   // LONG
constexpr int tile_address = 0x001B52;
constexpr int tile_address_floor = 0x001B5A;
constexpr int torch_data = 0x2736A;
constexpr int blocks_pointer1 = 0x15AFA;
constexpr int pit_pointer = 0x394AB;
constexpr int doorPointers = 0xF83C0;
```

---

## Reference

### File Organization

**Core Rendering**:
- `src/app/zelda3/dungeon/room.{h,cc}` - Room state, buffers
- `src/app/gfx/background_buffer.{h,cc}` - Tile → bitmap drawing
- `src/app/core/renderer.cc` - Bitmap → texture conversion
- `src/app/editor/dungeon/dungeon_canvas_viewer.cc` - Canvas display

**Object Drawing**:
- `src/app/zelda3/dungeon/object_drawer.{h,cc}` - Native C++ patterns
- `src/app/gui/widgets/dungeon_object_emulator_preview.{h,cc}` - Research tool

**Editor UI**:
- `src/app/editor/dungeon/dungeon_editor_v2.{h,cc}` - Main coordinator
- `src/app/gui/widgets/editor_card.{h,cc}` - Independent card system
- `src/app/editor/dungeon/dungeon_object_interaction.{h,cc}` - Object selection

**Palette System**:
- `src/app/gfx/snes_palette.{h,cc}` - Palette loading
- `src/app/gui/widgets/palette_editor_widget.{h,cc}` - Visual editor
- `src/app/gfx/bitmap.cc` - SetPalette() implementation

### Quick Reference: Key Functions

```cpp
// Load dungeon palette
auto& pal_group = rom->palette_group().dungeon_main;
auto palette = pal_group[palette_id];  // NOT .palette(id)!

// Apply palette to bitmap
bitmap.SetPalette(palette);  // NOT SetPaletteWithTransparent()!

// Create texture from indexed bitmap
Renderer::Get().RenderBitmap(&bitmap);  // NOT UpdateBitmap()!

// Tile sheet offset (128px wide)
int offset = (tile_id / 16) * 8 * 128 + (tile_id % 16) * 8;

// Canvas offset (512px wide)
int offset = (tile_row * 8 * 512) + (tile_col * 8);

// Palette offset (3BPP)
int offset = palette_id * 8;  // NOT << 4 !
```

### Performance Metrics

**Matrix Loading**:
- Load time: < 50ms (pure calculation, no I/O)
- Memory allocations: ~20 per matrix draw (cached colors)
- Frame drops: None

**Room Loading**:
- Lazy loading: Rooms loaded on-demand
- Graphics caching: Reused across room switches
- Texture batching: Up to 8 textures processed per frame

---

## Summary

The Dungeon Editor is production-ready with all core features implemented and tested. Recent fixes ensure objects render correctly, property changes trigger re-renders, and the context menu provides dungeon-specific functionality. The card-based architecture provides maximum flexibility while maintaining stability.

### Critical Points
1. **Texture Update**: Always call UPDATE after modifying bitmap data
2. **Per-Room Buffers**: Each room has independent bg1/bg2 buffers
3. **Property Changes**: Automatically detected and trigger re-renders
4. **Palette Format**: Use SetPalette() for full 90-color dungeon palettes
5. **Context Menu**: Dungeon-specific options available via right-click

---

**For detailed debugging**: See `QUICK-DEBUG-REFERENCE.txt` for command-line shortcuts.

