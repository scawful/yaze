#ifndef YAZE_APP_EDITOR_SYSTEM_EXTENSION_MANAGER_H
#define YAZE_APP_EDITOR_SYSTEM_EXTENSION_MANAGER_H

#include <vector>

#include "ext/extension.h"

namespace yaze {
namespace app {
namespace editor {

class ExtensionManager {
 public:
  void RegisterExtension(yaze_extension* extension);
  void InitializeExtensions(yaze_editor_context* context);
  void ShutdownExtensions();
  void ExecuteExtensionUI();

 private:
  std::vector<yaze_extension*> extensions_;
};

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
}

void ExtensionManager::ExecuteExtensionUI() {
  for (auto& extension : extensions_) {
    if (extension->extend_ui) {
      extension->extend_ui();
    }
  }
}

}  // namespace editor
}  // namespace app
}  // namespace yaze