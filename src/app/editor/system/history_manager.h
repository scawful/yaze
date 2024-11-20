#ifndef YAZE_APP_EDITOR_SYSTEM_HISTORY_MANAGER_H
#define YAZE_APP_EDITOR_SYSTEM_HISTORY_MANAGER_H

#include <cstddef>
#include <stack>
#include <vector>

namespace yaze {
namespace app {
namespace editor {

// System history manager, undo and redo.
class HistoryManager {
 public:
  HistoryManager() = default;
  ~HistoryManager() = default;

  void Add(const char* data);
  void Undo();
  void Redo();

 private:
  std::vector<const char*> history_;
  std::stack<const char*> undo_;
  std::stack<const char*> redo_;
};

}  // namespace editor
}  // namespace app
}  // namespace yaze

#endif  // YAZE_APP_EDITOR_SYSTEM_HISTORY_MANAGER_H
