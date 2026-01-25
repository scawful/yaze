#ifndef YAZE_GRAPHICS_H
#define YAZE_GRAPHICS_H

/**
 * @file yaze_graphics.h
 * @brief Bitmap, palette, and tile helpers for the YAZE public API.
 */

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stdint.h>

/**
 * @brief Bitmap data structure
 *
 * Represents a bitmap image with pixel data and metadata.
 */
typedef struct yaze_bitmap {
  int width;      /**< Width in pixels */
  int height;     /**< Height in pixels */
  uint8_t bpp;    /**< Bits per pixel (1, 2, 4, 8) */
  uint8_t* data;  /**< Pixel data (caller owns memory) */
} yaze_bitmap;

/**
 * @brief Load a bitmap from file
 *
 * Loads a bitmap image from the specified file. Supports common
 * image formats and SNES-specific formats.
 *
 * @param filename Path to the image file
 * @return Bitmap structure with loaded data, or empty bitmap on error
 *
 * @note The caller is responsible for freeing the data pointer
 */
yaze_bitmap yaze_load_bitmap(const char* filename);

/**
 * @brief Free bitmap data
 *
 * Releases memory allocated for bitmap pixel data.
 *
 * @param bitmap Pointer to bitmap structure to free
 */
void yaze_free_bitmap(yaze_bitmap* bitmap);

/**
 * @brief Create an empty bitmap
 *
 * Allocates a new bitmap with the specified dimensions.
 *
 * @param width Width in pixels
 * @param height Height in pixels
 * @param bpp Bits per pixel
 * @return Initialized bitmap structure, or empty bitmap on error
 */
yaze_bitmap yaze_create_bitmap(int width, int height, uint8_t bpp);

/**
 * @brief SNES color in 15-bit RGB format (BGR555)
 *
 * Represents a color in the SNES native format. Colors are stored
 * as 8-bit values but only the lower 5 bits are used by the SNES.
 */
typedef struct snes_color {
  uint16_t red;   /**< Red component (0-255, but SNES uses 0-31) */
  uint16_t green; /**< Green component (0-255, but SNES uses 0-31) */
  uint16_t blue;  /**< Blue component (0-255, but SNES uses 0-31) */
} snes_color;

/**
 * @brief Convert RGB888 color to SNES color
 *
 * @param r Red component (0-255)
 * @param g Green component (0-255)
 * @param b Blue component (0-255)
 * @return SNES color structure
 */
snes_color yaze_rgb_to_snes_color(uint8_t r, uint8_t g, uint8_t b);

/**
 * @brief Convert SNES color to RGB888
 *
 * @param color SNES color to convert
 * @param r Pointer to store red component (0-255)
 * @param g Pointer to store green component (0-255)
 * @param b Pointer to store blue component (0-255)
 */
void yaze_snes_color_to_rgb(snes_color color, uint8_t* r, uint8_t* g, uint8_t* b);

/**
 * @brief SNES color palette
 *
 * Represents a color palette used by the SNES. Each palette contains
 * up to 256 colors, though most modes use fewer colors per palette.
 */
typedef struct snes_palette {
  uint16_t id;        /**< Palette ID (0-255) */
  uint16_t size;      /**< Number of colors in palette (1-256) */
  snes_color* colors; /**< Array of colors (caller owns memory) */
} snes_palette;

/**
 * @brief Create an empty palette
 *
 * @param id Palette ID
 * @param size Number of colors to allocate
 * @return Initialized palette structure, or NULL on error
 */
snes_palette* yaze_create_palette(uint16_t id, uint16_t size);

/**
 * @brief Free palette memory
 *
 * @param palette Pointer to palette to free
 */
void yaze_free_palette(snes_palette* palette);

/**
 * @brief 8x8 SNES tile data
 *
 * Represents an 8x8 pixel tile with indexed color data.
 * Each pixel value is an index into a palette.
 */
typedef struct snes_tile8 {
  uint32_t id;         /**< Tile ID for reference */
  uint32_t palette_id; /**< Associated palette ID */
  uint8_t data[64];    /**< 64 pixels in row-major order (y*8+x) */
} snes_tile8;

/**
 * @brief Convert tile data between different bit depths
 *
 * @param tile Source tile data
 * @param from_bpp Source bits per pixel
 * @param to_bpp Target bits per pixel
 * @return Converted tile data
 */
snes_tile8 yaze_convert_tile_bpp(const snes_tile8* tile, uint8_t from_bpp, uint8_t to_bpp);

typedef struct snes_tile_info {
  uint16_t id;
  uint8_t palette;
  bool priority;
  bool vertical_mirror;
  bool horizontal_mirror;
} snes_tile_info;

typedef struct snes_tile16 {
  snes_tile_info tiles[4];
} snes_tile16;

typedef struct snes_tile32 {
  uint16_t t0;
  uint16_t t1;
  uint16_t t2;
  uint16_t t3;
} snes_tile32;

#ifdef __cplusplus
}
#endif

#endif  // YAZE_GRAPHICS_H
