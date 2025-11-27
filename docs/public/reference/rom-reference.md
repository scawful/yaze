# A Link to the Past ROM Reference

## Graphics System

### Graphics Sheets
The ROM contains 223 graphics sheets, each 128x32 pixels (16 tiles × 4 tiles, 8×8 tile size):

**Sheet Categories**:
- Sheets 0-112: Overworld/Dungeon graphics (compressed 3BPP)
- Sheets 113-114: Uncompressed 2BPP graphics
- Sheets 115-126: Uncompressed 3BPP graphics  
- Sheets 127-217: Additional dungeon/sprite graphics
- Sheets 218-222: Menu/HUD graphics (2BPP)

**Graphics Format**:
- 3BPP (3 bits per pixel): 8 colors per tile, commonly used for backgrounds
- 2BPP (2 bits per pixel): 4 colors per tile, used for fonts and simple graphics

### Palette System

#### Color Format
SNES uses 15-bit BGR555 color format:
- 2 bytes per color
- Format: `0BBB BBGG GGGR RRRR`
- Each channel: 0-31 (5 bits)
- Total possible colors: 32,768

**Conversion Formulas:**
```cpp
// From BGR555 to RGB888 (0-255 range)
uint8_t red   = (snes_color & 0x1F) * 8;
uint8_t green = ((snes_color >> 5) & 0x1F) * 8;
uint8_t blue  = ((snes_color >> 10) & 0x1F) * 8;

// From RGB888 to BGR555
uint16_t snes_color = ((r / 8) & 0x1F) |
                      (((g / 8) & 0x1F) << 5) |
                      (((b / 8) & 0x1F) << 10);
```

#### 16-Color Row Structure

The SNES organizes palettes in **rows of 16 colors**:
- Each row starts with a **transparent color** at indices 0, 16, 32, 48, 64, etc.
- Palettes must respect these boundaries for proper display
- Multiple sub-palettes can share a row if they're 8 colors or less

#### Palette Groups

**Dungeon Palettes** (0xDD734):
- 20 palettes × 90 colors each
- Structure: 5 full rows (0-79) + 10 colors (80-89)
- Transparent at: indices 0, 16, 32, 48, 64
- Distributed as: BG1 colors, BG2 colors, sprite colors
- Applied per-room via palette ID

**Overworld Palettes**:
- Main: 35 colors per palette (0xDE6C8)
  - Structure: 2 full rows (0-15, 16-31) + 3 colors (32-34)
  - Transparent at: 0, 16
- Auxiliary: 21 colors per palette (0xDE86C)
  - Structure: 1 full row (0-15) + 5 colors (16-20)
  - Transparent at: 0
- Animated: 7 colors per palette (0xDE604)
  - Overlay palette (no transparent)

**Sprite Palettes**:
- Global sprites: 2 palettes of 60 colors each
  - Light World: 0xDD218
  - Dark World: 0xDD290
  - Structure: 4 rows per set (0-15, 16-31, 32-47, 48-59)
  - Transparent at: 0, 16, 32, 48
- Auxiliary 1: 12 palettes × 7 colors (0xDD39E)
- Auxiliary 2: 11 palettes × 7 colors (0xDD446)
- Auxiliary 3: 24 palettes × 7 colors (0xDD4E0)
- Note: Aux palettes store 7 colors; transparent added at runtime

**Other Palettes**:
- HUD: 2 palettes × 32 colors (0xDD218)
  - Structure: 2 full rows (0-15, 16-31)
  - Transparent at: 0, 16
- Armors: 5 palettes × 15 colors (0xDD630)
  - 15 colors + transparent at runtime = full 16-color row
- Swords: 4 palettes × 3 colors (overlay, no transparent)
- Shields: 3 palettes × 4 colors (overlay, no transparent)
- Grass: 3 individual hardcoded colors (LW, DW, Special)
- 3D Objects: 2 palettes × 8 colors (Triforce, Crystal)
- Overworld Mini Map: 2 palettes × 128 colors
  - Structure: 8 full rows (0-127)
  - Transparent at: 0, 16, 32, 48, 64, 80, 96, 112

#### Palette Application to Graphics

**8-Color Sub-Palettes:**
- Index 0: Transparent (not rendered)
- Indices 1-7: Visible colors
- Each tile/sprite references a specific 8-color sub-palette

**Graphics Format:**
- 2BPP: 4 colors per tile (uses indices 0-3)
- 3BPP: 8 colors per tile (uses indices 0-7)
- 4BPP: 16 colors per tile (uses indices 0-15)

**Rendering Process:**
1. Load compressed graphics sheet from ROM
2. Decompress and convert to indexed pixels (values 0-7 for 3BPP)
3. Select appropriate palette group for room/area
4. Map pixel indices to actual colors from palette
5. Upload to GPU texture

## Dungeon System

### Room Data Structure

**Room Objects** (3 bytes each):
```
Byte 1: YYYXXXXX
  YYY = Y coordinate (0-31)
  XXXXX = X coordinate (0-31) + layer flag (bit 5)

Byte 2: SSSSSSSS  
  Size byte (interpretation varies by object type)

Byte 3: IIIIIIII
  Object ID or ID component (0-255)
```

**Object Patterns**:
- 0x34: 1x1 solid block
- 0x00-0x08: Rightward 2x2 expansion
- 0x60-0x68: Downward 2x2 expansion
- 0x09-0x14: Diagonal acute (/)
- 0x15-0x20: Diagonal grave (\)
- 0x33, 0x70-0x71: 4x4 blocks

### Tile16 Format

Each Tile16 is composed of four 8x8 tiles:
```
[Top-Left] [Top-Right]
[Bot-Left] [Bot-Right]
```

Stored as 8 bytes (2 bytes per 8x8 tile):
```cpp
// 16-bit tile info: vhopppcccccccccc
// v = vertical flip
// h = horizontal flip  
// o = priority
// ppp = palette index (0-7)
// cccccccccc = tile ID (0-1023)
```

### Blocksets

Each dungeon room uses a blockset defining which graphics sheets are loaded:
- 16 sheets per blockset
- First 8: Background graphics
- Last 8: Sprite graphics

**Blockset Pointer**: 0x0FFC40

## Message System

### Text Data Locations
- Text Block 1: 0xE0000 - 0xE7FFF (32KB)
- Text Block 2: 0x75F40 - 0x773FF (5.3KB)
- Dictionary Pointers: 0x74703

### Character Encoding
- 0x00-0x66: Standard characters (A-Z, a-z, 0-9, punctuation)
- 0x67-0x80: Text commands (line breaks, colors, window controls)
- 0x88-0xE8: Dictionary references (compressed common words)
- 0x7F: Message terminator

### Text Commands
- 0x6A: Player name insertion
- 0x6B: Window border (+ 1 argument byte)
- 0x73: Scroll text vertically
- 0x74-0x76: Line 1, 2, 3 markers
- 0x77: Text color (+ 1 argument byte)
- 0x7E: Wait for key press

### Font Graphics
- Location: 0x70000
- Format: 2BPP
- Size: 8KB (0x2000 bytes)
- Organized as 8x8 tiles for characters

## Overworld System

### Map Structure
- 3 worlds: Light, Dark, Special
- Each world: 8×8 grid of 512×512 pixel maps
- Total maps: 192 (64 per world)

### Area Sizes (ZSCustomOverworld v3+)
- Small: 512×512 (1 map)
- Wide: 1024×512 (2 maps horizontal)
- Tall: 512×1024 (2 maps vertical)
- Large: 1024×1024 (4 maps in 2×2 grid)

### Tile Format
- Map uses Tile16 (16×16 pixel tiles)
- Each Tile16 composed of four 8x8 tiles
- 32×32 Tile16s per 512×512 map

## Compression

### LC-LZ2 Algorithm
Both graphics and map data use a variant of LZ compression:

**Commands**:
- 0x00-0x1F: Direct copy (copy N bytes verbatim)
- 0x20-0x3F: Byte fill (repeat 1 byte N times)
- 0x40-0x5F: Word fill (repeat 2 bytes N times)
- 0x60-0x7F: Incremental fill (0x05, 0x06, 0x07...)
- 0x80-0xFF: LZ copy (copy from earlier in buffer)

**Extended Headers** (for lengths > 31):
- 0xE0-0xFF: Extended command headers
- Allows compression of up to 1024 bytes

**Modes**:
- Mode 1 (Nintendo): Graphics decompression (byte order variation)
- Mode 2 (Nintendo): Overworld decompression (byte order variation)

## Memory Map

### ROM Banks (LoROM)
- Bank 0x00-0x7F: ROM data
- Bank 0x80-0xFF: Mirror of 0x00-0x7F

### Important ROM Locations
```
Graphics:
  0x080000+  Link graphics (uncompressed)
  0x0F8000+  Dungeon object subtype tables
  0x0FB373   Object subtype 1 table
  0x0FFC40   Blockset pointers

Palettes:
  0x0DD218   HUD palettes
  0x0DD308   Global sprite palettes
  0x0DD734   Dungeon main palettes  
  0x0DE6C8   Overworld main palettes
  0x0DE604   Animated palettes

Text:
  0x070000   Font graphics
  0x0E0000   Main text data
  0x075F40   Extended text data

Tile Data:
  0x0F8000   Tile16 data (4032 tiles)
```

---

## Additional Format Documentation

For more detailed format specifications, see:

- [SNES Graphics Format](SNES_GRAPHICS.md) - Tile and sprite format specifications
- [SNES Compression](SNES_COMPRESSION.md) - Detailed LC-LZ2 compression algorithm
- [Symbol Format](SYMBOL_FORMAT.md) - Assembler symbol file format
- [ZSM Format](ZSM_FORMAT.md) - Music and sound effect format
- [Save State Format](SAVE_STATE_FORMAT.md) - Emulator save state specifications

---

**Last Updated**: November 27, 2025

