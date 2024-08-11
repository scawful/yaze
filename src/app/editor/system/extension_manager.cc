#include "extension_manager.h"

#include <vector>

#include "ext/extension.h"

namespace yaze {
namespace app {
namespace editor {

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
