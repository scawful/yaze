#ifndef YAZE_APP_EDITOR_SYSTEM_PROJECT_MANAGER_H_
#define YAZE_APP_EDITOR_SYSTEM_PROJECT_MANAGER_H_

#include <functional>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "app/editor/system/session/background_command_task.h"

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "app/editor/system/session/project_build_workflow.h"
#include "core/project.h"

namespace yaze {
namespace editor {

class ToastManager;
class RomLifecycleManager;

/**
 * @class ProjectManager
 * @brief Handles all project file operations with ROM-first workflow
 *
 * Extracted from EditorManager to provide focused project management:
 * - Project creation and templates (ROM-first workflow)
 * - Project loading and saving
 * - Project import/export
 * - Project validation and repair
 * - ZSCustomOverworld preset integration
 *
 * ROM-First Workflow:
 * 1. User creates new project
 * 2. ROM selection dialog opens
 * 3. ROM load options (ZSO version, feature flags)
 * 4. Project configured with selected options
 */
class ProjectManager {
 public:
  explicit ProjectManager(ToastManager* toast_manager);
  ~ProjectManager() = default;

  // Project file operations
  absl::Status CreateNewProject(const std::string& template_name = "");
  absl::Status OpenProject(const std::string& filename = "");
  absl::Status SaveProject();
  absl::Status SaveProjectAs(const std::string& filename = "");

  // Project import/export
  absl::Status ImportProject(const std::string& project_path);
  absl::Status ExportProject(const std::string& export_path);

  // Project maintenance
  absl::Status RepairCurrentProject();
  absl::Status ValidateProject();

  // Project information
  project::YazeProject& GetCurrentProject() { return current_project_; }
  const project::YazeProject& GetCurrentProject() const {
    return current_project_;
  }
  bool HasActiveProject() const { return !current_project_.filepath.empty(); }
  std::string GetProjectName() const;
  std::string GetProjectPath() const;

  // Project templates
  std::vector<std::string> GetAvailableTemplates() const;
  absl::Status CreateFromTemplate(const std::string& template_name,
                                  const std::string& project_name);

  // ============================================================================
  // ROM-First Workflow
  // ============================================================================

  /**
   * @brief Check if project is waiting for ROM selection
   */
  bool IsPendingRomSelection() const { return pending_rom_selection_; }

  /**
   * @brief Set the ROM for the current project
   * @param rom_path Path to the ROM file
   */
  absl::Status SetProjectRom(const std::string& rom_path);

  /**
   * @brief Complete project creation after ROM is loaded
   * @param project_name Name for the project
   * @param project_path Path to save project file
   */
  absl::Status FinalizeProjectCreation(const std::string& project_name,
                                       const std::string& project_path);
  absl::Status ValidateProjectCreationTarget(
      const std::string& project_name,
      const std::string& project_path = std::string()) const;

  /**
   * @brief Cancel pending project creation
   */
  void CancelPendingProject();

  /**
   * @brief Set callback for when ROM selection is needed
   */
  void SetRomSelectionCallback(std::function<void()> callback) {
    rom_selection_callback_ = callback;
  }

  /**
   * @brief Request ROM selection (triggers callback)
   */
  void RequestRomSelection();

  // ============================================================================
  // ZSCustomOverworld Presets
  // ============================================================================

  /**
   * @brief Get ZSO-specific project templates
   */
  static std::vector<project::ProjectManager::ProjectTemplate>
  GetZsoTemplates();

  /**
   * @brief Apply a ZSO preset to the current project
   * @param preset_name Name of the preset ("vanilla", "zso_v2", "zso_v3", "rando")
   */
  absl::Status ApplyZsoPreset(const std::string& preset_name);

  // ============================================================================
  // Project Build / Run workflow (identity-aware)
  // ============================================================================

  void SetBuildWorkflowServices(RomLifecycleManager* rom_lifecycle);

  bool HasActiveProjectBuild() const;

  absl::StatusOr<ProjectBuildPreparedRequest> PrepareProjectBuildRequest(
      const ProjectBuildPrepareInputs& inputs);

  absl::Status StartPreparedProjectBuild(
      const ProjectBuildPreparedRequest& request, bool synchronous);

  absl::StatusOr<std::string> WaitForPreparedProjectBuild(
      const ProjectBuildPreparedRequest& request);

  ProjectBuildPollUpdate PollProjectBuildForViewer(
      size_t viewer_session_id, const std::string& viewer_descriptor_path);

  absl::Status CancelProjectBuildForOwner(size_t owner_session_id,
                                          const std::string& descriptor_path);

  void ResetProjectBuildPresentationForDescriptor(
      const std::string& descriptor_path);

  absl::StatusOr<ProjectValidatedRunSelection> ValidateProjectRunSelection(
      const ProjectRunPrepareInputs& inputs);

  std::optional<ProjectBuildAttemptResult> GetLastBuildResultForOwner(
      size_t owner_session_id, const std::string& descriptor_path) const;

 private:
  project::YazeProject current_project_;
  ToastManager* toast_manager_ = nullptr;

  // ROM-first workflow state
  bool pending_rom_selection_ = false;
  std::string pending_template_name_;
  std::function<void()> rom_selection_callback_;

  // Helper methods
  absl::Status LoadProjectFromFile(const std::string& filename);
  absl::Status SaveProjectToFile(const std::string& filename);
  std::string ResolveProjectCreationPath(const std::string& project_name,
                                         const std::string& project_path) const;
  std::string GenerateProjectFilename(const std::string& project_name) const;
  bool IsValidProjectFile(const std::string& filename) const;
  absl::Status InitializeProjectStructure(const std::string& project_path);

  absl::Status FinalizeActiveProjectBuild(bool synchronous_wait);
  absl::StatusOr<ProjectBuildAttemptResult> CompleteBuildAttempt(
      const ProjectBuildPreparedRequest& request,
      const BackgroundCommandTask::Snapshot& snapshot);
  absl::Status ValidatePreparedBuildDoesNotOverlap(
      const ProjectBuildPreparedRequest& request) const;

  RomLifecycleManager* rom_lifecycle_ = nullptr;
  std::unique_ptr<BackgroundCommandTask> active_project_build_;
  std::optional<ProjectBuildPreparedRequest> active_build_request_;
  bool active_build_reported_ = false;
  uint64_t next_build_attempt_id_ = 1;
  std::map<std::string, ProjectBuildAttemptResult> terminal_build_results_;
};

}  // namespace editor
}  // namespace yaze

#endif  // YAZE_APP_EDITOR_SYSTEM_PROJECT_MANAGER_H_
