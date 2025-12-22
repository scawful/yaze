# SNES Compression Format (ALttP)

Decompression algorithm used in A Link to the Past, documented from ZSpriteMaker.

**Source:** `~/Documents/Zelda/Editors/ZSpriteMaker-1/ZSpriteMaker/Utils.cs`

## Command Format

Each compressed block starts with a command byte:

```
Normal Command (when upper 3 bits != 0b111):
┌─────────────────────────────────────┐
│ 7  6  5 │ 4  3  2  1  0            │
│   CMD   │     LENGTH (0-31)         │
└─────────────────────────────────────┘
Length = (byte & 0x1F) + 1

Expanded Command (when upper 3 bits == 0b111):
┌─────────────────────────────────────┬──────────────┐
│ 7  6  5 │ 4  3  2 │ 1  0            │    Byte 2    │
│  0b111  │   CMD   │ LENGTH_HI       │  LENGTH_LO   │
└─────────────────────────────────────┴──────────────┘
Length = ((byte & 0x03) << 8 | nextByte) + 1
```

## Commands

| CMD | Name | Description |
|-----|------|-------------|
| 0 | Direct Copy | Copy `length` bytes directly from ROM to output |
| 1 | Byte Fill | Repeat single byte `length` times |
| 2 | Word Fill | Repeat 2-byte word `length/2` times |
| 3 | Increasing Fill | Write byte, increment, repeat `length` times |
| 4 | Repeat | Copy `length` bytes from earlier in output buffer |

## Terminator

`0xFF` byte terminates decompression.

## C++ Implementation

```cpp
#include <vector>
#include <cstdint>

std::vector<uint8_t> decompress_alttp(const uint8_t* rom, int pos) {
    std::vector<uint8_t> buffer(0x1000, 0);
    int bufferPos = 0;

    while (true) {
        uint8_t databyte = rom[pos];
        if (databyte == 0xFF) break;  // End marker

        uint8_t cmd;
        int length;

        if ((databyte & 0xE0) == 0xE0) {
            // Expanded command
            cmd = (databyte >> 2) & 0x07;
            length = ((databyte & 0x03) << 8) | rom[pos + 1];
            pos += 2;
        } else {
            // Normal command
            cmd = (databyte >> 5) & 0x07;
            length = databyte & 0x1F;
            pos += 1;
        }
        length += 1;  // Minimum length is 1

        switch (cmd) {
            case 0:  // Direct Copy
                for (int i = 0; i < length; i++) {
                    buffer[bufferPos++] = rom[pos++];
                }
                break;

            case 1:  // Byte Fill
                for (int i = 0; i < length; i++) {
                    buffer[bufferPos++] = rom[pos];
                }
                pos += 1;
                break;

            case 2:  // Word Fill
                for (int i = 0; i < length; i += 2) {
                    buffer[bufferPos++] = rom[pos];
                    buffer[bufferPos++] = rom[pos + 1];
                }
                pos += 2;
                break;

            case 3:  // Increasing Fill
                {
                    uint8_t val = rom[pos];
                    for (int i = 0; i < length; i++) {
                        buffer[bufferPos++] = val++;
                    }
                    pos += 1;
                }
                break;

            case 4:  // Repeat from buffer
                {
                    // Little-endian address
                    int addr = rom[pos] | (rom[pos + 1] << 8);
                    for (int i = 0; i < length; i++) {
                        buffer[bufferPos++] = buffer[addr++];
                    }
                    pos += 2;
                }
                break;
        }
    }

    buffer.resize(bufferPos);
    return buffer;
}
```

## Address Conversion

```cpp
// SNES LoROM to PC file offset
inline int snes_to_pc(int addr) {
    return (addr & 0x7FFF) | ((addr & 0x7F0000) >> 1);
}

// PC file offset to SNES LoROM
inline int pc_to_snes(int addr) {
    return (addr & 0x7FFF) | 0x8000 | ((addr & 0x7F8000) << 1);
}
```

## Notes

- Buffer size is 0x1000 (4KB) - typical for tile/map data
- Command 4 (Repeat) uses little-endian address within output buffer
- Expanded commands allow lengths up to 1024 bytes
- Used for: tile graphics, tilemaps, some sprite data
