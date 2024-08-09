#ifndef EXTENSION_INTERFACE_H
#define EXTENSION_INTERFACE_H

#ifdef __cplusplus
extern "C" {
#endif

typedef void (*yaze_imgui_render_callback)(void* editor_context);

typedef void (*yaze_rom_operation)(z3_rom* rom);

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

yaze_extension* get_yaze_extension();

void yaze_load_c_extension(const char* extension_path);

void yaze_load_py_extension(const char* script_path);

#ifdef __cplusplus
}
#endif

#endif  // EXTENSION_INTERFACE_H