#ifndef YAZE_APP_EDITOR_SHELL_WINDOWS_PROJECT_MANAGEMENT_PANEL_H_
#define YAZE_APP_EDITOR_SHELL_WINDOWS_PROJECT_MANAGEMENT_PANEL_H_

#include <functional>
#include <string>
#include <vector>

#include "app/editor/system/session/project_workflow_status.h"
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
  using BuildProjectCallback = std::function<void()>;
  using CancelBuildCallback = std::function<void()>;
  using RunProjectCallback = std::function<void()>;
  using BrowseFolderCallback = std::function<void(const std::string& type)>;

  void SetSwapRomCallback(SwapRomCallback cb) { swap_rom_callback_ = cb; }
  void SetReloadRomCallback(ReloadRomCallback cb) { reload_rom_callback_ = cb; }
  void SetSaveProjectCallback(SaveProjectCallback cb) {
    save_project_callback_ = cb;
  }
  void SetBuildProjectCallback(BuildProjectCallback cb) {
    build_project_callback_ = std::move(cb);
  }
  void SetCancelBuildCallback(CancelBuildCallback cb) {
    cancel_build_callback_ = std::move(cb);
  }
  void SetRunProjectCallback(RunProjectCallback cb) {
    run_project_callback_ = std::move(cb);
  }
  void SetBrowseFolderCallback(BrowseFolderCallback cb) {
    browse_folder_callback_ = cb;
  }
  void SetBuildStatus(const ProjectWorkflowStatus& status) {
    build_status_ = status;
  }
  void SetRunStatus(const ProjectWorkflowStatus& status) {
    run_status_ = status;
  }
  void SetBuildLogOutput(const std::string& output) {
    build_log_output_ = output;
  }

  // Main draw entry point
  void Draw();

 private:
  void DrawProjectOverview();
  void DrawStorageLocations();
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
  BuildProjectCallback build_project_callback_;
  CancelBuildCallback cancel_build_callback_;
  RunProjectCallback run_project_callback_;
  BrowseFolderCallback browse_folder_callback_;

  // Snapshot creation UI state
  char snapshot_message_[256] = {};
  bool show_snapshot_dialog_ = false;

  // History cache
  std::vector<std::string> history_cache_;
  bool history_dirty_ = true;

  // Project edit state
  bool project_dirty_ = false;

  ProjectWorkflowStatus build_status_;
  ProjectWorkflowStatus run_status_;
  std::string build_log_output_;
};

}  // namespace editor
}  // namespace yaze

#endif  // YAZE_APP_EDITOR_SHELL_WINDOWS_PROJECT_MANAGEMENT_PANEL_H_
