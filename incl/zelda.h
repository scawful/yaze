#ifndef ZELDA_H
#define ZELDA_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stdint.h>

/**
 * @brief Different versions of the game supported by yaze.
 */
enum zelda3_version {
  US = 1,     // US version
  JP = 2,     // JP version
  SD = 3,     // Super Donkey Proto (Experimental)
  RANDO = 4,  // Randomizer (Unimplemented)
};

/**
 * @brief Pointers for each version of the game.
 */
struct zelda3_version_pointers {
  uint32_t kGfxAnimatedPointer;
  uint32_t kOverworldGfxGroups1;
  uint32_t kOverworldGfxGroups2;
  uint32_t kCompressedAllMap32PointersHigh;
  uint32_t kCompressedAllMap32PointersLow;
  uint32_t kOverworldMapPaletteGroup;
  uint32_t kOverlayPointers;
  uint32_t kOverlayPointersBank;
  uint32_t kOverworldTilesType;
  uint32_t kOverworldGfxPtr1;
  uint32_t kOverworldGfxPtr2;
  uint32_t kOverworldGfxPtr3;
  uint32_t kMap32TileTL;
  uint32_t kMap32TileTR;
  uint32_t kMap32TileBL;
  uint32_t kMap32TileBR;
  uint32_t kSpriteBlocksetPointer;
  uint32_t kDungeonPalettesGroups;
};

const static zelda3_version_pointers zelda3_us_pointers = {
    0x10275,  // kGfxAnimatedPointer
    0x5D97,   // kOverworldGfxGroups1
    0x6073,   // kOverworldGfxGroups2
    0x1794D,  // kCompressedAllMap32PointersHigh
    0x17B2D,  // kCompressedAllMap32PointersLow
    0x75504,  // kOverworldMapPaletteGroup
    0x77664,  // kOverlayPointers
    0x0E,     // kOverlayPointersBank
    0x71459,  // kOverworldTilesType
    0x4F80,   // kOverworldGfxPtr1
    0x505F,   // kOverworldGfxPtr2
    0x513E,   // kOverworldGfxPtr3
    0x18000,  // kMap32TileTL
    0x1B400,  // kMap32TileTR
    0x20000,  // kMap32TileBL
    0x23400,  // kMap32TileBR
    0x5B57,   // kSpriteBlocksetPointer
    0x75460,  // kDungeonPalettesGroups
};

const static zelda3_version_pointers zelda3_jp_pointers = {
    0x10624,  // kGfxAnimatedPointer
    0x5DD7,   // kOverworldGfxGroups1
    0x60B3,   // kOverworldGfxGroups2
    0x176B1,  // kCompressedAllMap32PointersHigh
    0x17891,  // kCompressedAllMap32PointersLow
    0x67E74,  // kOverworldMapPaletteGroup
    0x3FAF4,  // kOverlayPointers
    0x07,     // kOverlayPointersBank
    0x7FD94,  // kOverworldTilesType
    0x4FC0,   // kOverworldGfxPtr1
    0x509F,   // kOverworldGfxPtr2
    0x517E,   // kOverworldGfxPtr3
    0x18000,  // kMap32TileTL
    0x1B3C0,  // kMap32TileTR
    0x20000,  // kMap32TileBL
    0x233C0,  // kMap32TileBR
    0x5B97,   // kSpriteBlocksetPointer
    0x67DD0,  // kDungeonPalettesGroups
};

typedef struct zelda3_rom {
  const char* filename;
  const uint8_t* data;
  size_t size;
  void* impl;  // yaze::Rom*
} zelda3_rom;

zelda3_rom* yaze_load_rom(const char* filename);
void yaze_unload_rom(zelda3_rom* rom);
void yaze_save_rom(zelda3_rom* rom, const char* filename);

/**
 * @brief Primitive of an overworld map.
 */
typedef struct zelda3_overworld_map {
  uint8_t id; /**< ID of the overworld map. */
  uint8_t parent_id;
  uint8_t quadrant_id;
  uint8_t world_id;
  uint8_t game_state;
  uint8_t area_graphics;
  uint8_t area_palette;

  uint8_t sprite_graphics[3];
  uint8_t sprite_palette[3];
  uint8_t area_music[4];
  uint8_t static_graphics[16];
} zelda3_overworld_map;

/**
 * @brief Primitive of the overworld.
 */
typedef struct zelda3_overworld {
  void* impl;                  // yaze::Overworld*
  zelda3_overworld_map** maps; /**< Pointer to the overworld maps. */
} zelda3_overworld;

typedef struct dungeon_sprite {
  const char* name;
  uint8_t id;
  uint8_t subtype;
} dungeon_sprite;

typedef enum background2 {
  Off,
  Parallax,
  Dark,
  OnTop,
  Translucent,
  Addition,
  Normal,
  Transparent,
  DarkRoom
} background2;

typedef struct object_door {
  short id;
  uint8_t x;
  uint8_t y;
  uint8_t size;
  uint8_t type;
  uint8_t layer;
} object_door;

typedef struct staircase {
  uint8_t id;
  uint8_t room;
  const char* label;
} staircase;

typedef struct chest {
  uint8_t x;
  uint8_t y;
  uint8_t item;
  bool picker;
  bool big_chest;
} chest;

typedef struct chest_data {
  uint8_t id;
  bool size;
} chest_data;

typedef struct destination {
  uint8_t index;
  uint8_t target;
  uint8_t target_layer;
} destination;

typedef struct zelda3_dungeon_room {
  background2 bg2;
  dungeon_sprite* sprites;
  object_door* doors;
  staircase* staircases;
  chest* chests;
  chest_data* chests_in_room;
  destination pits;
  destination stairs[4];
} zelda3_dungeon_room;

#ifdef __cplusplus
}
#endif

#endif  // ZELDA_H