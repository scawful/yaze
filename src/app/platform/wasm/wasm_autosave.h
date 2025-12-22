#ifndef YAZE_APP_PLATFORM_WASM_AUTOSAVE_H_
#define YAZE_APP_PLATFORM_WASM_AUTOSAVE_H_

#ifdef __EMSCRIPTEN__

#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "nlohmann/json.hpp"

namespace yaze {
namespace platform {

/**
 * @class AutoSaveManager
 * @brief Manages automatic saving and crash recovery for WASM builds
 *
 * This class provides periodic auto-save functionality, emergency save on
 * page unload, and session recovery after crashes or unexpected closures.
 * It integrates with WasmStorage for data persistence and uses browser
 * event handlers for lifecycle management.
 */
class AutoSaveManager {
 public:
  // Callback types
  using SaveCallback = std::function<nlohmann::json()>;
  using RestoreCallback = std::function<void(const nlohmann::json&)>;

  /**
   * @brief Get the singleton instance of AutoSaveManager
   * @return Reference to the AutoSaveManager instance
   */
  static AutoSaveManager& Instance();

  /**
   * @brief Start periodic auto-save
   * @param interval_seconds Interval between saves (default 60 seconds)
   * @return Status indicating success or failure
   */
  absl::Status Start(int interval_seconds = 60);

  /**
   * @brief Stop auto-save
   * @return Status indicating success or failure
   */
  absl::Status Stop();

  /**
   * @brief Check if auto-save is currently running
   * @return true if auto-save is active
   */
  bool IsRunning() const;

  /**
   * @brief Register a component for auto-save
   * @param component_id Unique identifier for the component
   * @param save_fn Function that returns JSON data to save
   * @param restore_fn Function that accepts JSON data to restore
   */
  void RegisterComponent(const std::string& component_id,
                         SaveCallback save_fn,
                         RestoreCallback restore_fn);

  /**
   * @brief Unregister a component from auto-save
   * @param component_id Component identifier to unregister
   */
  void UnregisterComponent(const std::string& component_id);

  /**
   * @brief Manually trigger a save of all registered components
   * @return Status indicating success or failure
   */
  absl::Status SaveNow();

  /**
   * @brief Save data immediately (called on page unload)
   * @note This is called automatically by the browser event handler
   */
  void EmergencySave();

  /**
   * @brief Check if there's recovery data available
   * @return true if recovery data exists
   */
  bool HasRecoveryData();

  /**
   * @brief Get information about available recovery data
   * @return JSON object with recovery metadata (timestamp, components, etc.)
   */
  absl::StatusOr<nlohmann::json> GetRecoveryInfo();

  /**
   * @brief Recover the last saved session
   * @return Status indicating success or failure
   */
  absl::Status RecoverLastSession();

  /**
   * @brief Clear all recovery data
   * @return Status indicating success or failure
   */
  absl::Status ClearRecoveryData();

  /**
   * @brief Set the auto-save interval
   * @param seconds New interval in seconds
   * @note Restarts auto-save if currently running
   */
  void SetInterval(int seconds);

  /**
   * @brief Get the current auto-save interval
   * @return Interval in seconds
   */
  int GetInterval() const { return interval_seconds_; }

  /**
   * @brief Enable or disable auto-save
   * @param enabled true to enable, false to disable
   */
  void SetEnabled(bool enabled);

  /**
   * @brief Check if auto-save is enabled
   * @return true if enabled
   */
  bool IsEnabled() const { return enabled_; }

  /**
   * @brief Get the last auto-save timestamp
   * @return Timestamp of last successful save
   */
  std::chrono::system_clock::time_point GetLastSaveTime() const {
    return last_save_time_;
  }

  /**
   * @brief Get statistics about auto-save operations
   * @return JSON object with save count, error count, etc.
   */
  nlohmann::json GetStatistics() const;

 private:
  // Private constructor for singleton
  AutoSaveManager();
  ~AutoSaveManager();

  // Delete copy and move constructors
  AutoSaveManager(const AutoSaveManager&) = delete;
  AutoSaveManager& operator=(const AutoSaveManager&) = delete;
  AutoSaveManager(AutoSaveManager&&) = delete;
  AutoSaveManager& operator=(AutoSaveManager&&) = delete;

  // Component registration
  struct Component {
    SaveCallback save_fn;
    RestoreCallback restore_fn;
  };

  // Internal methods
  void InitializeEventHandlers();
  void CleanupEventHandlers();
  static void TimerCallback(void* user_data);
  absl::Status PerformSave();
  nlohmann::json CollectComponentData();
  absl::Status SaveToStorage(const nlohmann::json& data);
  absl::StatusOr<nlohmann::json> LoadFromStorage();

  // Storage keys
  static constexpr const char* kAutoSaveDataKey = "yaze_autosave_data";
  static constexpr const char* kAutoSaveMetaKey = "yaze_autosave_meta";
  static constexpr const char* kRecoveryFlagKey = "yaze_has_recovery";

  // Member variables
  mutable std::mutex mutex_;
  std::unordered_map<std::string, Component> components_;
  int interval_seconds_;
  bool enabled_;
  bool running_;
  int timer_id_;
  std::chrono::system_clock::time_point last_save_time_;

  // Statistics
  size_t save_count_;
  size_t error_count_;
  size_t recovery_count_;

  // Event handler registration state
  bool event_handlers_initialized_;

 public:
  // Must be public for emergency save callback access
  static bool emergency_save_triggered_;
};

}  // namespace platform
}  // namespace yaze

#endif  // __EMSCRIPTEN__

#endif  // YAZE_APP_PLATFORM_WASM_AUTOSAVE_H_