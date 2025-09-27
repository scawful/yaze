#include "project.h"

#include <chrono>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <sstream>

#include "absl/strings/str_format.h"
#include "absl/strings/str_join.h"
#include "absl/strings/str_split.h"
#include "app/core/platform/file_dialog.h"
#include "app/gui/icons.h"
#include "imgui/imgui.h"
#include "yaze_config.h"

namespace yaze {
namespace core {

namespace {
  // Helper functions for parsing key-value pairs
  std::pair<std::string, std::string> ParseKeyValue(const std::string& line) {
    size_t eq_pos = line.find('=');
    if (eq_pos == std::string::npos) return {"", ""};
    
    std::string key = line.substr(0, eq_pos);
    std::string value = line.substr(eq_pos + 1);
    
    // Trim whitespace
    key.erase(0, key.find_first_not_of(" \t"));
    key.erase(key.find_last_not_of(" \t") + 1);
    value.erase(0, value.find_first_not_of(" \t"));
    value.erase(value.find_last_not_of(" \t") + 1);
    
    return {key, value};
  }
  
  bool ParseBool(const std::string& value) {
    return value == "true" || value == "1" || value == "yes";
  }
  
  float ParseFloat(const std::string& value) {
    try {
      return std::stof(value);
    } catch (...) {
      return 0.0f;
    }
  }
  
  std::vector<std::string> ParseStringList(const std::string& value) {
    std::vector<std::string> result;
    if (value.empty()) return result;
    
    std::vector<std::string> parts = absl::StrSplit(value, ',');
    for (const auto& part : parts) {
      std::string trimmed = std::string(part);
      trimmed.erase(0, trimmed.find_first_not_of(" \t"));
      trimmed.erase(trimmed.find_last_not_of(" \t") + 1);
      if (!trimmed.empty()) {
        result.push_back(trimmed);
      }
    }
    return result;
  }
}

// YazeProject Implementation
absl::Status YazeProject::Create(const std::string& project_name, const std::string& base_path) {
  name = project_name;
  filepath = base_path + "/" + project_name + ".yaze";
  
  // Initialize metadata
  auto now = std::chrono::system_clock::now();
  auto time_t = std::chrono::system_clock::to_time_t(now);
  std::stringstream ss;
  ss << std::put_time(std::localtime(&time_t), "%Y-%m-%d %H:%M:%S");
  
  metadata.created_date = ss.str();
  metadata.last_modified = ss.str();
  metadata.yaze_version = "0.3.1"; // TODO: Get from version header
  metadata.version = "2.0";
  metadata.created_by = "YAZE";
  
  InitializeDefaults();
  
  // Create project directory structure
  std::filesystem::path project_dir(base_path + "/" + project_name);
  std::filesystem::create_directories(project_dir);
  std::filesystem::create_directories(project_dir / "code");
  std::filesystem::create_directories(project_dir / "assets");
  std::filesystem::create_directories(project_dir / "patches");
  std::filesystem::create_directories(project_dir / "backups");
  std::filesystem::create_directories(project_dir / "output");
  
  // Set folder paths
  code_folder = (project_dir / "code").string();
  assets_folder = (project_dir / "assets").string();
  patches_folder = (project_dir / "patches").string();
  rom_backup_folder = (project_dir / "backups").string();
  output_folder = (project_dir / "output").string();
  labels_filename = (project_dir / "labels.txt").string();
  symbols_filename = (project_dir / "symbols.txt").string();
  
  return Save();
}

absl::Status YazeProject::Open(const std::string& project_path) {
  filepath = project_path;
  
  // Determine format and load accordingly
  if (project_path.ends_with(".yaze")) {
    format = ProjectFormat::kYazeNative;
    return LoadFromYazeFormat(project_path);
  } else if (project_path.ends_with(".zsproj")) {
    format = ProjectFormat::kZScreamCompat;
    return ImportFromZScreamFormat(project_path);
  }
  
  return absl::InvalidArgumentError("Unsupported project file format");
}

absl::Status YazeProject::Save() {
  return SaveToYazeFormat();
}

absl::Status YazeProject::SaveAs(const std::string& new_path) {
  std::string old_filepath = filepath;
  filepath = new_path;
  
  auto status = Save();
  if (!status.ok()) {
    filepath = old_filepath; // Restore on failure
  }
  
  return status;
}

absl::Status YazeProject::LoadFromYazeFormat(const std::string& project_path) {
  std::ifstream file(project_path);
  if (!file.is_open()) {
    return absl::InvalidArgumentError(absl::StrFormat("Cannot open project file: %s", project_path));
  }

  std::string line;
  std::string current_section = "";
  
  while (std::getline(file, line)) {
    // Skip empty lines and comments
    if (line.empty() || line[0] == '#') continue;
    
    // Check for section headers [section_name]
    if (line[0] == '[' && line.back() == ']') {
      current_section = line.substr(1, line.length() - 2);
      continue;
    }
    
    auto [key, value] = ParseKeyValue(line);
    if (key.empty()) continue;
    
    // Parse based on current section
    if (current_section == "project") {
      if (key == "name") name = value;
      else if (key == "description") metadata.description = value;
      else if (key == "author") metadata.author = value;
      else if (key == "license") metadata.license = value;
      else if (key == "version") metadata.version = value;
      else if (key == "created_date") metadata.created_date = value;
      else if (key == "last_modified") metadata.last_modified = value;
      else if (key == "yaze_version") metadata.yaze_version = value;
      else if (key == "tags") metadata.tags = ParseStringList(value);
    }
    else if (current_section == "files") {
      if (key == "rom_filename") rom_filename = value;
      else if (key == "rom_backup_folder") rom_backup_folder = value;
      else if (key == "code_folder") code_folder = value;
      else if (key == "assets_folder") assets_folder = value;
      else if (key == "patches_folder") patches_folder = value;
      else if (key == "labels_filename") labels_filename = value;
      else if (key == "symbols_filename") symbols_filename = value;
      else if (key == "output_folder") output_folder = value;
      else if (key == "additional_roms") additional_roms = ParseStringList(value);
    }
    else if (current_section == "feature_flags") {
      if (key == "load_custom_overworld") feature_flags.overworld.kLoadCustomOverworld = ParseBool(value);
      else if (key == "apply_zs_custom_overworld_asm") feature_flags.overworld.kApplyZSCustomOverworldASM = ParseBool(value);
      else if (key == "save_dungeon_maps") feature_flags.kSaveDungeonMaps = ParseBool(value);
      else if (key == "save_graphics_sheet") feature_flags.kSaveGraphicsSheet = ParseBool(value);
      else if (key == "log_instructions") feature_flags.kLogInstructions = ParseBool(value);
    }
    else if (current_section == "workspace") {
      if (key == "font_global_scale") workspace_settings.font_global_scale = ParseFloat(value);
      else if (key == "dark_mode") workspace_settings.dark_mode = ParseBool(value);
      else if (key == "ui_theme") workspace_settings.ui_theme = value;
      else if (key == "autosave_enabled") workspace_settings.autosave_enabled = ParseBool(value);
      else if (key == "autosave_interval_secs") workspace_settings.autosave_interval_secs = ParseFloat(value);
      else if (key == "backup_on_save") workspace_settings.backup_on_save = ParseBool(value);
      else if (key == "show_grid") workspace_settings.show_grid = ParseBool(value);
      else if (key == "show_collision") workspace_settings.show_collision = ParseBool(value);
      else if (key == "last_layout_preset") workspace_settings.last_layout_preset = value;
      else if (key == "saved_layouts") workspace_settings.saved_layouts = ParseStringList(value);
      else if (key == "recent_files") workspace_settings.recent_files = ParseStringList(value);
    }
    else if (current_section == "build") {
      if (key == "build_script") build_script = value;
      else if (key == "output_folder") output_folder = value;
      else if (key == "git_repository") git_repository = value;
      else if (key == "track_changes") track_changes = ParseBool(value);
      else if (key == "build_configurations") build_configurations = ParseStringList(value);
    }
    else if (current_section.starts_with("labels_")) {
      // Resource labels: [labels_type_name] followed by key=value pairs
      std::string label_type = current_section.substr(7); // Remove "labels_" prefix
      resource_labels[label_type][key] = value;
    }
    else if (current_section == "keybindings") {
      workspace_settings.custom_keybindings[key] = value;
    }
    else if (current_section == "editor_visibility") {
      workspace_settings.editor_visibility[key] = ParseBool(value);
    }
    else if (current_section == "zscream_compatibility") {
      if (key == "original_project_file") zscream_project_file = value;
      else zscream_mappings[key] = value;
    }
  }
  
  file.close();
  return absl::OkStatus();
}

absl::Status YazeProject::SaveToYazeFormat() {
  // Update last modified timestamp
  auto now = std::chrono::system_clock::now();
  auto time_t = std::chrono::system_clock::to_time_t(now);
  std::stringstream ss;
  ss << std::put_time(std::localtime(&time_t), "%Y-%m-%d %H:%M:%S");
  metadata.last_modified = ss.str();
  
  std::ofstream file(filepath);
  if (!file.is_open()) {
    return absl::InvalidArgumentError(absl::StrFormat("Cannot create project file: %s", filepath));
  }
  
  // Write header comment
  file << "# YAZE Project File\n";
  file << "# Format Version: 2.0\n";
  file << "# Generated by YAZE " << metadata.yaze_version << "\n";
  file << "# Last Modified: " << metadata.last_modified << "\n\n";
  
  // Project section
  file << "[project]\n";
  file << "name=" << name << "\n";
  file << "description=" << metadata.description << "\n";
  file << "author=" << metadata.author << "\n";
  file << "license=" << metadata.license << "\n";
  file << "version=" << metadata.version << "\n";
  file << "created_date=" << metadata.created_date << "\n";
  file << "last_modified=" << metadata.last_modified << "\n";
  file << "yaze_version=" << metadata.yaze_version << "\n";
  file << "tags=" << absl::StrJoin(metadata.tags, ",") << "\n\n";
  
  // Files section
  file << "[files]\n";
  file << "rom_filename=" << GetRelativePath(rom_filename) << "\n";
  file << "rom_backup_folder=" << GetRelativePath(rom_backup_folder) << "\n";
  file << "code_folder=" << GetRelativePath(code_folder) << "\n";
  file << "assets_folder=" << GetRelativePath(assets_folder) << "\n";
  file << "patches_folder=" << GetRelativePath(patches_folder) << "\n";
  file << "labels_filename=" << GetRelativePath(labels_filename) << "\n";
  file << "symbols_filename=" << GetRelativePath(symbols_filename) << "\n";
  file << "output_folder=" << GetRelativePath(output_folder) << "\n";
  file << "additional_roms=" << absl::StrJoin(additional_roms, ",") << "\n\n";
  
  // Feature flags section
  file << "[feature_flags]\n";
  file << "load_custom_overworld=" << (feature_flags.overworld.kLoadCustomOverworld ? "true" : "false") << "\n";
  file << "apply_zs_custom_overworld_asm=" << (feature_flags.overworld.kApplyZSCustomOverworldASM ? "true" : "false") << "\n";
  file << "save_dungeon_maps=" << (feature_flags.kSaveDungeonMaps ? "true" : "false") << "\n";
  file << "save_graphics_sheet=" << (feature_flags.kSaveGraphicsSheet ? "true" : "false") << "\n";
  file << "log_instructions=" << (feature_flags.kLogInstructions ? "true" : "false") << "\n\n";
  
  // Workspace settings section
  file << "[workspace]\n";
  file << "font_global_scale=" << workspace_settings.font_global_scale << "\n";
  file << "dark_mode=" << (workspace_settings.dark_mode ? "true" : "false") << "\n";
  file << "ui_theme=" << workspace_settings.ui_theme << "\n";
  file << "autosave_enabled=" << (workspace_settings.autosave_enabled ? "true" : "false") << "\n";
  file << "autosave_interval_secs=" << workspace_settings.autosave_interval_secs << "\n";
  file << "backup_on_save=" << (workspace_settings.backup_on_save ? "true" : "false") << "\n";
  file << "show_grid=" << (workspace_settings.show_grid ? "true" : "false") << "\n";
  file << "show_collision=" << (workspace_settings.show_collision ? "true" : "false") << "\n";
  file << "last_layout_preset=" << workspace_settings.last_layout_preset << "\n";
  file << "saved_layouts=" << absl::StrJoin(workspace_settings.saved_layouts, ",") << "\n";
  file << "recent_files=" << absl::StrJoin(workspace_settings.recent_files, ",") << "\n\n";
  
  // Custom keybindings section
  if (!workspace_settings.custom_keybindings.empty()) {
    file << "[keybindings]\n";
    for (const auto& [key, value] : workspace_settings.custom_keybindings) {
      file << key << "=" << value << "\n";
    }
    file << "\n";
  }
  
  // Editor visibility section
  if (!workspace_settings.editor_visibility.empty()) {
    file << "[editor_visibility]\n";
    for (const auto& [key, value] : workspace_settings.editor_visibility) {
      file << key << "=" << (value ? "true" : "false") << "\n";
    }
    file << "\n";
  }
  
  // Resource labels sections
  for (const auto& [type, labels] : resource_labels) {
    if (!labels.empty()) {
      file << "[labels_" << type << "]\n";
      for (const auto& [key, value] : labels) {
        file << key << "=" << value << "\n";
      }
      file << "\n";
    }
  }
  
  // Build settings section
  file << "[build]\n";
  file << "build_script=" << build_script << "\n";
  file << "output_folder=" << GetRelativePath(output_folder) << "\n";
  file << "git_repository=" << git_repository << "\n";
  file << "track_changes=" << (track_changes ? "true" : "false") << "\n";
  file << "build_configurations=" << absl::StrJoin(build_configurations, ",") << "\n\n";
  
  // ZScream compatibility section
  if (!zscream_project_file.empty()) {
    file << "[zscream_compatibility]\n";
    file << "original_project_file=" << zscream_project_file << "\n";
    for (const auto& [key, value] : zscream_mappings) {
      file << key << "=" << value << "\n";
    }
    file << "\n";
  }
  
  file << "# End of YAZE Project File\n";
  file.close();

  return absl::OkStatus();
}

absl::Status YazeProject::ImportZScreamProject(const std::string& zscream_project_path) {
  // Basic ZScream project import (to be expanded based on ZScream format)
  zscream_project_file = zscream_project_path;
  format = ProjectFormat::kZScreamCompat;
  
  // Extract project name from path
  std::filesystem::path zs_path(zscream_project_path);
  name = zs_path.stem().string() + "_imported";
  
  // Set up basic mapping for common fields
  zscream_mappings["rom_file"] = "rom_filename";
  zscream_mappings["source_code"] = "code_folder";
  zscream_mappings["project_name"] = "name";
  
  InitializeDefaults();
  
  // TODO: Implement actual ZScream format parsing when format is known
  // For now, just create a project structure that can be manually configured

  return absl::OkStatus();
}

absl::Status YazeProject::ExportForZScream(const std::string& target_path) {
  // Create a simplified project file that ZScream might understand
  std::ofstream file(target_path);
  if (!file.is_open()) {
    return absl::InvalidArgumentError(absl::StrFormat("Cannot create ZScream project file: %s", target_path));
  }
  
  // Write in a simple format that ZScream might understand
  file << "# ZScream Compatible Project File\n";
  file << "# Exported from YAZE " << metadata.yaze_version << "\n\n";
  file << "name=" << name << "\n";
  file << "rom_file=" << rom_filename << "\n";
  file << "source_code=" << code_folder << "\n";
  file << "description=" << metadata.description << "\n";
  file << "author=" << metadata.author << "\n";
  file << "created_with=YAZE " << metadata.yaze_version << "\n";
  
  file.close();
  return absl::OkStatus();
}

absl::Status YazeProject::LoadAllSettings() {
  // Consolidated loading of all settings from project file
  // This replaces scattered config loading throughout the application
  return LoadFromYazeFormat(filepath);
}

absl::Status YazeProject::SaveAllSettings() {
  // Consolidated saving of all settings to project file
  return SaveToYazeFormat();
}

absl::Status YazeProject::ResetToDefaults() {
  InitializeDefaults();
  return Save();
}

absl::Status YazeProject::Validate() const {
  std::vector<std::string> errors;
  
  if (name.empty()) errors.push_back("Project name is required");
  if (filepath.empty()) errors.push_back("Project file path is required");
  if (rom_filename.empty()) errors.push_back("ROM file is required");
  
  // Check if files exist
  if (!rom_filename.empty() && !std::filesystem::exists(GetAbsolutePath(rom_filename))) {
    errors.push_back("ROM file does not exist: " + rom_filename);
  }
  
  if (!code_folder.empty() && !std::filesystem::exists(GetAbsolutePath(code_folder))) {
    errors.push_back("Code folder does not exist: " + code_folder);
  }
  
  if (!labels_filename.empty() && !std::filesystem::exists(GetAbsolutePath(labels_filename))) {
    errors.push_back("Labels file does not exist: " + labels_filename);
  }
  
  if (!errors.empty()) {
    return absl::InvalidArgumentError(absl::StrJoin(errors, "; "));
  }

  return absl::OkStatus();
}

std::vector<std::string> YazeProject::GetMissingFiles() const {
  std::vector<std::string> missing;
  
  if (!rom_filename.empty() && !std::filesystem::exists(GetAbsolutePath(rom_filename))) {
    missing.push_back(rom_filename);
  }
  if (!labels_filename.empty() && !std::filesystem::exists(GetAbsolutePath(labels_filename))) {
    missing.push_back(labels_filename);
  }
  if (!symbols_filename.empty() && !std::filesystem::exists(GetAbsolutePath(symbols_filename))) {
    missing.push_back(symbols_filename);
  }
  
  return missing;
}

absl::Status YazeProject::RepairProject() {
  // Create missing directories
  std::vector<std::string> folders = {code_folder, assets_folder, patches_folder, 
                                     rom_backup_folder, output_folder};
  
  for (const auto& folder : folders) {
    if (!folder.empty()) {
      std::filesystem::path abs_path = GetAbsolutePath(folder);
      if (!std::filesystem::exists(abs_path)) {
        std::filesystem::create_directories(abs_path);
      }
    }
  }
  
  // Create missing files with defaults
  if (!labels_filename.empty()) {
    std::filesystem::path abs_labels = GetAbsolutePath(labels_filename);
    if (!std::filesystem::exists(abs_labels)) {
      std::ofstream labels_file(abs_labels);
      labels_file << "# YAZE Resource Labels\n";
      labels_file << "# Format: [type] key=value\n\n";
      labels_file.close();
    }
  }
  
  return absl::OkStatus();
}

std::string YazeProject::GetDisplayName() const {
  if (!metadata.description.empty()) {
    return metadata.description;
  }
  return name.empty() ? "Untitled Project" : name;
}

std::string YazeProject::GetRelativePath(const std::string& absolute_path) const {
  if (absolute_path.empty() || filepath.empty()) return absolute_path;
  
  std::filesystem::path project_dir = std::filesystem::path(filepath).parent_path();
  std::filesystem::path abs_path(absolute_path);
  
  try {
    std::filesystem::path relative = std::filesystem::relative(abs_path, project_dir);
    return relative.string();
  } catch (...) {
    return absolute_path; // Return absolute path if relative conversion fails
  }
}

std::string YazeProject::GetAbsolutePath(const std::string& relative_path) const {
  if (relative_path.empty() || filepath.empty()) return relative_path;
  
  std::filesystem::path project_dir = std::filesystem::path(filepath).parent_path();
  std::filesystem::path abs_path = project_dir / relative_path;
  
  return abs_path.string();
}

bool YazeProject::IsEmpty() const {
  return name.empty() && rom_filename.empty() && code_folder.empty();
}

absl::Status YazeProject::ImportFromZScreamFormat(const std::string& project_path) {
  // TODO: Implement ZScream format parsing when format specification is available
  // For now, create a basic project that can be manually configured
  
  std::filesystem::path zs_path(project_path);
  name = zs_path.stem().string() + "_imported";
  zscream_project_file = project_path;
  
  InitializeDefaults();
  
  return absl::OkStatus();
}

void YazeProject::InitializeDefaults() {
  // Initialize default feature flags
  feature_flags.overworld.kLoadCustomOverworld = false;
  feature_flags.overworld.kApplyZSCustomOverworldASM = false;
  feature_flags.kSaveDungeonMaps = true;
  feature_flags.kSaveGraphicsSheet = true;
  feature_flags.kLogInstructions = false;
  
  // Initialize default workspace settings
  workspace_settings.font_global_scale = 1.0f;
  workspace_settings.dark_mode = true;
  workspace_settings.ui_theme = "default";
  workspace_settings.autosave_enabled = true;
  workspace_settings.autosave_interval_secs = 300.0f; // 5 minutes
  workspace_settings.backup_on_save = true;
  workspace_settings.show_grid = true;
  workspace_settings.show_collision = false;
  
  // Initialize default build configurations
  build_configurations = {"Debug", "Release", "Distribution"};
  
  track_changes = true;
}

std::string YazeProject::GenerateProjectId() const {
  auto now = std::chrono::system_clock::now().time_since_epoch();
  auto timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(now).count();
  return absl::StrFormat("yaze_project_%lld", timestamp);
}

// ProjectManager Implementation
std::vector<ProjectManager::ProjectTemplate> ProjectManager::GetProjectTemplates() {
  return {
    {
      "Basic ROM Hack",
      "Simple project for modifying an existing ROM with basic tools",
      ICON_MD_VIDEOGAME_ASSET,
      {} // Basic defaults
    },
    {
      "Full Overworld Mod",
      "Complete overworld modification with custom graphics and maps",
      ICON_MD_MAP,
      {} // Overworld-focused settings
    },
    {
      "Dungeon Designer",
      "Focused on dungeon creation and modification",
      ICON_MD_DOMAIN,
      {} // Dungeon-focused settings
    },
    {
      "Graphics Pack",
      "Project focused on graphics, sprites, and visual modifications",
      ICON_MD_PALETTE,
      {} // Graphics-focused settings
    },
    {
      "Complete Overhaul",
      "Full-scale ROM hack with all features enabled",
      ICON_MD_BUILD,
      {} // All features enabled
    }
  };
}

absl::StatusOr<YazeProject> ProjectManager::CreateFromTemplate(
    const std::string& template_name, 
    const std::string& project_name,
    const std::string& base_path) {
  
  YazeProject project;
  auto status = project.Create(project_name, base_path);
  if (!status.ok()) {
    return status;
  }
  
  // Customize based on template
  if (template_name == "Full Overworld Mod") {
    project.feature_flags.overworld.kLoadCustomOverworld = true;
    project.feature_flags.overworld.kApplyZSCustomOverworldASM = true;
    project.metadata.description = "Overworld modification project";
    project.metadata.tags = {"overworld", "maps", "graphics"};
  } else if (template_name == "Dungeon Designer") {
    project.feature_flags.kSaveDungeonMaps = true;
    project.workspace_settings.show_grid = true;
    project.metadata.description = "Dungeon design and modification project";
    project.metadata.tags = {"dungeons", "rooms", "design"};
  } else if (template_name == "Graphics Pack") {
    project.feature_flags.kSaveGraphicsSheet = true;
    project.workspace_settings.show_grid = true;
    project.metadata.description = "Graphics and sprite modification project";
    project.metadata.tags = {"graphics", "sprites", "palettes"};
  } else if (template_name == "Complete Overhaul") {
    project.feature_flags.overworld.kLoadCustomOverworld = true;
    project.feature_flags.overworld.kApplyZSCustomOverworldASM = true;
    project.feature_flags.kSaveDungeonMaps = true;
    project.feature_flags.kSaveGraphicsSheet = true;
    project.metadata.description = "Complete ROM overhaul project";
    project.metadata.tags = {"complete", "overhaul", "full-mod"};
  }
  
  status = project.Save();
  if (!status.ok()) {
    return status;
  }
  
  return project;
}

std::vector<std::string> ProjectManager::FindProjectsInDirectory(const std::string& directory) {
  std::vector<std::string> projects;
  
  try {
    for (const auto& entry : std::filesystem::directory_iterator(directory)) {
      if (entry.is_regular_file()) {
        std::string filename = entry.path().filename().string();
        if (filename.ends_with(".yaze") || filename.ends_with(".zsproj")) {
          projects.push_back(entry.path().string());
        }
      }
    }
  } catch (const std::filesystem::filesystem_error& e) {
    // Directory doesn't exist or can't be accessed
  }
  
  return projects;
}

absl::Status ProjectManager::BackupProject(const YazeProject& project) {
  if (project.filepath.empty()) {
    return absl::InvalidArgumentError("Project has no file path");
  }
  
  std::filesystem::path project_path(project.filepath);
  std::filesystem::path backup_dir = project_path.parent_path() / "backups";
  std::filesystem::create_directories(backup_dir);
  
  auto now = std::chrono::system_clock::now();
  auto time_t = std::chrono::system_clock::to_time_t(now);
  std::stringstream ss;
  ss << std::put_time(std::localtime(&time_t), "%Y%m%d_%H%M%S");
  
  std::string backup_filename = project.name + "_backup_" + ss.str() + ".yaze";
  std::filesystem::path backup_path = backup_dir / backup_filename;
  
  try {
    std::filesystem::copy_file(project.filepath, backup_path);
  } catch (const std::filesystem::filesystem_error& e) {
    return absl::InternalError(absl::StrFormat("Failed to backup project: %s", e.what()));
  }
  
  return absl::OkStatus();
}

absl::Status ProjectManager::ValidateProjectStructure(const YazeProject& project) {
  return project.Validate();
}

std::vector<std::string> ProjectManager::GetRecommendedFixesForProject(const YazeProject& project) {
  std::vector<std::string> recommendations;
  
  if (project.rom_filename.empty()) {
    recommendations.push_back("Add a ROM file to begin editing");
  }
  
  if (project.code_folder.empty()) {
    recommendations.push_back("Set up a code folder for assembly patches");
  }
  
  if (project.labels_filename.empty()) {
    recommendations.push_back("Create a labels file for better organization");
  }
  
  if (project.metadata.description.empty()) {
    recommendations.push_back("Add a project description for documentation");
  }
  
  if (project.git_repository.empty() && project.track_changes) {
    recommendations.push_back("Consider setting up version control for your project");
  }
  
  auto missing_files = project.GetMissingFiles();
  if (!missing_files.empty()) {
    recommendations.push_back("Some project files are missing - use Project > Repair to fix");
  }
  
  return recommendations;
}

// Compatibility implementations for ResourceLabelManager and related classes
bool ResourceLabelManager::LoadLabels(const std::string& filename) {
  filename_ = filename;
  std::ifstream file(filename);
  if (!file.is_open()) {
    labels_loaded_ = false;
    return false;
  }
  
  labels_.clear();
  std::string line;
  std::string current_type = "";
  
  while (std::getline(file, line)) {
    if (line.empty() || line[0] == '#') continue;
    
    // Check for type headers [type_name]
    if (line[0] == '[' && line.back() == ']') {
      current_type = line.substr(1, line.length() - 2);
      continue;
    }
    
    // Parse key=value pairs
    size_t eq_pos = line.find('=');
    if (eq_pos != std::string::npos && !current_type.empty()) {
      std::string key = line.substr(0, eq_pos);
      std::string value = line.substr(eq_pos + 1);
      labels_[current_type][key] = value;
    }
  }
  
  file.close();
  labels_loaded_ = true;
  return true;
}

bool ResourceLabelManager::SaveLabels() {
  if (filename_.empty()) return false;
  
  std::ofstream file(filename_);
  if (!file.is_open()) return false;
  
  file << "# YAZE Resource Labels\n";
  file << "# Format: [type] followed by key=value pairs\n\n";
  
  for (const auto& [type, type_labels] : labels_) {
    if (!type_labels.empty()) {
      file << "[" << type << "]\n";
      for (const auto& [key, value] : type_labels) {
        file << key << "=" << value << "\n";
      }
      file << "\n";
    }
  }
  
  file.close();
  return true;
}

void ResourceLabelManager::DisplayLabels(bool* p_open) {
  if (!p_open || !*p_open) return;
  
  // Basic implementation - can be enhanced later
  if (ImGui::Begin("Resource Labels", p_open)) {
    ImGui::Text("Resource Labels Manager");
    ImGui::Text("Labels loaded: %s", labels_loaded_ ? "Yes" : "No");
    ImGui::Text("Total types: %zu", labels_.size());
    
    for (const auto& [type, type_labels] : labels_) {
      if (ImGui::TreeNode(type.c_str())) {
        ImGui::Text("Labels: %zu", type_labels.size());
        for (const auto& [key, value] : type_labels) {
          ImGui::Text("%s = %s", key.c_str(), value.c_str());
        }
        ImGui::TreePop();
      }
    }
  }
  ImGui::End();
}

void ResourceLabelManager::EditLabel(const std::string& type, const std::string& key,
                                    const std::string& newValue) {
  labels_[type][key] = newValue;
}

void ResourceLabelManager::SelectableLabelWithNameEdit(bool selected, const std::string& type,
                                                      const std::string& key,
                                                      const std::string& defaultValue) {
  // Basic implementation
  if (ImGui::Selectable(absl::StrFormat("%s: %s", key.c_str(), GetLabel(type, key).c_str()).c_str(), selected)) {
    // Handle selection
  }
}

std::string ResourceLabelManager::GetLabel(const std::string& type, const std::string& key) {
  auto type_it = labels_.find(type);
  if (type_it == labels_.end()) return "";
  
  auto label_it = type_it->second.find(key);
  if (label_it == type_it->second.end()) return "";
  
  return label_it->second;
}

std::string ResourceLabelManager::CreateOrGetLabel(const std::string& type, const std::string& key,
                                                  const std::string& defaultValue) {
  auto existing = GetLabel(type, key);
  if (!existing.empty()) return existing;
  
  labels_[type][key] = defaultValue;
  return defaultValue;
}

} // namespace core
} // namespace yaze
