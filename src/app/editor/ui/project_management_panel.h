#ifndef YAZE_APP_EDITOR_UI_PROJECT_MANAGEMENT_PANEL_H_
#define YAZE_APP_EDITOR_UI_PROJECT_MANAGEMENT_PANEL_H_

#include <functional>
#include <string>
#include <vector>

#include "core/project.h"
#include "core/version_manager.h"

namespace yaze {

class Rom;

namespace editor {

class ToastManager;

/**
 * @class ProjectManagementPanel
 * @brief Panel for managing project settings, ROM versions, and snapshots
 *
 * Displayed in the right sidebar when a project is loaded. Features:
 * - Project overview (name, ROM file, paths)
 * - ROM version management (swap ROMs, reload)
 * - Git/snapshot integration for versioning
 * - Quick access to project configuration
 */
class ProjectManagementPanel {
 public:
  ProjectManagementPanel() = default;

  // Dependencies
  void SetProject(project::YazeProject* project) { project_ = project; }
  void SetVersionManager(core::VersionManager* manager) {
    version_manager_ = manager;
  }
  void SetRom(Rom* rom) { rom_ = rom; }
  void SetToastManager(ToastManager* manager) { toast_manager_ = manager; }

  // Callbacks for actions that need EditorManager
  using SwapRomCallback = std::function<void()>;
  using ReloadRomCallback = std::function<void()>;
  using SaveProjectCallback = std::function<void()>;
  using BrowseFolderCallback = std::function<void(const std::string& type)>;

  void SetSwapRomCallback(SwapRomCallback cb) { swap_rom_callback_ = cb; }
  void SetReloadRomCallback(ReloadRomCallback cb) { reload_rom_callback_ = cb; }
  void SetSaveProjectCallback(SaveProjectCallback cb) {
    save_project_callback_ = cb;
  }
  void SetBrowseFolderCallback(BrowseFolderCallback cb) {
    browse_folder_callback_ = cb;
  }

  // Main draw entry point
  void Draw();

 private:
  void DrawProjectOverview();
  void DrawRomManagement();
  void DrawVersionControl();
  void DrawSnapshotHistory();
  void DrawQuickActions();

  project::YazeProject* project_ = nullptr;
  core::VersionManager* version_manager_ = nullptr;
  Rom* rom_ = nullptr;
  ToastManager* toast_manager_ = nullptr;

  // Callbacks
  SwapRomCallback swap_rom_callback_;
  ReloadRomCallback reload_rom_callback_;
  SaveProjectCallback save_project_callback_;
  BrowseFolderCallback browse_folder_callback_;

  // Snapshot creation UI state
  char snapshot_message_[256] = {};
  bool show_snapshot_dialog_ = false;

  // History cache
  std::vector<std::string> history_cache_;
  bool history_dirty_ = true;

  // Project edit state
  bool project_dirty_ = false;
};

}  // namespace editor
}  // namespace yaze

#endif  // YAZE_APP_EDITOR_UI_PROJECT_MANAGEMENT_PANEL_H_

