#ifndef YAZE_OVERWORLD_H
#define YAZE_OVERWORLD_H

/**
 * @file yaze_overworld.h
 * @brief Overworld data helpers for the YAZE public API.
 */

#ifdef __cplusplus
extern "C" {
#endif

#include "zelda.h"

/**
 * @brief Load the overworld from ROM
 *
 * Loads and parses the overworld data from the ROM, including all maps,
 * sprites, and related data structures.
 *
 * @param rom The ROM to load the overworld from
 * @return Pointer to overworld structure, or NULL on error
 *
 * @note Caller must free the returned pointer when done
 */
zelda3_overworld* yaze_load_overworld(const zelda3_rom* rom);

/**
 * @brief Free overworld data
 *
 * @param overworld Pointer to overworld to free
 */
void yaze_free_overworld(zelda3_overworld* overworld);

/**
 * @brief Get overworld map by index
 *
 * @param overworld Overworld data
 * @param map_index Map index (0-159 for most ROMs)
 * @return Pointer to map data, or NULL if invalid index
 */
const zelda3_overworld_map* yaze_get_overworld_map(const zelda3_overworld* overworld, int map_index);

/**
 * @brief Get total number of overworld maps
 *
 * @param overworld Overworld data
 * @return Number of maps available
 */
int yaze_get_overworld_map_count(const zelda3_overworld* overworld);

#ifdef __cplusplus
}
#endif

#endif  // YAZE_OVERWORLD_H
