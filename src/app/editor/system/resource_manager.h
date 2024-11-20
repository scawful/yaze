#ifndef YAZE_APP_EDITOR_SYSTEM_RESOURCE_MANAGER_H
#define YAZE_APP_EDITOR_SYSTEM_RESOURCE_MANAGER_H

#include <cstddef>

namespace yaze {
namespace app {
namespace editor {

// System resource manager.
class ResourceManager {
 public:
  ResourceManager();
  ~ResourceManager();

  void Load(const char* path);
  void Unload(const char* path);
  void UnloadAll();

  size_t Count() const;

 private:
  size_t count_;
};

} // namespace editor
} // namespace app
} // namespace yaze

#endif // YAZE_APP_EDITOR_SYSTEM_RESOURCE_MANAGER_H
