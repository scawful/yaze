#include "extension.h"

#include <dlfcn.h>

#include <iostream>

static void* handle = nullptr;
static yaze_extension* extension = nullptr;

yaze_extension* get_yaze_extension() { return extension; }

void yaze_load_c_extension(const char* extension_path, yaze_editor_context* context) {
  handle = dlopen(extension_path, RTLD_LAZY);
  if (!handle) {
    std::cerr << "Cannot open extension: " << dlerror() << std::endl;
    return;
  }
  dlerror();  // Clear any existing error

  // Load the symbol to retrieve the extension
  auto get_extension = reinterpret_cast<yaze_extension* (*)()>(
      dlsym(handle, "get_yaze_extension"));
  const char* dlsym_error = dlerror();
  if (dlsym_error) {
    std::cerr << "Cannot load symbol 'get_yaze_extension': " << dlsym_error
              << std::endl;
    dlclose(handle);
    return;
  }

  extension = get_extension();
  if (extension && extension->initialize) {
    extension->initialize(context);
  } else {
    std::cerr << "Failed to initialize the extension." << std::endl;
    dlclose(handle);
    handle = nullptr;
    extension = nullptr;
  }
}

void yaze_load_py_extension(const char* script_path) {
  // TODO: Use Boost.Python to load Python extensions
}

void yaze_cleanup_extension() {
  if (extension && extension->cleanup) {
    extension->cleanup();
  }
  if (handle) {
    dlclose(handle);
    handle = nullptr;
    extension = nullptr;
  }
}