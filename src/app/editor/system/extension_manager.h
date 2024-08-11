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
  void ExecuteExtensionUI(yaze_editor_context* context);

 private:
  std::vector<yaze_extension*> extensions_;
};

}  // namespace editor
}  // namespace app
}  // namespace yaze

#endif  // YAZE_APP_EDITOR_SYSTEM_EXTENSION_MANAGER_H