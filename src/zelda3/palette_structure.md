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
- **ROM**: 0xDE6C8 (`PaletteData_owmain_00` / `#_1BE6C8` in usdasm `bank_1B.asm`)
- **Sets**: 6
- **Bytes per Set**: 70 (35 colors × 2 bytes)
- **Stored Layout**: 5 sub-palettes × 7 colors (transparent slots are implicit, not stored)
- **Runtime Load** (`PaletteLoad_OWBGMain`):
  - Destination: CGRAM byte offset `0x0042` (color index `0x21`, row 2 col 1)
  - Loads: 5 palettes × 7 colors (35 total)
  - Placement: CGRAM rows 2-6, cols 1-7

#### Overworld Auxiliary (21 colors per set)
- **ROM**: 0xDE86C (`PaletteData_owaux_00` / `#_1BE86C`)
- **Sets**: 20
- **Bytes per Set**: 42 (21 colors × 2 bytes)
- **Stored Layout**: 3 sub-palettes × 7 colors
- **Runtime Load**:
  - `PaletteLoad_OWBG1`: destination `0x0052` (color index `0x29`, row 2 col 9), loads 3×7
  - `PaletteLoad_OWBG2`: destination `0x00B2` (color index `0x59`, row 5 col 9), loads 3×7

#### Overworld Animated (7 colors per set)
- **ROM**: 0xDE604 (`PaletteData_owanim_00` / `#_1BE604`)
- **Sets**: 14
- **Bytes per Set**: 14 (7 colors × 2 bytes)
- **Stored Layout**: 7 colors (transparent slot is implicit)
- **Runtime Load** (`PaletteLoad_OWBG3`):
  - Destination: `0x00E2` (color index `0x71`, row 7 col 1)
  - Loads: 7 colors (row 7, cols 1-7)

#### Dungeon Main (90 colors per set)
- **ROM**: 0xDD734 (`PaletteData_dungeon_00` / `#_1BD734`)
- **Sets**: 20 (indices 0-19)
- **Bytes per Set**: 180 (90 colors × 2 bytes)
- **Stored Layout**: 6 CGRAM banks × 15 colors (transparent slot per bank is implicit, not stored)
- **Runtime Load** (`PaletteLoad_UnderworldSet`):
  - Destination: `0x0042` (color index `0x21`, row 2 col 1)
  - Loads: 6 palettes × 15 colors (90 total)
  - Placement: CGRAM rows 2-7, cols 1-15

##### Palette Lookup System

Room headers store a "palette set ID" (0-71), NOT a direct dungeon palette index!

**Two-Level Lookup**:
1. `paletteset_ids[room.palette][0]` → byte offset (at ROM 0x75460)
2. Read word at `0xDEC4B + offset` → ROM offset into palette data
3. Divide by 180 → actual palette index (0-19)

```
Example: Room palette = 16
  → paletteset_ids[16][0] = 0x10 (offset 16)
  → Word at 0xDEC4B + 16 = 0x05A0 (1440)
  → Palette = 1440 / 180 = 8
```

### Sprite Palettes (OAM)

Sprite palettes use rows 8-15 (colors 128-255).

#### Global Sprites (60 colors total)
- **ROM LW**: 0xDD218
- **ROM DW**: 0xDD290
- **Total**: 2 sets (LW and DW)
- **Stored Layout**: 4 banks × 15 colors (transparent slot per bank is implicit, not stored)
- **CGRAM Placement (Yaze compositor)**:
  - Row 8: used by sprite aux half-palettes (cols 1-7 and 9-15)
  - Rows 9-12: global sprite banks 0-3 (cols 1-15)

#### Sprites Auxiliary 1 (7 colors per palette)
- **Structure**: 12 palettes, each occupying half a row
  - Palette 0: 7 colors (CGRAM row 8, cols 1-7)
  - Palette 1: 7 colors (CGRAM row 8, cols 9-15)
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
