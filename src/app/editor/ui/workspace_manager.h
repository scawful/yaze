#ifndef YAZE_APP_EDITOR_UI_WORKSPACE_MANAGER_H_
#define YAZE_APP_EDITOR_UI_WORKSPACE_MANAGER_H_

#include <deque>
#include <string>

#include "absl/status/status.h"

namespace yaze {
class Rom;

namespace editor {

class EditorSet;
class ToastManager;
class PanelManager;

/**
 * @brief Manages workspace layouts, sessions, and presets
 */
class WorkspaceManager {
 public:
  struct SessionInfo {
    Rom* rom;
    EditorSet* editor_set;
    std::string custom_name;
    std::string filepath;
  };

  explicit WorkspaceManager(ToastManager* toast_manager)
      : toast_manager_(toast_manager) {}

  // Set panel manager for window visibility management
  void set_panel_manager(PanelManager* manager) {
    panel_manager_ = manager;
  }

  // Layout management
  absl::Status SaveWorkspaceLayout(const std::string& name = "");
  absl::Status LoadWorkspaceLayout(const std::string& name = "");
  absl::Status ResetWorkspaceLayout();

  // Preset management
  void SaveWorkspacePreset(const std::string& name);
  void LoadWorkspacePreset(const std::string& name);
  void RefreshPresets();
  void LoadDeveloperLayout();
  void LoadDesignerLayout();
  void LoadModderLayout();

  // Window management
  void ShowAllWindows();
  void HideAllWindows();
  void MaximizeCurrentWindow();
  void RestoreAllWindows();
  void CloseAllFloatingWindows();

  // Window operations for keyboard navigation
  void FocusNextWindow();
  void FocusPreviousWindow();
  void SplitWindowHorizontal();
  void SplitWindowVertical();
  void CloseCurrentWindow();

  // Command execution (for WhichKey integration)
  void ExecuteWorkspaceCommand(const std::string& command_id);

  // Session queries
  size_t GetActiveSessionCount() const;
  bool HasDuplicateSession(const std::string& filepath) const;

  void set_sessions(std::deque<SessionInfo>* sessions) { sessions_ = sessions; }

  const std::vector<std::string>& workspace_presets() const {
    return workspace_presets_;
  }
  bool workspace_presets_loaded() const { return workspace_presets_loaded_; }

 private:
  ToastManager* toast_manager_;
  PanelManager* panel_manager_ = nullptr;
  std::deque<SessionInfo>* sessions_ = nullptr;
  std::string last_workspace_preset_;
  std::vector<std::string> workspace_presets_;
  bool workspace_presets_loaded_ = false;
};

}  // namespace editor
}  // namespace yaze

#endif  // YAZE_APP_EDITOR_UI_WORKSPACE_MANAGER_H_
