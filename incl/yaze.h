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

typedef struct z3_rom z3_rom;
typedef struct yaze_project yaze_project;

typedef struct yaze_editor_context {
  z3_rom* rom;
  yaze_project* project;
} yaze_editor_context;

void yaze_check_version(const char* version);
int yaze_init(yaze_editor_context*);
void yaze_cleanup(yaze_editor_context*);

struct yaze_project {
  const char* name;
  const char* filepath;
  const char* rom_filename;
  const char* code_folder;
  const char* labels_filename;
};

yaze_project yaze_load_project(const char* filename);

struct z3_rom {
  const char* filename;
  const uint8_t* data;
  size_t size;
  void* impl;  // yaze::Rom*
};

z3_rom* yaze_load_rom(const char* filename);
void yaze_unload_rom(z3_rom* rom);

typedef struct yaze_bitmap {
  int width;
  int height;
  uint8_t bpp;
  uint8_t* data;
} yaze_bitmap;

yaze_bitmap yaze_load_bitmap(const char* filename);

snes_color yaze_get_color_from_paletteset(const z3_rom* rom, int palette_set,
                                          int palette, int color);

z3_overworld* yaze_load_overworld(const z3_rom* rom);

z3_dungeon_room* yaze_load_all_rooms(const z3_rom* rom);

typedef void (*yaze_initialize_func)(yaze_editor_context* context);
typedef void (*yaze_cleanup_func)(void);

/**
 * @brief Extension interface for Yaze.
 *
 * @details Yaze extensions can be written in C or Python.
 */
typedef struct yaze_extension {
  const char* name;
  const char* version;

  /**
   * @brief Function to initialize the extension.
   *
   * @details This function is called when the extension is loaded. It can be
   * used to set up any resources or state needed by the extension.
   */
  yaze_initialize_func initialize;

  /**
   * @brief Function to clean up the extension.
   *
   * @details This function is called when the extension is unloaded. It can be
   * used to clean up any resources or state used by the extension.
   */
  yaze_cleanup_func cleanup;
} yaze_extension;

#ifdef __cplusplus
}
#endif

#endif  // YAZE_H
