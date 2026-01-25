#ifndef YAZE_DUNGEON_H
#define YAZE_DUNGEON_H

/**
 * @file yaze_dungeon.h
 * @brief Dungeon room helpers for the YAZE public API.
 */

#ifdef __cplusplus
extern "C" {
#endif

#include "zelda.h"

/**
 * @brief Load all dungeon rooms from ROM
 *
 * Loads and parses all dungeon room data from the ROM.
 *
 * @param rom The ROM to load rooms from
 * @param room_count Pointer to store the number of rooms loaded
 * @return Array of room structures, or NULL on error
 *
 * @note Caller must free the returned array when done
 */
zelda3_dungeon_room* yaze_load_all_rooms(const zelda3_rom* rom, int* room_count);

/**
 * @brief Load a specific dungeon room
 *
 * @param rom ROM to load from
 * @param room_id Room ID to load (0-295 for most ROMs)
 * @return Pointer to room data, or NULL on error
 */
const zelda3_dungeon_room* yaze_load_room(const zelda3_rom* rom, int room_id);

/**
 * @brief Free dungeon room data
 *
 * @param rooms Array of rooms to free
 * @param room_count Number of rooms in array
 */
void yaze_free_rooms(zelda3_dungeon_room* rooms, int room_count);

#ifdef __cplusplus
}
#endif

#endif  // YAZE_DUNGEON_H
