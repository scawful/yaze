#ifndef YAZE_OVERWORLD_H
#define YAZE_OVERWORLD_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

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
  void *impl;                  // yaze::Overworld*
  zelda3_overworld_map **maps; /**< Pointer to the overworld maps. */
} zelda3_overworld;

#ifdef __cplusplus
}
#endif

#endif  // YAZE_OVERWORLD_H
