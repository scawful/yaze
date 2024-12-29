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

typedef struct yaze_editor_context {
  z3_rom* rom;
  yaze_project* project;

  yaze_command_registry* command_registry;
  yaze_event_dispatcher* event_dispatcher;
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

struct yaze_command_registry {
  void (*register_command)(const char* name, void (*command)(void));
};

struct yaze_event_dispatcher {
  void (*register_event_hook)(void (*event_hook)(void));
};

typedef void (*yaze_initialize_func)(yaze_editor_context* context);
typedef void (*yaze_cleanup_func)(void);
typedef void (*yaze_extend_ui_func)(yaze_editor_context* context);
typedef void (*yaze_manipulate_rom_func)(z3_rom* rom);
typedef void (*yaze_command_func)(void);
typedef void (*yaze_event_hook_func)(void);

typedef enum {
  YAZE_EVENT_ROM_LOADED,
  YAZE_EVENT_ROM_SAVED,
  YAZE_EVENT_SPRITE_MODIFIED,
  YAZE_EVENT_PALETTE_CHANGED,
} yaze_event_type;

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

  /**
   * @brief Function to manipulate the ROM.
   *
   * @param rom The ROM to manipulate.
   *
   */
  yaze_manipulate_rom_func manipulate_rom;

  /**
   * @brief Function to extend the UI.
   *
   * @param context The editor context.
   *
   * @details This function is called when the extension is loaded. It can be
   * used to add custom UI elements to the editor. The context parameter
   * provides access to the project, command registry, event dispatcher, and
   * ImGui context.
   */
  yaze_extend_ui_func extend_ui;

  /**
   * @brief Register commands in the yaze_command_registry.
   */
  yaze_command_func register_commands;

  /**
   * @brief Register custom tools in the yaze_command_registry.
   */
  yaze_command_func register_custom_tools;

  /**
   * @brief Register event hooks in the yaze_event_dispatcher.
   */
  void (*register_event_hooks)(yaze_event_type event,
                               yaze_event_hook_func hook);

} yaze_extension;

#ifdef __cplusplus
}
#endif

#endif  // YAZE_H
