#ifndef YAZE_BASE_SPRITE_H_
#define YAZE_BASE_SPRITE_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

/**
 * @brief Primitive of a sprite.
 */
struct z3_sprite {
  const char* name; /**< Name of the sprite. */
  uint8_t id;       /**< ID of the sprite. */
};
typedef struct z3_sprite z3_sprite;

#ifdef __cplusplus
}
#endif

#endif  // YAZE_BASE_SPRITE_H_