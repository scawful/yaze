# Overworld Editor

The Overworld Editor is the primary tool for editing the Legend of Zelda: A Link to the Past overworld maps. It provides visual editing capabilities for tiles, entities, and map properties across the Light World, Dark World, and Special World areas.

## Architecture Overview

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                           OverworldEditor                                    │
│  (Main orchestrator - coordinates all subsystems)                           │
├─────────────────────────────────────────────────────────────────────────────┤
│                                                                             │
│  ┌─────────────────┐  ┌─────────────────┐  ┌─────────────────────────────┐ │
│  │  Tile16Editor   │  │ MapProperties   │  │    Entity System            │ │
│  │                 │  │    System       │  │                             │ │
│  │ • Tile editing  │  │ • Toolbar UI    │  │ • entity.cc (rendering)     │ │
│  │ • Pending       │  │ • Context menus │  │ • entity_operations.cc      │ │
│  │   changes       │  │ • Property      │  │ • overworld_entity_         │ │
│  │ • Palette coord │  │   panels        │  │   interaction.cc            │ │
│  └────────┬────────┘  └────────┬────────┘  │ • overworld_entity_         │ │
│           │                    │           │   renderer.cc               │ │
│           │                    │           └─────────────┬───────────────┘ │
│           │                    │                         │                 │
├───────────┴────────────────────┴─────────────────────────┴─────────────────┤
│                           Data Layer (zelda3/overworld/)                    │
│                                                                             │
│  ┌─────────────────┐  ┌─────────────────┐  ┌─────────────────────────────┐ │
│  │   Overworld     │  │  OverworldMap   │  │        Entities             │ │
│  │                 │  │                 │  │                             │ │
│  │ • 160 maps      │  │ • Single map    │  │ • OverworldEntrance         │ │
│  │ • Tile assembly │  │ • Palette/GFX   │  │ • OverworldExit             │ │
│  │ • Save/Load     │  │ • Bitmap gen    │  │ • OverworldItem             │ │
│  │ • Sprites       │  │ • Overlay data  │  │ • Sprite                    │ │
│  └─────────────────┘  └─────────────────┘  └─────────────────────────────┘ │
└─────────────────────────────────────────────────────────────────────────────┘
```

## Directory Structure

### Editor Layer (`src/app/editor/overworld/`)

| File | Lines | Purpose |
|------|-------|---------|
| `overworld_editor.h/cc` | ~3,750 | Main editor class, coordinates all subsystems |
| `tile16_editor.h/cc` | ~3,400 | Tile16 editing with pending changes workflow |
| `map_properties.h/cc` | ~1,900 | Toolbar, context menus, property panels |
| `entity.h/cc` | ~820 | Entity popup rendering and editing |
| `entity_operations.h/cc` | ~370 | Entity insertion helper functions |
| `overworld_entity_interaction.h/cc` | ~200 | Entity drag/drop and click handling |
| `overworld_entity_renderer.h/cc` | ~200 | Entity drawing delegation |
| `overworld_sidebar.h/cc` | ~420 | Sidebar property tabs |
| `overworld_toolbar.h/cc` | ~210 | Mode toggle toolbar |
| `scratch_space.cc` | ~420 | Tile layout scratch space |
| `automation.cc` | ~230 | Canvas automation API |
| `ui_constants.h` | ~75 | Shared UI constants and enums |
| `usage_statistics_card.h/cc` | ~130 | Tile usage tracking |
| `debug_window_card.h/cc` | ~100 | Debug information display |

### Panels Subdirectory (`panels/`)

Thin wrappers implementing `EditorPanel` interface that delegate to main editor methods:

| Panel | Purpose |
|-------|---------|
| `overworld_canvas_panel` | Main map canvas display |
| `tile16_selector_panel` | Tile palette for painting |
| `tile8_selector_panel` | Individual tile8 selector |
| `area_graphics_panel` | Current area graphics display |
| `map_properties_panel` | Map property editing |
| `scratch_space_panel` | Tile layout workspace |
| `gfx_groups_panel` | Graphics group editor |
| `usage_statistics_panel` | Tile usage analytics |
| `v3_settings_panel` | ZScustom v3 feature settings |
| `debug_window_panel` | Debug information |

### Data Layer (`src/zelda3/overworld/`)

| File | Purpose |
|------|---------|
| `overworld.h/cc` | Core data management for 160+ maps |
| `overworld_map.h/cc` | Individual map data and bitmap generation |
| `overworld_entrance.h/cc` | Entrance entity data structures |
| `overworld_exit.h/cc` | Exit entity data structures |
| `overworld_item.h/cc` | Overworld item data |
| `overworld_version_helper.h` | ROM version detection for ZScustom features |
| `diggable_tiles.h/cc` | Diggable tile management |

---

## Key Workflows

### 1. Tile16 Editing Workflow

The tile16 editing system uses a **pending changes** pattern to prevent accidental ROM modifications:

```
┌─────────────┐     ┌─────────────┐     ┌─────────────┐     ┌─────────────┐
│   Select    │────▶│    Edit     │────▶│   Preview   │────▶│   Commit    │
│   Tile16    │     │  Tile8s     │     │  (Pending)  │     │  or Discard │
└─────────────┘     └─────────────┘     └─────────────┘     └─────────────┘
```

**Key Files:**
- `tile16_editor.cc` - Main editing logic
- `overworld_editor.cc` - Integration with overworld

**Pending Changes System:**
```cpp
// Changes are tracked per-tile in maps:
std::map<int, gfx::Tile16> pending_tile16_changes_;
std::map<int, gfx::Bitmap> pending_tile16_bitmaps_;

// Check for unsaved changes:
bool has_pending_changes() const;
int pending_changes_count() const;
bool is_tile_modified(int tile_id) const;

// Commit or discard:
absl::Status CommitAllChanges();
void DiscardAllChanges();
```

**Palette Coordination (Critical for Color Fixes):**

The overworld uses a 256-color palette organized as 16 rows of 16 colors. Different graphics sheets map to different palette regions:

| Sheet Index | Palette Region | Purpose |
|-------------|----------------|---------|
| 0, 3, 4 | AUX1 (rows 10-15) | Main blockset graphics |
| 1, 2 | MAIN (rows 2-6) | Main area graphics |
| 5, 6 | AUX2 (rows 10-15) | Secondary blockset |
| 7 | ANIMATED (row 7) | Animated tiles |

Key palette methods in `tile16_editor.cc`:
```cpp
// Get palette slot for a graphics sheet
int GetPaletteSlotForSheet(int sheet_index) const;

// Get actual palette row for palette button + sheet combination
int GetActualPaletteSlot(int palette_button, int sheet_index) const;

// Get the palette slot for the current tile being edited
int GetActualPaletteSlotForCurrentTile16() const;
```

### 2. ZScustom Overworld Features

ZScustom is an ASM patch system that extends overworld capabilities. Version detection is centralized in `overworld_version_helper.h`:

```cpp
enum class OverworldVersion {
  kVanilla = 0,     // No patches applied (0xFF in ROM)
  kZSCustomV1 = 1,  // Basic expanded pointers
  kZSCustomV2 = 2,  // + BG colors, main palettes
  kZSCustomV3 = 3   // + Area enum, wide/tall areas, all features
};
```

**Feature Detection:**
```cpp
// In overworld_version_helper.h:
static OverworldVersion GetVersion(const Rom& rom);
static bool SupportsAreaEnum(OverworldVersion version);      // v3+ only
static bool SupportsCustomBGColors(OverworldVersion version); // v2+
static bool SupportsCustomTileGFX(OverworldVersion version);  // v3+
static bool SupportsAnimatedGFX(OverworldVersion version);    // v3+
static bool SupportsSubscreenOverlay(OverworldVersion version); // v3+
```

**Version-Specific Features:**

| Version | Features |
|---------|----------|
| Vanilla | Standard 64 Light World + 64 Dark World + 32 Special World maps |
| v1 | Expanded pointers, map data overflow space |
| v2 | + Custom BG colors per area, Main palette selection |
| v3 | + Area size enum (Wide 2x1, Tall 1x2), Mosaic, Animated GFX, Subscreen overlays, Tile GFX groups |

### 4. Large Map / Multi-Area System

The overworld uses a parent-child system to manage multi-area maps (Large 2x2, Wide 2x1, Tall 1x2).

**Version-Specific Parent ID Loading:**

| Version | Parent ID Source | Area Size Source |
|---------|------------------|------------------|
| Vanilla | `kOverworldMapParentId` (0x125EC) | `kOverworldScreenSize + (index & 0x3F)` |
| v1 | `kOverworldMapParentId` (0x125EC) | `kOverworldScreenSize + (index & 0x3F)` |
| v2 | `kOverworldMapParentId` (0x125EC) | `kOverworldScreenSize + (index & 0x3F)` |
| v3+ | `kOverworldMapParentIdExpanded` (0x140998) | `kOverworldScreenSize + index` |

**Area Size Enum (v3+ only):**
```cpp
enum class AreaSizeEnum {
  SmallArea = 0,  // 1x1 (512x512 pixels)
  LargeArea = 1,  // 2x2 (1024x1024 pixels)
  WideArea = 2,   // 2x1 (1024x512 pixels) - v3 only
  TallArea = 3,   // 1x2 (512x1024 pixels) - v3 only
};
```

**Sibling Map Calculation:**

For a parent at index `P`:
- **Large (2x2):** Siblings are P, P+1, P+8, P+9
- **Wide (2x1):** Siblings are P, P+1
- **Tall (1x2):** Siblings are P, P+8

**Parent ID Loading (Version-Specific):**

The vanilla parent table at `0x125EC` (`kOverworldMapParentId`) only contains 64 entries for Light World maps. Different worlds require different handling:

| Version | Light World (0x00-0x3F) | Dark World (0x40-0x7F) | Special World (0x80-0x9F) |
|---------|-------------------------|------------------------|---------------------------|
| v3+ | Expanded table (0x140998) with 160 entries | Same expanded table | Same expanded table |
| Vanilla/v1/v2 | Direct lookup from 64-entry table | Mirror LW parent + 0x40 offset | Hardcoded (Zora's Domain = 0x81, others = self) |

**Example:** DW map 0x43's parent = LW map 0x03's parent (0x03) + 0x40 = 0x43

**Graphics Cache Hash:**

The tileset cache uses a comprehensive hash that includes:
- `static_graphics[0-11]` - Main blockset sheet IDs (excluding sprite sheets 12-15)
- `game_state` - Game state (Beginning=0, Zelda=1, Master Sword=2, Agahnim=3) - affects sprite sheets
- `sprite_graphics[game_state]` - Sprite graphics config for current game state
- `area_graphics` - Area-specific graphics group ID
- `main_gfx_id` - World-specific graphics group (LW=0x20, DW=0x21, SW=0x20/0x24)
- `parent` - Parent map ID for sibling coordination
- `map_index` - **Critical for SW**: Unique hardcoded configs per map (0x80 Master Sword, 0x88/0x93 Triforce, 0x95 DM clone, etc.)
- `main_palette` - World palette (LW=0, DW=1, Death Mountain=2/3, Triforce=4)
- `animated_gfx` - Death Mountain (0x59) vs normal water/clouds (0x5B)
- `area_palette` - Area-specific palette configuration
- `subscreen_overlay` - Visual effects (fog, curtains, sky, lava)

**Important:** `static_graphics[12-15]` (sprite sheets) are loaded using `sprite_graphics_[game_state_]`, which may be stale at hash computation time. The hash includes `game_state` and `sprite_graphics` directly to avoid collisions.

**Refresh Coordination:**

When any map in a multi-area group is modified, all siblings must be refreshed to maintain visual consistency. Key methods:
- `RefreshMultiAreaMapsSafely()` - Coordinates refresh from parent perspective
- `InvalidateSiblingMapCaches()` - Clears graphics cache for all siblings
- `RefreshSiblingMapGraphics()` - Forces immediate refresh of sibling bitmaps

**World Boundary Protection:**

Sibling calculations in `FetchLargeMaps()` verify that siblings stay within the same world (LW: 0-63, DW: 64-127, SW: 128-159) to prevent cross-world corruption.

**Upgrade Workflow (in `overworld_editor.cc`):**
```cpp
// Apply ZScustom ASM patch
absl::Status ApplyZSCustomOverworldASM(int target_version);

// Update ROM markers after patching
absl::Status UpdateROMVersionMarkers(int target_version);
```

### 3. Save System

Saving is controlled by feature flags in `core::FeatureFlags`. Each component saves independently:

```cpp
// In overworld_editor.cc:
absl::Status OverworldEditor::Save() {
  if (core::FeatureFlags::get().overworld.kSaveOverworldMaps) {
    RETURN_IF_ERROR(overworld_.CreateTile32Tilemap());
    RETURN_IF_ERROR(overworld_.SaveMap32Tiles());
    RETURN_IF_ERROR(overworld_.SaveMap16Tiles());
    RETURN_IF_ERROR(overworld_.SaveOverworldMaps());
  }
  if (core::FeatureFlags::get().overworld.kSaveOverworldEntrances) {
    RETURN_IF_ERROR(overworld_.SaveEntrances());
  }
  if (core::FeatureFlags::get().overworld.kSaveOverworldExits) {
    RETURN_IF_ERROR(overworld_.SaveExits());
  }
  if (core::FeatureFlags::get().overworld.kSaveOverworldItems) {
    RETURN_IF_ERROR(overworld_.SaveItems());
  }
  if (core::FeatureFlags::get().overworld.kSaveOverworldProperties) {
    RETURN_IF_ERROR(overworld_.SaveMapProperties());
    RETURN_IF_ERROR(overworld_.SaveMusic());
  }
  return absl::OkStatus();
}
```

**Save Order Dependencies:**

1. **Tile32 Tilemap** must be created before saving map tiles
2. **Map32 Tiles** must be saved before Map16 tiles
3. **Map16 Tiles** are the individual 16x16 tile definitions
4. **Overworld Maps** reference the tile definitions
5. **Entrances/Exits/Items** are independent and can save in any order
6. **Properties/Music** save area-specific metadata

**Feature Flags (in `core/features.h`):**
```cpp
struct OverworldFlags {
  bool kSaveOverworldMaps = true;
  bool kSaveOverworldEntrances = true;
  bool kSaveOverworldExits = true;
  bool kSaveOverworldItems = true;
  bool kSaveOverworldProperties = true;
  bool kDrawOverworldSprites = true;
  bool kLoadCustomOverworld = true;
  bool kApplyZSCustomOverworldASM = false;
  bool kEnableSpecialWorldExpansion = false;
};
```

---

## Testing Guidance

### Testing Tile16 Editing

1. **Palette Colors Wrong:**
   - Check `GetPaletteSlotForSheet()` for correct sheet-to-palette mapping
   - Verify `ApplyPaletteToCurrentTile16Bitmap()` is called after palette changes
   - Ensure `set_palette()` callback from overworld editor is working

2. **Changes Not Appearing:**
   - Check `has_pending_changes()` returns true after editing
   - Verify `CommitAllChanges()` is called before expecting ROM changes
   - Check `on_changes_committed_` callback is properly set

3. **Tile Not Updating on Map:**
   - Verify `RefreshTile16Blockset()` is called after commit
   - Check `RefreshOverworldMap()` is triggered

### Testing ZScustom Features

1. **Version Detection:**
   ```cpp
   auto version = OverworldVersionHelper::GetVersion(*rom_);
   LOG_DEBUG("Version: %s", OverworldVersionHelper::GetVersionName(version));
   ```

2. **Feature Gating:**
   - Test with vanilla ROM (should gracefully degrade)
   - Test with v2 ROM (BG colors should work, area enum should not)
   - Test with v3 ROM (all features should work)

3. **Upgrade Path:**
   - Start with vanilla ROM
   - Apply v2 patch, verify BG color support
   - Apply v3 patch, verify area enum support

### Testing Full Overworld Save

1. **Incremental Testing:**
   - Disable all save flags except one
   - Make changes to that component
   - Save and verify in emulator

2. **Component Order:**
   - Test maps save (tile data)
   - Test entrances save (warp destinations)
   - Test exits save (underworld return points)
   - Test items save (secret items)
   - Test properties save (graphics, palettes, music)

3. **Round-Trip Testing:**
   - Load ROM → Make changes → Save → Reload → Verify changes persist

---

## Editing Modes

Defined in `ui_constants.h`:

```cpp
enum class EditingMode {
  MOUSE = 0,     // Entity selection and interaction
  DRAW_TILE = 1  // Tile painting mode
};

enum class EntityEditMode {
  NONE = 0,
  ENTRANCES = 1,
  EXITS = 2,
  ITEMS = 3,
  SPRITES = 4,
  TRANSPORTS = 5,
  MUSIC = 6
};
```

---

## Undo/Redo System

The overworld editor has its own undo/redo stack for tile painting operations:

```cpp
struct OverworldUndoPoint {
  int map_id = 0;
  int world = 0;  // 0=Light, 1=Dark, 2=Special
  std::vector<std::pair<std::pair<int, int>, int>> tile_changes;
  std::chrono::steady_clock::time_point timestamp;
};

// Key methods:
void CreateUndoPoint(int map_id, int world, int x, int y, int old_tile_id);
void FinalizePaintOperation();
void ApplyUndoPoint(const OverworldUndoPoint& point);
```

Paint operations within 500ms are batched together to avoid creating too many undo points for drag operations.

---

## Performance Considerations

1. **Deferred Texture Creation:**
   - Map textures are created on-demand, not during initial load
   - `ProcessDeferredTextures()` handles background texture creation

2. **LRU Map Cache:**
   - Only ~20 maps are kept fully built in memory
   - Evicted maps are rebuilt when needed via `EnsureMapBuilt()`

3. **Graphics Config Caching:**
   - Maps with identical graphics configurations share tileset data
   - `ComputeGraphicsConfigHash()` identifies identical configs
   - Cache invalidation for sibling maps:
     - `InvalidateSiblingMapCaches()` clears cache for all maps in a multi-area group
     - Called when graphics properties change on any map
     - Ensures stale tilesets aren't reused after palette/graphics changes

4. **Hover Debouncing:**
   - Map building during rapid hover is delayed by 150ms
   - Prevents unnecessary rebuilds while panning

---

## Related Documentation

- [Composite Layer System](../../../docs/internal/agents/composite-layer-system.md) - Graphics layer architecture
- [ZScream Wiki](https://github.com/Zarby89/ZScreamDungeon/wiki) - Reference for ZScream compatibility

