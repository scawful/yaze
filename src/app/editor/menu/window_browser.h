#ifndef YAZE_APP_EDITOR_MENU_WINDOW_BROWSER_H_
#define YAZE_APP_EDITOR_MENU_WINDOW_BROWSER_H_

#include <cstddef>
#include <functional>
#include <string>

namespace yaze {
namespace editor {

class WorkspaceWindowManager;

class WindowBrowser {
 public:
  explicit WindowBrowser(WorkspaceWindowManager& window_manager,
                         std::function<bool()> is_dungeon_workbench_mode = {});

  void Draw(size_t session_id, bool* p_open);

 private:
  WorkspaceWindowManager& window_manager_;
  std::function<bool()> is_dungeon_workbench_mode_;
  char search_filter_[256] = {};
  std::string category_filter_ = "All";
};

}  // namespace editor
}  // namespace yaze

#endif  // YAZE_APP_EDITOR_MENU_WINDOW_BROWSER_H_
