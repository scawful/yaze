#ifndef YAZE_APP_EDITOR_UI_DASHBOARD_PANEL_H_
#define YAZE_APP_EDITOR_UI_DASHBOARD_PANEL_H_

#include <functional>
#include <string>
#include <vector>

#include "app/editor/editor.h"
#include "app/gui/app/editor_layout.h"
#include "imgui/imgui.h"

namespace yaze {
namespace editor {

class EditorManager;

class DashboardPanel {
 public:
  explicit DashboardPanel(EditorManager* editor_manager);
  ~DashboardPanel() = default;

  void Draw();

  void Show() { show_ = true; }
  void Hide() { show_ = false; }
  bool IsVisible() const { return show_; }
  bool* visibility_flag() { return &show_; }

  void MarkRecentlyUsed(EditorType type);
  void LoadRecentEditors();
  void SaveRecentEditors();
  void ClearRecentEditors();

 private:
  struct EditorInfo {
    std::string name;
    std::string icon;
    std::string description;
    std::string shortcut;
    EditorType type;
    bool recently_used = false;
    bool requires_rom = true;
  };

  void DrawWelcomeHeader();
  void DrawRecentEditors();
  void DrawEditorGrid();
  void DrawEditorPanel(const EditorInfo& info, int index,
                       const ImVec2& card_size, bool enabled);

  EditorManager* editor_manager_;
  gui::PanelWindow window_;
  bool show_ = true;
  bool has_rom_ = false;

  std::vector<EditorInfo> editors_;
  std::vector<EditorType> recent_editors_;
  static constexpr size_t kMaxRecentEditors = 5;
};

}  // namespace editor
}  // namespace yaze

#endif  // YAZE_APP_EDITOR_UI_DASHBOARD_PANEL_H_
