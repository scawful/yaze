#ifndef YAZE_APP_EDITOR_MENU_WINDOW_SIDEBAR_H_
#define YAZE_APP_EDITOR_MENU_WINDOW_SIDEBAR_H_

#include <functional>
#include <string>

#include "app/editor/editor_context_snapshot.h"

namespace yaze {
namespace editor {

class WorkspaceWindowManager;
class UserSettings;
struct WindowDescriptor;

class WindowSidebar {
 public:
  explicit WindowSidebar(
      WorkspaceWindowManager& window_manager,
      std::function<bool()> is_dungeon_workbench_mode = {},
      std::function<void(bool)> set_dungeon_workflow_mode = {},
      std::function<float()> get_bottom_reserved_height = {},
      std::function<EditorContextSnapshot(const std::string&)>
          context_provider = {});

  void SetUserSettings(UserSettings* settings) { user_settings_ = settings; }

  static bool MatchesWindowSearch(const std::string& query,
                                  const std::string& display_name,
                                  const std::string& window_id,
                                  const std::string& shortcut_hint);
  static bool IsDungeonWindowModeTarget(const std::string& window_id);

  // Maps WindowDescriptor::workflow_group to a sidebar section label.
  // Empty / "Windows" → "Editors". Known groups pass through unchanged.
  static std::string SidebarSectionFor(const std::string& workflow_group);
  static std::string SidebarSectionFor(const WindowDescriptor& window);

  // Workbench mode hides Room List / Matrix / per-room windows from the list.
  static bool ShouldOmitWindowInSidebar(const std::string& window_id,
                                        bool dungeon_workbench_mode);
  static bool HasRenderableContext(const EditorContextSnapshot& snapshot);

  void Draw(size_t session_id, const std::string& category,
            std::function<bool()> has_rom);

 private:
  friend class WindowSidebarTestPeer;

  void DrawEditorContext(size_t session_id, const std::string& category,
                         const EditorContextSnapshot& snapshot);
  void DispatchContextAction(size_t session_id,
                             const EditorContextAction& action);

  WorkspaceWindowManager& window_manager_;
  std::function<bool()> is_dungeon_workbench_mode_;
  std::function<void(bool)> set_dungeon_workflow_mode_;
  std::function<float()> get_bottom_reserved_height_;
  std::function<EditorContextSnapshot(const std::string&)> context_provider_;
  UserSettings* user_settings_ = nullptr;
  char sidebar_search_[256] = {};
};

}  // namespace editor
}  // namespace yaze

#endif  // YAZE_APP_EDITOR_MENU_WINDOW_SIDEBAR_H_
