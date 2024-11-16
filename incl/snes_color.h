#ifndef YAZE_BASE_SNES_COLOR_H_
#define YAZE_BASE_SNES_COLOR_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

/**
 * @brief Primitive of 16-bit RGB SNES color.
 */
typedef struct snes_color {
  uint16_t red;   /**< Red component of the color. */
  uint16_t blue;  /**< Blue component of the color. */
  uint16_t green; /**< Green component of the color. */
} snes_color;

/**
 * @brief Primitive of a SNES color palette.
 */
typedef struct snes_palette {
  unsigned int id;    /**< ID of the palette. */
  unsigned int size;  /**< Size of the palette. */
  snes_color* colors; /**< Pointer to the colors in the palette. */
} snes_palette;

#ifdef __cplusplus
}
#endif

#endif  // YAZE_BASE_SNES_COLOR_H_
