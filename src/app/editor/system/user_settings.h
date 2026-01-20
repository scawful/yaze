#ifndef YAZE_APP_EDITOR_SYSTEM_USER_SETTINGS_H_
#define YAZE_APP_EDITOR_SYSTEM_USER_SETTINGS_H_

#include <string>
#include <unordered_map>

#include "absl/status/status.h"

namespace yaze {
namespace editor {

/**
 * @brief Manages user preferences and settings persistence
 */
class UserSettings {
 public:
  struct Preferences {
    // General
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
    bool prefer_hmagic_sprite_names = true;

    // Editor Behavior
    bool backup_before_save = true;
    int default_editor = 0;  // 0=None, 1=Overworld, 2=Dungeon, 3=Graphics

    // Performance
    bool vsync = true;
    int target_fps = 60;
    int cache_size_mb = 512;
    int undo_history_size = 50;

    // AI Agent
    int ai_provider = 0;  // 0=Ollama, 1=Gemini, 2=Mock
    std::string ollama_url = "http://localhost:11434";
    std::string gemini_api_key;
    float ai_temperature = 0.7f;
    int ai_max_tokens = 2048;
    bool ai_proactive = true;
    bool ai_auto_learn = true;
    bool ai_multimodal = true;

    // CLI Logging
    int log_level = 1;  // 0=Debug, 1=Info, 2=Warning, 3=Error, 4=Fatal
    bool log_to_file = false;
    std::string log_file_path;
    bool log_ai_requests = true;
    bool log_rom_operations = true;
    bool log_gui_automation = true;
    bool log_proposals = true;

    // Shortcut Overrides
    // Maps panel_id -> shortcut string (e.g., "dungeon.room_selector" -> "Ctrl+Shift+R")
    std::unordered_map<std::string, std::string> panel_shortcuts;
    // Maps global action id -> shortcut string (e.g., "Open", "Save")
    std::unordered_map<std::string, std::string> global_shortcuts;
    // Maps editor-scoped action id -> shortcut string (keyed by editor namespace)
    std::unordered_map<std::string, std::string> editor_shortcuts;

    // Sidebar State
    bool sidebar_visible = true;         // Controls Activity Bar visibility
    bool sidebar_panel_expanded = true;  // Controls Side Panel visibility
    std::string sidebar_active_category; // Last active category

    // Status Bar
    bool show_status_bar = false;  // Show status bar at bottom (disabled by default)

    // Panel Visibility State (per editor type)
    // Maps editor_type_name -> (panel_id -> visible)
    // e.g., "Overworld" -> {"overworld.tile16_selector" -> true, ...}
    std::unordered_map<std::string, std::unordered_map<std::string, bool>>
        panel_visibility_state;

    // Pinned panels (persists across sessions)
    // Maps panel_id -> pinned
    std::unordered_map<std::string, bool> pinned_panels;

    // Named layouts (user-saved workspace configurations)
    // Maps layout_name -> (panel_id -> visible)
    std::unordered_map<std::string, std::unordered_map<std::string, bool>>
        saved_layouts;
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
