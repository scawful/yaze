#ifndef YAZE_APP_EDITOR_SHELL_WINDOWS_SETTINGS_PANEL_H_
#define YAZE_APP_EDITOR_SHELL_WINDOWS_SETTINGS_PANEL_H_

#include <array>
#include <functional>
#include <string>
#include <utility>

#include "absl/status/status.h"
#include "app/editor/editor.h"
#include "app/editor/menu/status_bar.h"
#include "app/editor/system/session/user_settings.h"
#include "app/gui/core/theme_manager.h"
#include "core/patch/patch_manager.h"
#include "core/project.h"

namespace yaze {

class Rom;

namespace editor {

class WorkspaceWindowManager;
class ShortcutManager;
class SettingsPanelTestPeer;

/**
 * @class SettingsPanel
 * @brief Manages the settings UI displayed in the right sidebar
 *
 * Replaces the old SettingsEditor. Handles configuration of:
 * - General settings (feature flags)
 * - Appearance (themes, fonts)
 * - Editor behavior
 * - Performance
 * - AI Agent
 * - Keyboard shortcuts
 * - Project configuration
 */
class SettingsPanel : public Editor {
 public:
  using OpenMinecartTracksCallback = std::function<absl::Status()>;

  SettingsPanel() { type_ = EditorType::kSettings; }

  void SetDependencies(const EditorDependencies& deps) override;

  void Initialize() override {}
  absl::Status Load() override { return absl::OkStatus(); }
  absl::Status Save() override { return absl::OkStatus(); }
  absl::Status Update() override {
    Draw();
    return absl::OkStatus();
  }

  absl::Status Undo() override { return absl::OkStatus(); }
  absl::Status Redo() override { return absl::OkStatus(); }
  absl::Status Cut() override { return absl::OkStatus(); }
  absl::Status Copy() override { return absl::OkStatus(); }
  absl::Status Paste() override { return absl::OkStatus(); }
  absl::Status Find() override { return absl::OkStatus(); }

  void SetUserSettings(UserSettings* settings) { user_settings_ = settings; }
  void SetWindowManager(WorkspaceWindowManager* registry) {
    window_manager_ = registry;
  }
  void SetShortcutManager(ShortcutManager* manager) {
    shortcut_manager_ = manager;
  }
  void SetStatusBar(StatusBar* bar) { status_bar_ = bar; }
  void SetRom(Rom* rom) { rom_ = rom; }
  void SetProject(project::YazeProject* project) { project_ = project; }
  void SetOpenMinecartTracksCallback(OpenMinecartTracksCallback callback) {
    open_minecart_tracks_callback_ = std::move(callback);
  }

  // Main draw entry point
  void Draw();

 private:
  friend class SettingsPanelTestPeer;

  using DungeonOverlaySummary = std::array<std::pair<std::string, bool>, 5>;

  void DrawGeneralSettings();
  void DrawAppearanceSettings();
  // Switches the active theme to `preset` density. Lives outside the combo
  // callback so it can be tested: the combo is one line, but the behaviour
  // that matters — routing through ReapplyTheme so Classic YAZE repaints via
  // ColorsYaze() rather than its struct — is not reachable from a unit test
  // while it is buried in an ImGui interaction.
  void ApplyDisplayDensity(gui::DensityPreset preset);
  void DrawWorkspaceSettings();
  // Loads `name` from UserSettings::named_layouts, validates the
  // serialized DockTree, and applies it to the live main dockspace via
  // LayoutManager. Surface for the Workspace section's combo + Re-apply
  // button. On success, persists `last_applied_layout_name = name` so a
  // subsequent startup reapplies the same layout (mirrors the
  // theme-persistence pattern shipped in Phase 5.1). Status is reported
  // via workspace_status_message_.
  void ApplyNamedLayoutToDockspace(const std::string& name);
  void DrawEditorBehavior();
  void DrawPerformanceSettings();
  void DrawAIAgentSettings();
  void DrawFilesystemSettings();
  void DrawKeyboardShortcuts();
  void DrawGlobalShortcuts();
  void DrawEditorShortcuts();
  void DrawPanelShortcuts();
  bool MatchesShortcutFilter(const std::string& text) const;
  void DrawPatchSettings();
  void DrawProjectSettings();  // New method
  void DrawPatchList(const std::string& folder);
  void DrawPatchDetails();
  void DrawParameterWidget(core::PatchParameter* param);
  static DungeonOverlaySummary BuildDungeonOverlaySummary(
      const project::DungeonOverlaySettings& overlay);
  absl::Status RequestOpenMinecartTracks();

  UserSettings* user_settings_ = nullptr;
  WorkspaceWindowManager* window_manager_ = nullptr;
  ShortcutManager* shortcut_manager_ = nullptr;
  StatusBar* status_bar_ = nullptr;
  Rom* rom_ = nullptr;
  project::YazeProject* project_ = nullptr;  // Project reference

  // Shortcut editing state
  char shortcut_edit_buffer_[64] = {};
  std::string editing_card_id_;
  bool is_editing_shortcut_ = false;
  std::string shortcut_filter_;

  // Patch system state
  core::PatchManager patch_manager_;
  std::string selected_folder_;
  core::AsmPatch* selected_patch_ = nullptr;
  bool patches_loaded_ = false;

  // Workspace layout picker transient status — last apply attempt's
  // outcome, drawn beneath the section. Persists across frames within a
  // session; cleared on the next apply attempt.
  std::string workspace_status_message_;
  bool workspace_status_is_error_ = false;

  OpenMinecartTracksCallback open_minecart_tracks_callback_;
  std::string project_status_message_;
};

}  // namespace editor
}  // namespace yaze

#endif  // YAZE_APP_EDITOR_SHELL_WINDOWS_SETTINGS_PANEL_H_
