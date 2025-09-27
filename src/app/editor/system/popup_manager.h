#ifndef YAZE_APP_EDITOR_POPUP_MANAGER_H
#define YAZE_APP_EDITOR_POPUP_MANAGER_H

#include <functional>
#include <string>
#include <unordered_map>

#include "absl/status/status.h"

namespace yaze {
namespace editor {

// Forward declaration
class EditorManager;

struct PopupParams {
  std::string name;
  bool is_visible = false;
  std::function<void()> draw_function;
};

// ImGui popup manager.
class PopupManager {
 public:
  PopupManager(EditorManager* editor_manager);

  // Initialize popups
  void Initialize();

  // Draw all popups
  void DrawPopups();

  // Show a specific popup
  void Show(const char* name);

  // Hide a specific popup
  void Hide(const char* name);

  // Check if a popup is visible
  bool IsVisible(const char* name) const;

  // Set the status for status popup
  void SetStatus(const absl::Status& status);

  // Get the current status
  absl::Status GetStatus() const { return status_; }

 private:
  // Helper function to begin a centered popup
  bool BeginCentered(const char* name);

  // Draw the about popup
  void DrawAboutPopup();

  // Draw the ROM info popup
  void DrawRomInfoPopup();

  // Draw the status popup
  void DrawStatusPopup();

  // Draw the save as popup
  void DrawSaveAsPopup();

  // Draw the new project popup
  void DrawNewProjectPopup();

  // Draw the supported features popup
  void DrawSupportedFeaturesPopup();

  // Draw the open ROM help popup
  void DrawOpenRomHelpPopup();

  // Draw the manage project popup
  void DrawManageProjectPopup();

  // v0.3 Help Documentation popups
  void DrawGettingStartedPopup();
  void DrawAsarIntegrationPopup();
  void DrawBuildInstructionsPopup();
  void DrawCLIUsagePopup();
  void DrawTroubleshootingPopup();
  void DrawContributingPopup();
  void DrawWhatsNewPopup();
  
  // Workspace-related popups
  void DrawWorkspaceHelpPopup();
  void DrawSessionLimitWarningPopup();
  void DrawLayoutResetConfirmPopup();
  
  // Settings popups (accessible without ROM)
  void DrawDisplaySettingsPopup();

  EditorManager* editor_manager_;
  std::unordered_map<std::string, PopupParams> popups_;
  absl::Status status_;
  bool show_status_ = false;
  absl::Status prev_status_;
};

}  // namespace editor
}  // namespace yaze

#endif  // YAZE_APP_EDITOR_POPUP_MANAGER_H
