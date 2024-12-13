#ifndef YAZE_H
#define YAZE_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stddef.h>
#include <stdint.h>

#include "dungeon.h"
#include "overworld.h"
#include "snes_color.h"
#include "sprite.h"

typedef struct z3_rom z3_rom;

typedef struct yaze_project yaze_project;
typedef struct yaze_command_registry yaze_command_registry;
typedef struct yaze_event_dispatcher yaze_event_dispatcher;

/**
 * @brief Extension editor context.
 */
typedef struct yaze_editor_context {
  z3_rom* rom;
  yaze_project* project;

  yaze_command_registry* command_registry;
  yaze_event_dispatcher* event_dispatcher;
} yaze_editor_context;

/**
 * @brief Initialize the Yaze library.
 */
int yaze_init(yaze_editor_context*);

/**
 * @brief Clean up the Yaze library.
 */
void yaze_cleanup(yaze_editor_context*);

/**
 * @brief Primitive of a Yaze project.
 */
struct yaze_project {
  const char* name;
  const char* filepath;
  const char* rom_filename;
  const char* code_folder;
  const char* labels_filename;
};

yaze_project yaze_load_project(const char* filename);

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
 * @brief Primitive of a Bitmap
 */
typedef struct yaze_bitmap {
  int width;
  int height;
  uint8_t bpp;
  uint8_t* data;
} yaze_bitmap;

/**
 * @brief Load a bitmap from a file.
 */
yaze_bitmap yaze_load_bitmap(const char* filename);

/**
 * @brief Get a color from a palette set.
 */
snes_color yaze_get_color_from_paletteset(const z3_rom* rom, int palette_set,
                                          int palette, int color);

/**
 * @brief Load the overworld from a Zelda3 ROM.
 */
z3_overworld* yaze_load_overworld(const z3_rom* rom);

/**
 * @brief Check the version of the Yaze library.
 */
void yaze_check_version(const char* version);

/**
 * @brief Command registry.
 */
struct yaze_command_registry {
  void (*register_command)(const char* name, void (*command)(void));
};

/**
 * @brief Event dispatcher.
 */
struct yaze_event_dispatcher {
  void (*register_event_hook)(void (*event_hook)(void));
};

#ifdef __cplusplus
}
#endif

#endif  // YAZE_H
