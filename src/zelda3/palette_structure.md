# SNES Palette Structure for ALTTP

## SNES Palette Memory Layout

The SNES has 256 color palette entries organized as:
- **16 palette rows** of **16 colors each**
- Each row starts with color index 0, which is **transparent**
- Palettes must be aligned to 16-color boundaries

### Example Layout
```
Row 0: Colors 0-15   (Color 0 = transparent)
Row 1: Colors 16-31  (Color 16 = transparent)
Row 2: Colors 32-47  (Color 32 = transparent)
...
```

## ALTTP Palette Groups - Corrected Structure

### Background Palettes (BG)

#### Overworld Main (35 colors per set)
- **Structure**: 2 full rows + 3 colors
  - Row 0: Colors 0-15 (transparent + 15 colors)
  - Row 1: Colors 16-31 (transparent + 15 colors)
  - Row 2: Colors 32-34 (3 colors)
- **ROM**: 0xDE6C8
- **Sets**: 60 (20 LW, 20 DW, 20 Special)

#### Overworld Auxiliary (21 colors per set)
- **Structure**: 1 full row + 5 colors
  - Row 0: Colors 0-15 (transparent + 15 colors)
  - Row 1: Colors 16-20 (5 colors)
- **ROM**: 0xDE86C
- **Sets**: 20

#### Overworld Animated (7 colors per set)
- **Structure**: Half-row without transparent
  - Colors 0-6 (7 colors, no transparent marker as these overlay existing)
- **ROM**: 0xDE604
- **Sets**: 14

#### Dungeon Main (90 colors per set)
- **Structure**: 5 full rows + 10 colors
  - Row 0: Colors 0-15 (transparent + 15 colors)
  - Row 1: Colors 16-31 (transparent + 15 colors)
  - Row 2: Colors 32-47 (transparent + 15 colors)
  - Row 3: Colors 48-63 (transparent + 15 colors)
  - Row 4: Colors 64-79 (transparent + 15 colors)
  - Row 5: Colors 80-89 (10 colors)
- **ROM**: 0xDD734
- **Sets**: 20 (one per dungeon)

### Sprite Palettes (OAM)

Sprite palettes use rows 8-15 (colors 128-255).

#### Global Sprites (60 colors total)
- **Structure**: 4 rows (each with 15 actual colors + transparent)
  - Row 8: Colors 128-143 (Sprite Palette 0: transparent + 15 colors)
  - Row 9: Colors 144-159 (Sprite Palette 1: transparent + 15 colors)
  - Row 10: Colors 160-175 (Sprite Palette 2: transparent + 15 colors)
  - Row 11: Colors 176-191 (Sprite Palette 3: transparent + 15 colors)
- **ROM LW**: 0xDD218
- **ROM DW**: 0xDD290
- **Total**: 2 sets (LW and DW)

#### Sprites Auxiliary 1 (7 colors per palette)
- **Structure**: 12 palettes, each occupying half a row
  - Palette 0: 7 colors (indices 1-7 of first half-row)
  - Palette 1: 7 colors (indices 9-15 of second half-row)
  - ...and so on
- **ROM**: 0xDD39E
- **Palettes**: 12

#### Sprites Auxiliary 2 (7 colors per palette)
- **Structure**: 11 palettes, each occupying half a row
- **ROM**: 0xDD446
- **Palettes**: 11

#### Sprites Auxiliary 3 (7 colors per palette)
- **Structure**: 24 palettes, each occupying half a row
- **ROM**: 0xDD4E0
- **Palettes**: 24

### Equipment/Link Palettes

#### Armor/Link (15 colors per palette)
- **Structure**: Full row minus transparent
  - Each palette: transparent + 15 colors
- **ROM**: 0xDD308
- **Palettes**: 5 (Green Mail, Blue Mail, Red Mail, Bunny, Electrocuted)

#### Swords (3 colors per palette)
- **Structure**: 3 colors within row (no transparent needed as overlay)
- **ROM**: 0xDD630
- **Palettes**: 4 (Fighter, Master, Tempered, Golden)

#### Shields (4 colors per palette)
- **Structure**: 4 colors within row (no transparent needed as overlay)
- **ROM**: 0xDD648
- **Palettes**: 3 (Fighter, Fire, Mirror)

### HUD Palettes

#### HUD (32 colors per set)
- **Structure**: 2 full rows
  - Row 0: Colors 0-15 (transparent + 15 colors)
  - Row 1: Colors 16-31 (transparent + 15 colors)
- **ROM**: 0xDD660
- **Sets**: 2

### Special Colors

#### Grass (3 individual colors)
- LW: 0x5FEA9
- DW: 0x5FEB3
- Special: 0x75640

#### 3D Objects (8 colors per palette)
- **Triforce**: 0x64425
- **Crystal**: 0xF4CD3

#### Overworld Mini Map (128 colors per set)
- **Structure**: 8 full rows
- **ROM**: 0x55B27
- **Sets**: 2

## Key Principles

1. **Transparent Color**: Always at indices 0, 16, 32, 48, 64, etc. (multiples of 16)
2. **Row Alignment**: Palettes should respect 16-color row boundaries
3. **Overlay Palettes**: Some palettes (like animated, swords, shields) overlay existing colors and don't have their own transparent
4. **Sub-Palettes**: Multiple small palettes can share a row if they're 8 colors or less

## Implementation Notes

When loading palettes, we must:
1. Mark color index 0 of each 16-color row as transparent
2. For palettes < 16 colors, understand if they're standalone (need transparent) or overlays (don't need transparent)
3. Display palettes in UI with proper row alignment for clarity

