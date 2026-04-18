#ifndef YAZE_APP_EDITOR_MENU_ACTIVITY_BAR_H_
#define YAZE_APP_EDITOR_MENU_ACTIVITY_BAR_H_

#include <functional>
#include <string>
#include <unordered_set>
#include <vector>

#include "app/editor/menu/window_browser.h"
#include "app/editor/menu/window_sidebar.h"

namespace yaze {
namespace editor {

class WorkspaceWindowManager;

class ActivityBar {
 public:
  explicit ActivityBar(
      WorkspaceWindowManager& window_manager,
      std::function<bool()> is_dungeon_workbench_mode = {},
      std::function<void(bool)> set_dungeon_workflow_mode = {});

  // `is_rom_dirty` is optional — when supplied and true, the icon for the
  // selected+open editor gets a small dot badge in the top-right corner.
  void Render(size_t session_id, const std::string& active_category,
              const std::vector<std::string>& all_categories,
              const std::unordered_set<std::string>& active_editor_categories,
              std::function<bool()> has_rom,
              std::function<bool()> is_rom_dirty = {});

  void DrawWindowBrowser(size_t session_id, bool* p_open);

 private:
  void DrawUtilityButtons(std::function<bool()> has_rom);
  void DrawActivityBarStrip(
      size_t session_id, const std::string& active_category,
      const std::vector<std::string>& all_categories,
      const std::unordered_set<std::string>& active_editor_categories,
      std::function<bool()> has_rom,
      std::function<bool()> is_rom_dirty);
  void DrawSidePanel(size_t session_id, const std::string& category,
                     std::function<bool()> has_rom);

  WorkspaceWindowManager& window_manager_;
  WindowBrowser window_browser_;
  WindowSidebar window_sidebar_;
};

}  // namespace editor
}  // namespace yaze

#endif  // YAZE_APP_EDITOR_MENU_ACTIVITY_BAR_H_
