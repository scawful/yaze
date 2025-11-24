#ifndef YAZE_APP_PLATFORM_WASM_WASM_STORAGE_H_
#define YAZE_APP_PLATFORM_WASM_WASM_STORAGE_H_

#ifdef __EMSCRIPTEN__

#include <atomic>
#include <functional>
#include <string>
#include <vector>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "nlohmann/json.hpp"

namespace yaze {
namespace platform {

/**
 * @class WasmStorage
 * @brief WASM storage implementation using Emscripten IndexedDB
 *
 * This class provides persistent storage in the browser using IndexedDB
 * for ROM files, project data, and user preferences.
 *
 * All operations are asynchronous but exposed as synchronous for ease of use.
 * The implementation uses condition variables to wait for completion.
 */
class WasmStorage {
 public:
  // ROM Storage Operations

  /**
   * @brief Save ROM data to IndexedDB
   * @param name Unique name/identifier for the ROM
   * @param data Binary ROM data
   * @return Status indicating success or failure
   */
  static absl::Status SaveRom(const std::string& name,
                              const std::vector<uint8_t>& data);

  /**
   * @brief Load ROM data from IndexedDB
   * @param name Name of the ROM to load
   * @return ROM data or error status
   */
  static absl::StatusOr<std::vector<uint8_t>> LoadRom(const std::string& name);

  /**
   * @brief Delete a ROM from IndexedDB
   * @param name Name of the ROM to delete
   * @return Status indicating success or failure
   */
  static absl::Status DeleteRom(const std::string& name);

  /**
   * @brief List all saved ROM names
   * @return Vector of ROM names in storage
   */
  static std::vector<std::string> ListRoms();

  // Project Storage Operations

  /**
   * @brief Save project JSON data
   * @param name Project name/identifier
   * @param json JSON string containing project data
   * @return Status indicating success or failure
   */
  static absl::Status SaveProject(const std::string& name,
                                  const std::string& json);

  /**
   * @brief Load project JSON data
   * @param name Project name to load
   * @return JSON string or error status
   */
  static absl::StatusOr<std::string> LoadProject(const std::string& name);

  /**
   * @brief Delete a project from storage
   * @param name Project name to delete
   * @return Status indicating success or failure
   */
  static absl::Status DeleteProject(const std::string& name);

  /**
   * @brief List all saved project names
   * @return Vector of project names in storage
   */
  static std::vector<std::string> ListProjects();

  // User Preferences Storage

  /**
   * @brief Save user preferences as JSON
   * @param prefs JSON object containing preferences
   * @return Status indicating success or failure
   */
  static absl::Status SavePreferences(const nlohmann::json& prefs);

  /**
   * @brief Load user preferences
   * @return JSON preferences or error status
   */
  static absl::StatusOr<nlohmann::json> LoadPreferences();

  /**
   * @brief Clear all preferences
   * @return Status indicating success or failure
   */
  static absl::Status ClearPreferences();

  // Utility Operations

  /**
   * @brief Get total storage used (in bytes)
   * @return Total bytes used or error status
   */
  static absl::StatusOr<size_t> GetStorageUsage();

  /**
   * @brief Check if storage is available and initialized
   * @return true if IndexedDB is available and ready
   */
  static bool IsStorageAvailable();

  /**
   * @brief Initialize IndexedDB (called automatically on first use)
   * @return Status indicating success or failure
   */
  static absl::Status Initialize();

 private:
  // Database constants
  static constexpr const char* kDatabaseName = "YazeStorage";
  static constexpr int kDatabaseVersion = 1;
  static constexpr const char* kRomStoreName = "roms";
  static constexpr const char* kProjectStoreName = "projects";
  static constexpr const char* kPreferencesStoreName = "preferences";
  static constexpr const char* kPreferencesKey = "user_preferences";

  // Internal helper for async operations
  struct AsyncResult {
    bool completed = false;
    bool success = false;
    std::string error_message;
    std::vector<uint8_t> binary_data;
    std::string string_data;
    std::vector<std::string> string_list;
  };

  // Ensure database is initialized
  static void EnsureInitialized();

  // Check if we're running in a web context
  static bool IsWebContext();

  // Database initialized flag (thread-safe)
  static std::atomic<bool> initialized_;
};

}  // namespace platform
}  // namespace yaze

#endif  // __EMSCRIPTEN__

#endif  // YAZE_APP_PLATFORM_WASM_WASM_STORAGE_H_