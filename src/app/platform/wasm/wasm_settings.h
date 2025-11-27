#ifndef YAZE_APP_PLATFORM_WASM_SETTINGS_H_
#define YAZE_APP_PLATFORM_WASM_SETTINGS_H_

#ifdef __EMSCRIPTEN__

#include <string>
#include <vector>
#include <chrono>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "nlohmann/json.hpp"

namespace yaze {
namespace platform {

/**
 * @class WasmSettings
 * @brief Browser-based settings persistence for WASM builds
 *
 * This class provides persistent storage for user preferences, recent files,
 * workspace layouts, and undo history using browser localStorage and IndexedDB.
 * All methods are static and thread-safe.
 */
class WasmSettings {
 public:
  // Theme Management

  /**
   * @brief Save the current theme selection
   * @param theme Theme name (e.g., "dark", "light", "classic")
   * @return Status indicating success or failure
   */
  static absl::Status SaveTheme(const std::string& theme);

  /**
   * @brief Load the saved theme selection
   * @return Theme name or default if not found
   */
  static std::string LoadTheme();

  /**
   * @brief Get the full JSON data for the current theme
   * @return JSON string containing all theme colors and style settings
   */
  static std::string GetCurrentThemeData();

  /**
   * @brief Load a user-provided font from binary data
   * @param name Font name
   * @param data Binary font data (TTF/OTF)
   * @param size Font size in pixels
   */
  static absl::Status LoadUserFont(const std::string& name,
                                   const std::string& data, float size);

  // Recent Files Management

  /**
   * @brief Add a file to the recent files list
   * @param filename Name/identifier of the file
   * @param timestamp Optional timestamp (uses current time if not provided)
   * @return Status indicating success or failure
   */
  static absl::Status AddRecentFile(
      const std::string& filename,
      std::chrono::system_clock::time_point timestamp =
          std::chrono::system_clock::now());

  /**
   * @brief Get the list of recent files
   * @param max_count Maximum number of files to return (default 10)
   * @return Vector of recent file names, newest first
   */
  static std::vector<std::string> GetRecentFiles(size_t max_count = 10);

  /**
   * @brief Clear the recent files list
   * @return Status indicating success or failure
   */
  static absl::Status ClearRecentFiles();

  /**
   * @brief Remove a specific file from the recent files list
   * @param filename Name of the file to remove
   * @return Status indicating success or failure
   */
  static absl::Status RemoveRecentFile(const std::string& filename);

  // Workspace Layout Management

  /**
   * @brief Save a workspace layout configuration
   * @param name Workspace name (e.g., "default", "debugging", "art")
   * @param layout_json JSON string containing layout configuration
   * @return Status indicating success or failure
   */
  static absl::Status SaveWorkspace(const std::string& name,
                                    const std::string& layout_json);

  /**
   * @brief Load a workspace layout configuration
   * @param name Workspace name to load
   * @return JSON string containing layout or error
   */
  static absl::StatusOr<std::string> LoadWorkspace(const std::string& name);

  /**
   * @brief List all saved workspace names
   * @return Vector of workspace names
   */
  static std::vector<std::string> ListWorkspaces();

  /**
   * @brief Delete a workspace layout
   * @param name Workspace name to delete
   * @return Status indicating success or failure
   */
  static absl::Status DeleteWorkspace(const std::string& name);

  /**
   * @brief Set the active workspace
   * @param name Name of the workspace to make active
   * @return Status indicating success or failure
   */
  static absl::Status SetActiveWorkspace(const std::string& name);

  /**
   * @brief Get the name of the active workspace
   * @return Name of active workspace or "default" if none set
   */
  static std::string GetActiveWorkspace();

  // Undo History Persistence (for crash recovery)

  /**
   * @brief Save undo history for an editor
   * @param editor_id Editor identifier (e.g., "overworld", "dungeon")
   * @param history Serialized undo history data
   * @return Status indicating success or failure
   */
  static absl::Status SaveUndoHistory(const std::string& editor_id,
                                      const std::vector<uint8_t>& history);

  /**
   * @brief Load undo history for an editor
   * @param editor_id Editor identifier
   * @return Undo history data or error
   */
  static absl::StatusOr<std::vector<uint8_t>> LoadUndoHistory(
      const std::string& editor_id);

  /**
   * @brief Clear undo history for an editor
   * @param editor_id Editor identifier
   * @return Status indicating success or failure
   */
  static absl::Status ClearUndoHistory(const std::string& editor_id);

  /**
   * @brief Clear all undo histories
   * @return Status indicating success or failure
   */
  static absl::Status ClearAllUndoHistory();

  // General Settings

  /**
   * @brief Save a general setting value
   * @param key Setting key
   * @param value Setting value as JSON
   * @return Status indicating success or failure
   */
  static absl::Status SaveSetting(const std::string& key,
                                  const nlohmann::json& value);

  /**
   * @brief Load a general setting value
   * @param key Setting key
   * @return Setting value as JSON or error
   */
  static absl::StatusOr<nlohmann::json> LoadSetting(const std::string& key);

  /**
   * @brief Check if a setting exists
   * @param key Setting key
   * @return true if setting exists
   */
  static bool HasSetting(const std::string& key);

  /**
   * @brief Save all settings as a batch
   * @param settings JSON object containing all settings
   * @return Status indicating success or failure
   */
  static absl::Status SaveAllSettings(const nlohmann::json& settings);

  /**
   * @brief Load all settings
   * @return JSON object containing all settings or error
   */
  static absl::StatusOr<nlohmann::json> LoadAllSettings();

  /**
   * @brief Clear all settings (reset to defaults)
   * @return Status indicating success or failure
   */
  static absl::Status ClearAllSettings();

  // Utility

  /**
   * @brief Export all settings to a JSON string for backup
   * @return JSON string containing all settings and data
   */
  static absl::StatusOr<std::string> ExportSettings();

  /**
   * @brief Import settings from a JSON string
   * @param json_str JSON string containing settings to import
   * @return Status indicating success or failure
   */
  static absl::Status ImportSettings(const std::string& json_str);

  /**
   * @brief Get the last time settings were saved
   * @return Timestamp of last save or error
   */
  static absl::StatusOr<std::chrono::system_clock::time_point> GetLastSaveTime();

 private:
  // Storage keys for localStorage
  static constexpr const char* kThemeKey = "yaze_theme";
  static constexpr const char* kRecentFilesKey = "yaze_recent_files";
  static constexpr const char* kActiveWorkspaceKey = "yaze_active_workspace";
  static constexpr const char* kSettingsPrefix = "yaze_setting_";
  static constexpr const char* kLastSaveTimeKey = "yaze_last_save_time";

  // Storage keys for IndexedDB (via WasmStorage)
  static constexpr const char* kWorkspacePrefix = "workspace_";
  static constexpr const char* kUndoHistoryPrefix = "undo_";

  // Helper structure for recent files
  struct RecentFile {
    std::string filename;
    std::chrono::system_clock::time_point timestamp;
  };

  // Helper to serialize/deserialize recent files
  static nlohmann::json RecentFilesToJson(const std::vector<RecentFile>& files);
  static std::vector<RecentFile> JsonToRecentFiles(const nlohmann::json& json);

  // Prevent instantiation
  WasmSettings() = delete;
  ~WasmSettings() = delete;
};

}  // namespace platform
}  // namespace yaze

#endif  // __EMSCRIPTEN__

#endif  // YAZE_APP_PLATFORM_WASM_SETTINGS_H_