#ifndef YAZE_BASE_SNES_COLOR_H_
#define YAZE_BASE_SNES_COLOR_H_

#include <cstdint>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Primitive of 16-bit RGB SNES color.
 */
struct snes_color {
  uint16_t red;   /**< Red component of the color. */
  uint16_t blue;  /**< Blue component of the color. */
  uint16_t green; /**< Green component of the color. */
};
typedef struct snes_color snes_color;

#ifdef __cplusplus
}
#endif

#endif  // YAZE_BASE_SNES_COLOR_H_