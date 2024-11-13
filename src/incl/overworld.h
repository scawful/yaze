#ifndef YAZE_OVERWORLD_H
#define YAZE_OVERWORLD_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

#include "incl/sprite.h"

/**
 * @brief Primitive of an overworld map.
 */
typedef struct z3_overworld_map {
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
} z3_overworld_map;

/**
 * @brief Primitive of the overworld.
 */
typedef struct z3_overworld {
  void *impl; // yaze::app::Overworld*

  uint8_t *tile32_data; /**< Pointer to the 32x32 tile data. */
  uint8_t *tile16_data; /**< Pointer to the 16x16 tile data. */

  z3_sprite **sprites;     /**< Pointer to the sprites per map. */
  z3_overworld_map **maps; /**< Pointer to the overworld maps. */
} z3_overworld;

#ifdef __cplusplus
}
#endif

#endif // YAZE_OVERWORLD_H
