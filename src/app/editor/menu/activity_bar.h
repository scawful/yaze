#ifndef YAZE_APP_EDITOR_MENU_ACTIVITY_BAR_H_
#define YAZE_APP_EDITOR_MENU_ACTIVITY_BAR_H_

#include <functional>
#include <string>
#include <unordered_set>
#include <vector>

namespace yaze {
namespace editor {

class PanelManager;

class ActivityBar {
 public:
  explicit ActivityBar(
      PanelManager& panel_manager,
      std::function<bool()> is_dungeon_workbench_mode = {},
      std::function<void(bool)> set_dungeon_workflow_mode = {});

  void Render(size_t session_id, const std::string& active_category,
              const std::vector<std::string>& all_categories,
              const std::unordered_set<std::string>& active_editor_categories,
              std::function<bool()> has_rom);

  void DrawPanelBrowser(size_t session_id, bool* p_open);

 private:
  void DrawUtilityButtons(std::function<bool()> has_rom);
  void DrawActivityBarStrip(
      size_t session_id, const std::string& active_category,
      const std::vector<std::string>& all_categories,
      const std::unordered_set<std::string>& active_editor_categories,
      std::function<bool()> has_rom);
  void DrawSidePanel(size_t session_id, const std::string& category,
                     std::function<bool()> has_rom);

  PanelManager& panel_manager_;
  std::function<bool()> is_dungeon_workbench_mode_;
  std::function<void(bool)> set_dungeon_workflow_mode_;
};

}  // namespace editor
}  // namespace yaze

#endif  // YAZE_APP_EDITOR_MENU_ACTIVITY_BAR_H_
