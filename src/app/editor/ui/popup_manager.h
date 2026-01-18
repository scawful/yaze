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

/**
 * @enum PopupType
 * @brief Type classification for popups to enable future filtering and
 * organization
 */
enum class PopupType {
  kInfo,           // Information display (About, ROM Info, etc.)
  kHelp,           // Help documentation (Getting Started, etc.)
  kSettings,       // Settings dialogs (Display Settings, etc.)
  kFileOperation,  // File operations (Save As, New Project, etc.)
  kConfirmation,   // Confirmation dialogs (Layout Reset, etc.)
  kWarning,        // Warning messages (Session Limit, etc.)
  kEditor          // Editor-specific dialogs
};

/**
 * @struct PopupDefinition
 * @brief Complete definition of a popup including metadata
 */
struct PopupDefinition {
  const char* id;                       // Unique constant identifier
  const char* display_name;             // Human-readable name for UI
  PopupType type;                       // Type classification
  bool allow_resize;                    // Whether popup can be resized
  std::function<void()> draw_function;  // Drawing callback (set at runtime)
};

/**
 * @struct PopupParams
 * @brief Runtime state for a registered popup
 */
struct PopupParams {
  std::string name;
  PopupType type;
  bool is_visible = false;
  bool allow_resize = false;
  std::function<void()> draw_function;
};

/**
 * @namespace PopupID
 * @brief String constants for all popup identifiers to prevent typos
 */
namespace PopupID {
// File Operations
constexpr const char* kSaveAs = "Save As..";
constexpr const char* kNewProject = "New Project";
constexpr const char* kManageProject = "Manage Project";

// Information
constexpr const char* kAbout = "About";
constexpr const char* kRomInfo = "ROM Information";
constexpr const char* kSupportedFeatures = "Supported Features";
constexpr const char* kStatus = "Status";

// Help Documentation
constexpr const char* kGettingStarted = "Getting Started";
constexpr const char* kAsarIntegration = "Asar Integration";
constexpr const char* kBuildInstructions = "Build Instructions";
constexpr const char* kCLIUsage = "CLI Usage";
constexpr const char* kTroubleshooting = "Troubleshooting";
constexpr const char* kContributing = "Contributing";
constexpr const char* kWhatsNew = "What's New";
constexpr const char* kOpenRomHelp = "Open a ROM";

// Settings
constexpr const char* kDisplaySettings = "Display Settings";
constexpr const char* kFeatureFlags = "Feature Flags";

// Workspace
constexpr const char* kWorkspaceHelp = "Workspace Help";
constexpr const char* kSessionLimitWarning = "Session Limit Warning";
constexpr const char* kLayoutResetConfirm = "Reset Layout Confirmation";
constexpr const char* kLayoutPresets = "Layout Presets";
constexpr const char* kSessionManager = "Session Manager";

// Debug/Testing
constexpr const char* kDataIntegrity = "Data Integrity Check";

// Future expansion
constexpr const char* kQuickExport = "Quick Export";
constexpr const char* kAssetImport = "Asset Import";
constexpr const char* kScriptGenerator = "Script Generator";
}  // namespace PopupID

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

  // Help Documentation popups
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
  void DrawLayoutPresetsPopup();
  void DrawSessionManagerPopup();

  // Settings popups (accessible without ROM)
  void DrawDisplaySettingsPopup();
  void DrawFeatureFlagsPopup();

  // Debug/Testing popups
  void DrawDataIntegrityPopup();

  EditorManager* editor_manager_;
  std::unordered_map<std::string, PopupParams> popups_;
  absl::Status status_;
  bool show_status_ = false;
  absl::Status prev_status_;
};

}  // namespace editor
}  // namespace yaze

#endif  // YAZE_APP_EDITOR_POPUP_MANAGER_H
