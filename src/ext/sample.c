#include <stdio.h>

#include "ext/extension.h"
#include "yaze.h"

void my_extension_initialize(yaze_editor_context* context) {
  printf("My Extension Initialized\n");
}

void my_extension_cleanup(void) { printf("My Extension Cleaned Up\n"); }

void my_extension_extend_ui(yaze_editor_context* context) {
  // Add a custom tab or panel to the editor
}

void my_extension_manipulate_rom(z3_rom* rom) {
  // Modify ROM data
}

void my_extension_register_commands(void) {
  // Register custom commands
}

void my_extension_handle_file_format(void) {
  // Handle custom file formats
}

void my_extension_register_event_hooks(yaze_event_type event,
                                       yaze_event_hook_func hook) {
  // Register event hooks for specific events
}

void my_extension_register_custom_tools(void) {
  // Register custom tools or editors
}

yaze_extension* get_yaze_extension(void) {
  static yaze_extension ext = {"My Extension",
                               "1.0",
                               my_extension_initialize,
                               my_extension_cleanup,
                               my_extension_manipulate_rom,
                               my_extension_extend_ui,
                               my_extension_register_commands,
                               my_extension_register_custom_tools,
                               my_extension_register_event_hooks};
  return &ext;
}