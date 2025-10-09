# SNES Palette System Overview

## Understanding SNES Color and Palette Organization

### Core Concepts

#### 1. SNES Color Format (15-bit BGR555)
- **Storage**: 2 bytes per color (16 bits total, 15 bits used)
- **Format**: `0BBB BBGG GGGR RRRR`
  - Bits 0-4: Red (5 bits, 0-31)
  - Bits 5-9: Green (5 bits, 0-31)
  - Bits 10-14: Blue (5 bits, 0-31)
  - Bit 15: Unused (always 0)
- **Range**: Each channel has 32 levels (0-31)
- **Total Colors**: 32,768 possible colors (2^15)

#### 2. Palette Groups in Zelda 3
Zelda 3 organizes palettes into logical groups for different game areas and entities:

```cpp
struct PaletteGroupMap {
  PaletteGroup overworld_main;      // Main overworld graphics (35 colors each)
  PaletteGroup overworld_aux;       // Auxiliary overworld (21 colors each)
  PaletteGroup overworld_animated;  // Animated colors (7 colors each)
  PaletteGroup hud;                 // HUD graphics (32 colors each)
  PaletteGroup global_sprites;      // Sprite palettes (60 colors each)
  PaletteGroup armors;              // Armor colors (15 colors each)
  PaletteGroup swords;              // Sword colors (3 colors each)
  PaletteGroup shields;             // Shield colors (4 colors each)
  PaletteGroup sprites_aux1;        // Auxiliary sprite palette 1 (7 colors each)
  PaletteGroup sprites_aux2;        // Auxiliary sprite palette 2 (7 colors each)
  PaletteGroup sprites_aux3;        // Auxiliary sprite palette 3 (7 colors each)
  PaletteGroup dungeon_main;        // Dungeon palettes (90 colors each)
  PaletteGroup grass;               // Grass colors (special handling)
  PaletteGroup object_3d;           // 3D object palettes (8 colors each)
  PaletteGroup overworld_mini_map;  // Mini-map palettes (128 colors each)
};
```

### Dungeon Palette System

#### Structure
- **20 dungeon palettes** in the `dungeon_main` group
- **90 colors per palette** (full SNES palette for BG layers)
- **ROM Location**: `kDungeonMainPalettes` (check `snes_palette.cc` for exact address)

#### Usage
```cpp
// Loading a dungeon palette
auto& dungeon_pal_group = rom->palette_group().dungeon_main;
int num_palettes = dungeon_pal_group.size();  // Should be 20
int palette_id = room.palette;  // Room's palette ID (0-19)

// IMPORTANT: Use operator[] not palette() method!
auto palette = dungeon_pal_group[palette_id];  // Returns reference
// NOT: auto palette = dungeon_pal_group.palette(palette_id);  // Returns copy!
```

#### Color Distribution (90 colors)
The 90 colors are typically distributed as:
- **BG1 Palette** (Background Layer 1): First 8-16 subpalettes
- **BG2 Palette** (Background Layer 2): Next 8-16 subpalettes
- **Sprite Palettes**: Remaining colors (handled separately)

Each "subpalette" is 16 colors (one SNES palette unit).

### Overworld Palette System

#### Structure
- **Main Overworld**: 35 colors per palette
- **Auxiliary**: 21 colors per palette
- **Animated**: 7 colors per palette (for water, lava effects)

#### 3BPP Graphics and Left/Right Palettes
Overworld graphics use 3BPP (3 bits per pixel) format:
- **8 colors per tile** (2^3 = 8)
- **Left Side**: Uses palette 0-7
- **Right Side**: Uses palette 8-15

When decompressing 3BPP graphics:
```cpp
// Palette assignment for 3BPP overworld tiles
if (tile_position < half_screen_width) {
  // Left side of screen
  tile_palette_offset = 0;  // Use colors 0-7
} else {
  // Right side of screen
  tile_palette_offset = 8;  // Use colors 8-15
}
```

### Common Issues and Solutions

#### Issue 1: Empty Palette
**Symptom**: "Palette size: 0 colors"
**Cause**: Using `palette()` method instead of `operator[]`
**Solution**:
```cpp
// WRONG:
auto palette = group.palette(id);  // Returns copy, may be empty

// CORRECT:
auto palette = group[id];  // Returns reference
```

#### Issue 2: Bitmap Corruption
**Symptom**: Graphics render only in top portion of image
**Cause**: Wrong depth parameter in `CreateAndRenderBitmap`
**Solution**:
```cpp
// WRONG:
CreateAndRenderBitmap(0x200, 0x200, 0x200, data, bitmap, palette);
//                              depth ^^^^ should be 8!

// CORRECT:
CreateAndRenderBitmap(0x200, 0x200, 8, data, bitmap, palette);
//                              width, height, depth=8 bits
```

#### Issue 3: ROM Not Loaded in Preview
**Symptom**: "ROM not loaded" error in emulator preview
**Cause**: Initializing before ROM is set
**Solution**:
```cpp
// Initialize emulator preview AFTER ROM is loaded and set
void Load() {
  // ... load ROM data ...
  // ... set up other components ...
  
  // NOW initialize emulator preview with loaded ROM
  object_emulator_preview_.Initialize(rom_);
}
```

### Palette Editor Integration

#### Key Functions for UI
```cpp
// Reading a color from ROM
absl::StatusOr<uint16_t> ReadColorFromRom(uint32_t address, const uint8_t* rom);

// Converting SNES color to RGB
SnesColor color(snes_value);  // snes_value is uint16_t
uint8_t r = color.red();      // 0-255 (converted from 0-31)
uint8_t g = color.green();    // 0-255
uint8_t b = color.blue();     // 0-255

// Writing color back to ROM
uint16_t snes_value = color.snes();  // Get 15-bit BGR555 value
rom->WriteByte(address, snes_value & 0xFF);        // Low byte
rom->WriteByte(address + 1, (snes_value >> 8) & 0xFF);  // High byte
```

#### Palette Widget Requirements
1. **Display**: Show colors in organized grids (16 colors per row for SNES standard)
2. **Selection**: Allow clicking to select a color
3. **Editing**: Provide RGB sliders (0-255) or color picker
4. **Conversion**: Auto-convert RGB (0-255) ↔ SNES (0-31) values
5. **Preview**: Show before/after comparison
6. **Save**: Write modified palette back to ROM

### Graphics Manager Integration

#### Sheet Palette Assignment
```cpp
// Assigning palette to graphics sheet
if (sheet_id > 115) {
  // Sprite sheets use sprite palette
  graphics_sheet.SetPaletteWithTransparent(
      rom.palette_group().global_sprites[0], 0);
} else {
  // Dungeon sheets use dungeon palette
  graphics_sheet.SetPaletteWithTransparent(
      rom.palette_group().dungeon_main[0], 0);
}
```

### Best Practices

1. **Always use `operator[]` for palette access** - returns reference, not copy
2. **Validate palette IDs** before accessing:
   ```cpp
   if (palette_id >= 0 && palette_id < group.size()) {
     auto palette = group[palette_id];
   }
   ```
3. **Use correct depth parameter** when creating bitmaps (usually 8 for indexed color)
4. **Initialize ROM-dependent components** only after ROM is fully loaded
5. **Cache palettes** when repeatedly accessing the same palette
6. **Update textures** after changing palettes (textures don't auto-update)

### ROM Addresses (for reference)

```cpp
// From snes_palette.cc
constexpr uint32_t kOverworldPaletteMain = 0xDE6C8;
constexpr uint32_t kOverworldPaletteAux = 0xDE86C;
constexpr uint32_t kOverworldPaletteAnimated = 0xDE604;
constexpr uint32_t kHudPalettes = 0xDD218;
constexpr uint32_t kGlobalSpritesLW = 0xDD308;
constexpr uint32_t kArmorPalettes = 0xDD630;
constexpr uint32_t kSwordPalettes = 0xDD630;
constexpr uint32_t kShieldPalettes = 0xDD648;
constexpr uint32_t kSpritesPalettesAux1 = 0xDD39E;
constexpr uint32_t kSpritesPalettesAux2 = 0xDD446;
constexpr uint32_t kSpritesPalettesAux3 = 0xDD4E0;
constexpr uint32_t kDungeonMainPalettes = 0xDD734;
constexpr uint32_t kHardcodedGrassLW = 0x5FEA9;
constexpr uint32_t kTriforcePalette = 0xF4CD0;
constexpr uint32_t kOverworldMiniMapPalettes = 0x55B27;
```

## Graphics Sheet Palette Application

### Default Palette Assignment
Graphics sheets receive default palettes during ROM loading based on their index:

```cpp
// In LoadAllGraphicsData() - rom.cc
if (i < 113) {
  // Sheets 0-112: Overworld/Dungeon graphics
  graphics_sheets[i].SetPalette(rom.palette_group().dungeon_main[0]);
} else if (i < 128) {
  // Sheets 113-127: Sprite graphics
  graphics_sheets[i].SetPalette(rom.palette_group().sprites_aux1[0]);
} else {
  // Sheets 128-222: Auxiliary/HUD graphics
  graphics_sheets[i].SetPalette(rom.palette_group().hud.palette(0));
}
```

This ensures graphics are visible immediately after loading rather than appearing white.

### Palette Update Workflow
When changing a palette in any editor:

1. Apply the palette: `bitmap.SetPalette(new_palette)`
2. Notify Arena: `gfx::Arena::Get().NotifySheetModified(sheet_index)`
3. Changes propagate to all editors automatically

### Common Pitfalls

**Wrong Palette Access**:
```cpp
// WRONG - Returns copy, may be empty
auto palette = group.palette(id);

// CORRECT - Returns reference
auto palette = group[id];
```

**Missing Surface Update**:
```cpp
// WRONG - Only updates vector, not SDL surface
bitmap.mutable_data() = new_data;

// CORRECT - Updates both vector and surface
bitmap.set_data(new_data);
```

