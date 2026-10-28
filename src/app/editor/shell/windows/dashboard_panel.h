#ifndef YAZE_APP_EDITOR_SHELL_WINDOWS_DASHBOARD_PANEL_H_
#define YAZE_APP_EDITOR_SHELL_WINDOWS_DASHBOARD_PANEL_H_

#include <functional>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "absl/status/status.h"
#include "app/editor/editor.h"
#include "app/gui/app/editor_layout.h"
#include "imgui/imgui.h"

namespace yaze {
namespace editor {

class IEditorSwitcher;
class ShortcutManager;

class DashboardPanel {
 public:
  explicit DashboardPanel(IEditorSwitcher* editor_switcher);
  ~DashboardPanel() = default;

  void Draw();

  void Show() { show_ = true; }
  void Hide() { show_ = false; }
  bool IsVisible() const { return show_; }
  bool* visibility_flag() { return &show_; }

  using RoleSelectionCallback =
      std::function<absl::Status(EditorType, const std::string&)>;
  void SetRoleSelectionCallback(RoleSelectionCallback callback) {
    role_selection_callback_ = std::move(callback);
  }
  void SetShortcutManager(ShortcutManager* shortcuts) {
    shortcut_manager_ = shortcuts;
  }

  void MarkRecentlyUsed(EditorType type);
  std::optional<EditorType> MostRecentEditor() const {
    if (recent_editors_.empty()) {
      return std::nullopt;
    }
    return recent_editors_.front();
  }
  // Parses the recent-editors file body. Pure and static so the validation
  // rules are testable without touching the user's real config directory,
  // which LoadFileFromConfigDir/SaveFile resolve through PlatformPaths.
  static std::vector<EditorType> ParseRecentEditors(const std::string& data,
                                                    size_t max_entries);
  void LoadRecentEditors();
  void SaveRecentEditors();
  void ClearRecentEditors();

 private:
  friend class DashboardPanelTestPeer;
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
  void DrawRoleChoices();
  void DrawCommandPaletteHint();
  void DrawRecentEditors();
  void DrawEditorGrid();
  void DrawEditorPanel(const EditorInfo& info, int index,
                       const ImVec2& card_size, bool enabled);

  IEditorSwitcher* editor_switcher_;
  ShortcutManager* shortcut_manager_ = nullptr;
  RoleSelectionCallback role_selection_callback_;
  std::string role_selection_error_;
  gui::PanelWindow window_;
  bool show_ = true;
  bool has_rom_ = false;

  std::vector<EditorInfo> editors_;
  std::vector<EditorType> recent_editors_;
  static constexpr size_t kMaxRecentEditors = 5;
};

}  // namespace editor
}  // namespace yaze

#endif  // YAZE_APP_EDITOR_SHELL_WINDOWS_DASHBOARD_PANEL_H_
