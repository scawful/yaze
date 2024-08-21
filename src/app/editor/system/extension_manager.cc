#include "extension_manager.h"

#include <dlfcn.h>

#include <iostream>
#include <vector>

#include "base/extension.h"

namespace yaze {
namespace app {
namespace editor {

void ExtensionManager::LoadExtension(const std::string& filename,
                                     yaze_editor_context* context) {
  auto extension_path = filename.c_str();
  void* handle = dlopen(extension_path, RTLD_LAZY);
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

  yaze_extension* extension = get_extension();
  if (extension && extension->initialize) {
    extension->initialize(context);
  } else {
    std::cerr << "Failed to initialize the extension." << std::endl;
    dlclose(handle);
    return;
  }

  extensions_.push_back(extension);
}

void ExtensionManager::RegisterExtension(yaze_extension* extension) {
  extensions_.push_back(extension);
}

void ExtensionManager::InitializeExtensions(yaze_editor_context* context) {
  for (auto& extension : extensions_) {
    extension->initialize(context);
  }
}

void ExtensionManager::ShutdownExtensions() {
  for (auto& extension : extensions_) {
    extension->cleanup();
  }

  // if (handle) {
  //   dlclose(handle);
  //   handle = nullptr;
  //   extension = nullptr;
  // }
}

void ExtensionManager::ExecuteExtensionUI(yaze_editor_context* context) {
  for (auto& extension : extensions_) {
    if (extension->extend_ui) {
      extension->extend_ui(context);
    }
  }
}

}  // namespace editor
}  // namespace app
}  // namespace yaze
