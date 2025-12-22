# YAZE Overworld System - Complete Technical Reference

Comprehensive reference for AI agents working on the YAZE Overworld editing system.

---

## 1. Architecture Overview

### File Structure
```
src/zelda3/overworld/
├── overworld.h/cc              # Main Overworld class (2,500+ lines)
├── overworld_map.h/cc          # Individual map (OverworldMap class)
├── overworld_version_helper.h  # Version detection & feature gates
├── overworld_item.h/cc         # Item entities
├── overworld_entrance.h        # Entrance/hole entities
├── overworld_exit.h            # Exit entities

src/app/editor/overworld/
├── overworld_editor.h/cc       # Main editor (3,204 lines)
├── map_properties.cc           # MapPropertiesSystem (1,759 lines)
├── tile16_editor.h/cc          # Tile16Editor (2,584 lines)
├── entity.cc                   # Entity UI popups (491 lines)
├── entity_operations.cc        # Entity CRUD helpers (239 lines)
├── overworld_entity_renderer.cc # Entity drawing (151 lines)
├── scratch_space.cc            # Tile storage utilities (444 lines)
```

### Data Model Hierarchy
```
Rom (raw data)
  └── Overworld (coordinator)
        ├── tiles16_[] (3,752-4,096 Tile16 definitions)
        ├── tiles32_unique_[] (up to 8,864 Tile32 definitions)
        ├── overworld_maps_[160] (individual map screens)
        │     └── OverworldMap
        │           ├── area_graphics_, area_palette_
        │           ├── bitmap_data_[] (256x256 rendered pixels)
        │           └── current_gfx_[] (graphics buffer)
        ├── all_entrances_[] (~129 entrance points)
        ├── all_holes_[] (~19 hole entrances)
        ├── all_exits_[] (~79 exit points)
        ├── all_items_[] (collectible items)
        └── all_sprites_[3][] (sprites per game state)
```

---

## 2. ZSCustomOverworld Version System

### Version Detection
```cpp
// ROM address for version byte
constexpr int OverworldCustomASMHasBeenApplied = 0x140145;

// Version values
0xFF = Vanilla ROM (unpatched)
0x01 = ZSCustomOverworld v1
0x02 = ZSCustomOverworld v2
0x03 = ZSCustomOverworld v3+
```

### Feature Matrix

| Feature | Vanilla | v1 | v2 | v3 |
|---------|---------|----|----|-----|
| Basic map editing | Y | Y | Y | Y |
| Large maps (2x2) | Y | Y | Y | Y |
| Expanded pointers (0x130000+) | - | Y | Y | Y |
| Custom BG colors | - | - | Y | Y |
| Main palette per area | - | - | Y | Y |
| Wide areas (2x1) | - | - | - | Y |
| Tall areas (1x2) | - | - | - | Y |
| Custom tile GFX (8 per map) | - | - | - | Y |
| Animated GFX | - | - | - | Y |
| Subscreen overlays | - | - | - | Y |

### OverworldVersionHelper API
```cpp
// src/zelda3/overworld/overworld_version_helper.h

enum class OverworldVersion { kVanilla=0, kZSCustomV1=1, kZSCustomV2=2, kZSCustomV3=3 };
enum class AreaSizeEnum { SmallArea=0, LargeArea=1, WideArea=2, TallArea=3 };

// Detection
static OverworldVersion GetVersion(const Rom& rom);
static uint8_t GetAsmVersion(const Rom& rom);

// Feature gates (use these, not raw version checks!)
static bool SupportsAreaEnum(OverworldVersion v);      // v3 only
static bool SupportsExpandedSpace(OverworldVersion v); // v1+
static bool SupportsCustomBGColors(OverworldVersion v);// v2+
static bool SupportsCustomTileGFX(OverworldVersion v); // v3 only
static bool SupportsAnimatedGFX(OverworldVersion v);   // v3 only
static bool SupportsSubscreenOverlay(OverworldVersion v); // v3 only
```

---

## 3. Tile System Architecture

### Tile Hierarchy
```
Tile8 (8x8 pixels) - Base SNES tile
  ↓
Tile16 (16x16 pixels) - 2x2 grid of Tile8s
  ↓
Tile32 (32x32 pixels) - 2x2 grid of Tile16s
  ↓
Map Screen (256x256 pixels) - 8x8 grid of Tile32s
```

### Tile16 Structure
```cpp
// src/app/gfx/types/snes_tile.h

class TileInfo {
  uint16_t id_;              // 9-bit tile8 ID (0-511)
  uint8_t palette_;          // 3-bit palette (0-7)
  bool over_;                // Priority flag
  bool vertical_mirror_;     // Y-flip
  bool horizontal_mirror_;   // X-flip
};

class Tile16 {
  TileInfo tile0_;  // Top-left
  TileInfo tile1_;  // Top-right
  TileInfo tile2_;  // Bottom-left
  TileInfo tile3_;  // Bottom-right
};

// ROM storage: 8 bytes per Tile16 at 0x78000 + (ID * 8)
// Total: 4,096 Tile16s (0x0000-0x0FFF)
```

### Tile16Editor Features (COMPLETE)
The Tile16Editor at `tile16_editor.cc` is **fully implemented** with:

- **Undo/Redo System** (lines 1681-1760)
  - `SaveUndoState()` - captures current state
  - `Undo()` / `Redo()` - restore states
  - Ctrl+Z / Ctrl+Y keyboard shortcuts
  - 50-state stack with rate limiting

- **Clipboard Operations**
  - Copy/Paste Tile16s
  - 4 scratch space slots

- **Editing Features**
  - Tile8 composition into Tile16
  - Flip horizontal/vertical
  - Palette cycling (0-7)
  - Fill with single Tile8

---

## 4. Map Organization

### Index Scheme
```
Index 0x00-0x3F: Light World (64 maps, 8x8 grid)
Index 0x40-0x7F: Dark World (64 maps, 8x8 grid)
Index 0x80-0x9F: Special World (32 maps, 8x4 grid)

Total: 160 maps

Grid position: X = index % 8, Y = index / 8
World position: X * 512 pixels, Y * 512 pixels
```

### Multi-Area Maps
```cpp
enum class AreaSizeEnum {
  SmallArea = 0,  // 1x1 screen (standard)
  LargeArea = 1,  // 2x2 screens (4 quadrants)
  WideArea = 2,   // 2x1 screens (v3 only)
  TallArea = 3,   // 1x2 screens (v3 only)
};

// IMPORTANT: Always use ConfigureMultiAreaMap() for size changes!
// Never set area_size_ directly - it handles parent IDs and ROM persistence
absl::Status Overworld::ConfigureMultiAreaMap(int parent_index, AreaSizeEnum size);
```

---

## 5. Entity System

### Entity Types
| Type | Storage | Count | ROM Address |
|------|---------|-------|-------------|
| Entrances | `all_entrances_` | ~129 | 0xDB96F+ |
| Holes | `all_holes_` | ~19 | 0xDB800+ |
| Exits | `all_exits_` | ~79 | 0x15D8A+ |
| Items | `all_items_` | Variable | 0xDC2F9+ |
| Sprites | `all_sprites_[3]` | Variable | 0x4C881+ |

### Entity Deletion Pattern
Entities use a `deleted` flag pattern - this is **CORRECT** for ROM editors:
```cpp
// Entities live at fixed ROM offsets, cannot be truly "removed"
// Setting deleted = true marks them as inactive
// entity_operations.cc reuses deleted slots for new entities
item.deleted = true;  // Proper pattern

// Renderer skips deleted entities (overworld_entity_renderer.cc)
if (!item.deleted) { /* render */ }
```

---

## 6. Graphics Loading Pipeline

### Load Sequence
```
1. Overworld::Load(rom)
   └── LoadOverworldMaps()
         └── For each map (0-159):
               └── OverworldMap::ctor()
                     ├── LoadAreaInfo()
                     └── LoadCustomOverworldData() [v3]

2. On map access: EnsureMapBuilt(map_index)
   └── BuildMap()
         ├── LoadAreaGraphics()
         ├── BuildTileset()
         ├── BuildTiles16Gfx()
         ├── LoadPalette()
         ├── LoadOverlay()
         └── BuildBitmap()
```

### Texture Queue System
Use deferred texture loading via `gfx::Arena`:
```cpp
// CORRECT: Non-blocking, uses queue
gfx::Arena::Get().QueueTextureCommand(gfx::TextureCommand{
    .operation = gfx::TextureOperation::kCreate,
    .bitmap = &some_bitmap_,
    .priority = gfx::TexturePriority::kHigh
});

// WRONG: Blocking, causes UI freeze
Renderer::Get().RenderBitmap(&some_bitmap_);
```

---

## 7. ROM Addresses (Key Locations)

### Vanilla Addresses
```cpp
// Tile data
kTile16Ptr = 0x78000              // Tile16 definitions
kOverworldMapSize = 0x12844       // Map size bytes

// Graphics & Palettes
kAreaGfxIdPtr = 0x7C9C            // Area graphics IDs
kOverworldMapPaletteGroup = 0x7D1C // Palette IDs

// Entities
kOverworldEntranceMap = 0xDB96F   // Entrance data
kOverworldExitRooms = 0x15D8A     // Exit room IDs
kOverworldItemPointers = 0xDC2F9  // Item pointers
```

### Expanded Addresses (v1+)
```cpp
// Custom data at 0x140000+
OverworldCustomASMHasBeenApplied = 0x140145  // Version byte
OverworldCustomAreaSpecificBGPalette = 0x140000  // BG colors (160*2)
OverworldCustomMainPaletteArray = 0x140160   // Main palettes (160)
OverworldCustomAnimatedGFXArray = 0x1402A0   // Animated GFX (160)
OverworldCustomTileGFXGroupArray = 0x140480  // Tile GFX (160*8)
OverworldCustomSubscreenOverlayArray = 0x140340 // Overlays (160*2)
kOverworldMapParentIdExpanded = 0x140998     // Parent IDs (160)
kOverworldMessagesExpanded = 0x1417F8        // Messages (160*2)
```

---

## 8. Known Gaps in OverworldEditor

### Critical: Texture Queueing TODOs (6 locations)
```cpp
// overworld_editor.cc - these Renderer calls need to be converted:
Line 1392: // TODO: Queue texture for later rendering
Line 1397: // TODO: Queue texture for later rendering
Line 1740: // TODO: Queue texture for later rendering
Line 1809: // TODO: Queue texture for later rendering
Line 1819: // TODO: Queue texture for later rendering
Line 1962: // TODO: Queue texture for later rendering
```

### Unimplemented Core Methods
```cpp
// overworld_editor.h lines 82-87
Undo()  → Returns UnimplementedError
Redo()  → Returns UnimplementedError
Cut()   → Returns UnimplementedError
Find()  → Returns UnimplementedError
```

### Entity Popup Static Variable Bug
```cpp
// entity.cc - Multiple popups use static variables that persist
// Causes state contamination when editing multiple entities
bool DrawExitEditorPopup() {
  static bool set_done = false;  // BUG: persists across calls
  static int doorType = ...;      // BUG: wrong entity's data shown
}
```

### Exit Editor Unimplemented Features
```cpp
// entity.cc lines 216-264
// UI exists but not connected to data:
- Door type editing (Wooden, Bombable, Sanctuary, Palace)
- Door X/Y position
- Center X/Y, Link posture, sprite/BG GFX, palette
```

---

## 9. Code Patterns to Follow

### Graphics Refresh
```cpp
// 1. Update model
map.SetProperty(new_value);

// 2. Reload from ROM
map.LoadAreaGraphics();

// 3. Queue texture update (NOT RenderBitmap!)
gfx::Arena::Get().QueueTextureCommand(gfx::TextureCommand{
    .operation = gfx::TextureOperation::kUpdate,
    .bitmap = &map_bitmap_,
    .priority = gfx::TexturePriority::kHigh
});
```

### Version-Aware Code
```cpp
auto version = OverworldVersionHelper::GetVersion(*rom_);

// Use semantic helpers, not raw version checks
if (OverworldVersionHelper::SupportsAreaEnum(version)) {
  // v3+ only code
}
```

### Entity Rendering Colors (0.85f alpha)
```cpp
ImVec4 entrance_color(1.0f, 0.85f, 0.0f, 0.85f);  // Yellow-gold
ImVec4 exit_color(0.0f, 1.0f, 1.0f, 0.85f);       // Cyan
ImVec4 item_color(1.0f, 0.0f, 0.0f, 0.85f);       // Red
ImVec4 sprite_color(1.0f, 0.0f, 1.0f, 0.85f);     // Magenta
```

---

## 10. Testing

### Run Overworld Tests
```bash
# Unit tests
ctest --test-dir build -R "Overworld" -V

# Regression tests
ctest --test-dir build -R "OverworldRegression" -V
```

### Test Files
- `test/unit/zelda3/overworld_test.cc` - Core tests
- `test/unit/zelda3/overworld_regression_test.cc` - Version helper tests
