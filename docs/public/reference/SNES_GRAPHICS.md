# SNES Graphics Conversion

Tile format conversion routines for ALttP graphics, documented from ZSpriteMaker.

**Source:** `~/Documents/Zelda/Editors/ZSpriteMaker-1/ZSpriteMaker/Utils.cs`

## SNES Tile Formats

### 3BPP (3 bits per pixel, 8 colors)
- Used for: Sprites, some backgrounds
- 24 bytes per 8x8 tile
- Planar format: 2 bytes per row (planes 0-1) + 1 byte per row (plane 2)

### 4BPP (4 bits per pixel, 16 colors)
- Used for: Most backgrounds, UI
- 32 bytes per 8x8 tile
- Planar format: 2 interleaved bitplanes per 16 bytes

## 3BPP Tile Layout

```
Bytes 0-15:  Planes 0 and 1 (interleaved, 2 bytes per row)
Bytes 16-23: Plane 2 (1 byte per row)

Row 0: [Plane0_Row0][Plane1_Row0]
Row 1: [Plane0_Row1][Plane1_Row1]
...
Row 7: [Plane0_Row7][Plane1_Row7]
[Plane2_Row0][Plane2_Row1]...[Plane2_Row7]
```

## C++ Implementation: 3BPP to 8BPP

Converts a sheet of 64 tiles (16x4 arrangement, 128x32 pixels) from 3BPP to indexed 8BPP:

```cpp
#include <array>
#include <cstdint>

constexpr std::array<uint8_t, 8> bitmask = {
    0x80, 0x40, 0x20, 0x10, 0x08, 0x04, 0x02, 0x01
};

// Input: 24 * 64 = 1536 bytes (64 tiles in 3BPP)
// Output: 128 * 32 = 4096 bytes (8BPP indexed)
std::array<uint8_t, 0x1000> snes_3bpp_to_8bpp(const uint8_t* data) {
    std::array<uint8_t, 0x1000> sheet{};
    int index = 0;

    for (int tileRow = 0; tileRow < 4; tileRow++) {           // 4 rows of tiles
        for (int tileCol = 0; tileCol < 16; tileCol++) {      // 16 tiles per row
            int tileOffset = (tileCol + tileRow * 16) * 24;   // 24 bytes per tile

            for (int y = 0; y < 8; y++) {                     // 8 pixel rows
                uint8_t plane0 = data[tileOffset + y * 2];
                uint8_t plane1 = data[tileOffset + y * 2 + 1];
                uint8_t plane2 = data[tileOffset + 16 + y];

                for (int x = 0; x < 8; x++) {                 // 8 pixels per row
                    uint8_t mask = bitmask[x];
                    uint8_t pixel = 0;

                    if (plane0 & mask) pixel |= 1;
                    if (plane1 & mask) pixel |= 2;
                    if (plane2 & mask) pixel |= 4;

                    // Calculate output position in 128-wide sheet
                    int outX = tileCol * 8 + x;
                    int outY = tileRow * 8 + y;
                    sheet[outY * 128 + outX] = pixel;
                }
            }
        }
    }

    return sheet;
}
```

## Alternative: Direct Index Calculation

```cpp
std::array<uint8_t, 0x1000> snes_3bpp_to_8bpp_v2(const uint8_t* data) {
    std::array<uint8_t, 0x1000> sheet{};
    int index = 0;

    for (int j = 0; j < 4; j++) {           // Tile row
        for (int i = 0; i < 16; i++) {      // Tile column
            for (int y = 0; y < 8; y++) {   // Pixel row
                int base = y * 2 + i * 24 + j * 384;

                uint8_t line0 = data[base];
                uint8_t line1 = data[base + 1];
                uint8_t line2 = data[base - y * 2 + y + 16];

                for (uint8_t mask = 0x80; mask > 0; mask >>= 1) {
                    uint8_t pixel = 0;
                    if (line0 & mask) pixel |= 1;
                    if (line1 & mask) pixel |= 2;
                    if (line2 & mask) pixel |= 4;
                    sheet[index++] = pixel;
                }
            }
        }
    }

    return sheet;
}
```

## Palette Reading

SNES uses 15-bit BGR color (5 bits per channel):

```cpp
#include <cstdint>

struct Color {
    uint8_t r, g, b, a;
};

Color read_snes_color(const uint8_t* data, int offset) {
    uint16_t color = data[offset] | (data[offset + 1] << 8);

    return {
        static_cast<uint8_t>((color & 0x001F) << 3),        // R: bits 0-4
        static_cast<uint8_t>(((color >> 5) & 0x1F) << 3),   // G: bits 5-9
        static_cast<uint8_t>(((color >> 10) & 0x1F) << 3),  // B: bits 10-14
        255                                                   // A: opaque
    };
}

// Read full 8-color palette (3BPP)
std::array<Color, 8> read_3bpp_palette(const uint8_t* data, int offset) {
    std::array<Color, 8> palette;
    for (int i = 0; i < 8; i++) {
        palette[i] = read_snes_color(data, offset + i * 2);
    }
    return palette;
}

// Read full 16-color palette (4BPP)
std::array<Color, 16> read_4bpp_palette(const uint8_t* data, int offset) {
    std::array<Color, 16> palette;
    for (int i = 0; i < 16; i++) {
        palette[i] = read_snes_color(data, offset + i * 2);
    }
    return palette;
}
```

## OAM Tile Positioning

From ZSpriteMaker's OamTile class - convert tile ID to sheet coordinates:

```cpp
// Tile ID to sprite sheet pixel position
// Assumes 16 tiles per row (128 pixels wide sheet)
inline int tile_to_sheet_x(uint16_t id) { return (id % 16) * 8; }
inline int tile_to_sheet_y(uint16_t id) { return (id / 16) * 8; }

// Packed OAM format (32-bit)
inline uint32_t pack_oam_tile(uint16_t id, uint8_t x, uint8_t y,
                               uint8_t palette, uint8_t priority,
                               bool mirrorX, bool mirrorY) {
    return (id << 16) |
           ((mirrorY ? 0 : 1) << 31) |
           ((mirrorX ? 0 : 1) << 30) |
           (priority << 28) |
           (palette << 25) |
           (x << 8) |
           y;
}
```

## Sheet Dimensions

| Format | Tiles | Sheet Size | Bytes/Tile | Total Bytes |
|--------|-------|------------|------------|-------------|
| 3BPP 64-tile | 16x4 | 128x32 px | 24 | 1,536 |
| 4BPP 64-tile | 16x4 | 128x32 px | 32 | 2,048 |
| 3BPP 128-tile | 16x8 | 128x64 px | 24 | 3,072 |

## Integration Notes

- Color index 0 is typically transparent
- SNES sprites use 3BPP (8 colors per palette row)
- Background tiles often use 4BPP (16 colors)
- ALttP Link sprites: 3BPP, multiple sheets for different states
