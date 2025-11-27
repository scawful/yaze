# ZSM File Format Specification

ZSpriteMaker Project File format (`.zsm`) - used by ZSpriteMaker for ALttP custom sprites.

**Source:** `~/Documents/Zelda/Editors/ZSpriteMaker-1/`

## Format Overview

Binary file format using .NET BinaryWriter/BinaryReader conventions:
- Strings: Length-prefixed (7-bit encoded length + UTF-8 bytes)
- Integers: Little-endian 32-bit
- Booleans: Single byte (0x00 = false, 0x01 = true)

## File Structure

```
┌─────────────────────────────────────────────────┐
│ ANIMATIONS SECTION                              │
├─────────────────────────────────────────────────┤
│ int32     animationCount                        │
│ for each animation:                             │
│   string  name         (length-prefixed)        │
│   byte    frameStart                            │
│   byte    frameEnd                              │
│   byte    frameSpeed                            │
├─────────────────────────────────────────────────┤
│ FRAMES SECTION                                  │
├─────────────────────────────────────────────────┤
│ int32     frameCount                            │
│ for each frame:                                 │
│   int32   tileCount                             │
│   for each tile:                                │
│     uint16  id         (tile index in sheet)    │
│     byte    palette    (0-7)                    │
│     bool    mirrorX                             │
│     bool    mirrorY                             │
│     byte    priority   (0-3, default 3)         │
│     bool    size       (false=8x8, true=16x16)  │
│     byte    x          (0-251)                  │
│     byte    y          (0-219)                  │
│     byte    z          (depth/layer)            │
├─────────────────────────────────────────────────┤
│ SPRITE PROPERTIES (20 booleans)                 │
├─────────────────────────────────────────────────┤
│ bool      blockable                             │
│ bool      canFall                               │
│ bool      collisionLayer                        │
│ bool      customDeath                           │
│ bool      damageSound                           │
│ bool      deflectArrows                         │
│ bool      deflectProjectiles                    │
│ bool      fast                                  │
│ bool      harmless                              │
│ bool      impervious                            │
│ bool      imperviousArrow                       │
│ bool      imperviousMelee                       │
│ bool      interaction                           │
│ bool      isBoss                                │
│ bool      persist                               │
│ bool      shadow                                │
│ bool      smallShadow                           │
│ statis    (stasis)                              │
│ bool      statue                                │
│ bool      waterSprite                           │
├─────────────────────────────────────────────────┤
│ SPRITE STATS (6 bytes)                          │
├─────────────────────────────────────────────────┤
│ byte      prize       (drop item ID)            │
│ byte      palette     (sprite palette)          │
│ byte      oamNbr      (OAM slot count)          │
│ byte      hitbox      (collision box ID)        │
│ byte      health                                │
│ byte      damage                                │
├─────────────────────────────────────────────────┤
│ USER ROUTINES SECTION (optional)                │
├─────────────────────────────────────────────────┤
│ string    spriteName  (length-prefixed)         │
│ int32     routineCount                          │
│ for each routine:                               │
│   string  name        (e.g., "Long Main")       │
│   string  code        (ASM code)                │
├─────────────────────────────────────────────────┤
│ SPRITE ID (optional)                            │
├─────────────────────────────────────────────────┤
│ string    spriteId    (hex string)              │
└─────────────────────────────────────────────────┘
```

## Data Types

### OamTile
```cpp
struct OamTile {
    uint16_t id;        // Tile index in sprite sheet (0-511)
    uint8_t  palette;   // Palette index (0-7)
    bool     mirrorX;   // Horizontal flip
    bool     mirrorY;   // Vertical flip
    uint8_t  priority;  // BG priority (0-3)
    bool     size;      // false=8x8, true=16x16
    uint8_t  x;         // X position (0-251)
    uint8_t  y;         // Y position (0-219)
    uint8_t  z;         // Z depth for sorting
};

// Tile sheet position from ID:
// sheet_x = (id % 16) * 8
// sheet_y = (id / 16) * 8
```

### AnimationGroup
```cpp
struct AnimationGroup {
    std::string name;       // Animation name
    uint8_t     frameStart; // First frame index
    uint8_t     frameEnd;   // Last frame index
    uint8_t     frameSpeed; // Frames per tick
};
```

### Frame
```cpp
struct Frame {
    std::vector<OamTile> tiles;
};
```

## Sprite Properties Bitfield (Alternative)

The 20 boolean properties could be packed as bitfield:
```cpp
enum SpriteFlags : uint32_t {
    BLOCKABLE           = 1 << 0,
    CAN_FALL            = 1 << 1,
    COLLISION_LAYER     = 1 << 2,
    CUSTOM_DEATH        = 1 << 3,
    DAMAGE_SOUND        = 1 << 4,
    DEFLECT_ARROWS      = 1 << 5,
    DEFLECT_PROJECTILES = 1 << 6,
    FAST                = 1 << 7,
    HARMLESS            = 1 << 8,
    IMPERVIOUS          = 1 << 9,
    IMPERVIOUS_ARROW    = 1 << 10,
    IMPERVIOUS_MELEE    = 1 << 11,
    INTERACTION         = 1 << 12,
    IS_BOSS             = 1 << 13,
    PERSIST             = 1 << 14,
    SHADOW              = 1 << 15,
    SMALL_SHADOW        = 1 << 16,
    STASIS              = 1 << 17,
    STATUE              = 1 << 18,
    WATER_SPRITE        = 1 << 19,
};
```

## Default User Routines

New projects include three template routines:
1. **Long Main** - Main sprite loop (`TemplateLongMain.asm`)
2. **Sprite Prep** - Initialization (`TemplatePrep.asm`)
3. **Sprite Draw** - Rendering (`TemplateDraw.asm`)

## Related Formats

### ZSPR (ALttP Randomizer Format)
Different format used by ALttP Randomizer for Link sprite replacements.
- Magic: `ZSPR` (4 bytes)
- Contains: sprite sheet, palette, glove colors, metadata
- Spec: https://github.com/Zarby89/ZScreamRandomizer

### ZSM vs ZSPR
| Feature | ZSM | ZSPR |
|---------|-----|------|
| Purpose | Custom enemy/NPC sprites | Link sprite replacement |
| Contains ASM | Yes (routines) | No |
| Animation data | Yes | No (uses ROM animations) |
| Properties | Sprite behavior flags | Metadata only |
| Editor | ZSpriteMaker | SpriteSomething, others |

## Implementation Notes

### Reading ZSM in C++
```cpp
// .NET BinaryReader string format:
std::string read_dotnet_string(std::istream& is) {
    uint32_t length = 0;
    uint8_t byte;
    int shift = 0;
    do {
        is.read(reinterpret_cast<char*>(&byte), 1);
        length |= (byte & 0x7F) << shift;
        shift += 7;
    } while (byte & 0x80);

    std::string result(length, '\0');
    is.read(&result[0], length);
    return result;
}
```

### Validation
- Frame count typically 0-255 (byte range in UI)
- Tile positions clamped: x < 252, y < 220
- Palette 0-7
- Priority 0-3

## Source Files

From `~/Documents/Zelda/Editors/ZSpriteMaker-1/ZSpriteMaker/`:
- `MainWindow.xaml.cs:323-419` - Save_Command (write format)
- `MainWindow.xaml.cs:209-319` - Open_Command (read format)
- `OamTile.cs` - Tile data structure
- `Frame.cs` - Frame container
- `AnimationGroup.cs` - Animation definition
