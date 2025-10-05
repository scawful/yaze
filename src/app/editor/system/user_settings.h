#ifndef YAZE_APP_EDITOR_SYSTEM_USER_SETTINGS_H_
#define YAZE_APP_EDITOR_SYSTEM_USER_SETTINGS_H_

#include <string>
#include "absl/status/status.h"

namespace yaze {
namespace editor {

/**
 * @brief Manages user preferences and settings persistence
 */
class UserSettings {
 public:
  struct Preferences {
    float font_global_scale = 1.0f;
    bool backup_rom = false;
    bool save_new_auto = true;
    bool autosave_enabled = true;
    float autosave_interval = 300.0f;
    int recent_files_limit = 10;
    std::string last_rom_path;
    std::string last_project_path;
    bool show_welcome_on_startup = true;
    bool restore_last_session = true;
  };
  
  UserSettings();
  
  absl::Status Load();
  absl::Status Save();
  
  Preferences& prefs() { return prefs_; }
  const Preferences& prefs() const { return prefs_; }
  
 private:
  Preferences prefs_;
  std::string settings_file_path_;
};

}  // namespace editor
}  // namespace yaze

#endif  // YAZE_APP_EDITOR_SYSTEM_USER_SETTINGS_H_
