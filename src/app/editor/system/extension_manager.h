#ifndef YAZE_APP_EDITOR_SYSTEM_EXTENSION_MANAGER_H
#define YAZE_APP_EDITOR_SYSTEM_EXTENSION_MANAGER_H

#include <yaze.h>

#include <string>
#include <vector>

namespace yaze {
namespace editor {

class ExtensionManager {
 public:
  void LoadExtension(const std::string& filename, yaze_editor_context* context);
  void RegisterExtension(yaze_extension* extension);
  void InitializeExtensions(yaze_editor_context* context);
  void ShutdownExtensions();
  void ExecuteExtensionUI(yaze_editor_context* context);

 private:
  std::vector<yaze_extension*> extensions_;
};

}  // namespace editor
}  // namespace yaze

#endif  // YAZE_APP_EDITOR_SYSTEM_EXTENSION_MANAGER_H
