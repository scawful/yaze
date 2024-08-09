#ifndef EXTENSION_INTERFACE_H
#define EXTENSION_INTERFACE_H

#ifdef __cplusplus
extern "C" {
#endif

#include "yaze.h"

typedef void (*yaze_imgui_render_callback)(void* editor_context);

typedef void (*yaze_rom_operation)(z3_rom* rom);

/**
 * @brief Extension interface for Yaze.
 *
 * @details Yaze extensions can be written in C or Python.
 */
typedef struct yaze_extension {
  const char* name;
  const char* version;

  // Initialization function
  void (*initialize)(void);

  // Cleanup function
  void (*cleanup)(void);

  // Function to extend editor functionality
  void (*extend_functionality)(void* editor_context);

  // ImGui rendering callback
  yaze_imgui_render_callback render_ui;

  // ROM manipulation callback
  yaze_rom_operation manipulate_rom;

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