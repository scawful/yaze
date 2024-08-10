#ifndef EXTENSION_INTERFACE_H
#define EXTENSION_INTERFACE_H

#ifdef __cplusplus
extern "C" {
#endif

#include "yaze.h"

typedef void (*yaze_initialize_func)(void);
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

/**
 * @brief Get the extension interface.
 */
yaze_extension* get_yaze_extension();

/**
 * @brief Load a C extension.
 */
void yaze_load_c_extension(const char* extension_path);

/**
 * @brief Load a Python extension.
 */
void yaze_load_py_extension(const char* script_path);

/**
 * @brief Clean up the extension.
 */
void yaze_cleanup_extension();

#ifdef __cplusplus
}
#endif

#endif  // EXTENSION_INTERFACE_H