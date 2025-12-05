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

#### 3. Color Representations in Code
- **SNES 15-bit (`uint16_t`)**: On-disk format `0bbbbbgggggrrrrr`; store raw ROM
  words or write back with `ConvertRgbToSnes`.
- **`gfx::snes_color` struct**: Expands each channel to 0-255 for arithmetic
  without floating point; use in converters and palette math.
- **`gfx::SnesColor` class**: High-level wrapper retaining the original SNES
  value, a `snes_color`, and an ImVec4. Its `rgb()` accessor purposely returns
  0-255 components—run the helper converters (e.g., `ConvertSnesColorToImVec4`)
  before handing colors to ImGui widgets that expect 0.0-1.0 floats.

### Dungeon Palette System

#### Structure
- **20 dungeon palettes** in the `dungeon_main` group
- **90 colors per palette** (full SNES palette for BG layers)
- **180 bytes per palette** (90 colors × 2 bytes per color)
- **ROM Location**: `kDungeonMainPalettes = 0xDD734`

#### Palette Lookup System (CRITICAL)

**IMPORTANT**: Room headers store a "palette set ID" (0-71), NOT a direct palette index!

The game uses a **two-level lookup system** to convert room palette properties to actual
dungeon palette indices:

1. **Palette Set Table** (`paletteset_ids` at ROM `0x75460`)
   - 72 entries, each 4 bytes: `[bg_palette_offset, aux1, aux2, aux3]`
   - The first byte is a **byte offset** into the palette pointer table

2. **Palette Pointer Table** (ROM `0xDEC4B`)
   - Contains 16-bit words that, when divided by 180, give the palette index
   - Each word = ROM offset into dungeon palette data

**Correct Lookup Algorithm**:
```cpp
constexpr uint32_t kPalettesetIds = 0x75460;
constexpr uint32_t kDungeonPalettePointerTable = 0xDEC4B;

// room.palette is 0-71 (palette set ID, NOT palette index!)
uint8_t byte_offset = paletteset_ids[room.palette][0];  // Step 1
uint16_t word = rom.ReadWord(kDungeonPalettePointerTable + byte_offset);  // Step 2  
int palette_id = word / 180;  // Step 3: convert ROM offset to palette index
```

**Example Lookup**:
```
Room palette property = 16
→ paletteset_ids[16][0] = 0x10 (byte offset 16)
→ Word at 0xDEC4B + 16 = 0x05A0 (1440)
→ Palette ID = 1440 / 180 = 8
→ Use dungeon_main[8], NOT dungeon_main[16]!
```

**The Pointer Table (0xDEC4B)**:
| Offset | Word   | Palette ID |
|--------|--------|------------|
| 0      | 0x0000 | 0          |
| 2      | 0x00B4 | 1          |
| 4      | 0x0168 | 2          |
| 6      | 0x021C | 3          |
| ...    | ...    | ...        |
| 38     | 0x0D5C | 19         |

#### Common Pitfall: Direct Palette ID Usage

**WRONG** (causes purple/wrong colors for palette sets 16+):
```cpp
// BUG: Uses byte offset directly as palette ID!
palette_id = paletteset_ids[room.palette][0];
```

**CORRECT**:
```cpp
auto offset = paletteset_ids[room.palette][0];
auto word = rom->ReadWord(0xDEC4B + offset);
palette_id = word.value() / 180;
```

#### Standard Usage
```cpp
// Loading a dungeon palette (with proper lookup)
auto& dungeon_pal_group = rom->palette_group().dungeon_main;
int num_palettes = dungeon_pal_group.size();  // Should be 20

// Perform the two-level lookup
constexpr uint32_t kDungeonPalettePointerTable = 0xDEC4B;
int palette_id = room.palette;  // Default fallback
if (room.palette < paletteset_ids.size()) {
  auto offset = paletteset_ids[room.palette][0];
  auto word = rom->ReadWord(kDungeonPalettePointerTable + offset);
  if (word.ok()) {
    palette_id = word.value() / 180;
  }
}

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

### Transparency and Conversion Best Practices

- Preserve ROM palette words exactly as read; hardware enforces transparency on
  index 0 so we no longer call `set_transparent(true)` while loading.
- Apply transparency only at render time via `SetPaletteWithTransparent()` for
  3BPP sub-palettes or `SetPalette()` for full 256-color assets.
- `SnesColor::rgb()` yields components in 0-255 space; convert to ImGui’s
  expected 0.0-1.0 floats with the helper functions instead of manual divides.
- Use the provided conversion helpers (`ConvertSnesToRgb`, `ImVec4ToSnesColor`,
  `SnesTo8bppColor`) to prevent rounding mistakes and alpha bugs.

```cpp
ImVec4 rgb_255 = snes_color.rgb();
ImVec4 display = ConvertSnesColorToImVec4(snes_color);
ImGui::ColorButton("color", display);
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

#### Palette UI Helpers
- `InlinePaletteSelector` renders a lightweight selection strip (no editing)
  ideal for 8- or 16-color sub-palettes.
- `InlinePaletteEditor` supplies the full editing experience with ImGui color
  pickers, context menus, and optional live preview toggles.
- `PopupPaletteEditor` fits in context menus or modals; it caps at 64 colors to
  keep popups manageable.
- Legacy helpers such as `DisplayPalette()` remain for backward compatibility
  but inherit the 32-color limit—prefer the new helpers for new UI.

-### Metadata-Driven Palette Application

`gfx::BitmapMetadata` tracks the source BPP, palette format, type string, and
expected color count. Set it immediately after creating a bitmap so later code
can make the right choice automatically:

```cpp
bitmap.metadata() = BitmapMetadata{/*source_bpp=*/3,
                                   /*palette_format=*/1,  // 0=full, 1=sub-palette
                                   /*source_type=*/"graphics_sheet",
                                   /*palette_colors=*/8};
bitmap.ApplyPaletteByMetadata(palette);
```

- `palette_format == 0` routes to `SetPalette()` and preserves every color
  (Mode 7, HUD assets, etc.).
- `palette_format == 1` routes to `SetPaletteWithTransparent()` and injects the
  transparent color 0 for 3BPP workflows.
- Validation hooks help catch mismatched palette sizes before they hit SDL.

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

### Texture Synchronization and Regression Notes

- Call `bitmap.UpdateSurfacePixels()` after mutating `bitmap.mutable_data()` to
  copy rendered bytes into the SDL surface before queuing texture creation or
  updates.
- `Bitmap::ApplyStoredPalette()` now rebuilds an `SDL_Color` array sized to the
  actual palette instead of forcing 256 entries—this fixes regressions where
  8- or 16-color palettes were padded with opaque black.
- When updating SDL palette data yourself, mirror that pattern:

```cpp
std::vector<SDL_Color> colors(palette.size());
for (size_t i = 0; i < palette.size(); ++i) {
  const auto& c = palette[i];
  const ImVec4 rgb = c.rgb();  // 0-255 components
  colors[i] = SDL_Color{static_cast<Uint8>(rgb.x),
                        static_cast<Uint8>(rgb.y),
                        static_cast<Uint8>(rgb.z),
                        c.is_transparent() ? 0 : 255};
}
SDL_SetPaletteColors(surface->format->palette, colors.data(), 0,
                     static_cast<int>(colors.size()));
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

### User Workflow Tips

- Choose the widget that matches the task: selectors for choosing colors,
  editors for full control, popups for contextual tweaks.
- The live preview toggle trades responsiveness for performance; disable it
  while batch-editing large (64+ color) palettes.
- Right-click any swatch in the editor to copy the color as SNES hex, RGB
  tuples, or HTML hex—useful when coordinating with external art tools.
- Remember hardware rules: palette index 0 is always transparent and will not
  display even if the stored value is non-zero.
- Keep ROM backups when performing large palette sweeps; palette groups are
  shared across screens so a change can have multiple downstream effects.

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

// Dungeon palette lookup tables (critical for room rendering!)
constexpr uint32_t kPalettesetIds = 0x75460;           // 72 entries × 4 bytes
constexpr uint32_t kDungeonPalettePointerTable = 0xDEC4B;  // Palette ROM offsets
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

## Bitmap Dual Palette System

### Understanding the Two Palette Storage Mechanisms

The `Bitmap` class has **two separate palette storage locations**, which can cause confusion:

| Storage | Location | Populated By | Used For |
|---------|----------|--------------|----------|
| Internal SnesPalette | `bitmap.palette_` | `SetPalette(SnesPalette)` | Serialization, palette editing |
| SDL Surface Palette | `surface_->format->palette` | Both `SetPalette` overloads | Actual rendering to textures |

### The Problem: Empty palette() Returns

When dungeon rooms apply palettes to their layer buffers, they use `SetPalette(vector<SDL_Color>)`:

```cpp
// In room.cc - CreateAllGraphicsLayers()
auto set_dungeon_palette = [](gfx::Bitmap& bmp, const gfx::SnesPalette& pal) {
  std::vector<SDL_Color> colors(256);
  for (size_t i = 0; i < pal.size() && i < 256; ++i) {
    ImVec4 rgb = pal[i].rgb();
    colors[i] = { static_cast<Uint8>(rgb.x), static_cast<Uint8>(rgb.y),
                  static_cast<Uint8>(rgb.z), 255 };
  }
  colors[255] = {0, 0, 0, 0};  // Transparent
  bmp.SetPalette(colors);  // Uses SDL_Color overload!
};
```

This means `bitmap.palette().size()` returns **0** even though the bitmap renders correctly!

### Solution: Extract Palette from SDL Surface

When you need to copy a palette between bitmaps (e.g., for layer compositing), extract it from the SDL surface:

```cpp
void CopyPaletteBetweenBitmaps(const gfx::Bitmap& src, gfx::Bitmap& dst) {
  SDL_Surface* src_surface = src.surface();
  if (!src_surface || !src_surface->format) return;

  SDL_Palette* src_pal = src_surface->format->palette;
  if (!src_pal || src_pal->ncolors == 0) return;

  // Extract palette colors into a vector
  std::vector<SDL_Color> colors(256);
  int colors_to_copy = std::min(src_pal->ncolors, 256);
  for (int i = 0; i < colors_to_copy; ++i) {
    colors[i] = src_pal->colors[i];
  }

  // Apply to destination bitmap
  dst.SetPalette(colors);
}
```

### Layer Compositing with Correct Palettes

When merging multiple layers into a single composite bitmap (as done in `RoomLayerManager::CompositeToOutput()`), the correct approach is:

1. Create/clear the output bitmap
2. For each visible layer:
   - Extract the SDL palette from the first layer with a valid surface
   - Apply it to the output bitmap using `SetPalette(vector<SDL_Color>)`
   - Composite the pixel data (skip transparent indices 0 and 255)
3. Sync pixel data to surface with `UpdateSurfacePixels()`
4. Mark as modified for texture update

**Example from RoomLayerManager**:
```cpp
void RoomLayerManager::CompositeToOutput(Room& room, gfx::Bitmap& output) const {
  // Create output bitmap
  output.Create(512, 512, 8, std::vector<uint8_t>(512*512, 255));

  bool palette_copied = false;
  for (auto layer_type : GetDrawOrder()) {
    auto& buffer = GetLayerBuffer(room, layer_type);
    const auto& src_bitmap = buffer.bitmap();

    // Copy palette from first visible layer
    if (!palette_copied && src_bitmap.surface()) {
      ApplySDLPaletteToBitmap(src_bitmap.surface(), output);
      palette_copied = true;
    }

    // Composite pixels...
  }

  output.UpdateSurfacePixels();
  output.set_modified(true);
}
```

### Best Practices for Palette Handling

1. **Don't assume palette() has data**: Always check `palette().size() > 0` before using it
2. **Use SDL surface as authoritative source**: For rendering-related palette operations
3. **Use SetPalette(SnesPalette) for persistence**: When the palette needs to be saved or edited
4. **Use SetPalette(vector<SDL_Color>) for performance**: When you already have SDL colors
5. **Always call UpdateSurfacePixels()**: After modifying pixel data and before rendering
