#include "app/editor/system/user_settings.h"

#include <filesystem>
#include <fstream>
#include <sstream>

#include "absl/strings/str_format.h"
#include "app/gui/core/style.h"
#include "imgui/imgui.h"
#include "util/file_util.h"
#include "util/log.h"
#include "util/platform_paths.h"

#ifdef YAZE_WITH_JSON
#include "nlohmann/json.hpp"
#endif

namespace yaze {
namespace editor {

#ifdef YAZE_WITH_JSON
using json = nlohmann::json;
#endif

namespace {

absl::Status EnsureParentDirectory(const std::filesystem::path& path) {
  auto parent = path.parent_path();
  if (parent.empty()) {
    return absl::OkStatus();
  }
  return util::PlatformPaths::EnsureDirectoryExists(parent);
}

absl::Status LoadPreferencesFromIni(const std::filesystem::path& path,
                                    UserSettings::Preferences* prefs) {
  if (!prefs) {
    return absl::InvalidArgumentError("prefs is null");
  }

  auto data = util::LoadFile(path.string());
  if (data.empty()) {
    return absl::OkStatus();
  }

  std::istringstream ss(data);
  std::string line;
  while (std::getline(ss, line)) {
    size_t eq_pos = line.find('=');
    if (eq_pos == std::string::npos) {
      continue;
    }

    std::string key = line.substr(0, eq_pos);
    std::string val = line.substr(eq_pos + 1);

    // General
    if (key == "font_global_scale") {
      prefs->font_global_scale = std::stof(val);
    } else if (key == "backup_rom") {
      prefs->backup_rom = (val == "1");
    } else if (key == "save_new_auto") {
      prefs->save_new_auto = (val == "1");
    } else if (key == "autosave_enabled") {
      prefs->autosave_enabled = (val == "1");
    } else if (key == "autosave_interval") {
      prefs->autosave_interval = std::stof(val);
    } else if (key == "recent_files_limit") {
      prefs->recent_files_limit = std::stoi(val);
    } else if (key == "last_rom_path") {
      prefs->last_rom_path = val;
    } else if (key == "last_project_path") {
      prefs->last_project_path = val;
    } else if (key == "show_welcome_on_startup") {
      prefs->show_welcome_on_startup = (val == "1");
    } else if (key == "restore_last_session") {
      prefs->restore_last_session = (val == "1");
    } else if (key == "prefer_hmagic_sprite_names") {
      prefs->prefer_hmagic_sprite_names = (val == "1");
    }
    // Editor Behavior
    else if (key == "backup_before_save") {
      prefs->backup_before_save = (val == "1");
    } else if (key == "default_editor") {
      prefs->default_editor = std::stoi(val);
    }
    // Performance
    else if (key == "vsync") {
      prefs->vsync = (val == "1");
    } else if (key == "target_fps") {
      prefs->target_fps = std::stoi(val);
    } else if (key == "cache_size_mb") {
      prefs->cache_size_mb = std::stoi(val);
    } else if (key == "undo_history_size") {
      prefs->undo_history_size = std::stoi(val);
    }
    // AI Agent
    else if (key == "ai_provider") {
      prefs->ai_provider = std::stoi(val);
    } else if (key == "ai_model") {
      prefs->ai_model = val;
    } else if (key == "ollama_url") {
      prefs->ollama_url = val;
    } else if (key == "gemini_api_key") {
      prefs->gemini_api_key = val;
    } else if (key == "ai_temperature") {
      prefs->ai_temperature = std::stof(val);
    } else if (key == "ai_max_tokens") {
      prefs->ai_max_tokens = std::stoi(val);
    } else if (key == "ai_proactive") {
      prefs->ai_proactive = (val == "1");
    } else if (key == "ai_auto_learn") {
      prefs->ai_auto_learn = (val == "1");
    } else if (key == "ai_multimodal") {
      prefs->ai_multimodal = (val == "1");
    }
    // CLI Logging
    else if (key == "log_level") {
      prefs->log_level = std::stoi(val);
    } else if (key == "log_to_file") {
      prefs->log_to_file = (val == "1");
    } else if (key == "log_file_path") {
      prefs->log_file_path = val;
    } else if (key == "log_ai_requests") {
      prefs->log_ai_requests = (val == "1");
    } else if (key == "log_rom_operations") {
      prefs->log_rom_operations = (val == "1");
    } else if (key == "log_gui_automation") {
      prefs->log_gui_automation = (val == "1");
    } else if (key == "log_proposals") {
      prefs->log_proposals = (val == "1");
    }
    // Panel Shortcuts (format: panel_shortcut.panel_id=shortcut)
    else if (key.substr(0, 15) == "panel_shortcut.") {
      std::string panel_id = key.substr(15);
      prefs->panel_shortcuts[panel_id] = val;
    }
    // Backward compatibility for card_shortcut
    else if (key.substr(0, 14) == "card_shortcut.") {
      std::string panel_id = key.substr(14);
      prefs->panel_shortcuts[panel_id] = val;
    }
    // Sidebar State
    else if (key == "sidebar_visible") {
      prefs->sidebar_visible = (val == "1");
    } else if (key == "sidebar_panel_expanded") {
      prefs->sidebar_panel_expanded = (val == "1");
    } else if (key == "sidebar_active_category") {
      prefs->sidebar_active_category = val;
    }
    // Status Bar
    else if (key == "show_status_bar") {
      prefs->show_status_bar = (val == "1");
    }
    // Panel Visibility State (format: panel_visibility.EditorType.panel_id=1)
    else if (key.substr(0, 17) == "panel_visibility.") {
      std::string rest = key.substr(17);
      size_t dot_pos = rest.find('.');
      if (dot_pos != std::string::npos) {
        std::string editor_type = rest.substr(0, dot_pos);
        std::string panel_id = rest.substr(dot_pos + 1);
        prefs->panel_visibility_state[editor_type][panel_id] = (val == "1");
      }
    }
    // Pinned Panels (format: pinned_panel.panel_id=1)
    else if (key.substr(0, 13) == "pinned_panel.") {
      std::string panel_id = key.substr(13);
      prefs->pinned_panels[panel_id] = (val == "1");
    }
    // Saved Layouts (format: saved_layout.LayoutName.panel_id=1)
    else if (key.substr(0, 13) == "saved_layout.") {
      std::string rest = key.substr(13);
      size_t dot_pos = rest.find('.');
      if (dot_pos != std::string::npos) {
        std::string layout_name = rest.substr(0, dot_pos);
        std::string panel_id = rest.substr(dot_pos + 1);
        prefs->saved_layouts[layout_name][panel_id] = (val == "1");
      }
    }
  }

  return absl::OkStatus();
}

absl::Status SavePreferencesToIni(const std::filesystem::path& path,
                                  const UserSettings::Preferences& prefs) {
  auto ensure_status = EnsureParentDirectory(path);
  if (!ensure_status.ok()) {
    return ensure_status;
  }

  std::ostringstream ss;
  // General
  ss << "font_global_scale=" << prefs.font_global_scale << "\n";
  ss << "backup_rom=" << (prefs.backup_rom ? 1 : 0) << "\n";
  ss << "save_new_auto=" << (prefs.save_new_auto ? 1 : 0) << "\n";
  ss << "autosave_enabled=" << (prefs.autosave_enabled ? 1 : 0) << "\n";
  ss << "autosave_interval=" << prefs.autosave_interval << "\n";
  ss << "recent_files_limit=" << prefs.recent_files_limit << "\n";
  ss << "last_rom_path=" << prefs.last_rom_path << "\n";
  ss << "last_project_path=" << prefs.last_project_path << "\n";
  ss << "show_welcome_on_startup=" << (prefs.show_welcome_on_startup ? 1 : 0)
     << "\n";
  ss << "restore_last_session=" << (prefs.restore_last_session ? 1 : 0)
     << "\n";
  ss << "prefer_hmagic_sprite_names="
     << (prefs.prefer_hmagic_sprite_names ? 1 : 0) << "\n";

  // Editor Behavior
  ss << "backup_before_save=" << (prefs.backup_before_save ? 1 : 0) << "\n";
  ss << "default_editor=" << prefs.default_editor << "\n";

  // Performance
  ss << "vsync=" << (prefs.vsync ? 1 : 0) << "\n";
  ss << "target_fps=" << prefs.target_fps << "\n";
  ss << "cache_size_mb=" << prefs.cache_size_mb << "\n";
  ss << "undo_history_size=" << prefs.undo_history_size << "\n";

  // AI Agent
  ss << "ai_provider=" << prefs.ai_provider << "\n";
  ss << "ai_model=" << prefs.ai_model << "\n";
  ss << "ollama_url=" << prefs.ollama_url << "\n";
  ss << "gemini_api_key=" << prefs.gemini_api_key << "\n";
  ss << "ai_temperature=" << prefs.ai_temperature << "\n";
  ss << "ai_max_tokens=" << prefs.ai_max_tokens << "\n";
  ss << "ai_proactive=" << (prefs.ai_proactive ? 1 : 0) << "\n";
  ss << "ai_auto_learn=" << (prefs.ai_auto_learn ? 1 : 0) << "\n";
  ss << "ai_multimodal=" << (prefs.ai_multimodal ? 1 : 0) << "\n";

  // CLI Logging
  ss << "log_level=" << prefs.log_level << "\n";
  ss << "log_to_file=" << (prefs.log_to_file ? 1 : 0) << "\n";
  ss << "log_file_path=" << prefs.log_file_path << "\n";
  ss << "log_ai_requests=" << (prefs.log_ai_requests ? 1 : 0) << "\n";
  ss << "log_rom_operations=" << (prefs.log_rom_operations ? 1 : 0) << "\n";
  ss << "log_gui_automation=" << (prefs.log_gui_automation ? 1 : 0) << "\n";
  ss << "log_proposals=" << (prefs.log_proposals ? 1 : 0) << "\n";

  // Panel Shortcuts
  for (const auto& [panel_id, shortcut] : prefs.panel_shortcuts) {
    ss << "panel_shortcut." << panel_id << "=" << shortcut << "\n";
  }

  // Sidebar State
  ss << "sidebar_visible=" << (prefs.sidebar_visible ? 1 : 0) << "\n";
  ss << "sidebar_panel_expanded=" << (prefs.sidebar_panel_expanded ? 1 : 0)
     << "\n";
  ss << "sidebar_active_category=" << prefs.sidebar_active_category << "\n";

  // Status Bar
  ss << "show_status_bar=" << (prefs.show_status_bar ? 1 : 0) << "\n";

  // Panel Visibility State
  for (const auto& [editor_type, panel_state] : prefs.panel_visibility_state) {
    for (const auto& [panel_id, visible] : panel_state) {
      ss << "panel_visibility." << editor_type << "." << panel_id << "="
         << (visible ? 1 : 0) << "\n";
    }
  }

  // Pinned Panels
  for (const auto& [panel_id, pinned] : prefs.pinned_panels) {
    ss << "pinned_panel." << panel_id << "=" << (pinned ? 1 : 0) << "\n";
  }

  // Saved Layouts
  for (const auto& [layout_name, panel_state] : prefs.saved_layouts) {
    for (const auto& [panel_id, visible] : panel_state) {
      ss << "saved_layout." << layout_name << "." << panel_id << "="
         << (visible ? 1 : 0) << "\n";
    }
  }

  std::ofstream file(path);
  if (!file.is_open()) {
    return absl::InternalError(
        absl::StrFormat("Failed to open settings file: %s", path.string()));
  }
  file << ss.str();
  return absl::OkStatus();
}

#ifdef YAZE_WITH_JSON
void EnsureDefaultAiHosts(UserSettings::Preferences* prefs) {
  if (!prefs) {
    return;
  }

  if (!prefs->ai_hosts.empty()) {
    if (prefs->active_ai_host_id.empty()) {
      prefs->active_ai_host_id = prefs->ai_hosts.front().id;
    }
    return;
  }

  if (!prefs->ollama_url.empty()) {
    UserSettings::Preferences::AiHost host;
    host.id = "ollama-local";
    host.label = "Ollama (local)";
    host.base_url = prefs->ollama_url;
    host.api_type = "ollama";
    host.supports_tools = true;
    host.supports_streaming = true;
    prefs->ai_hosts.push_back(host);
  }

  if (!prefs->ai_hosts.empty() && prefs->active_ai_host_id.empty()) {
    prefs->active_ai_host_id = prefs->ai_hosts.front().id;
  }
}

void EnsureDefaultAiProfiles(UserSettings::Preferences* prefs) {
  if (!prefs) {
    return;
  }
  if (!prefs->ai_profiles.empty()) {
    if (prefs->active_ai_profile.empty()) {
      prefs->active_ai_profile = prefs->ai_profiles.front().name;
    }
    return;
  }
  if (!prefs->ai_model.empty()) {
    UserSettings::Preferences::AiModelProfile profile;
    profile.name = "default";
    profile.model = prefs->ai_model;
    profile.temperature = prefs->ai_temperature;
    profile.top_p = 0.95f;
    profile.max_output_tokens = prefs->ai_max_tokens;
    profile.supports_tools = true;
    prefs->ai_profiles.push_back(profile);
    prefs->active_ai_profile = profile.name;
  }
}

void EnsureDefaultFilesystemRoots(UserSettings::Preferences* prefs) {
  if (!prefs) {
    return;
  }

  if (!prefs->project_root_paths.empty()) {
    if (prefs->default_project_root.empty()) {
      prefs->default_project_root = prefs->project_root_paths.front();
    }
    return;
  }

  auto docs_dir = util::PlatformPaths::GetUserDocumentsDirectory();
  if (docs_dir.ok()) {
    prefs->project_root_paths.push_back(docs_dir->string());
    prefs->default_project_root = docs_dir->string();
  }
}

void LoadStringMap(const json& src,
                   std::unordered_map<std::string, std::string>* target) {
  if (!target || !src.is_object()) {
    return;
  }
  target->clear();
  for (const auto& [key, value] : src.items()) {
    if (value.is_string()) {
      (*target)[key] = value.get<std::string>();
    }
  }
}

void LoadBoolMap(const json& src,
                 std::unordered_map<std::string, bool>* target) {
  if (!target || !src.is_object()) {
    return;
  }
  target->clear();
  for (const auto& [key, value] : src.items()) {
    if (value.is_boolean()) {
      (*target)[key] = value.get<bool>();
    }
  }
}

void LoadNestedBoolMap(
    const json& src,
    std::unordered_map<std::string, std::unordered_map<std::string, bool>>*
        target) {
  if (!target || !src.is_object()) {
    return;
  }
  target->clear();
  for (const auto& [outer_key, outer_val] : src.items()) {
    if (!outer_val.is_object()) {
      continue;
    }
    auto& inner = (*target)[outer_key];
    inner.clear();
    for (const auto& [inner_key, inner_val] : outer_val.items()) {
      if (inner_val.is_boolean()) {
        inner[inner_key] = inner_val.get<bool>();
      }
    }
  }
}

json ToStringMap(const std::unordered_map<std::string, std::string>& map) {
  json obj = json::object();
  for (const auto& [key, value] : map) {
    obj[key] = value;
  }
  return obj;
}

json ToBoolMap(const std::unordered_map<std::string, bool>& map) {
  json obj = json::object();
  for (const auto& [key, value] : map) {
    obj[key] = value;
  }
  return obj;
}

json ToNestedBoolMap(
    const std::unordered_map<std::string, std::unordered_map<std::string, bool>>&
        map) {
  json obj = json::object();
  for (const auto& [outer_key, inner] : map) {
    obj[outer_key] = ToBoolMap(inner);
  }
  return obj;
}

absl::Status LoadPreferencesFromJson(const std::filesystem::path& path,
                                     UserSettings::Preferences* prefs) {
  if (!prefs) {
    return absl::InvalidArgumentError("prefs is null");
  }

  std::ifstream file(path);
  if (!file.is_open()) {
    return absl::NotFoundError(
        absl::StrFormat("Settings file not found: %s", path.string()));
  }

  json root;
  try {
    file >> root;
  } catch (const std::exception& e) {
    return absl::InternalError(
        absl::StrFormat("Failed to parse settings.json: %s", e.what()));
  }

  if (root.contains("general")) {
    const auto& g = root["general"];
    prefs->font_global_scale =
        g.value("font_global_scale", prefs->font_global_scale);
    prefs->backup_rom = g.value("backup_rom", prefs->backup_rom);
    prefs->save_new_auto = g.value("save_new_auto", prefs->save_new_auto);
    prefs->autosave_enabled =
        g.value("autosave_enabled", prefs->autosave_enabled);
    prefs->autosave_interval =
        g.value("autosave_interval", prefs->autosave_interval);
    prefs->recent_files_limit =
        g.value("recent_files_limit", prefs->recent_files_limit);
    prefs->last_rom_path = g.value("last_rom_path", prefs->last_rom_path);
    prefs->last_project_path =
        g.value("last_project_path", prefs->last_project_path);
    prefs->show_welcome_on_startup =
        g.value("show_welcome_on_startup", prefs->show_welcome_on_startup);
    prefs->restore_last_session =
        g.value("restore_last_session", prefs->restore_last_session);
    prefs->prefer_hmagic_sprite_names = g.value(
        "prefer_hmagic_sprite_names", prefs->prefer_hmagic_sprite_names);
  }

  if (root.contains("editor")) {
    const auto& e = root["editor"];
    prefs->backup_before_save =
        e.value("backup_before_save", prefs->backup_before_save);
    prefs->default_editor = e.value("default_editor", prefs->default_editor);
  }

  if (root.contains("performance")) {
    const auto& p = root["performance"];
    prefs->vsync = p.value("vsync", prefs->vsync);
    prefs->target_fps = p.value("target_fps", prefs->target_fps);
    prefs->cache_size_mb = p.value("cache_size_mb", prefs->cache_size_mb);
    prefs->undo_history_size =
        p.value("undo_history_size", prefs->undo_history_size);
  }

  if (root.contains("ai")) {
    const auto& ai = root["ai"];
    prefs->ai_provider = ai.value("provider", prefs->ai_provider);
    prefs->ai_model = ai.value("model", prefs->ai_model);
    prefs->ollama_url = ai.value("ollama_url", prefs->ollama_url);
    prefs->gemini_api_key = ai.value("gemini_api_key", prefs->gemini_api_key);
    prefs->ai_temperature =
        ai.value("temperature", prefs->ai_temperature);
    prefs->ai_max_tokens = ai.value("max_tokens", prefs->ai_max_tokens);
    prefs->ai_proactive = ai.value("proactive", prefs->ai_proactive);
    prefs->ai_auto_learn = ai.value("auto_learn", prefs->ai_auto_learn);
    prefs->ai_multimodal = ai.value("multimodal", prefs->ai_multimodal);
    prefs->active_ai_host_id =
        ai.value("active_host_id", prefs->active_ai_host_id);
    prefs->active_ai_profile =
        ai.value("active_profile", prefs->active_ai_profile);
    prefs->remote_build_host_id =
        ai.value("remote_build_host_id", prefs->remote_build_host_id);

    if (ai.contains("hosts") && ai["hosts"].is_array()) {
      prefs->ai_hosts.clear();
      for (const auto& host : ai["hosts"]) {
        if (!host.is_object()) {
          continue;
        }
        UserSettings::Preferences::AiHost entry;
        entry.id = host.value("id", "");
        entry.label = host.value("label", "");
        entry.base_url = host.value("base_url", "");
        entry.api_type = host.value("api_type", "");
        entry.supports_vision =
            host.value("supports_vision", entry.supports_vision);
        entry.supports_tools =
            host.value("supports_tools", entry.supports_tools);
        entry.supports_streaming =
            host.value("supports_streaming", entry.supports_streaming);
        entry.allow_insecure =
            host.value("allow_insecure", entry.allow_insecure);
        entry.api_key = host.value("api_key", "");
        entry.credential_id = host.value("credential_id", "");
        prefs->ai_hosts.push_back(entry);
      }
    }

    if (ai.contains("profiles") && ai["profiles"].is_array()) {
      prefs->ai_profiles.clear();
      for (const auto& profile : ai["profiles"]) {
        if (!profile.is_object()) {
          continue;
        }
        UserSettings::Preferences::AiModelProfile entry;
        entry.name = profile.value("name", "");
        entry.model = profile.value("model", "");
        entry.temperature =
            profile.value("temperature", entry.temperature);
        entry.top_p = profile.value("top_p", entry.top_p);
        entry.max_output_tokens =
            profile.value("max_output_tokens", entry.max_output_tokens);
        entry.supports_vision =
            profile.value("supports_vision", entry.supports_vision);
        entry.supports_tools =
            profile.value("supports_tools", entry.supports_tools);
        prefs->ai_profiles.push_back(entry);
      }
    }
  }

  if (root.contains("logging")) {
    const auto& log = root["logging"];
    prefs->log_level = log.value("level", prefs->log_level);
    prefs->log_to_file = log.value("to_file", prefs->log_to_file);
    prefs->log_file_path = log.value("file_path", prefs->log_file_path);
    prefs->log_ai_requests = log.value("ai_requests", prefs->log_ai_requests);
    prefs->log_rom_operations =
        log.value("rom_operations", prefs->log_rom_operations);
    prefs->log_gui_automation =
        log.value("gui_automation", prefs->log_gui_automation);
    prefs->log_proposals = log.value("proposals", prefs->log_proposals);
  }

  if (root.contains("shortcuts")) {
    const auto& shortcuts = root["shortcuts"];
    if (shortcuts.contains("panel")) {
      LoadStringMap(shortcuts["panel"], &prefs->panel_shortcuts);
    }
    if (shortcuts.contains("global")) {
      LoadStringMap(shortcuts["global"], &prefs->global_shortcuts);
    }
    if (shortcuts.contains("editor")) {
      LoadStringMap(shortcuts["editor"], &prefs->editor_shortcuts);
    }
  }

  if (root.contains("sidebar")) {
    const auto& sidebar = root["sidebar"];
    prefs->sidebar_visible =
        sidebar.value("visible", prefs->sidebar_visible);
    prefs->sidebar_panel_expanded =
        sidebar.value("panel_expanded", prefs->sidebar_panel_expanded);
    prefs->sidebar_active_category =
        sidebar.value("active_category", prefs->sidebar_active_category);
  }

  if (root.contains("status_bar")) {
    const auto& status_bar = root["status_bar"];
    prefs->show_status_bar =
        status_bar.value("visible", prefs->show_status_bar);
  }

  if (root.contains("layouts")) {
    const auto& layouts = root["layouts"];
    if (layouts.contains("panel_visibility")) {
      LoadNestedBoolMap(layouts["panel_visibility"],
                        &prefs->panel_visibility_state);
    }
    if (layouts.contains("pinned_panels")) {
      LoadBoolMap(layouts["pinned_panels"], &prefs->pinned_panels);
    }
    if (layouts.contains("saved_layouts")) {
      LoadNestedBoolMap(layouts["saved_layouts"], &prefs->saved_layouts);
    }
  }

  if (root.contains("filesystem")) {
    const auto& fs = root["filesystem"];
    if (fs.contains("project_root_paths") &&
        fs["project_root_paths"].is_array()) {
      prefs->project_root_paths.clear();
      for (const auto& item : fs["project_root_paths"]) {
        if (item.is_string()) {
          prefs->project_root_paths.push_back(item.get<std::string>());
        }
      }
    }
    prefs->default_project_root =
        fs.value("default_project_root", prefs->default_project_root);
    prefs->use_files_app = fs.value("use_files_app", prefs->use_files_app);
    prefs->use_icloud_sync = fs.value("use_icloud_sync", prefs->use_icloud_sync);
  }

  EnsureDefaultAiHosts(prefs);
  EnsureDefaultAiProfiles(prefs);
  EnsureDefaultFilesystemRoots(prefs);

  return absl::OkStatus();
}

absl::Status SavePreferencesToJson(const std::filesystem::path& path,
                                   const UserSettings::Preferences& prefs) {
  auto ensure_status = EnsureParentDirectory(path);
  if (!ensure_status.ok()) {
    return ensure_status;
  }

  json root;
  root["version"] = 1;
  root["general"] = {
      {"font_global_scale", prefs.font_global_scale},
      {"backup_rom", prefs.backup_rom},
      {"save_new_auto", prefs.save_new_auto},
      {"autosave_enabled", prefs.autosave_enabled},
      {"autosave_interval", prefs.autosave_interval},
      {"recent_files_limit", prefs.recent_files_limit},
      {"last_rom_path", prefs.last_rom_path},
      {"last_project_path", prefs.last_project_path},
      {"show_welcome_on_startup", prefs.show_welcome_on_startup},
      {"restore_last_session", prefs.restore_last_session},
      {"prefer_hmagic_sprite_names", prefs.prefer_hmagic_sprite_names},
  };

  root["editor"] = {
      {"backup_before_save", prefs.backup_before_save},
      {"default_editor", prefs.default_editor},
  };

  root["performance"] = {
      {"vsync", prefs.vsync},
      {"target_fps", prefs.target_fps},
      {"cache_size_mb", prefs.cache_size_mb},
      {"undo_history_size", prefs.undo_history_size},
  };

  json ai_hosts = json::array();
  for (const auto& host : prefs.ai_hosts) {
    ai_hosts.push_back({
        {"id", host.id},
        {"label", host.label},
        {"base_url", host.base_url},
        {"api_type", host.api_type},
        {"supports_vision", host.supports_vision},
        {"supports_tools", host.supports_tools},
        {"supports_streaming", host.supports_streaming},
        {"allow_insecure", host.allow_insecure},
        {"api_key", host.api_key},
        {"credential_id", host.credential_id},
    });
  }

  json ai_profiles = json::array();
  for (const auto& profile : prefs.ai_profiles) {
    ai_profiles.push_back({
        {"name", profile.name},
        {"model", profile.model},
        {"temperature", profile.temperature},
        {"top_p", profile.top_p},
        {"max_output_tokens", profile.max_output_tokens},
        {"supports_vision", profile.supports_vision},
        {"supports_tools", profile.supports_tools},
    });
  }

  root["ai"] = {
      {"provider", prefs.ai_provider},
      {"model", prefs.ai_model},
      {"ollama_url", prefs.ollama_url},
      {"gemini_api_key", prefs.gemini_api_key},
      {"temperature", prefs.ai_temperature},
      {"max_tokens", prefs.ai_max_tokens},
      {"proactive", prefs.ai_proactive},
      {"auto_learn", prefs.ai_auto_learn},
      {"multimodal", prefs.ai_multimodal},
      {"hosts", ai_hosts},
      {"active_host_id", prefs.active_ai_host_id},
      {"profiles", ai_profiles},
      {"active_profile", prefs.active_ai_profile},
      {"remote_build_host_id", prefs.remote_build_host_id},
  };

  root["logging"] = {
      {"level", prefs.log_level},
      {"to_file", prefs.log_to_file},
      {"file_path", prefs.log_file_path},
      {"ai_requests", prefs.log_ai_requests},
      {"rom_operations", prefs.log_rom_operations},
      {"gui_automation", prefs.log_gui_automation},
      {"proposals", prefs.log_proposals},
  };

  root["shortcuts"] = {
      {"panel", ToStringMap(prefs.panel_shortcuts)},
      {"global", ToStringMap(prefs.global_shortcuts)},
      {"editor", ToStringMap(prefs.editor_shortcuts)},
  };

  root["sidebar"] = {
      {"visible", prefs.sidebar_visible},
      {"panel_expanded", prefs.sidebar_panel_expanded},
      {"active_category", prefs.sidebar_active_category},
  };

  root["status_bar"] = {
      {"visible", prefs.show_status_bar},
  };

  root["layouts"] = {
      {"panel_visibility", ToNestedBoolMap(prefs.panel_visibility_state)},
      {"pinned_panels", ToBoolMap(prefs.pinned_panels)},
      {"saved_layouts", ToNestedBoolMap(prefs.saved_layouts)},
  };

  root["filesystem"] = {
      {"project_root_paths", prefs.project_root_paths},
      {"default_project_root", prefs.default_project_root},
      {"use_files_app", prefs.use_files_app},
      {"use_icloud_sync", prefs.use_icloud_sync},
  };

  std::ofstream file(path);
  if (!file.is_open()) {
    return absl::InternalError(
        absl::StrFormat("Failed to open settings file: %s", path.string()));
  }

  file << root.dump(2) << "\n";
  return absl::OkStatus();
}
#endif  // YAZE_WITH_JSON

}  // namespace

UserSettings::UserSettings() {
  auto docs_dir_status = util::PlatformPaths::GetUserDocumentsDirectory();
  auto config_dir_status = util::PlatformPaths::GetConfigDirectory();
  if (docs_dir_status.ok()) {
    settings_file_path_ = (*docs_dir_status / "settings.json").string();
  } else if (config_dir_status.ok()) {
    settings_file_path_ = (*config_dir_status / "settings.json").string();
  } else {
    LOG_WARN("UserSettings",
             "Could not determine user documents or config directory. Using local settings.json.");
    settings_file_path_ = "settings.json";
  }

  if (config_dir_status.ok()) {
    legacy_settings_file_path_ =
        (*config_dir_status / "yaze_settings.ini").string();
  } else {
    legacy_settings_file_path_ = "yaze_settings.ini";
  }
}

absl::Status UserSettings::Load() {
  try {
    bool loaded = false;
#ifdef YAZE_WITH_JSON
    bool json_exists = util::PlatformPaths::Exists(settings_file_path_);
    if (json_exists) {
      auto status =
          LoadPreferencesFromJson(settings_file_path_, &prefs_);
      if (status.ok()) {
        loaded = true;
      } else {
        LOG_WARN("UserSettings", "Failed to load settings.json: %s",
                 status.ToString().c_str());
      }
    }
#endif

    if (!loaded && util::PlatformPaths::Exists(legacy_settings_file_path_)) {
      auto status =
          LoadPreferencesFromIni(legacy_settings_file_path_, &prefs_);
      if (!status.ok()) {
        return status;
      }
      loaded = true;
#ifdef YAZE_WITH_JSON
      if (!util::PlatformPaths::Exists(settings_file_path_)) {
        (void)SavePreferencesToJson(settings_file_path_, prefs_);
      }
#endif
    }

    if (!loaded) {
#if defined(__APPLE__) && (TARGET_OS_IPHONE == 1 || TARGET_IPHONE_SIMULATOR == 1)
      prefs_.sidebar_visible = false;
      prefs_.sidebar_panel_expanded = false;
#endif
      LOG_INFO("UserSettings",
               "Settings not found, creating defaults at: %s",
               settings_file_path_.c_str());
      return Save();
    }

#ifdef YAZE_WITH_JSON
    EnsureDefaultAiHosts(&prefs_);
    EnsureDefaultAiProfiles(&prefs_);
    EnsureDefaultFilesystemRoots(&prefs_);
#endif

    ImGui::GetIO().FontGlobalScale = prefs_.font_global_scale;
  } catch (const std::exception& e) {
    return absl::InternalError(
        absl::StrFormat("Failed to load user settings: %s", e.what()));
  }
  return absl::OkStatus();
}

absl::Status UserSettings::Save() {
  try {
    absl::Status status = absl::OkStatus();
#ifdef YAZE_WITH_JSON
    status = SavePreferencesToJson(settings_file_path_, prefs_);
    if (!status.ok()) {
      return status;
    }
#endif
    status = SavePreferencesToIni(legacy_settings_file_path_, prefs_);
    if (!status.ok()) {
      return status;
    }
  } catch (const std::exception& e) {
    return absl::InternalError(
        absl::StrFormat("Failed to save user settings: %s", e.what()));
  }
  return absl::OkStatus();
}

}  // namespace editor
}  // namespace yaze
