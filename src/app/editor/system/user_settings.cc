#include "app/editor/system/user_settings.h"

#include <fstream>
#include <sstream>

#include "absl/strings/str_format.h"
#include "app/gui/core/style.h"
#include "imgui/imgui.h"
#include "util/file_util.h"
#include "util/log.h"
#include "util/platform_paths.h"

namespace yaze {
namespace editor {

UserSettings::UserSettings() {
  auto config_dir_status = util::PlatformPaths::GetConfigDirectory();
  if (config_dir_status.ok()) {
    settings_file_path_ = (*config_dir_status / "yaze_settings.ini").string();
  } else {
    LOG_WARN("UserSettings",
             "Could not determine config directory. Using local.");
    settings_file_path_ = "yaze_settings.ini";
  }
}

absl::Status UserSettings::Load() {
  try {
    auto data = util::LoadFile(settings_file_path_);
    if (data.empty()) {
      return absl::OkStatus();  // No settings file yet, use defaults.
    }

    std::istringstream ss(data);
    std::string line;
    while (std::getline(ss, line)) {
      size_t eq_pos = line.find('=');
      if (eq_pos == std::string::npos) continue;

      std::string key = line.substr(0, eq_pos);
      std::string val = line.substr(eq_pos + 1);

      // General
      if (key == "font_global_scale") {
        prefs_.font_global_scale = std::stof(val);
      } else if (key == "backup_rom") {
        prefs_.backup_rom = (val == "1");
      } else if (key == "save_new_auto") {
        prefs_.save_new_auto = (val == "1");
      } else if (key == "autosave_enabled") {
        prefs_.autosave_enabled = (val == "1");
      } else if (key == "autosave_interval") {
        prefs_.autosave_interval = std::stof(val);
      } else if (key == "recent_files_limit") {
        prefs_.recent_files_limit = std::stoi(val);
      } else if (key == "last_rom_path") {
        prefs_.last_rom_path = val;
      } else if (key == "last_project_path") {
        prefs_.last_project_path = val;
      } else if (key == "show_welcome_on_startup") {
        prefs_.show_welcome_on_startup = (val == "1");
      } else if (key == "restore_last_session") {
        prefs_.restore_last_session = (val == "1");
      }
      // Editor Behavior
      else if (key == "backup_before_save") {
        prefs_.backup_before_save = (val == "1");
      } else if (key == "default_editor") {
        prefs_.default_editor = std::stoi(val);
      }
      // Performance
      else if (key == "vsync") {
        prefs_.vsync = (val == "1");
      } else if (key == "target_fps") {
        prefs_.target_fps = std::stoi(val);
      } else if (key == "cache_size_mb") {
        prefs_.cache_size_mb = std::stoi(val);
      } else if (key == "undo_history_size") {
        prefs_.undo_history_size = std::stoi(val);
      }
      // AI Agent
      else if (key == "ai_provider") {
        prefs_.ai_provider = std::stoi(val);
      } else if (key == "ollama_url") {
        prefs_.ollama_url = val;
      } else if (key == "gemini_api_key") {
        prefs_.gemini_api_key = val;
      } else if (key == "ai_temperature") {
        prefs_.ai_temperature = std::stof(val);
      } else if (key == "ai_max_tokens") {
        prefs_.ai_max_tokens = std::stoi(val);
      } else if (key == "ai_proactive") {
        prefs_.ai_proactive = (val == "1");
      } else if (key == "ai_auto_learn") {
        prefs_.ai_auto_learn = (val == "1");
      } else if (key == "ai_multimodal") {
        prefs_.ai_multimodal = (val == "1");
      }
      // CLI Logging
      else if (key == "log_level") {
        prefs_.log_level = std::stoi(val);
      } else if (key == "log_to_file") {
        prefs_.log_to_file = (val == "1");
      } else if (key == "log_file_path") {
        prefs_.log_file_path = val;
      } else if (key == "log_ai_requests") {
        prefs_.log_ai_requests = (val == "1");
      } else if (key == "log_rom_operations") {
        prefs_.log_rom_operations = (val == "1");
      } else if (key == "log_gui_automation") {
        prefs_.log_gui_automation = (val == "1");
      } else if (key == "log_proposals") {
        prefs_.log_proposals = (val == "1");
      }
    }
    ImGui::GetIO().FontGlobalScale = prefs_.font_global_scale;
  } catch (const std::exception& e) {
    return absl::InternalError(
        absl::StrFormat("Failed to load user settings: %s", e.what()));
  }
  return absl::OkStatus();
}

absl::Status UserSettings::Save() {
  try {
    std::ostringstream ss;
    // General
    ss << "font_global_scale=" << prefs_.font_global_scale << "\n";
    ss << "backup_rom=" << (prefs_.backup_rom ? 1 : 0) << "\n";
    ss << "save_new_auto=" << (prefs_.save_new_auto ? 1 : 0) << "\n";
    ss << "autosave_enabled=" << (prefs_.autosave_enabled ? 1 : 0) << "\n";
    ss << "autosave_interval=" << prefs_.autosave_interval << "\n";
    ss << "recent_files_limit=" << prefs_.recent_files_limit << "\n";
    ss << "last_rom_path=" << prefs_.last_rom_path << "\n";
    ss << "last_project_path=" << prefs_.last_project_path << "\n";
    ss << "show_welcome_on_startup=" << (prefs_.show_welcome_on_startup ? 1 : 0)
       << "\n";
    ss << "restore_last_session=" << (prefs_.restore_last_session ? 1 : 0)
       << "\n";

    // Editor Behavior
    ss << "backup_before_save=" << (prefs_.backup_before_save ? 1 : 0) << "\n";
    ss << "default_editor=" << prefs_.default_editor << "\n";

    // Performance
    ss << "vsync=" << (prefs_.vsync ? 1 : 0) << "\n";
    ss << "target_fps=" << prefs_.target_fps << "\n";
    ss << "cache_size_mb=" << prefs_.cache_size_mb << "\n";
    ss << "undo_history_size=" << prefs_.undo_history_size << "\n";

    // AI Agent
    ss << "ai_provider=" << prefs_.ai_provider << "\n";
    ss << "ollama_url=" << prefs_.ollama_url << "\n";
    ss << "gemini_api_key=" << prefs_.gemini_api_key << "\n";
    ss << "ai_temperature=" << prefs_.ai_temperature << "\n";
    ss << "ai_max_tokens=" << prefs_.ai_max_tokens << "\n";
    ss << "ai_proactive=" << (prefs_.ai_proactive ? 1 : 0) << "\n";
    ss << "ai_auto_learn=" << (prefs_.ai_auto_learn ? 1 : 0) << "\n";
    ss << "ai_multimodal=" << (prefs_.ai_multimodal ? 1 : 0) << "\n";

    // CLI Logging
    ss << "log_level=" << prefs_.log_level << "\n";
    ss << "log_to_file=" << (prefs_.log_to_file ? 1 : 0) << "\n";
    ss << "log_file_path=" << prefs_.log_file_path << "\n";
    ss << "log_ai_requests=" << (prefs_.log_ai_requests ? 1 : 0) << "\n";
    ss << "log_rom_operations=" << (prefs_.log_rom_operations ? 1 : 0) << "\n";
    ss << "log_gui_automation=" << (prefs_.log_gui_automation ? 1 : 0) << "\n";
    ss << "log_proposals=" << (prefs_.log_proposals ? 1 : 0) << "\n";

    util::SaveFile(settings_file_path_, ss.str());
  } catch (const std::exception& e) {
    return absl::InternalError(
        absl::StrFormat("Failed to save user settings: %s", e.what()));
  }
  return absl::OkStatus();
}

}  // namespace editor
}  // namespace yaze
