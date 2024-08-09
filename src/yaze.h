#ifndef YAZE_H
#define YAZE_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stddef.h>
#include <stdint.h>

#include "base/snes_color.h"
#include "base/sprite.h"
#include "base/overworld.h"

typedef struct yaze_flags yaze_flags;
typedef struct yaze_project yaze_project;
typedef struct z3_rom z3_rom;

/**
 * @brief Flags to initialize the Yaze library.
 */
struct yaze_flags {
  int debug;
  const char* rom_filename;
  z3_rom* rom;
};

/**
 * @brief Initialize the Yaze library.
 *
 * @param flags Flags to initialize the library.
 */
void yaze_init(yaze_flags*);

/**
 * @brief Clean up the Yaze library.
 *
 * @param flags Flags used to initialize the library.
 */
void yaze_cleanup(yaze_flags*);

/**
 * @brief Primitive of a Yaze project.
 */
struct yaze_project {
  const char* filename;

  z3_rom* rom;
  z3_overworld* overworld;
};

/**
 * @brief Primitive of a Zelda3 ROM.
 */
struct z3_rom {
  const char* filename;
  const uint8_t* data;
  size_t size;
  void* impl;  // yaze::app::Rom*
};

/**
 * @brief Load a Zelda3 ROM from a file.
 */
z3_rom* yaze_load_rom(const char* filename);

/**
 * @brief Unload a Zelda3 ROM.
 */
void yaze_unload_rom(z3_rom* rom);

/**
 * @brief Get a color from a palette set.
 */
snes_color yaze_get_color_from_paletteset(const z3_rom* rom, int palette_set,
                                          int palette, int color);

z3_overworld* yaze_load_overworld(const z3_rom* rom);

#ifdef __cplusplus
}
#endif

#endif  // YAZE_H
