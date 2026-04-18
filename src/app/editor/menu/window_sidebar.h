#ifndef YAZE_APP_EDITOR_MENU_WINDOW_SIDEBAR_H_
#define YAZE_APP_EDITOR_MENU_WINDOW_SIDEBAR_H_

#include <functional>
#include <string>

namespace yaze {
namespace editor {

class WorkspaceWindowManager;

class WindowSidebar {
 public:
  explicit WindowSidebar(
      WorkspaceWindowManager& window_manager,
      std::function<bool()> is_dungeon_workbench_mode = {},
      std::function<void(bool)> set_dungeon_workflow_mode = {});

  static bool MatchesWindowSearch(const std::string& query,
                                  const std::string& display_name,
                                  const std::string& window_id,
                                  const std::string& shortcut_hint);
  static bool IsDungeonWindowModeTarget(const std::string& window_id);

  void Draw(size_t session_id, const std::string& category,
            std::function<bool()> has_rom);

 private:
  WorkspaceWindowManager& window_manager_;
  std::function<bool()> is_dungeon_workbench_mode_;
  std::function<void(bool)> set_dungeon_workflow_mode_;
  char sidebar_search_[256] = {};
};

}  // namespace editor
}  // namespace yaze

#endif  // YAZE_APP_EDITOR_MENU_WINDOW_SIDEBAR_H_
