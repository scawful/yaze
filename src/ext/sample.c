#include "ext/extension.h"
#include "yaze.h"

void my_extension_initialize() {
  // Custom initialization code
}

void my_extension_cleanup() {
  // Custom cleanup code
}

void my_extension_manipulate_rom(z3_rom* rom) {
  // Modify ROM data
}

yaze_extension* get_yaze_extension() {
  static yaze_extension ext = {
      "My Extension",
      "1.0",
      my_extension_initialize,     // Initialization function
      my_extension_cleanup,        // Cleanup function
      NULL,                        // extend_functionality
      NULL,                        // render_ui
      my_extension_manipulate_rom  // manipulate_rom
  };
  return &ext;
}