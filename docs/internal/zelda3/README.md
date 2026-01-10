# Zelda3 Core Module Reference

Technical reference for the `src/zelda3/` module, which handles ALttP-specific ROM data structures.

## Module Structure

```
src/zelda3/
├── dungeon/     # Dungeon room data (296 rooms)
├── overworld/   # Overworld map data (160 maps)
├── music/       # SPC700 music data
├── sprite/      # Sprite graphics and properties
├── screen/      # Screen/HUD data
├── formats/     # File format handlers
├── game_data.h  # Core GameData struct
├── common.h     # Shared utilities
└── resource_labels.h  # Human-readable labels
```

## Core Constants

| Constant | Value | Description |
|----------|-------|-------------|
| `kNumGfxSheets` | 223 | Total graphics sheets |
| `kNumLinkSheets` | 14 | Link sprite sheets |
| `kNumMainBlocksets` | 37 | Main tile blocksets |
| `kNumRoomBlocksets` | 82 | Room-specific blocksets |
| `kNumSpritesets` | 144 | Sprite graphics sets |
| `kNumPalettesets` | 72 | Palette sets |
| `kOverworldMapCount` | 160 | Overworld maps |
| `kNumRooms` | 296 | Dungeon rooms |

## GameData Structure

The central `GameData` struct (`game_data.h`) holds all loaded ROM data:

```cpp
struct GameData {
  Rom* rom_;                    // ROM reference (non-owning)
  zelda3_version version;       // US, JP, SD, RANDO
  std::string title;            // ROM title from header

  // Graphics
  std::vector<uint8_t> graphics_buffer;
  std::array<std::vector<uint8_t>, 223> raw_gfx_sheets;
  std::array<gfx::Bitmap, 223> gfx_bitmaps;
  std::array<gfx::Bitmap, 14> link_graphics;
  gfx::Bitmap font_graphics;

  // Lookup tables
  gfx::PaletteGroupMap palette_groups;
  std::array<std::array<uint8_t, 8>, 37> main_blockset_ids;
  std::array<std::array<uint8_t, 4>, 82> room_blockset_ids;
  std::array<std::array<uint8_t, 4>, 144> spriteset_ids;
  std::array<std::array<uint8_t, 4>, 72> paletteset_ids;
};
```

### Loading Flow

```cpp
Rom rom;
rom.LoadFromFile("zelda3.sfc");

GameData data(&rom);
LoadGameData(rom, data);  // Loads all graphics, palettes, lookup tables
```

Individual loaders for fine-grained control:
- `LoadMetadata()` - ROM version, title
- `LoadPalettes()` - All palette groups
- `LoadGfxGroups()` - Blockset/spriteset lookup tables
- `LoadGraphics()` - Decompress and index all 223 sheets

## Dungeon Module

Located in `src/zelda3/dungeon/`. Key files:

| File | Purpose |
|------|---------|
| `room.h` | `Room` class - single dungeon room |
| `room_object.h` | Objects within rooms |
| `dungeon_room_sprite.h` | Sprite placement in rooms |
| `dungeon_map.h` | Dungeon floor plan data |
| `object_names.h` | Human-readable object names |

### Room Data Structure

Each room has:
- Header data (palette, blockset, spriteset, etc.)
- Layer 1/2/3 tile data (floor, collision, background)
- Object list (doors, chests, pots, etc.)
- Sprite list (enemies, NPCs)
- Entrance/exit points

### Key Addresses

```cpp
// From dungeon_rom_addresses.h
constexpr uint32_t kRoomHeaderPointer = 0xB5E7;
constexpr uint32_t kRoomHeaderBank = 0x04;
constexpr uint32_t kRoomObjectPointer = 0x874C;
constexpr uint32_t kRoomSpritePointer = 0x4D62;
```

## Overworld Module

Located in `src/zelda3/overworld/`. Key files:

| File | Purpose |
|------|---------|
| `overworld.h` | `Overworld` class - all 160 maps |
| `overworld_map.h` | Single map data |
| `overworld_entrance.h` | Entrance points |
| `overworld_exit.h` | Exit points |
| `overworld_item.h` | Item locations |

### Map Structure

160 maps organized as:
- 0-63: Light World (8x8 grid)
- 64-127: Dark World (8x8 grid)
- 128-144: Special areas (caves, houses)
- 145-159: Master Sword area, etc.

Maps can be small (32x32 tiles), large (64x64), or multi-area (linked maps).

### ZSCustomOverworld Support

The editor supports ZSCustomOverworld (ZSOW) v2 and v3 patches which expand:
- Tile variety
- Palette options
- Map linking

See `overworld-tail-expansion.md` for ROM expansion details.

## Palette System

Palettes are stored in groups by type:

| Group | Count | Purpose |
|-------|-------|---------|
| `ow_main` | 6 | Overworld main palettes |
| `ow_aux` | 20 | Overworld auxiliary |
| `ow_animated` | 14 | Animated tile palettes |
| `hud` | 2 | HUD/menu palettes |
| `dungeon_main` | 20 | Dungeon main palettes |
| `sprite_aux` | 23 | Sprite palettes |
| `armor` | 5 | Link's armor colors |
| `sword` | 4 | Sword colors |
| `shield` | 3 | Shield colors |

### Palette Lookup

Dungeon rooms use a two-level lookup:
1. Room header stores `palette_set_id` (0-71)
2. `paletteset_ids[id][0]` gives byte offset into `kDungeonPalettePointerTable`
3. Word at that offset / 180 = actual palette index

See `palette_constants.h` for addresses and `palette_structure.md` for format details.

## Graphics Decompression

ALttP uses LC_LZ2 compression for graphics. Key functions:

```cpp
// Decompress sheet to 8bpp indexed
DecompressV2(rom_data, address, output_buffer, 0x800);

// Get graphics address from pointer tables
GetGraphicsAddress(data, sheet_index, ptr1, ptr2, ptr3, rom_size);
```

Graphics are stored as 4bpp planar, converted to 8bpp indexed on load for easier manipulation.

## Music Module

Located in `src/zelda3/music/`. SPC700-related data:

| File | Purpose |
|------|---------|
| `tracker.h` | Music sequence data |
| `instrument.h` | Instrument definitions |
| `sample.h` | BRR audio samples |

Music editing is experimental. The SPC700 data format is partially documented in ZSNES/bsnes sources.

## Version Differences

US and JP ROMs have different pointer tables. Version-specific offsets are in:

```cpp
// From game_data.h
static const std::map<zelda3_version, zelda3_version_pointers>
    kVersionConstantsMap = {
        {zelda3_version::US, zelda3_us_pointers},
        {zelda3_version::JP, zelda3_jp_pointers},
        // ...
    };
```

## Related Documentation

- `dungeon-spec.md` - Dungeon data format specification
- `overworld-tail-expansion.md` - ZSOW expansion details
- `alttp-object-handlers.md` - Object behavior tables
- `alttp-wram-state.md` - WRAM address reference
