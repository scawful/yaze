#ifndef YAZE_BASE_SPRITE_H_
#define YAZE_BASE_SPRITE_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

/**
 * @brief Sprite action.
 */
struct z3_sprite_action {
  const char* name; /**< Name of the action. */
  uint8_t id;       /**< ID of the action. */

  const char** instructions; /**< Pointer to the instructions of the action. */
};
typedef struct z3_sprite_action z3_sprite_action;

/**
 * @brief Primitive of a sprite.
 */
struct z3_sprite {
  const char* name; /**< Name of the sprite. */
  uint8_t id;       /**< ID of the sprite. */

  z3_sprite_action* actions; /**< Pointer to the actions of the sprite. */
};
typedef struct z3_sprite z3_sprite;

#ifdef __cplusplus
}
#endif

#endif  // YAZE_BASE_SPRITE_H_