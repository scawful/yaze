#ifndef YAZE_ROM_H
#define YAZE_ROM_H

/**
 * @file yaze_rom.h
 * @brief ROM loading and ROM-backed helpers for the YAZE public API.
 */

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

#include "yaze_errors.h"
#include "yaze_graphics.h"
#include "zelda.h"

/**
 * @brief Load a ROM file
 *
 * Loads a Zelda 3 ROM file and validates its format.
 *
 * @param filename Path to ROM file (.sfc, .smc, etc.)
 * @return Pointer to ROM structure, or NULL on error
 *
 * @note Caller must call yaze_unload_rom() to free memory
 */
zelda3_rom* yaze_load_rom_file(const char* filename);

/**
 * @brief Validate ROM integrity
 *
 * Checks if the ROM data is valid and uncorrupted.
 *
 * @param rom ROM to validate
 * @return YAZE_OK if valid, error code if corrupted
 */
yaze_status yaze_validate_rom(const zelda3_rom* rom);

/**
 * @brief Get ROM information
 *
 * @param rom ROM to query
 * @param version Pointer to store detected ROM version
 * @param size Pointer to store ROM size in bytes
 * @return YAZE_OK on success, error code on failure
 */
yaze_status yaze_get_rom_info(const zelda3_rom* rom, zelda3_version* version, uint64_t* size);

/**
 * @brief Load palette from ROM
 *
 * @param rom ROM to load palette from
 * @param palette_id ID of palette to load
 * @return Loaded palette, or NULL on error
 */
snes_palette* yaze_load_palette_from_rom(const zelda3_rom* rom, uint16_t palette_id);

/**
 * @brief Load tile data from ROM
 *
 * @param rom ROM to load from
 * @param tile_id ID of tile to load
 * @param bpp Bits per pixel (1, 2, 4, 8)
 * @return Loaded tile data, or empty tile on error
 */
snes_tile8 yaze_load_tile_from_rom(const zelda3_rom* rom, uint32_t tile_id, uint8_t bpp);

/**
 * @brief Get a color from a palette set
 *
 * Retrieves a specific color from a palette set in the ROM.
 *
 * @param rom The ROM to get the color from
 * @param palette_set The palette set index (0-255)
 * @param palette The palette index within the set (0-15)
 * @param color The color index within the palette (0-15)
 * @return The color from the palette set
 */
snes_color yaze_get_color_from_paletteset(const zelda3_rom* rom,
                                          int palette_set, int palette,
                                          int color);

#ifdef __cplusplus
}
#endif

#endif  // YAZE_ROM_H
