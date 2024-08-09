#ifndef YAZE_OVERWORLD_H
#define YAZE_OVERWORLD_H

#ifdef __cplusplus
extern "C" {
#endif

typedef struct z3_overworld z3_overworld;
typedef struct z3_overworld_map z3_overworld_map;

/**
 * @brief Primitive of an overworld map.
 */
struct z3_overworld_map {
  uint8_t id;           /**< ID of the overworld map. */
  uint8_t* tile32_data; /**< Pointer to the 32x32 tile data. */
  uint8_t* tile16_data; /**< Pointer to the 16x16 tile data. */
};

/**
 * @brief Primitive of the overworld.
 */
struct z3_overworld {
  z3_overworld_map* maps; /**< Pointer to the overworld maps. */
  void* impl;             // yaze::app::Overworld*
};

#ifdef __cplusplus
}
#endif

#endif  // YAZE_OVERWORLD_H