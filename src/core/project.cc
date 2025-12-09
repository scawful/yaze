#include "core/project.h"

#include <chrono>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <sstream>

#include "absl/strings/str_format.h"
#include "absl/strings/str_join.h"
#include "absl/strings/str_split.h"
#include "app/gui/core/icons.h"
#include "imgui/imgui.h"
#include "util/file_util.h"
#include "util/json.h"
#include "util/log.h"
#include "util/platform_paths.h"
#include "util/macro.h"
#include "yaze_config.h"
#include "zelda3/resource_labels.h"


#ifdef __EMSCRIPTEN__
#include "app/platform/wasm/wasm_storage.h"
#endif

// #ifdef YAZE_ENABLE_JSON_PROJECT_FORMAT
// #include "nlohmann/json.hpp"
// using json = nlohmann::json;
// #endif

namespace yaze {
namespace project {

namespace {
// Helper functions for parsing key-value pairs
std::pair<std::string, std::string> ParseKeyValue(const std::string& line) {
  size_t eq_pos = line.find('=');
  if (eq_pos == std::string::npos)
    return {"", ""};

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
  if (value.empty())
    return result;

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

std::string SanitizeStorageKey(absl::string_view input) {
  std::string key(input);
  for (char& c : key) {
    if (!std::isalnum(static_cast<unsigned char>(c))) {
      c = '_';
    }
  }
  if (key.empty()) {
    key = "project";
  }
  return key;
}
}  // namespace

// YazeProject Implementation
absl::Status YazeProject::Create(const std::string& project_name,
                                 const std::string& base_path) {
  name = project_name;
  filepath = base_path + "/" + project_name + ".yaze";

  // Initialize metadata
  auto now = std::chrono::system_clock::now();
  auto time_t = std::chrono::system_clock::to_time_t(now);
  std::stringstream ss;
  ss << std::put_time(std::localtime(&time_t), "%Y-%m-%d %H:%M:%S");

  metadata.created_date = ss.str();
  metadata.last_modified = ss.str();
  metadata.yaze_version = "0.3.2";  // TODO: Get from version header
  metadata.version = "2.0";
  metadata.created_by = "YAZE";
  metadata.project_id = GenerateProjectId();

  InitializeDefaults();

#ifndef __EMSCRIPTEN__
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
#else
  // WASM: keep paths relative; persistence handled by WasmStorage/IDBFS
  code_folder = "code";
  assets_folder = "assets";
  patches_folder = "patches";
  rom_backup_folder = "backups";
  output_folder = "output";
  labels_filename = "labels.txt";
  symbols_filename = "symbols.txt";
#endif

  return Save();
}

absl::Status YazeProject::Open(const std::string& project_path) {
  filepath = project_path;

#ifdef __EMSCRIPTEN__
  // Prefer persistent storage in WASM builds
  auto storage_key = MakeStorageKey("project");
  auto storage_or = platform::WasmStorage::LoadProject(storage_key);
  if (storage_or.ok()) {
    return ParseFromString(storage_or.value());
  }
#endif

  // Determine format and load accordingly
  if (project_path.ends_with(".yaze")) {
    format = ProjectFormat::kYazeNative;

    // Try to detect if it's JSON format by peeking at first character
    std::ifstream file(project_path);
    if (file.is_open()) {
      std::stringstream buffer;
      buffer << file.rdbuf();
      std::string content = buffer.str();

#ifdef YAZE_ENABLE_JSON_PROJECT_FORMAT
      if (!content.empty() && content.front() == '{') {
        LOG_DEBUG("Project", "Detected JSON format project file");
        return LoadFromJsonFormat(project_path);
      }
#endif

      return ParseFromString(content);
    }

    return absl::InvalidArgumentError(
        absl::StrFormat("Cannot open project file: %s", project_path));
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
    filepath = old_filepath;  // Restore on failure
  }

  return status;
}

std::string YazeProject::MakeStorageKey(absl::string_view suffix) const {
  std::string base;
  if (!metadata.project_id.empty()) {
    base = metadata.project_id;
  } else if (!name.empty()) {
    base = name;
  } else if (!filepath.empty()) {
    base = std::filesystem::path(filepath).stem().string();
  }
  base = SanitizeStorageKey(base);
  if (suffix.empty()) {
    return base;
  }
  return absl::StrFormat("%s_%s", base, suffix);
}

absl::StatusOr<std::string> YazeProject::SerializeToString() const {
  std::ostringstream file;

  // Write header comment
  file << "# yaze Project File\n";
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
  file << "created_by=" << metadata.created_by << "\n";
  file << "project_id=" << metadata.project_id << "\n";
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
  file << "custom_objects_folder=" << GetRelativePath(custom_objects_folder) << "\n";
  file << "additional_roms=" << absl::StrJoin(additional_roms, ",") << "\n\n";

  // Feature flags section
  file << "[feature_flags]\n";
  file << "load_custom_overworld="
       << (feature_flags.overworld.kLoadCustomOverworld ? "true" : "false")
       << "\n";
  file << "apply_zs_custom_overworld_asm="
       << (feature_flags.overworld.kApplyZSCustomOverworldASM ? "true"
                                                              : "false")
       << "\n";
  file << "save_dungeon_maps="
       << (feature_flags.kSaveDungeonMaps ? "true" : "false") << "\n";
  file << "save_graphics_sheet="
       << (feature_flags.kSaveGraphicsSheet ? "true" : "false") << "\n";
  file << "enable_custom_objects="
       << (feature_flags.kEnableCustomObjects ? "true" : "false") << "\n\n";

  // Workspace settings section
  file << "[workspace]\n";
  file << "font_global_scale=" << workspace_settings.font_global_scale << "\n";
  file << "dark_mode="
       << (workspace_settings.dark_mode ? "true" : "false") << "\n";
  file << "ui_theme=" << workspace_settings.ui_theme << "\n";
  file << "autosave_enabled="
       << (workspace_settings.autosave_enabled ? "true" : "false") << "\n";
  file << "autosave_interval_secs="
       << workspace_settings.autosave_interval_secs << "\n";
  file << "backup_on_save="
       << (workspace_settings.backup_on_save ? "true" : "false") << "\n";
  file << "show_grid="
       << (workspace_settings.show_grid ? "true" : "false") << "\n";
  file << "show_collision="
       << (workspace_settings.show_collision ? "true" : "false") << "\n";
  file << "prefer_hmagic_names="
       << (workspace_settings.prefer_hmagic_names ? "true" : "false") << "\n";
  file << "last_layout_preset=" << workspace_settings.last_layout_preset
       << "\n";
  file << "saved_layouts="
       << absl::StrJoin(workspace_settings.saved_layouts, ",") << "\n";
  file << "recent_files="
       << absl::StrJoin(workspace_settings.recent_files, ",") << "\n\n";

  // AI Agent settings section
  file << "[agent_settings]\n";
  file << "ai_provider=" << agent_settings.ai_provider << "\n";
  file << "ai_model=" << agent_settings.ai_model << "\n";
  file << "ollama_host=" << agent_settings.ollama_host << "\n";
  file << "gemini_api_key=" << agent_settings.gemini_api_key << "\n";
  file << "custom_system_prompt="
       << GetRelativePath(agent_settings.custom_system_prompt) << "\n";
  file << "use_custom_prompt="
       << (agent_settings.use_custom_prompt ? "true" : "false") << "\n";
  file << "show_reasoning="
       << (agent_settings.show_reasoning ? "true" : "false") << "\n";
  file << "verbose=" << (agent_settings.verbose ? "true" : "false") << "\n";
  file << "max_tool_iterations=" << agent_settings.max_tool_iterations << "\n";
  file << "max_retry_attempts=" << agent_settings.max_retry_attempts << "\n";
  file << "temperature=" << agent_settings.temperature << "\n";
  file << "top_p=" << agent_settings.top_p << "\n";
  file << "max_output_tokens=" << agent_settings.max_output_tokens << "\n";
  file << "stream_responses="
       << (agent_settings.stream_responses ? "true" : "false") << "\n";
  file << "favorite_models="
       << absl::StrJoin(agent_settings.favorite_models, ",") << "\n";
  file << "model_chain="
       << absl::StrJoin(agent_settings.model_chain, ",") << "\n";
  file << "chain_mode=" << agent_settings.chain_mode << "\n";
  file << "enable_tool_resources="
       << (agent_settings.enable_tool_resources ? "true" : "false") << "\n";
  file << "enable_tool_dungeon="
       << (agent_settings.enable_tool_dungeon ? "true" : "false") << "\n";
  file << "enable_tool_overworld="
       << (agent_settings.enable_tool_overworld ? "true" : "false") << "\n";
  file << "enable_tool_messages="
       << (agent_settings.enable_tool_messages ? "true" : "false") << "\n";
  file << "enable_tool_dialogue="
       << (agent_settings.enable_tool_dialogue ? "true" : "false") << "\n";
  file << "enable_tool_gui="
       << (agent_settings.enable_tool_gui ? "true" : "false") << "\n";
  file << "enable_tool_music="
       << (agent_settings.enable_tool_music ? "true" : "false") << "\n";
  file << "enable_tool_sprite="
       << (agent_settings.enable_tool_sprite ? "true" : "false") << "\n";
  file << "enable_tool_emulator="
       << (agent_settings.enable_tool_emulator ? "true" : "false") << "\n";
  file << "builder_blueprint_path=" << agent_settings.builder_blueprint_path
       << "\n\n";

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
  file << "build_configurations="
       << absl::StrJoin(build_configurations, ",") << "\n";
  file << "build_target=" << build_target << "\n";
  file << "asm_entry_point=" << asm_entry_point << "\n";
  file << "asm_sources=" << absl::StrJoin(asm_sources, ",") << "\n";
  file << "last_build_hash=" << last_build_hash << "\n";
  file << "build_number=" << build_number << "\n\n";

  // Music persistence section (for WASM/offline state)
  file << "[music]\n";
  file << "persist_custom_music="
       << (music_persistence.persist_custom_music ? "true" : "false") << "\n";
  file << "storage_key=" << music_persistence.storage_key << "\n";
  file << "last_saved_at=" << music_persistence.last_saved_at << "\n\n";

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
  return file.str();
}

absl::Status YazeProject::ParseFromString(const std::string& content) {
  std::istringstream stream(content);
  std::string line;
  std::string current_section;

  while (std::getline(stream, line)) {
    if (line.empty() || line[0] == '#')
      continue;

    if (line.front() == '[' && line.back() == ']') {
      current_section = line.substr(1, line.length() - 2);
      continue;
    }

    auto [key, value] = ParseKeyValue(line);
    if (key.empty())
      continue;

    if (current_section == "project") {
      if (key == "name")
        name = value;
      else if (key == "description")
        metadata.description = value;
      else if (key == "author")
        metadata.author = value;
      else if (key == "license")
        metadata.license = value;
      else if (key == "version")
        metadata.version = value;
      else if (key == "created_date")
        metadata.created_date = value;
      else if (key == "last_modified")
        metadata.last_modified = value;
      else if (key == "yaze_version")
        metadata.yaze_version = value;
      else if (key == "created_by")
        metadata.created_by = value;
      else if (key == "tags")
        metadata.tags = ParseStringList(value);
      else if (key == "project_id")
        metadata.project_id = value;
    } else if (current_section == "files") {
      if (key == "rom_filename")
        rom_filename = value;
      else if (key == "rom_backup_folder")
        rom_backup_folder = value;
      else if (key == "code_folder")
        code_folder = value;
      else if (key == "assets_folder")
        assets_folder = value;
      else if (key == "patches_folder")
        patches_folder = value;
      else if (key == "labels_filename")
        labels_filename = value;
      else if (key == "symbols_filename")
        symbols_filename = value;
      else if (key == "output_folder")
        output_folder = value;
      else if (key == "custom_objects_folder")
        custom_objects_folder = value;
      else if (key == "additional_roms")
        additional_roms = ParseStringList(value);
    } else if (current_section == "feature_flags") {
      if (key == "load_custom_overworld")
        feature_flags.overworld.kLoadCustomOverworld = ParseBool(value);
      else if (key == "apply_zs_custom_overworld_asm")
        feature_flags.overworld.kApplyZSCustomOverworldASM = ParseBool(value);
      else if (key == "save_dungeon_maps")
        feature_flags.kSaveDungeonMaps = ParseBool(value);
      else if (key == "save_graphics_sheet")
        feature_flags.kSaveGraphicsSheet = ParseBool(value);
      else if (key == "enable_custom_objects")
        feature_flags.kEnableCustomObjects = ParseBool(value);
    } else if (current_section == "workspace") {
      if (key == "font_global_scale")
        workspace_settings.font_global_scale = ParseFloat(value);
      else if (key == "dark_mode")
        workspace_settings.dark_mode = ParseBool(value);
      else if (key == "ui_theme")
        workspace_settings.ui_theme = value;
      else if (key == "autosave_enabled")
        workspace_settings.autosave_enabled = ParseBool(value);
      else if (key == "autosave_interval_secs")
        workspace_settings.autosave_interval_secs = ParseFloat(value);
      else if (key == "backup_on_save")
        workspace_settings.backup_on_save = ParseBool(value);
      else if (key == "show_grid")
        workspace_settings.show_grid = ParseBool(value);
      else if (key == "show_collision")
        workspace_settings.show_collision = ParseBool(value);
      else if (key == "prefer_hmagic_names")
        workspace_settings.prefer_hmagic_names = ParseBool(value);
      else if (key == "last_layout_preset")
        workspace_settings.last_layout_preset = value;
      else if (key == "saved_layouts")
        workspace_settings.saved_layouts = ParseStringList(value);
      else if (key == "recent_files")
        workspace_settings.recent_files = ParseStringList(value);
    } else if (current_section == "agent_settings") {
      if (key == "ai_provider")
        agent_settings.ai_provider = value;
      else if (key == "ai_model")
        agent_settings.ai_model = value;
      else if (key == "ollama_host")
        agent_settings.ollama_host = value;
      else if (key == "gemini_api_key")
        agent_settings.gemini_api_key = value;
      else if (key == "custom_system_prompt")
        agent_settings.custom_system_prompt = value;
      else if (key == "use_custom_prompt")
        agent_settings.use_custom_prompt = ParseBool(value);
      else if (key == "show_reasoning")
        agent_settings.show_reasoning = ParseBool(value);
      else if (key == "verbose")
        agent_settings.verbose = ParseBool(value);
      else if (key == "max_tool_iterations")
        agent_settings.max_tool_iterations = std::stoi(value);
      else if (key == "max_retry_attempts")
        agent_settings.max_retry_attempts = std::stoi(value);
      else if (key == "temperature")
        agent_settings.temperature = ParseFloat(value);
      else if (key == "top_p")
        agent_settings.top_p = ParseFloat(value);
      else if (key == "max_output_tokens")
        agent_settings.max_output_tokens = std::stoi(value);
      else if (key == "stream_responses")
        agent_settings.stream_responses = ParseBool(value);
      else if (key == "favorite_models")
        agent_settings.favorite_models = ParseStringList(value);
      else if (key == "model_chain")
        agent_settings.model_chain = ParseStringList(value);
      else if (key == "chain_mode")
        agent_settings.chain_mode = std::stoi(value);
      else if (key == "enable_tool_resources")
        agent_settings.enable_tool_resources = ParseBool(value);
      else if (key == "enable_tool_dungeon")
        agent_settings.enable_tool_dungeon = ParseBool(value);
      else if (key == "enable_tool_overworld")
        agent_settings.enable_tool_overworld = ParseBool(value);
      else if (key == "enable_tool_messages")
        agent_settings.enable_tool_messages = ParseBool(value);
      else if (key == "enable_tool_dialogue")
        agent_settings.enable_tool_dialogue = ParseBool(value);
      else if (key == "enable_tool_gui")
        agent_settings.enable_tool_gui = ParseBool(value);
      else if (key == "enable_tool_music")
        agent_settings.enable_tool_music = ParseBool(value);
      else if (key == "enable_tool_sprite")
        agent_settings.enable_tool_sprite = ParseBool(value);
      else if (key == "enable_tool_emulator")
        agent_settings.enable_tool_emulator = ParseBool(value);
      else if (key == "builder_blueprint_path")
        agent_settings.builder_blueprint_path = value;
    } else if (current_section == "build") {
      if (key == "build_script")
        build_script = value;
      else if (key == "output_folder")
        output_folder = value;
      else if (key == "git_repository")
        git_repository = value;
      else if (key == "track_changes")
        track_changes = ParseBool(value);
      else if (key == "build_configurations")
        build_configurations = ParseStringList(value);
      else if (key == "build_target")
        build_target = value;
      else if (key == "asm_entry_point")
        asm_entry_point = value;
      else if (key == "asm_sources")
        asm_sources = ParseStringList(value);
      else if (key == "last_build_hash")
        last_build_hash = value;
      else if (key == "build_number")
        build_number = std::stoi(value);
    } else if (current_section.rfind("labels_", 0) == 0) {
      std::string label_type = current_section.substr(7);
      resource_labels[label_type][key] = value;
    } else if (current_section == "keybindings") {
      workspace_settings.custom_keybindings[key] = value;
    } else if (current_section == "editor_visibility") {
      workspace_settings.editor_visibility[key] = ParseBool(value);
    } else if (current_section == "zscream_compatibility") {
      if (key == "original_project_file")
        zscream_project_file = value;
      else
        zscream_mappings[key] = value;
    } else if (current_section == "music") {
      if (key == "persist_custom_music")
        music_persistence.persist_custom_music = ParseBool(value);
      else if (key == "storage_key")
        music_persistence.storage_key = value;
      else if (key == "last_saved_at")
        music_persistence.last_saved_at = value;
    }
  }

  if (metadata.project_id.empty()) {
    metadata.project_id = GenerateProjectId();
  }
  if (metadata.created_by.empty()) {
    metadata.created_by = "YAZE";
  }
  if (music_persistence.storage_key.empty()) {
    music_persistence.storage_key = MakeStorageKey("music");
  }

  return absl::OkStatus();
}

absl::Status YazeProject::LoadFromYazeFormat(const std::string& project_path) {
#ifdef __EMSCRIPTEN__
  auto storage_key = MakeStorageKey("project");
  auto storage_or = platform::WasmStorage::LoadProject(storage_key);
  if (storage_or.ok()) {
    return ParseFromString(storage_or.value());
  }
#endif  // __EMSCRIPTEN__

  std::ifstream file(project_path);
  if (!file.is_open()) {
    return absl::InvalidArgumentError(
        absl::StrFormat("Cannot open project file: %s", project_path));
  }

  std::stringstream buffer;
  buffer << file.rdbuf();
  file.close();
  return ParseFromString(buffer.str());
}

absl::Status YazeProject::SaveToYazeFormat() {
  // Update last modified timestamp
  auto now = std::chrono::system_clock::now();
  auto time_t = std::chrono::system_clock::to_time_t(now);
  std::stringstream ss;
  ss << std::put_time(std::localtime(&time_t), "%Y-%m-%d %H:%M:%S");
  metadata.last_modified = ss.str();
  if (music_persistence.storage_key.empty()) {
    music_persistence.storage_key = MakeStorageKey("music");
  }

  ASSIGN_OR_RETURN(auto serialized, SerializeToString());

#ifdef __EMSCRIPTEN__
  auto storage_status =
      platform::WasmStorage::SaveProject(MakeStorageKey("project"), serialized);
  if (!storage_status.ok()) {
    return storage_status;
  }
#else
  if (!filepath.empty()) {
    std::ofstream file(filepath);
    if (!file.is_open()) {
      return absl::InvalidArgumentError(
          absl::StrFormat("Cannot create project file: %s", filepath));
    }
    file << serialized;
    file.close();
  }
#endif

  return absl::OkStatus();
}

absl::Status YazeProject::ImportZScreamProject(
    const std::string& zscream_project_path) {
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
    return absl::InvalidArgumentError(
        absl::StrFormat("Cannot create ZScream project file: %s", target_path));
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

  if (name.empty())
    errors.push_back("Project name is required");
  if (filepath.empty())
    errors.push_back("Project file path is required");
  if (rom_filename.empty())
    errors.push_back("ROM file is required");

#ifndef __EMSCRIPTEN__
  // Check if files exist
  if (!rom_filename.empty() &&
      !std::filesystem::exists(GetAbsolutePath(rom_filename))) {
    errors.push_back("ROM file does not exist: " + rom_filename);
  }

  if (!code_folder.empty() &&
      !std::filesystem::exists(GetAbsolutePath(code_folder))) {
    errors.push_back("Code folder does not exist: " + code_folder);
  }

  if (!labels_filename.empty() &&
      !std::filesystem::exists(GetAbsolutePath(labels_filename))) {
    errors.push_back("Labels file does not exist: " + labels_filename);
  }
#endif  // __EMSCRIPTEN__

  if (!errors.empty()) {
    return absl::InvalidArgumentError(absl::StrJoin(errors, "; "));
  }

  return absl::OkStatus();
}

std::vector<std::string> YazeProject::GetMissingFiles() const {
  std::vector<std::string> missing;

#ifndef __EMSCRIPTEN__
  if (!rom_filename.empty() &&
      !std::filesystem::exists(GetAbsolutePath(rom_filename))) {
    missing.push_back(rom_filename);
  }
  if (!labels_filename.empty() &&
      !std::filesystem::exists(GetAbsolutePath(labels_filename))) {
    missing.push_back(labels_filename);
  }
  if (!symbols_filename.empty() &&
      !std::filesystem::exists(GetAbsolutePath(symbols_filename))) {
    missing.push_back(symbols_filename);
  }
#endif  // __EMSCRIPTEN__

  return missing;
}

absl::Status YazeProject::RepairProject() {
#ifdef __EMSCRIPTEN__
  // In the web build, filesystem layout is virtual; nothing to repair eagerly.
  return absl::OkStatus();
#else
  // Create missing directories
  std::vector<std::string> folders = {code_folder, assets_folder,
                                      patches_folder, rom_backup_folder,
                                      output_folder};

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
      labels_file << "# yaze Resource Labels\n";
      labels_file << "# Format: [type] key=value\n\n";
      labels_file.close();
    }
  }

  return absl::OkStatus();
#endif
}

std::string YazeProject::GetDisplayName() const {
  if (!metadata.description.empty()) {
    return metadata.description;
  }
  return name.empty() ? "Untitled Project" : name;
}

std::string YazeProject::GetRelativePath(
    const std::string& absolute_path) const {
  if (absolute_path.empty() || filepath.empty())
    return absolute_path;

  std::filesystem::path project_dir =
      std::filesystem::path(filepath).parent_path();
  std::filesystem::path abs_path(absolute_path);

  try {
    std::filesystem::path relative =
        std::filesystem::relative(abs_path, project_dir);
    return relative.string();
  } catch (...) {
    return absolute_path;  // Return absolute path if relative conversion fails
  }
}

std::string YazeProject::GetAbsolutePath(
    const std::string& relative_path) const {
  if (relative_path.empty() || filepath.empty())
    return relative_path;

  std::filesystem::path project_dir =
      std::filesystem::path(filepath).parent_path();
  std::filesystem::path abs_path = project_dir / relative_path;

  return abs_path.string();
}

bool YazeProject::IsEmpty() const {
  return name.empty() && rom_filename.empty() && code_folder.empty();
}

absl::Status YazeProject::ImportFromZScreamFormat(
    const std::string& project_path) {
  // TODO: Implement ZScream format parsing when format specification is
  // available For now, create a basic project that can be manually configured

  std::filesystem::path zs_path(project_path);
  name = zs_path.stem().string() + "_imported";
  zscream_project_file = project_path;

  InitializeDefaults();

  return absl::OkStatus();
}

void YazeProject::InitializeDefaults() {
  if (metadata.project_id.empty()) {
    metadata.project_id = GenerateProjectId();
  }

  // Initialize default feature flags
  feature_flags.overworld.kLoadCustomOverworld = false;
  feature_flags.overworld.kApplyZSCustomOverworldASM = false;
  feature_flags.kSaveDungeonMaps = true;
  feature_flags.kSaveGraphicsSheet = true;
  // REMOVED: kLogInstructions (deprecated)

  // Initialize default workspace settings
  workspace_settings.font_global_scale = 1.0f;
  workspace_settings.dark_mode = true;
  workspace_settings.ui_theme = "default";
  workspace_settings.autosave_enabled = true;
  workspace_settings.autosave_interval_secs = 300.0f;  // 5 minutes
  workspace_settings.backup_on_save = true;
  workspace_settings.show_grid = true;
  workspace_settings.show_collision = false;

  // Initialize default build configurations
  build_configurations = {"Debug", "Release", "Distribution"};
  build_target.clear();
  asm_entry_point = "asm/main.asm";
  asm_sources = {"asm"};
  last_build_hash.clear();
  build_number = 0;

  track_changes = true;

  music_persistence.persist_custom_music = true;
  music_persistence.storage_key = MakeStorageKey("music");
  music_persistence.last_saved_at.clear();

  if (metadata.created_by.empty()) {
    metadata.created_by = "YAZE";
  }
}

std::string YazeProject::GenerateProjectId() const {
  auto now = std::chrono::system_clock::now().time_since_epoch();
  auto timestamp =
      std::chrono::duration_cast<std::chrono::milliseconds>(now).count();
  return absl::StrFormat("yaze_project_%lld", timestamp);
}

// ProjectManager Implementation
std::vector<ProjectManager::ProjectTemplate>
ProjectManager::GetProjectTemplates() {
  std::vector<ProjectTemplate> templates;
  
  // ==========================================================================
  // ZSCustomOverworld Templates (Recommended)
  // ==========================================================================
  
  // Vanilla ROM Hack - no ZSO
  {
    ProjectTemplate t;
    t.name = "Vanilla ROM Hack";
    t.description = "Standard ROM editing without custom ASM. Limited to vanilla features.";
    t.icon = ICON_MD_VIDEOGAME_ASSET;
    t.template_project.feature_flags.overworld.kLoadCustomOverworld = false;
    t.template_project.feature_flags.overworld.kApplyZSCustomOverworldASM = false;
    t.template_project.feature_flags.overworld.kSaveOverworldMaps = true;
    t.template_project.feature_flags.overworld.kSaveOverworldEntrances = true;
    t.template_project.feature_flags.overworld.kSaveOverworldExits = true;
    t.template_project.feature_flags.overworld.kSaveOverworldItems = true;
    templates.push_back(t);
  }
  
  // ZSCustomOverworld v2 - Basic expansion
  {
    ProjectTemplate t;
    t.name = "ZSCustomOverworld v2";
    t.description = "Basic overworld expansion: custom BG colors, main palettes, parent system.";
    t.icon = ICON_MD_MAP;
    t.template_project.feature_flags.overworld.kLoadCustomOverworld = true;
    t.template_project.feature_flags.overworld.kApplyZSCustomOverworldASM = true;
    t.template_project.feature_flags.overworld.kSaveOverworldMaps = true;
    t.template_project.feature_flags.overworld.kSaveOverworldEntrances = true;
    t.template_project.feature_flags.overworld.kSaveOverworldExits = true;
    t.template_project.feature_flags.overworld.kSaveOverworldItems = true;
    t.template_project.feature_flags.kSaveAllPalettes = true;
    t.template_project.feature_flags.kSaveGfxGroups = true;
    t.template_project.metadata.tags = {"zso_v2", "overworld", "expansion"};
    templates.push_back(t);
  }
  
  // ZSCustomOverworld v3 - Full features (Recommended)
  {
    ProjectTemplate t;
    t.name = "ZSCustomOverworld v3 (Recommended)";
    t.description = "Full overworld expansion: wide/tall areas, animated GFX, overlays, all features.";
    t.icon = ICON_MD_TERRAIN;
    t.template_project.feature_flags.overworld.kLoadCustomOverworld = true;
    t.template_project.feature_flags.overworld.kApplyZSCustomOverworldASM = true;
    t.template_project.feature_flags.overworld.kSaveOverworldMaps = true;
    t.template_project.feature_flags.overworld.kSaveOverworldEntrances = true;
    t.template_project.feature_flags.overworld.kSaveOverworldExits = true;
    t.template_project.feature_flags.overworld.kSaveOverworldItems = true;
    t.template_project.feature_flags.overworld.kSaveOverworldProperties = true;
    t.template_project.feature_flags.kSaveAllPalettes = true;
    t.template_project.feature_flags.kSaveGfxGroups = true;
    t.template_project.feature_flags.kSaveDungeonMaps = true;
    t.template_project.feature_flags.kSaveGraphicsSheet = true;
    t.template_project.metadata.tags = {"zso_v3", "overworld", "full", "recommended"};
    templates.push_back(t);
  }
  
  // Randomizer Compatible
  {
    ProjectTemplate t;
    t.name = "Randomizer Compatible";
    t.description = "Compatible with ALttP Randomizer. Minimal custom features to avoid conflicts.";
    t.icon = ICON_MD_SHUFFLE;
    t.template_project.feature_flags.overworld.kLoadCustomOverworld = false;
    t.template_project.feature_flags.overworld.kApplyZSCustomOverworldASM = false;
    t.template_project.feature_flags.overworld.kSaveOverworldMaps = false;
    t.template_project.feature_flags.kSaveDungeonMaps = false;
    t.template_project.metadata.tags = {"randomizer", "compatible", "minimal"};
    templates.push_back(t);
  }
  
  // ==========================================================================
  // Editor-Focused Templates
  // ==========================================================================
  
  // Dungeon Designer
  {
    ProjectTemplate t;
    t.name = "Dungeon Designer";
    t.description = "Focused on dungeon creation and modification.";
    t.icon = ICON_MD_DOMAIN;
    t.template_project.feature_flags.kSaveDungeonMaps = true;
    t.template_project.workspace_settings.show_grid = true;
    t.template_project.workspace_settings.last_layout_preset = "dungeon_default";
    t.template_project.metadata.tags = {"dungeons", "rooms", "design"};
    templates.push_back(t);
  }
  
  // Graphics Pack
  {
    ProjectTemplate t;
    t.name = "Graphics Pack";
    t.description = "Project focused on graphics, sprites, and visual modifications.";
    t.icon = ICON_MD_PALETTE;
    t.template_project.feature_flags.kSaveGraphicsSheet = true;
    t.template_project.feature_flags.kSaveAllPalettes = true;
    t.template_project.feature_flags.kSaveGfxGroups = true;
    t.template_project.workspace_settings.show_grid = true;
    t.template_project.workspace_settings.last_layout_preset = "graphics_default";
    t.template_project.metadata.tags = {"graphics", "sprites", "palettes"};
    templates.push_back(t);
  }
  
  // Complete Overhaul
  {
    ProjectTemplate t;
    t.name = "Complete Overhaul";
    t.description = "Full-scale ROM hack with all features enabled.";
    t.icon = ICON_MD_BUILD;
    t.template_project.feature_flags.overworld.kLoadCustomOverworld = true;
    t.template_project.feature_flags.overworld.kApplyZSCustomOverworldASM = true;
    t.template_project.feature_flags.overworld.kSaveOverworldMaps = true;
    t.template_project.feature_flags.overworld.kSaveOverworldEntrances = true;
    t.template_project.feature_flags.overworld.kSaveOverworldExits = true;
    t.template_project.feature_flags.overworld.kSaveOverworldItems = true;
    t.template_project.feature_flags.overworld.kSaveOverworldProperties = true;
    t.template_project.feature_flags.kSaveDungeonMaps = true;
    t.template_project.feature_flags.kSaveGraphicsSheet = true;
    t.template_project.feature_flags.kSaveAllPalettes = true;
    t.template_project.feature_flags.kSaveGfxGroups = true;
    t.template_project.metadata.tags = {"complete", "overhaul", "full-mod"};
    templates.push_back(t);
  }
  
  return templates;
}

absl::StatusOr<YazeProject> ProjectManager::CreateFromTemplate(
    const std::string& template_name, const std::string& project_name,
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

std::vector<std::string> ProjectManager::FindProjectsInDirectory(
    const std::string& directory) {
#ifdef __EMSCRIPTEN__
  (void)directory;
  return {};
#else
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
#endif  // __EMSCRIPTEN__
}

absl::Status ProjectManager::BackupProject(const YazeProject& project) {
#ifdef __EMSCRIPTEN__
  (void)project;
  return absl::UnimplementedError(
      "Project backups are not supported in the web build");
#else
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
    return absl::InternalError(
        absl::StrFormat("Failed to backup project: %s", e.what()));
  }

  return absl::OkStatus();
#endif
}

absl::Status ProjectManager::ValidateProjectStructure(
    const YazeProject& project) {
  return project.Validate();
}

std::vector<std::string> ProjectManager::GetRecommendedFixesForProject(
    const YazeProject& project) {
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
    recommendations.push_back(
        "Consider setting up version control for your project");
  }

  auto missing_files = project.GetMissingFiles();
  if (!missing_files.empty()) {
    recommendations.push_back(
        "Some project files are missing - use Project > Repair to fix");
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
    if (line.empty() || line[0] == '#')
      continue;

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
  if (filename_.empty())
    return false;

  std::ofstream file(filename_);
  if (!file.is_open())
    return false;

  file << "# yaze Resource Labels\n";
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
  if (!p_open || !*p_open)
    return;

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

void ResourceLabelManager::EditLabel(const std::string& type,
                                     const std::string& key,
                                     const std::string& newValue) {
  labels_[type][key] = newValue;
}

void ResourceLabelManager::SelectableLabelWithNameEdit(
    bool selected, const std::string& type, const std::string& key,
    const std::string& defaultValue) {
  // Basic implementation
  if (ImGui::Selectable(
          absl::StrFormat("%s: %s", key.c_str(), GetLabel(type, key).c_str())
              .c_str(),
          selected)) {
    // Handle selection
  }
}

std::string ResourceLabelManager::GetLabel(const std::string& type,
                                           const std::string& key) {
  auto type_it = labels_.find(type);
  if (type_it == labels_.end())
    return "";

  auto label_it = type_it->second.find(key);
  if (label_it == type_it->second.end())
    return "";

  return label_it->second;
}

std::string ResourceLabelManager::CreateOrGetLabel(
    const std::string& type, const std::string& key,
    const std::string& defaultValue) {
  auto existing = GetLabel(type, key);
  if (!existing.empty())
    return existing;

  labels_[type][key] = defaultValue;
  return defaultValue;
}

// ============================================================================
// Embedded Labels Support
// ============================================================================

absl::Status YazeProject::InitializeEmbeddedLabels(
    const std::unordered_map<std::string,
                             std::unordered_map<std::string, std::string>>&
        labels) {
  try {
    // Load all default Zelda3 resource names into resource_labels
    // We merge them with existing labels, prioritizing existing overrides?
    // Or just overwrite? The previous code was:
    // resource_labels = zelda3::Zelda3Labels::ToResourceLabels();
    // which implies overwriting. But we want to keep overrides if possible.
    // However, this is usually called on load.

    // Let's overwrite for now to match previous behavior, assuming overrides
    // are loaded afterwards or this is initial setup.
    // Actually, if we load project then init embedded labels, we might lose overrides.
    // But typically overrides are loaded from the project file *into* resource_labels.
    // If we call this, we might clobber them.
    // The previous implementation clobbered resource_labels.

    // However, if we want to support overrides + embedded, we should merge.
    // But `resource_labels` was treated as "overrides" in the old code?
    // No, `resource_labels` was the container for loaded labels.

    // If I look at `LoadFromYazeFormat`:
    // It parses `[labels_type]` into `resource_labels`.

    // If `use_embedded_labels` is true, `InitializeEmbeddedLabels` is called?
    // I need to check when `InitializeEmbeddedLabels` is called.

    resource_labels = labels;
    use_embedded_labels = true;

    LOG_DEBUG("Project", "Initialized embedded labels:");
    LOG_DEBUG("Project", "   - %d room names", resource_labels["room"].size());
    LOG_DEBUG("Project", "   - %d entrance names",
              resource_labels["entrance"].size());
    LOG_DEBUG("Project", "   - %d sprite names",
              resource_labels["sprite"].size());
    LOG_DEBUG("Project", "   - %d overlord names",
              resource_labels["overlord"].size());
    LOG_DEBUG("Project", "   - %d item names", resource_labels["item"].size());
    LOG_DEBUG("Project", "   - %d music names",
              resource_labels["music"].size());
    LOG_DEBUG("Project", "   - %d graphics names",
              resource_labels["graphics"].size());
    LOG_DEBUG("Project", "   - %d room effect names",
              resource_labels["room_effect"].size());
    LOG_DEBUG("Project", "   - %d room tag names",
              resource_labels["room_tag"].size());
    LOG_DEBUG("Project", "   - %d tile type names",
              resource_labels["tile_type"].size());

    return absl::OkStatus();
  } catch (const std::exception& e) {
    return absl::InternalError(
        absl::StrCat("Failed to initialize embedded labels: ", e.what()));
  }
}

std::string YazeProject::GetLabel(const std::string& resource_type, int id,
                                  const std::string& default_value) const {
  // First check if we have a custom label override
  auto type_it = resource_labels.find(resource_type);
  if (type_it != resource_labels.end()) {
    auto label_it = type_it->second.find(std::to_string(id));
    if (label_it != type_it->second.end()) {
      return label_it->second;
    }
  }

  return default_value.empty() ? resource_type + "_" + std::to_string(id)
                               : default_value;
}

absl::Status YazeProject::ImportLabelsFromZScream(const std::string& filepath) {
#ifdef __EMSCRIPTEN__
  (void)filepath;
  return absl::UnimplementedError(
      "File-based label import is not supported in the web build");
#else
  std::ifstream file(filepath);
  if (!file.is_open()) {
    return absl::InvalidArgumentError(
        absl::StrFormat("Cannot open labels file: %s", filepath));
  }

  std::stringstream buffer;
  buffer << file.rdbuf();
  file.close();

  return ImportLabelsFromZScreamContent(buffer.str());
#endif
}

absl::Status YazeProject::ImportLabelsFromZScreamContent(
    const std::string& content) {
  // Initialize the global provider with our labels
  auto& provider = zelda3::GetResourceLabels();
  provider.SetProjectLabels(&resource_labels);
  provider.SetPreferHMagicNames(workspace_settings.prefer_hmagic_names);

  // Use the provider to parse ZScream format
  auto status = provider.ImportFromZScreamFormat(content);
  if (!status.ok()) {
    return status;
  }

  LOG_DEBUG("Project", "Imported ZScream labels:");
  LOG_DEBUG("Project", "   - %d sprite labels", resource_labels["sprite"].size());
  LOG_DEBUG("Project", "   - %d room labels", resource_labels["room"].size());
  LOG_DEBUG("Project", "   - %d item labels", resource_labels["item"].size());
  LOG_DEBUG("Project", "   - %d room tag labels",
            resource_labels["room_tag"].size());

  return absl::OkStatus();
}

void YazeProject::InitializeResourceLabelProvider() {
  auto& provider = zelda3::GetResourceLabels();
  provider.SetProjectLabels(&resource_labels);
  provider.SetPreferHMagicNames(workspace_settings.prefer_hmagic_names);

  LOG_DEBUG("Project",
            "Initialized ResourceLabelProvider with project labels");
  LOG_DEBUG("Project", "   - prefer_hmagic_names: %s",
            workspace_settings.prefer_hmagic_names ? "true" : "false");
}

// ============================================================================
// JSON Format Support (Optional)
// ============================================================================

#ifdef YAZE_ENABLE_JSON_PROJECT_FORMAT

absl::Status YazeProject::LoadFromJsonFormat(const std::string& project_path) {
#ifdef __EMSCRIPTEN__
  return absl::UnimplementedError(
      "JSON project format loading is not supported in the web build");
#endif
  std::ifstream file(project_path);
  if (!file.is_open()) {
    return absl::InvalidArgumentError(
        absl::StrFormat("Cannot open JSON project file: %s", project_path));
  }

  try {
    json j;
    file >> j;

    // Parse project metadata
    if (j.contains("yaze_project")) {
      auto& proj = j["yaze_project"];

      if (proj.contains("name"))
        name = proj["name"].get<std::string>();
      if (proj.contains("description"))
        metadata.description = proj["description"].get<std::string>();
      if (proj.contains("author"))
        metadata.author = proj["author"].get<std::string>();
      if (proj.contains("version"))
        metadata.version = proj["version"].get<std::string>();
      if (proj.contains("created"))
        metadata.created_date = proj["created"].get<std::string>();
      if (proj.contains("modified"))
        metadata.last_modified = proj["modified"].get<std::string>();
      if (proj.contains("created_by"))
        metadata.created_by = proj["created_by"].get<std::string>();

      // Files
      if (proj.contains("rom_filename"))
        rom_filename = proj["rom_filename"].get<std::string>();
      if (proj.contains("code_folder"))
        code_folder = proj["code_folder"].get<std::string>();
      if (proj.contains("assets_folder"))
        assets_folder = proj["assets_folder"].get<std::string>();
      if (proj.contains("patches_folder"))
        patches_folder = proj["patches_folder"].get<std::string>();
      if (proj.contains("labels_filename"))
        labels_filename = proj["labels_filename"].get<std::string>();
      if (proj.contains("symbols_filename"))
        symbols_filename = proj["symbols_filename"].get<std::string>();

      // Embedded labels flag
      if (proj.contains("use_embedded_labels")) {
        use_embedded_labels = proj["use_embedded_labels"].get<bool>();
      }

      // Feature flags
      if (proj.contains("feature_flags")) {
        auto& flags = proj["feature_flags"];
        // REMOVED: kLogInstructions (deprecated - DisassemblyViewer always
        // active)
        if (flags.contains("kSaveDungeonMaps"))
          feature_flags.kSaveDungeonMaps =
              flags["kSaveDungeonMaps"].get<bool>();
        if (flags.contains("kSaveGraphicsSheet"))
          feature_flags.kSaveGraphicsSheet =
              flags["kSaveGraphicsSheet"].get<bool>();
      }

      // Workspace settings
      if (proj.contains("workspace_settings")) {
        auto& ws = proj["workspace_settings"];
        if (ws.contains("auto_save_enabled"))
          workspace_settings.autosave_enabled =
              ws["auto_save_enabled"].get<bool>();
        if (ws.contains("auto_save_interval"))
          workspace_settings.autosave_interval_secs =
              ws["auto_save_interval"].get<float>();
      }

      if (proj.contains("agent_settings") &&
          proj["agent_settings"].is_object()) {
        auto& agent = proj["agent_settings"];
        agent_settings.ai_provider =
            agent.value("ai_provider", agent_settings.ai_provider);
        agent_settings.ai_model =
            agent.value("ai_model", agent_settings.ai_model);
        agent_settings.ollama_host =
            agent.value("ollama_host", agent_settings.ollama_host);
        agent_settings.gemini_api_key =
            agent.value("gemini_api_key", agent_settings.gemini_api_key);
        agent_settings.use_custom_prompt =
            agent.value("use_custom_prompt", agent_settings.use_custom_prompt);
        agent_settings.custom_system_prompt = agent.value(
            "custom_system_prompt", agent_settings.custom_system_prompt);
        agent_settings.show_reasoning =
            agent.value("show_reasoning", agent_settings.show_reasoning);
        agent_settings.verbose = agent.value("verbose", agent_settings.verbose);
        agent_settings.max_tool_iterations = agent.value(
            "max_tool_iterations", agent_settings.max_tool_iterations);
        agent_settings.max_retry_attempts = agent.value(
            "max_retry_attempts", agent_settings.max_retry_attempts);
        agent_settings.temperature =
            agent.value("temperature", agent_settings.temperature);
        agent_settings.top_p = agent.value("top_p", agent_settings.top_p);
        agent_settings.max_output_tokens =
            agent.value("max_output_tokens", agent_settings.max_output_tokens);
        agent_settings.stream_responses =
            agent.value("stream_responses", agent_settings.stream_responses);
        if (agent.contains("favorite_models") &&
            agent["favorite_models"].is_array()) {
          agent_settings.favorite_models.clear();
          for (const auto& model : agent["favorite_models"]) {
            if (model.is_string())
              agent_settings.favorite_models.push_back(
                  model.get<std::string>());
          }
        }
        if (agent.contains("model_chain") && agent["model_chain"].is_array()) {
          agent_settings.model_chain.clear();
          for (const auto& model : agent["model_chain"]) {
            if (model.is_string())
              agent_settings.model_chain.push_back(model.get<std::string>());
          }
        }
        agent_settings.chain_mode =
            agent.value("chain_mode", agent_settings.chain_mode);
        agent_settings.enable_tool_resources = agent.value(
            "enable_tool_resources", agent_settings.enable_tool_resources);
        agent_settings.enable_tool_dungeon = agent.value(
            "enable_tool_dungeon", agent_settings.enable_tool_dungeon);
        agent_settings.enable_tool_overworld = agent.value(
            "enable_tool_overworld", agent_settings.enable_tool_overworld);
        agent_settings.enable_tool_messages = agent.value(
            "enable_tool_messages", agent_settings.enable_tool_messages);
        agent_settings.enable_tool_dialogue = agent.value(
            "enable_tool_dialogue", agent_settings.enable_tool_dialogue);
        agent_settings.enable_tool_gui =
            agent.value("enable_tool_gui", agent_settings.enable_tool_gui);
        agent_settings.enable_tool_music =
            agent.value("enable_tool_music", agent_settings.enable_tool_music);
        agent_settings.enable_tool_sprite = agent.value(
            "enable_tool_sprite", agent_settings.enable_tool_sprite);
        agent_settings.enable_tool_emulator = agent.value(
            "enable_tool_emulator", agent_settings.enable_tool_emulator);
        agent_settings.builder_blueprint_path = agent.value(
            "builder_blueprint_path", agent_settings.builder_blueprint_path);
      }

      // Build settings
      if (proj.contains("build_script"))
        build_script = proj["build_script"].get<std::string>();
      if (proj.contains("output_folder"))
        output_folder = proj["output_folder"].get<std::string>();
      if (proj.contains("git_repository"))
        git_repository = proj["git_repository"].get<std::string>();
      if (proj.contains("track_changes"))
        track_changes = proj["track_changes"].get<bool>();
    }

    return absl::OkStatus();
  } catch (const json::exception& e) {
    return absl::InvalidArgumentError(
        absl::StrFormat("JSON parse error: %s", e.what()));
  }
}

absl::Status YazeProject::SaveToJsonFormat() {
#ifdef __EMSCRIPTEN__
  return absl::UnimplementedError(
      "JSON project format saving is not supported in the web build");
#endif
  json j;
  auto& proj = j["yaze_project"];

  // Metadata
  proj["version"] = metadata.version;
  proj["name"] = name;
  proj["author"] = metadata.author;
  proj["created_by"] = metadata.created_by;
  proj["description"] = metadata.description;
  proj["created"] = metadata.created_date;
  proj["modified"] = metadata.last_modified;

  // Files
  proj["rom_filename"] = rom_filename;
  proj["code_folder"] = code_folder;
  proj["assets_folder"] = assets_folder;
  proj["patches_folder"] = patches_folder;
  proj["labels_filename"] = labels_filename;
  proj["symbols_filename"] = symbols_filename;
  proj["output_folder"] = output_folder;

  // Embedded labels
  proj["use_embedded_labels"] = use_embedded_labels;

  // Feature flags
  // REMOVED: kLogInstructions (deprecated)
  proj["feature_flags"]["kSaveDungeonMaps"] = feature_flags.kSaveDungeonMaps;
  proj["feature_flags"]["kSaveGraphicsSheet"] =
      feature_flags.kSaveGraphicsSheet;

  // Workspace settings
  proj["workspace_settings"]["auto_save_enabled"] =
      workspace_settings.autosave_enabled;
  proj["workspace_settings"]["auto_save_interval"] =
      workspace_settings.autosave_interval_secs;

  auto& agent = proj["agent_settings"];
  agent["ai_provider"] = agent_settings.ai_provider;
  agent["ai_model"] = agent_settings.ai_model;
  agent["ollama_host"] = agent_settings.ollama_host;
  agent["gemini_api_key"] = agent_settings.gemini_api_key;
  agent["use_custom_prompt"] = agent_settings.use_custom_prompt;
  agent["custom_system_prompt"] = agent_settings.custom_system_prompt;
  agent["show_reasoning"] = agent_settings.show_reasoning;
  agent["verbose"] = agent_settings.verbose;
  agent["max_tool_iterations"] = agent_settings.max_tool_iterations;
  agent["max_retry_attempts"] = agent_settings.max_retry_attempts;
  agent["temperature"] = agent_settings.temperature;
  agent["top_p"] = agent_settings.top_p;
  agent["max_output_tokens"] = agent_settings.max_output_tokens;
  agent["stream_responses"] = agent_settings.stream_responses;
  agent["favorite_models"] = agent_settings.favorite_models;
  agent["model_chain"] = agent_settings.model_chain;
  agent["chain_mode"] = agent_settings.chain_mode;
  agent["enable_tool_resources"] = agent_settings.enable_tool_resources;
  agent["enable_tool_dungeon"] = agent_settings.enable_tool_dungeon;
  agent["enable_tool_overworld"] = agent_settings.enable_tool_overworld;
  agent["enable_tool_messages"] = agent_settings.enable_tool_messages;
  agent["enable_tool_dialogue"] = agent_settings.enable_tool_dialogue;
  agent["enable_tool_gui"] = agent_settings.enable_tool_gui;
  agent["enable_tool_music"] = agent_settings.enable_tool_music;
  agent["enable_tool_sprite"] = agent_settings.enable_tool_sprite;
  agent["enable_tool_emulator"] = agent_settings.enable_tool_emulator;
  agent["builder_blueprint_path"] = agent_settings.builder_blueprint_path;

  // Build settings
  proj["build_script"] = build_script;
  proj["git_repository"] = git_repository;
  proj["track_changes"] = track_changes;

  // Write to file
  std::ofstream file(filepath);
  if (!file.is_open()) {
    return absl::InvalidArgumentError(
        absl::StrFormat("Cannot write JSON project file: %s", filepath));
  }

  file << j.dump(2);  // Pretty print with 2-space indent
  return absl::OkStatus();
}

#endif  // YAZE_ENABLE_JSON_PROJECT_FORMAT

// RecentFilesManager implementation
std::string RecentFilesManager::GetFilePath() const {
  auto config_dir = util::PlatformPaths::GetConfigDirectory();
  if (!config_dir.ok()) {
    return "";  // Or handle error appropriately
  }
  return (*config_dir / kRecentFilesFilename).string();
}

void RecentFilesManager::Save() {
#ifdef __EMSCRIPTEN__
  auto status = platform::WasmStorage::SaveProject(
      kRecentFilesFilename, absl::StrJoin(recent_files_, "\n"));
  if (!status.ok()) {
    LOG_WARN("RecentFilesManager", "Could not persist recent files: %s",
             status.ToString().c_str());
  }
  return;
#endif
  // Ensure config directory exists
  auto config_dir_status = util::PlatformPaths::GetConfigDirectory();
  if (!config_dir_status.ok()) {
    LOG_ERROR("Project", "Failed to get or create config directory: %s",
              config_dir_status.status().ToString().c_str());
    return;
  }

  std::string filepath = GetFilePath();
  std::ofstream file(filepath);
  if (!file.is_open()) {
    LOG_WARN("RecentFilesManager", "Could not save recent files to %s",
             filepath.c_str());
    return;
  }

  for (const auto& file_path : recent_files_) {
    file << file_path << std::endl;
  }
}

void RecentFilesManager::Load() {
#ifdef __EMSCRIPTEN__
  auto storage_or = platform::WasmStorage::LoadProject(kRecentFilesFilename);
  if (!storage_or.ok()) {
    return;
  }
  recent_files_.clear();
  std::istringstream stream(storage_or.value());
  std::string line;
  while (std::getline(stream, line)) {
    if (!line.empty()) {
      recent_files_.push_back(line);
    }
  }
#else
  std::string filepath = GetFilePath();
  std::ifstream file(filepath);
  if (!file.is_open()) {
    // File doesn't exist yet, which is fine
    return;
  }

  recent_files_.clear();
  std::string line;
  while (std::getline(file, line)) {
    if (!line.empty()) {
      recent_files_.push_back(line);
    }
  }
#endif
}

}  // namespace project
}  // namespace yaze
