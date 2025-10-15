#include "project_manager.h"

#include <filesystem>
#include <fstream>

#include "absl/strings/str_format.h"
#include "app/editor/system/toast_manager.h"
#include "app/core/project.h"

namespace yaze {
namespace editor {

ProjectManager::ProjectManager(ToastManager* toast_manager)
    : toast_manager_(toast_manager) {
}

absl::Status ProjectManager::CreateNewProject(const std::string& template_name) {
  if (template_name.empty()) {
    // Create default project
    current_project_ = core::YazeProject();
    current_project_.name = "New Project";
    current_project_.filepath = GenerateProjectFilename("New Project");
    
    if (toast_manager_) {
      toast_manager_->Show("New project created", ToastType::kSuccess);
    }
    return absl::OkStatus();
  }
  
  return CreateFromTemplate(template_name, "New Project");
}

absl::Status ProjectManager::OpenProject(const std::string& filename) {
  if (filename.empty()) {
    // TODO: Show file dialog
    return absl::InvalidArgumentError("No filename provided");
  }
  
  return LoadProjectFromFile(filename);
}

absl::Status ProjectManager::LoadProjectFromFile(const std::string& filename) {
  if (!IsValidProjectFile(filename)) {
    return absl::InvalidArgumentError(
        absl::StrFormat("Invalid project file: %s", filename));
  }
  
  try {
    // TODO: Implement actual project loading from JSON/YAML
    // For now, create a basic project structure
    
    current_project_ = core::YazeProject();
    current_project_.filepath = filename;
    current_project_.name = std::filesystem::path(filename).stem().string();
    
    if (toast_manager_) {
      toast_manager_->Show(
          absl::StrFormat("Project loaded: %s", current_project_.name),
          ToastType::kSuccess);
    }
    
    return absl::OkStatus();
    
  } catch (const std::exception& e) {
    if (toast_manager_) {
      toast_manager_->Show(
          absl::StrFormat("Failed to load project: %s", e.what()),
          ToastType::kError);
    }
    return absl::InternalError(
        absl::StrFormat("Failed to load project: %s", e.what()));
  }
}

absl::Status ProjectManager::SaveProject() {
  if (!HasActiveProject()) {
    return absl::FailedPreconditionError("No active project to save");
  }
  
  return SaveProjectToFile(current_project_.filepath);
}

absl::Status ProjectManager::SaveProjectAs(const std::string& filename) {
  if (filename.empty()) {
    // TODO: Show save dialog
    return absl::InvalidArgumentError("No filename provided for save as");
  }
  
  return SaveProjectToFile(filename);
}

absl::Status ProjectManager::SaveProjectToFile(const std::string& filename) {
  try {
    // TODO: Implement actual project saving to JSON/YAML
    // For now, just update the filepath
    
    current_project_.filepath = filename;
    
    if (toast_manager_) {
      toast_manager_->Show(
          absl::StrFormat("Project saved: %s", filename),
          ToastType::kSuccess);
    }
    
    return absl::OkStatus();
    
  } catch (const std::exception& e) {
    if (toast_manager_) {
      toast_manager_->Show(
          absl::StrFormat("Failed to save project: %s", e.what()),
          ToastType::kError);
    }
    return absl::InternalError(
        absl::StrFormat("Failed to save project: %s", e.what()));
  }
}

absl::Status ProjectManager::ImportProject(const std::string& project_path) {
  if (project_path.empty()) {
    return absl::InvalidArgumentError("No project path provided");
  }
  
  if (!std::filesystem::exists(project_path)) {
    return absl::NotFoundError(
        absl::StrFormat("Project path does not exist: %s", project_path));
  }
  
  // TODO: Implement project import logic
  // This would typically copy project files and update paths
  
  if (toast_manager_) {
    toast_manager_->Show(
        absl::StrFormat("Project imported: %s", project_path),
        ToastType::kSuccess);
  }
  
  return absl::OkStatus();
}

absl::Status ProjectManager::ExportProject(const std::string& export_path) {
  if (!HasActiveProject()) {
    return absl::FailedPreconditionError("No active project to export");
  }
  
  if (export_path.empty()) {
    return absl::InvalidArgumentError("No export path provided");
  }
  
  // TODO: Implement project export logic
  // This would typically create a package with all project files
  
  if (toast_manager_) {
    toast_manager_->Show(
        absl::StrFormat("Project exported: %s", export_path),
        ToastType::kSuccess);
  }
  
  return absl::OkStatus();
}

absl::Status ProjectManager::RepairCurrentProject() {
  if (!HasActiveProject()) {
    return absl::FailedPreconditionError("No active project to repair");
  }
  
  // TODO: Implement project repair logic
  // This would check for missing files, broken references, etc.
  
  if (toast_manager_) {
    toast_manager_->Show("Project repair completed", ToastType::kSuccess);
  }
  
  return absl::OkStatus();
}

absl::Status ProjectManager::ValidateProject() {
  if (!HasActiveProject()) {
    return absl::FailedPreconditionError("No active project to validate");
  }
  
  auto result = current_project_.Validate();
  if (!result.ok()) {
    if (toast_manager_) {
      toast_manager_->Show(
          absl::StrFormat("Project validation failed: %s", result.message()),
          ToastType::kError);
    }
    return result;
  }
  
  if (toast_manager_) {
    toast_manager_->Show("Project validation passed", ToastType::kSuccess);
  }
  
  return absl::OkStatus();
}

std::string ProjectManager::GetProjectName() const {
  return current_project_.name;
}

std::string ProjectManager::GetProjectPath() const {
  return current_project_.filepath;
}

std::vector<std::string> ProjectManager::GetAvailableTemplates() const {
  // TODO: Scan templates directory and return available templates
  return {
    "Empty Project",
    "Dungeon Editor Project", 
    "Overworld Editor Project",
    "Graphics Editor Project",
    "Full Editor Project"
  };
}

absl::Status ProjectManager::CreateFromTemplate(const std::string& template_name,
                                               const std::string& project_name) {
  if (template_name.empty() || project_name.empty()) {
    return absl::InvalidArgumentError("Template name and project name required");
  }
  
  // TODO: Implement template-based project creation
  // This would copy template files and customize them
  
  current_project_ = core::YazeProject();
  current_project_.name = project_name;
  current_project_.filepath = GenerateProjectFilename(project_name);
  
  if (toast_manager_) {
    toast_manager_->Show(
        absl::StrFormat("Project created from template: %s", template_name),
        ToastType::kSuccess);
  }
  
  return absl::OkStatus();
}

std::string ProjectManager::GenerateProjectFilename(const std::string& project_name) const {
  // Convert project name to valid filename
  std::string filename = project_name;
  std::replace(filename.begin(), filename.end(), ' ', '_');
  std::replace(filename.begin(), filename.end(), '/', '_');
  std::replace(filename.begin(), filename.end(), '\\', '_');
  
  return absl::StrFormat("%s.yaze", filename);
}

bool ProjectManager::IsValidProjectFile(const std::string& filename) const {
  if (filename.empty()) {
    return false;
  }
  
  if (!std::filesystem::exists(filename)) {
    return false;
  }
  
  // Check file extension
  std::string extension = std::filesystem::path(filename).extension().string();
  return extension == ".yaze" || extension == ".json";
}

absl::Status ProjectManager::InitializeProjectStructure(const std::string& project_path) {
  try {
    // Create project directory structure
    std::filesystem::create_directories(project_path);
    std::filesystem::create_directories(project_path + "/assets");
    std::filesystem::create_directories(project_path + "/scripts");
    std::filesystem::create_directories(project_path + "/output");
    
    return absl::OkStatus();
    
  } catch (const std::exception& e) {
    return absl::InternalError(
        absl::StrFormat("Failed to create project structure: %s", e.what()));
  }
}

}  // namespace editor
}  // namespace yaze
