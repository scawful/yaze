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
  
  // Layout management
  absl::Status SaveWorkspaceLayout(const std::string& name = "");
  absl::Status LoadWorkspaceLayout(const std::string& name = "");
  absl::Status ResetWorkspaceLayout();
  
  // Preset management
  void SaveWorkspacePreset(const std::string& name);
  void LoadWorkspacePreset(const std::string& name);
  void LoadDeveloperLayout();
  void LoadDesignerLayout();
  void LoadModderLayout();
  
  // Window management
  void ShowAllWindows();
  void HideAllWindows();
  void MaximizeCurrentWindow();
  void RestoreAllWindows();
  void CloseAllFloatingWindows();
  
  // Session queries
  size_t GetActiveSessionCount() const;
  bool HasDuplicateSession(const std::string& filepath) const;
  
  void set_sessions(std::deque<SessionInfo>* sessions) { sessions_ = sessions; }
  
 private:
  ToastManager* toast_manager_;
  std::deque<SessionInfo>* sessions_ = nullptr;
  std::string last_workspace_preset_;
};

}  // namespace editor
}  // namespace yaze

#endif  // YAZE_APP_EDITOR_UI_WORKSPACE_MANAGER_H_
