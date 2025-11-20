#ifndef YAZE_APP_EDITOR_SYSTEM_PROJECT_MANAGER_H_
#define YAZE_APP_EDITOR_SYSTEM_PROJECT_MANAGER_H_

#include <string>

#include "absl/status/status.h"
#include "core/project.h"

namespace yaze {
namespace editor {

class ToastManager;

/**
 * @class ProjectManager
 * @brief Handles all project file operations
 * 
 * Extracted from EditorManager to provide focused project management:
 * - Project creation and templates
 * - Project loading and saving
 * - Project import/export
 * - Project validation and repair
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

 private:
  project::YazeProject current_project_;
  ToastManager* toast_manager_ = nullptr;

  // Helper methods
  absl::Status LoadProjectFromFile(const std::string& filename);
  absl::Status SaveProjectToFile(const std::string& filename);
  std::string GenerateProjectFilename(const std::string& project_name) const;
  bool IsValidProjectFile(const std::string& filename) const;
  absl::Status InitializeProjectStructure(const std::string& project_path);
};

}  // namespace editor
}  // namespace yaze

#endif  // YAZE_APP_EDITOR_SYSTEM_PROJECT_MANAGER_H_
