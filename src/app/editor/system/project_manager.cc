#include "project_manager.h"

#include <filesystem>
#include <fstream>

#include "absl/strings/str_format.h"
#include "app/editor/ui/toast_manager.h"
#include "core/project.h"
#include "util/macro.h"

namespace yaze {
namespace editor {

ProjectManager::ProjectManager(ToastManager* toast_manager)
    : toast_manager_(toast_manager) {}

absl::Status ProjectManager::CreateNewProject(
    const std::string& template_name) {
  // ROM-first workflow: Creating a project requires a ROM to be loaded
  // The actual project creation happens after ROM selection in the wizard

  if (template_name.empty()) {
    // Create default project - will be configured after ROM load
    current_project_ = project::YazeProject();
    current_project_.name = "New Project";
    current_project_.filepath = GenerateProjectFilename("New Project");

    if (toast_manager_) {
      toast_manager_->Show("New project created - select a ROM to continue",
                           ToastType::kInfo);
    }

    // Mark that we're waiting for ROM selection
    pending_rom_selection_ = true;
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

  auto status = current_project_.Open(filename);
  if (!status.ok()) {
    if (toast_manager_) {
      toast_manager_->Show(
          absl::StrFormat("Failed to load project: %s", status.message()),
          ToastType::kError);
    }
    return status;
  }

  if (toast_manager_) {
    toast_manager_->Show(absl::StrFormat("Project loaded: %s",
                                         current_project_.GetDisplayName()),
                         ToastType::kSuccess);
  }

  return absl::OkStatus();
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
  auto status = current_project_.SaveAs(filename);
  if (!status.ok()) {
    if (toast_manager_) {
      toast_manager_->Show(
          absl::StrFormat("Failed to save project: %s", status.message()),
          ToastType::kError);
    }
    return status;
  }

  if (toast_manager_) {
    toast_manager_->Show(absl::StrFormat("Project saved: %s", filename),
                         ToastType::kSuccess);
  }

  return absl::OkStatus();
}

absl::Status ProjectManager::ImportProject(const std::string& project_path) {
  if (project_path.empty()) {
    return absl::InvalidArgumentError("No project path provided");
  }

#ifndef __EMSCRIPTEN__
  if (!std::filesystem::exists(project_path)) {
    return absl::NotFoundError(
        absl::StrFormat("Project path does not exist: %s", project_path));
  }
#endif

  project::YazeProject imported_project;

  // Handle ZScream project imports (.zsproj files)
  if (project_path.ends_with(".zsproj")) {
    RETURN_IF_ERROR(imported_project.ImportZScreamProject(project_path));
    if (toast_manager_) {
      toast_manager_->Show(
          "ZScream project imported successfully. Please configure ROM and "
          "folders.",
          ToastType::kInfo, 5.0f);
    }
  } else {
    // Standard yaze project import
    RETURN_IF_ERROR(imported_project.Open(project_path));
    if (toast_manager_) {
      toast_manager_->Show(
          absl::StrFormat("Project imported: %s", project_path),
          ToastType::kSuccess);
    }
  }

  current_project_ = std::move(imported_project);
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
    toast_manager_->Show(absl::StrFormat("Project exported: %s", export_path),
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
  return {"Empty Project", "Dungeon Editor Project", "Overworld Editor Project",
          "Graphics Editor Project", "Full Editor Project"};
}

absl::Status ProjectManager::CreateFromTemplate(
    const std::string& template_name, const std::string& project_name) {
  if (template_name.empty() || project_name.empty()) {
    return absl::InvalidArgumentError(
        "Template name and project name required");
  }

  // TODO: Implement template-based project creation
  // This would copy template files and customize them

  current_project_ = project::YazeProject();
  current_project_.name = project_name;
  current_project_.filepath = GenerateProjectFilename(project_name);

  if (toast_manager_) {
    toast_manager_->Show(
        absl::StrFormat("Project created from template: %s", template_name),
        ToastType::kSuccess);
  }

  return absl::OkStatus();
}

std::string ProjectManager::GenerateProjectFilename(
    const std::string& project_name) const {
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

#ifndef __EMSCRIPTEN__
  if (!std::filesystem::exists(filename)) {
    return false;
  }
#endif

  // Check file extension
  std::string extension = std::filesystem::path(filename).extension().string();
  return extension == ".yaze" || extension == ".json";
}

absl::Status ProjectManager::InitializeProjectStructure(
    const std::string& project_path) {
#ifdef __EMSCRIPTEN__
  // WASM uses virtual storage; nothing to provision eagerly.
  return absl::OkStatus();
#else
  try {
    std::filesystem::create_directories(project_path);
    std::filesystem::create_directories(project_path + "/assets");
    std::filesystem::create_directories(project_path + "/scripts");
    std::filesystem::create_directories(project_path + "/output");
    return absl::OkStatus();
  } catch (const std::exception& e) {
    return absl::InternalError(
        absl::StrFormat("Failed to create project structure: %s", e.what()));
  }
#endif
}

// ============================================================================
// ROM-First Workflow Implementation
// ============================================================================

absl::Status ProjectManager::SetProjectRom(const std::string& rom_path) {
  if (rom_path.empty()) {
    return absl::InvalidArgumentError("ROM path cannot be empty");
  }

#ifndef __EMSCRIPTEN__
  if (!std::filesystem::exists(rom_path)) {
    return absl::NotFoundError(
        absl::StrFormat("ROM file not found: %s", rom_path));
  }
#endif

  current_project_.rom_filename = rom_path;
  pending_rom_selection_ = false;

  if (toast_manager_) {
    toast_manager_->Show(
        absl::StrFormat("ROM set for project: %s",
                        std::filesystem::path(rom_path).filename().string()),
        ToastType::kSuccess);
  }

  return absl::OkStatus();
}

absl::Status ProjectManager::FinalizeProjectCreation(
    const std::string& project_name, const std::string& project_path) {
  if (project_name.empty()) {
    return absl::InvalidArgumentError("Project name cannot be empty");
  }

  current_project_.name = project_name;

  if (!project_path.empty()) {
    current_project_.filepath = project_path;
  } else {
    current_project_.filepath = GenerateProjectFilename(project_name);
  }

  pending_rom_selection_ = false;
  pending_template_name_.clear();

  // Initialize project structure if we have a directory
  std::string project_dir;
#ifndef __EMSCRIPTEN__
  project_dir =
      std::filesystem::path(current_project_.filepath).parent_path().string();
#endif
  if (!project_dir.empty()) {
    auto status = InitializeProjectStructure(project_dir);
    if (!status.ok()) {
      if (toast_manager_) {
        toast_manager_->Show("Could not create project directories",
                             ToastType::kWarning);
      }
    }
  }

  if (toast_manager_) {
    toast_manager_->Show(absl::StrFormat("Project created: %s", project_name),
                         ToastType::kSuccess);
  }

  return absl::OkStatus();
}

void ProjectManager::CancelPendingProject() {
  if (pending_rom_selection_) {
    pending_rom_selection_ = false;
    pending_template_name_.clear();
    current_project_ = project::YazeProject();

    if (toast_manager_) {
      toast_manager_->Show("Project creation cancelled", ToastType::kInfo);
    }
  }
}

void ProjectManager::RequestRomSelection() {
  if (rom_selection_callback_) {
    rom_selection_callback_();
  }
}

// ============================================================================
// ZSCustomOverworld Presets
// ============================================================================

std::vector<project::ProjectManager::ProjectTemplate>
ProjectManager::GetZsoTemplates() {
  std::vector<project::ProjectManager::ProjectTemplate> templates;

  // Vanilla ROM Hack
  {
    project::ProjectManager::ProjectTemplate t;
    t.name = "Vanilla ROM Hack";
    t.description =
        "Standard ROM editing without custom ASM patches. "
        "Limited to vanilla game features.";
    t.icon = "MD_GAMEPAD";
    t.template_project.feature_flags.overworld.kLoadCustomOverworld = false;
    t.template_project.feature_flags.overworld.kSaveOverworldMaps = true;
    templates.push_back(t);
  }

  // ZSCustomOverworld v2
  {
    project::ProjectManager::ProjectTemplate t;
    t.name = "ZSCustomOverworld v2";
    t.description =
        "Basic overworld expansion with custom BG colors, "
        "main palettes, and parent system.";
    t.icon = "MD_MAP";
    t.template_project.feature_flags.overworld.kLoadCustomOverworld = true;
    t.template_project.feature_flags.overworld.kSaveOverworldMaps = true;
    t.template_project.feature_flags.kSaveAllPalettes = true;
    templates.push_back(t);
  }

  // ZSCustomOverworld v3 (Recommended)
  {
    project::ProjectManager::ProjectTemplate t;
    t.name = "ZSCustomOverworld v3";
    t.description =
        "Full overworld expansion: wide/tall areas, animated GFX, "
        "subscreen overlays, and all custom features.";
    t.icon = "MD_TERRAIN";
    t.template_project.feature_flags.overworld.kLoadCustomOverworld = true;
    t.template_project.feature_flags.overworld.kSaveOverworldMaps = true;
    t.template_project.feature_flags.overworld.kSaveOverworldEntrances = true;
    t.template_project.feature_flags.overworld.kSaveOverworldExits = true;
    t.template_project.feature_flags.overworld.kSaveOverworldItems = true;
    t.template_project.feature_flags.overworld.kSaveOverworldProperties = true;
    t.template_project.feature_flags.kSaveAllPalettes = true;
    t.template_project.feature_flags.kSaveGfxGroups = true;
    t.template_project.feature_flags.kSaveDungeonMaps = true;
    templates.push_back(t);
  }

  // Randomizer Compatible
  {
    project::ProjectManager::ProjectTemplate t;
    t.name = "Randomizer Compatible";
    t.description =
        "Minimal editing preset compatible with ALttP Randomizer. "
        "Avoids changes that break randomizer compatibility.";
    t.icon = "MD_SHUFFLE";
    t.template_project.feature_flags.overworld.kLoadCustomOverworld = false;
    t.template_project.feature_flags.overworld.kSaveOverworldMaps = false;
    templates.push_back(t);
  }

  return templates;
}

absl::Status ProjectManager::ApplyZsoPreset(const std::string& preset_name) {
  auto templates = GetZsoTemplates();

  for (const auto& t : templates) {
    if (t.name == preset_name) {
      // Apply feature flags from template
      current_project_.feature_flags = t.template_project.feature_flags;

      if (toast_manager_) {
        toast_manager_->Show(absl::StrFormat("Applied preset: %s", preset_name),
                             ToastType::kSuccess);
      }

      return absl::OkStatus();
    }
  }

  return absl::NotFoundError(
      absl::StrFormat("Unknown preset: %s", preset_name));
}

}  // namespace editor
}  // namespace yaze
