#ifndef YAZE_H
#define YAZE_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stddef.h>
#include <stdint.h>

#include "dungeon.h"
#include "overworld.h"
#include "snes.h"
#include "zelda.h"

typedef struct yaze_project yaze_project;

typedef struct yaze_editor_context {
  zelda3_rom* rom;
  yaze_project* project;
  const char* error_message;
} yaze_editor_context;

typedef enum yaze_status {
  YAZE_UNKNOWN = -1,
  YAZE_OK = 0,
  YAZE_ERROR = 1,
} yaze_status;

void yaze_check_version(const char* version);

yaze_status yaze_init(yaze_editor_context*);
yaze_status yaze_shutdown(yaze_editor_context*);

struct yaze_project {
  const char* name;
  const char* filepath;
  const char* rom_filename;
  const char* code_folder;
  const char* labels_filename;
};

yaze_project yaze_load_project(const char* filename);

typedef struct yaze_bitmap {
  int width;
  int height;
  uint8_t bpp;
  uint8_t* data;
} yaze_bitmap;

yaze_bitmap yaze_load_bitmap(const char* filename);

snes_color yaze_get_color_from_paletteset(const zelda3_rom* rom,
                                          int palette_set, int palette,
                                          int color);

zelda3_overworld* yaze_load_overworld(const zelda3_rom* rom);

zelda3_dungeon_room* yaze_load_all_rooms(const zelda3_rom* rom);

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
