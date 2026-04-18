#ifndef YAZE_APP_EDITOR_MENU_ACTIVITY_BAR_H_
#define YAZE_APP_EDITOR_MENU_ACTIVITY_BAR_H_

#include <functional>
#include <memory>
#include <string>
#include <unordered_set>
#include <vector>

#include "app/editor/menu/activity_bar_actions_registry.h"
#include "app/editor/menu/window_browser.h"
#include "app/editor/menu/window_sidebar.h"

namespace yaze {
namespace editor {

class WorkspaceWindowManager;
class UserSettings;

class ActivityBar {
 public:
  explicit ActivityBar(
      WorkspaceWindowManager& window_manager,
      std::function<bool()> is_dungeon_workbench_mode = {},
      std::function<void(bool)> set_dungeon_workflow_mode = {});
  ~ActivityBar();

  // Optional: when set, the rail reads per-user pin/hide/order prefs and
  // mutates them in response to right-click menus and drag-drop. Safe to leave
  // null — the rail falls back to the incoming `all_categories` order.
  void SetUserSettings(UserSettings* settings) { user_settings_ = settings; }

  // Extensible registry powering the "More Actions" popup at the bottom of
  // the rail. Populated by EditorManager at construction time; callers may
  // Register/Unregister further entries at runtime.
  MoreActionsRegistry& actions_registry() { return *actions_registry_; }

  // `is_rom_dirty` is optional — when supplied and true, the icon for the
  // selected+open editor gets a small dot badge in the top-right corner.
  void Render(size_t session_id, const std::string& active_category,
              const std::vector<std::string>& all_categories,
              const std::unordered_set<std::string>& active_editor_categories,
              std::function<bool()> has_rom,
              std::function<bool()> is_rom_dirty = {});

  void DrawWindowBrowser(size_t session_id, bool* p_open);

  // Pure sort helper. Exposed for unit tests.
  //   - pinned set (intersected with visible) renders first, in `input` order
  //   - entries present in `order` render next, respecting `order`
  //   - remaining newcomers render alphabetically
  //   - entries in `hidden` are filtered out entirely
  static std::vector<std::string> SortCategories(
      const std::vector<std::string>& input,
      const std::vector<std::string>& order,
      const std::unordered_set<std::string>& pinned,
      const std::unordered_set<std::string>& hidden);

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

  // Right-click context menu on a sidebar icon. Mutates user_settings_ if set.
  void DrawCategoryContextMenu(const std::string& category);
  // Wraps an icon's ItemRect as both a drag source and drop target for the
  // reorder payload ("YAZE_SIDEBAR_CAT"). No-op when user_settings_ is null.
  void HandleReorderDragAndDrop(const std::string& category);

  WorkspaceWindowManager& window_manager_;
  WindowBrowser window_browser_;
  WindowSidebar window_sidebar_;
  UserSettings* user_settings_ = nullptr;
  std::unique_ptr<MoreActionsRegistry> actions_registry_;
};

}  // namespace editor
}  // namespace yaze

#endif  // YAZE_APP_EDITOR_MENU_ACTIVITY_BAR_H_
