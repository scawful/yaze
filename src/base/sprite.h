#ifndef YAZE_BASE_SPRITE_H_
#define YAZE_BASE_SPRITE_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

/**
 * @brief Sprite instruction.
 */

struct sprite_instruction {
  const char* content; /**< Content of the instruction. */
};
typedef struct sprite_instruction sprite_instruction;

/**
 * @brief Sprite action.
 */
struct sprite_action {
  const char* name; /**< Name of the action. */
  uint8_t id;       /**< ID of the action. */

  sprite_instruction*
      instructions; /**< Pointer to the instructions of the action. */
};
typedef struct sprite_action sprite_action;

/**
 * @brief Primitive of a sprite.
 */
struct sprite {
  const char* name; /**< Name of the sprite. */
  uint8_t id;       /**< ID of the sprite. */

  sprite_action* actions; /**< Pointer to the actions of the sprite. */
};
typedef struct sprite sprite;

#ifdef __cplusplus
}
#endif

#endif  // YAZE_BASE_SPRITE_H_