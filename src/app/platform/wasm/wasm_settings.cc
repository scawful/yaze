#ifdef __EMSCRIPTEN__

#include "app/platform/wasm/wasm_settings.h"

#include <emscripten.h>
#include <algorithm>
#include <sstream>

#include "absl/strings/str_cat.h"
#include "absl/strings/str_format.h"
#include "app/gui/core/theme_manager.h"
#include "app/platform/font_loader.h"
#include "app/platform/wasm/wasm_storage.h"

namespace yaze {
namespace platform {

// JavaScript localStorage interface using EM_JS
EM_JS(void, localStorage_setItem, (const char* key, const char* value), {
  try {
    localStorage.setItem(UTF8ToString(key), UTF8ToString(value));
  } catch (e) {
    console.error('Failed to save to localStorage:', e);
  }
});

EM_JS(char*, localStorage_getItem, (const char* key), {
  try {
    const value = localStorage.getItem(UTF8ToString(key));
    if (value == null)
      return null;
    const len = lengthBytesUTF8(value) + 1;
    const ptr = _malloc(len);
    stringToUTF8(value, ptr, len);
    return ptr;
  } catch (e) {
    console.error('Failed to read from localStorage:', e);
    return null;
  }
});

EM_JS(void, localStorage_removeItem, (const char* key), {
  try {
    localStorage.removeItem(UTF8ToString(key));
  } catch (e) {
    console.error('Failed to remove from localStorage:', e);
  }
});

EM_JS(int, localStorage_hasItem, (const char* key), {
  try {
    return localStorage.getItem(UTF8ToString(key)) != null ? 1 : 0;
  } catch (e) {
    console.error('Failed to check localStorage:', e);
    return 0;
  }
});

EM_JS(void, localStorage_clear, (), {
  try {
    // Only clear yaze-specific keys
    const keys = [];
    for (let i = 0; i < localStorage.length; i++) {
      const key = localStorage.key(i);
      if (key && key.startsWith('yaze_')) {
        keys.push(key);
      }
    }
    keys.forEach(function(key) { localStorage.removeItem(key); });
  } catch (e) {
    console.error('Failed to clear localStorage:', e);
  }
});

// Theme Management

absl::Status WasmSettings::SaveTheme(const std::string& theme) {
  localStorage_setItem(kThemeKey, theme.c_str());
  return absl::OkStatus();
}

std::string WasmSettings::LoadTheme() {
  char* theme = localStorage_getItem(kThemeKey);
  if (!theme) {
    return "dark";  // Default theme
  }
  std::string result(theme);
  free(theme);
  return result;
}

std::string WasmSettings::GetCurrentThemeData() {
  return gui::ThemeManager::Get().ExportCurrentThemeJson();
}

absl::Status WasmSettings::LoadUserFont(const std::string& name,
                                        const std::string& data, float size) {
  return LoadFontFromMemory(name, data, size);
}

// Recent Files Management

nlohmann::json WasmSettings::RecentFilesToJson(
    const std::vector<RecentFile>& files) {
  nlohmann::json json_array = nlohmann::json::array();
  for (const auto& file : files) {
    nlohmann::json entry;
    entry["filename"] = file.filename;
    entry["timestamp"] = std::chrono::duration_cast<std::chrono::milliseconds>(
                             file.timestamp.time_since_epoch())
                             .count();
    json_array.push_back(entry);
  }
  return json_array;
}

std::vector<WasmSettings::RecentFile> WasmSettings::JsonToRecentFiles(
    const nlohmann::json& json) {
  std::vector<RecentFile> files;
  if (!json.is_array())
    return files;

  for (const auto& entry : json) {
    if (entry.contains("filename") && entry.contains("timestamp")) {
      RecentFile file;
      file.filename = entry["filename"].get<std::string>();
      auto ms = std::chrono::milliseconds(entry["timestamp"].get<int64_t>());
      file.timestamp = std::chrono::system_clock::time_point(ms);
      files.push_back(file);
    }
  }
  return files;
}

absl::Status WasmSettings::AddRecentFile(
    const std::string& filename,
    std::chrono::system_clock::time_point timestamp) {
  // Load existing recent files
  char* json_str = localStorage_getItem(kRecentFilesKey);
  std::vector<RecentFile> files;

  if (json_str) {
    try {
      nlohmann::json json = nlohmann::json::parse(json_str);
      files = JsonToRecentFiles(json);
    } catch (const std::exception& e) {
      // Ignore parse errors and start fresh
      emscripten_log(EM_LOG_WARN, "Failed to parse recent files: %s", e.what());
    }
    free(json_str);
  }

  // Remove existing entry if present
  files.erase(std::remove_if(files.begin(), files.end(),
                             [&filename](const RecentFile& f) {
                               return f.filename == filename;
                             }),
              files.end());

  // Add new entry at the beginning
  files.insert(files.begin(), {filename, timestamp});

  // Limit to 20 recent files
  if (files.size() > 20) {
    files.resize(20);
  }

  // Save back to localStorage
  nlohmann::json json = RecentFilesToJson(files);
  localStorage_setItem(kRecentFilesKey, json.dump().c_str());

  return absl::OkStatus();
}

std::vector<std::string> WasmSettings::GetRecentFiles(size_t max_count) {
  std::vector<std::string> result;

  char* json_str = localStorage_getItem(kRecentFilesKey);
  if (!json_str) {
    return result;
  }

  try {
    nlohmann::json json = nlohmann::json::parse(json_str);
    std::vector<RecentFile> files = JsonToRecentFiles(json);

    size_t count = std::min(max_count, files.size());
    for (size_t i = 0; i < count; ++i) {
      result.push_back(files[i].filename);
    }
  } catch (const std::exception& e) {
    emscripten_log(EM_LOG_WARN, "Failed to parse recent files: %s", e.what());
  }

  free(json_str);
  return result;
}

absl::Status WasmSettings::ClearRecentFiles() {
  localStorage_removeItem(kRecentFilesKey);
  return absl::OkStatus();
}

absl::Status WasmSettings::RemoveRecentFile(const std::string& filename) {
  char* json_str = localStorage_getItem(kRecentFilesKey);
  if (!json_str) {
    return absl::OkStatus();  // Nothing to remove
  }

  try {
    nlohmann::json json = nlohmann::json::parse(json_str);
    std::vector<RecentFile> files = JsonToRecentFiles(json);

    files.erase(std::remove_if(files.begin(), files.end(),
                               [&filename](const RecentFile& f) {
                                 return f.filename == filename;
                               }),
                files.end());

    nlohmann::json new_json = RecentFilesToJson(files);
    localStorage_setItem(kRecentFilesKey, new_json.dump().c_str());
  } catch (const std::exception& e) {
    free(json_str);
    return absl::InternalError(
        absl::StrFormat("Failed to remove recent file: %s", e.what()));
  }

  free(json_str);
  return absl::OkStatus();
}

// Workspace Layout Management

absl::Status WasmSettings::SaveWorkspace(const std::string& name,
                                         const std::string& layout_json) {
  std::string key = absl::StrCat(kWorkspacePrefix, name);
  return WasmStorage::SaveProject(key, layout_json);
}

absl::StatusOr<std::string> WasmSettings::LoadWorkspace(
    const std::string& name) {
  std::string key = absl::StrCat(kWorkspacePrefix, name);
  return WasmStorage::LoadProject(key);
}

std::vector<std::string> WasmSettings::ListWorkspaces() {
  std::vector<std::string> all_projects = WasmStorage::ListProjects();
  std::vector<std::string> workspaces;

  const std::string prefix(kWorkspacePrefix);
  for (const auto& project : all_projects) {
    if (project.find(prefix) == 0) {
      workspaces.push_back(project.substr(prefix.length()));
    }
  }

  return workspaces;
}

absl::Status WasmSettings::DeleteWorkspace(const std::string& name) {
  std::string key = absl::StrCat(kWorkspacePrefix, name);
  return WasmStorage::DeleteProject(key);
}

absl::Status WasmSettings::SetActiveWorkspace(const std::string& name) {
  localStorage_setItem(kActiveWorkspaceKey, name.c_str());
  return absl::OkStatus();
}

std::string WasmSettings::GetActiveWorkspace() {
  char* workspace = localStorage_getItem(kActiveWorkspaceKey);
  if (!workspace) {
    return "default";
  }
  std::string result(workspace);
  free(workspace);
  return result;
}

// Undo History Persistence

absl::Status WasmSettings::SaveUndoHistory(
    const std::string& editor_id, const std::vector<uint8_t>& history) {
  std::string key = absl::StrCat(kUndoHistoryPrefix, editor_id);
  return WasmStorage::SaveRom(key, history);  // Use binary storage
}

absl::StatusOr<std::vector<uint8_t>> WasmSettings::LoadUndoHistory(
    const std::string& editor_id) {
  std::string key = absl::StrCat(kUndoHistoryPrefix, editor_id);
  return WasmStorage::LoadRom(key);
}

absl::Status WasmSettings::ClearUndoHistory(const std::string& editor_id) {
  std::string key = absl::StrCat(kUndoHistoryPrefix, editor_id);
  return WasmStorage::DeleteRom(key);
}

absl::Status WasmSettings::ClearAllUndoHistory() {
  std::vector<std::string> all_roms = WasmStorage::ListRoms();
  const std::string prefix(kUndoHistoryPrefix);

  for (const auto& rom : all_roms) {
    if (rom.find(prefix) == 0) {
      auto status = WasmStorage::DeleteRom(rom);
      if (!status.ok()) {
        return status;
      }
    }
  }

  return absl::OkStatus();
}

// General Settings

absl::Status WasmSettings::SaveSetting(const std::string& key,
                                       const nlohmann::json& value) {
  std::string storage_key = absl::StrCat(kSettingsPrefix, key);
  localStorage_setItem(storage_key.c_str(), value.dump().c_str());

  // Update last save time
  auto now = std::chrono::system_clock::now();
  auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                now.time_since_epoch())
                .count();
  localStorage_setItem(kLastSaveTimeKey, std::to_string(ms).c_str());

  return absl::OkStatus();
}

absl::StatusOr<nlohmann::json> WasmSettings::LoadSetting(
    const std::string& key) {
  std::string storage_key = absl::StrCat(kSettingsPrefix, key);
  char* value = localStorage_getItem(storage_key.c_str());

  if (!value) {
    return absl::NotFoundError(absl::StrFormat("Setting '%s' not found", key));
  }

  try {
    nlohmann::json json = nlohmann::json::parse(value);
    free(value);
    return json;
  } catch (const std::exception& e) {
    free(value);
    return absl::InvalidArgumentError(
        absl::StrFormat("Failed to parse setting '%s': %s", key, e.what()));
  }
}

bool WasmSettings::HasSetting(const std::string& key) {
  std::string storage_key = absl::StrCat(kSettingsPrefix, key);
  return localStorage_hasItem(storage_key.c_str()) == 1;
}

absl::Status WasmSettings::SaveAllSettings(const nlohmann::json& settings) {
  if (!settings.is_object()) {
    return absl::InvalidArgumentError("Settings must be a JSON object");
  }

  for (auto it = settings.begin(); it != settings.end(); ++it) {
    auto status = SaveSetting(it.key(), it.value());
    if (!status.ok()) {
      return status;
    }
  }

  return absl::OkStatus();
}

absl::StatusOr<nlohmann::json> WasmSettings::LoadAllSettings() {
  nlohmann::json settings = nlohmann::json::object();

  // This is a simplified implementation since we can't easily iterate localStorage
  // from C++. In practice, you'd maintain a list of known setting keys.
  // For now, we'll just return common settings if they exist.

  std::vector<std::string> common_keys = {
      "show_grid",          "grid_size",       "auto_save",
      "auto_save_interval", "show_tooltips",   "confirm_on_delete",
      "default_editor",     "animation_speed", "zoom_level",
      "show_minimap"};

  for (const auto& key : common_keys) {
    if (HasSetting(key)) {
      auto result = LoadSetting(key);
      if (result.ok()) {
        settings[key] = *result;
      }
    }
  }

  return settings;
}

absl::Status WasmSettings::ClearAllSettings() {
  localStorage_clear();
  return absl::OkStatus();
}

// Utility

absl::StatusOr<std::string> WasmSettings::ExportSettings() {
  nlohmann::json export_data = nlohmann::json::object();

  // Export theme
  export_data["theme"] = LoadTheme();

  // Export recent files
  char* recent_json = localStorage_getItem(kRecentFilesKey);
  if (recent_json) {
    try {
      export_data["recent_files"] = nlohmann::json::parse(recent_json);
    } catch (...) {
      // Ignore parse errors
    }
    free(recent_json);
  }

  // Export active workspace
  export_data["active_workspace"] = GetActiveWorkspace();

  // Export workspaces
  nlohmann::json workspaces = nlohmann::json::object();
  for (const auto& name : ListWorkspaces()) {
    auto workspace_data = LoadWorkspace(name);
    if (workspace_data.ok()) {
      workspaces[name] = nlohmann::json::parse(*workspace_data);
    }
  }
  export_data["workspaces"] = workspaces;

  // Export general settings
  auto all_settings = LoadAllSettings();
  if (all_settings.ok()) {
    export_data["settings"] = *all_settings;
  }

  return export_data.dump(2);  // Pretty print with 2 spaces
}

absl::Status WasmSettings::ImportSettings(const std::string& json_str) {
  try {
    nlohmann::json import_data = nlohmann::json::parse(json_str);

    // Import theme
    if (import_data.contains("theme")) {
      SaveTheme(import_data["theme"].get<std::string>());
    }

    // Import recent files
    if (import_data.contains("recent_files")) {
      localStorage_setItem(kRecentFilesKey,
                           import_data["recent_files"].dump().c_str());
    }

    // Import active workspace
    if (import_data.contains("active_workspace")) {
      SetActiveWorkspace(import_data["active_workspace"].get<std::string>());
    }

    // Import workspaces
    if (import_data.contains("workspaces") &&
        import_data["workspaces"].is_object()) {
      for (auto it = import_data["workspaces"].begin();
           it != import_data["workspaces"].end(); ++it) {
        SaveWorkspace(it.key(), it.value().dump());
      }
    }

    // Import general settings
    if (import_data.contains("settings") &&
        import_data["settings"].is_object()) {
      SaveAllSettings(import_data["settings"]);
    }

    return absl::OkStatus();
  } catch (const std::exception& e) {
    return absl::InvalidArgumentError(
        absl::StrFormat("Failed to import settings: %s", e.what()));
  }
}

absl::StatusOr<std::chrono::system_clock::time_point>
WasmSettings::GetLastSaveTime() {
  char* time_str = localStorage_getItem(kLastSaveTimeKey);
  if (!time_str) {
    return absl::NotFoundError("No save time recorded");
  }

  try {
    int64_t ms = std::stoll(time_str);
    free(time_str);
    return std::chrono::system_clock::time_point(std::chrono::milliseconds(ms));
  } catch (const std::exception& e) {
    free(time_str);
    return absl::InvalidArgumentError(
        absl::StrFormat("Failed to parse save time: %s", e.what()));
  }
}

}  // namespace platform
}  // namespace yaze

#endif  // __EMSCRIPTEN__
