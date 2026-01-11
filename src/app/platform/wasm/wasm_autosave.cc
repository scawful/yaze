#ifdef __EMSCRIPTEN__

#include "app/platform/wasm/wasm_autosave.h"

#include <emscripten.h>
#include <emscripten/html5.h>  // For emscripten_set_timeout/clear_timeout
#include <chrono>
#include <ctime>

#include "absl/strings/str_format.h"
#include "app/platform/wasm/wasm_config.h"
#include "app/platform/wasm/wasm_storage.h"

namespace yaze {
namespace platform {

using ::yaze::app::platform::WasmConfig;

// Static member initialization
bool AutoSaveManager::emergency_save_triggered_ = false;

// JavaScript event handler registration using EM_JS
EM_JS(void, register_beforeunload_handler, (), {
  // Store handler references for cleanup
  if (!window._yazeAutoSaveHandlers) {
    window._yazeAutoSaveHandlers = {};
  }

  // Register beforeunload event handler
  window._yazeAutoSaveHandlers.beforeunload = function(event) {
    // Call the C++ emergency save function
    if (Module._yazeEmergencySave) {
      Module._yazeEmergencySave();
    }

    // Some browsers require returnValue to be set
    event.returnValue =
        'You have unsaved changes. Are you sure you want to leave?';
    return event.returnValue;
  };
  window.addEventListener('beforeunload',
                          window._yazeAutoSaveHandlers.beforeunload);

  // Register visibilitychange event for when tab becomes hidden
  window._yazeAutoSaveHandlers.visibilitychange = function() {
    if (document.hidden && Module._yazeEmergencySave) {
      // Save when tab becomes hidden (user switches tabs or minimizes)
      Module._yazeEmergencySave();
    }
  };
  document.addEventListener('visibilitychange',
                            window._yazeAutoSaveHandlers.visibilitychange);

  // Register pagehide event as backup
  window._yazeAutoSaveHandlers.pagehide = function(event) {
    if (Module._yazeEmergencySave) {
      Module._yazeEmergencySave();
    }
  };
  window.addEventListener('pagehide', window._yazeAutoSaveHandlers.pagehide);

  console.log('AutoSave event handlers registered');
});

EM_JS(void, unregister_beforeunload_handler, (), {
  // Remove all event listeners using stored references
  if (window._yazeAutoSaveHandlers) {
    if (window._yazeAutoSaveHandlers.beforeunload) {
      window.removeEventListener('beforeunload',
                                 window._yazeAutoSaveHandlers.beforeunload);
    }
    if (window._yazeAutoSaveHandlers.visibilitychange) {
      document.removeEventListener(
          'visibilitychange', window._yazeAutoSaveHandlers.visibilitychange);
    }
    if (window._yazeAutoSaveHandlers.pagehide) {
      window.removeEventListener('pagehide',
                                 window._yazeAutoSaveHandlers.pagehide);
    }
    window._yazeAutoSaveHandlers = null;
  }
  console.log('AutoSave event handlers unregistered');
});

EM_JS(void, set_recovery_flag, (int has_recovery), {
  try {
    if (has_recovery) {
      sessionStorage.setItem('yaze_has_recovery', 'true');
    } else {
      sessionStorage.removeItem('yaze_has_recovery');
    }
  } catch (e) {
    console.error('Failed to set recovery flag:', e);
  }
});

EM_JS(int, get_recovery_flag, (), {
  try {
    return sessionStorage.getItem('yaze_has_recovery') == 'true' ? 1 : 0;
  } catch (e) {
    console.error('Failed to get recovery flag:', e);
    return 0;
  }
});

// C functions exposed to JavaScript for emergency save and recovery
extern "C" {
EMSCRIPTEN_KEEPALIVE
void yazeEmergencySave() {
  if (!AutoSaveManager::emergency_save_triggered_) {
    AutoSaveManager::emergency_save_triggered_ = true;
    AutoSaveManager::Instance().EmergencySave();
  }
}

EMSCRIPTEN_KEEPALIVE
int yazeRecoverSession() {
  auto status = AutoSaveManager::Instance().RecoverLastSession();
  if (status.ok()) {
    emscripten_log(EM_LOG_INFO, "Session recovery successful");
    return 1;
  } else {
    emscripten_log(EM_LOG_WARN, "Session recovery failed: %s",
                   std::string(status.message()).c_str());
    return 0;
  }
}

EMSCRIPTEN_KEEPALIVE
int yazeHasRecoveryData() {
  return AutoSaveManager::Instance().HasRecoveryData() ? 1 : 0;
}

EMSCRIPTEN_KEEPALIVE
void yazeClearRecoveryData() {
  AutoSaveManager::Instance().ClearRecoveryData();
}
}

// AutoSaveManager implementation

AutoSaveManager::AutoSaveManager()
    : interval_seconds_(
          app::platform::WasmConfig::Get().autosave.interval_seconds),
      enabled_(true),
      running_(false),
      timer_id_(-1),
      save_count_(0),
      error_count_(0),
      recovery_count_(0),
      event_handlers_initialized_(false) {
  // Set the JavaScript function pointer
  EM_ASM({
    Module._yazeEmergencySave = Module.cwrap('yazeEmergencySave', null, []);
  });
}

AutoSaveManager::~AutoSaveManager() {
  Stop();
  CleanupEventHandlers();
}

AutoSaveManager& AutoSaveManager::Instance() {
  static AutoSaveManager instance;
  return instance;
}

void AutoSaveManager::InitializeEventHandlers() {
  if (!event_handlers_initialized_) {
    register_beforeunload_handler();
    event_handlers_initialized_ = true;
  }
}

void AutoSaveManager::CleanupEventHandlers() {
  if (event_handlers_initialized_) {
    unregister_beforeunload_handler();
    event_handlers_initialized_ = false;
  }
}

void AutoSaveManager::TimerCallback(void* user_data) {
  AutoSaveManager* manager = static_cast<AutoSaveManager*>(user_data);
  if (manager && manager->IsRunning()) {
    auto status = manager->PerformSave();
    if (!status.ok()) {
      emscripten_log(EM_LOG_WARN, "Auto-save failed: %s",
                     status.ToString().c_str());
    }

    // Reschedule the timer
    if (manager->IsRunning()) {
      manager->timer_id_ = emscripten_set_timeout(
          TimerCallback, manager->interval_seconds_ * 1000, manager);
    }
  }
}

absl::Status AutoSaveManager::Start(int interval_seconds) {
  std::lock_guard<std::mutex> lock(mutex_);

  if (running_) {
    return absl::AlreadyExistsError("Auto-save is already running");
  }

  interval_seconds_ = interval_seconds;
  InitializeEventHandlers();

  // Start the timer
  timer_id_ =
      emscripten_set_timeout(TimerCallback, interval_seconds_ * 1000, this);

  running_ = true;
  emscripten_log(EM_LOG_INFO, "Auto-save started with %d second interval",
                 interval_seconds);

  return absl::OkStatus();
}

absl::Status AutoSaveManager::Stop() {
  std::lock_guard<std::mutex> lock(mutex_);

  if (!running_) {
    return absl::OkStatus();
  }

  // Cancel the timer
  if (timer_id_ >= 0) {
    emscripten_clear_timeout(timer_id_);
    timer_id_ = -1;
  }

  running_ = false;
  emscripten_log(EM_LOG_INFO, "Auto-save stopped");

  return absl::OkStatus();
}

bool AutoSaveManager::IsRunning() const {
  std::lock_guard<std::mutex> lock(mutex_);
  return running_;
}

void AutoSaveManager::RegisterComponent(const std::string& component_id,
                                        SaveCallback save_fn,
                                        RestoreCallback restore_fn) {
  std::lock_guard<std::mutex> lock(mutex_);
  components_[component_id] = {save_fn, restore_fn};
  emscripten_log(EM_LOG_INFO, "Registered component for auto-save: %s",
                 component_id.c_str());
}

void AutoSaveManager::UnregisterComponent(const std::string& component_id) {
  std::lock_guard<std::mutex> lock(mutex_);
  components_.erase(component_id);
  emscripten_log(EM_LOG_INFO, "Unregistered component from auto-save: %s",
                 component_id.c_str());
}

absl::Status AutoSaveManager::SaveNow() {
  std::lock_guard<std::mutex> lock(mutex_);
  return PerformSave();
}

nlohmann::json AutoSaveManager::CollectComponentData() {
  nlohmann::json data = nlohmann::json::object();

  for (const auto& [id, component] : components_) {
    try {
      if (component.save_fn) {
        data[id] = component.save_fn();
      }
    } catch (const std::exception& e) {
      emscripten_log(EM_LOG_ERROR, "Failed to save component %s: %s",
                     id.c_str(), e.what());
      error_count_++;
    }
  }

  return data;
}

absl::Status AutoSaveManager::PerformSave() {
  nlohmann::json save_data = nlohmann::json::object();

  // Collect data from all registered components
  save_data["components"] = CollectComponentData();

  // Add metadata
  auto now = std::chrono::system_clock::now();
  save_data["timestamp"] =
      std::chrono::duration_cast<std::chrono::milliseconds>(
          now.time_since_epoch())
          .count();
  save_data["version"] = 1;

  // Save to storage
  auto status = SaveToStorage(save_data);
  if (status.ok()) {
    last_save_time_ = now;
    save_count_++;
    set_recovery_flag(1);  // Mark that recovery data is available
  } else {
    error_count_++;
  }

  return status;
}

absl::Status AutoSaveManager::SaveToStorage(const nlohmann::json& data) {
  try {
    // Convert JSON to string
    std::string json_str = data.dump();

    // Save to IndexedDB via WasmStorage
    auto status = WasmStorage::SaveProject(kAutoSaveDataKey, json_str);
    if (!status.ok()) {
      return status;
    }

    // Save metadata separately for quick access
    nlohmann::json meta;
    meta["timestamp"] = data["timestamp"];
    meta["component_count"] = data["components"].size();
    meta["save_count"] = save_count_;

    return WasmStorage::SaveProject(kAutoSaveMetaKey, meta.dump());
  } catch (const std::exception& e) {
    return absl::InternalError(
        absl::StrFormat("Failed to save auto-save data: %s", e.what()));
  }
}

absl::StatusOr<nlohmann::json> AutoSaveManager::LoadFromStorage() {
  auto result = WasmStorage::LoadProject(kAutoSaveDataKey);
  if (!result.ok()) {
    return result.status();
  }

  try {
    return nlohmann::json::parse(*result);
  } catch (const std::exception& e) {
    return absl::InvalidArgumentError(
        absl::StrFormat("Failed to parse auto-save data: %s", e.what()));
  }
}

void AutoSaveManager::EmergencySave() {
  // Use try_lock to avoid blocking - emergency save should be fast
  // If we can't get the lock, another operation is in progress
  std::unique_lock<std::mutex> lock(mutex_, std::try_to_lock);

  try {
    nlohmann::json emergency_data = nlohmann::json::object();
    emergency_data["emergency"] = true;

    // Only collect component data if we got the lock
    if (lock.owns_lock()) {
      emergency_data["components"] = CollectComponentData();
    } else {
      // Can't safely access components, save empty state marker
      emergency_data["components"] = nlohmann::json::object();
      emergency_data["incomplete"] = true;
    }

    emergency_data["timestamp"] =
        std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch())
            .count();

    // Use synchronous localStorage for emergency save (faster than IndexedDB)
    EM_ASM(
        {
          try {
            const data = UTF8ToString($0);
            localStorage.setItem('yaze_emergency_save', data);
            console.log('Emergency save completed');
          } catch (e) {
            console.error('Emergency save failed:', e);
          }
        },
        emergency_data.dump().c_str());

    set_recovery_flag(1);
  } catch (...) {
    // Silently fail - we're in emergency mode
  }
}

bool AutoSaveManager::HasRecoveryData() {
  // Check both sessionStorage flag and actual data
  if (get_recovery_flag() == 0) {
    return false;
  }

  // Verify data actually exists
  auto meta = WasmStorage::LoadProject(kAutoSaveMetaKey);
  if (meta.ok()) {
    return true;
  }

  // Check for emergency save data
  int has_emergency = EM_ASM_INT(
      { return localStorage.getItem('yaze_emergency_save') != null ? 1 : 0; });

  return has_emergency == 1;
}

absl::StatusOr<nlohmann::json> AutoSaveManager::GetRecoveryInfo() {
  // First try to get regular auto-save metadata
  auto meta_result = WasmStorage::LoadProject(kAutoSaveMetaKey);
  if (meta_result.ok()) {
    try {
      return nlohmann::json::parse(*meta_result);
    } catch (...) {
      // Continue to check emergency save
    }
  }

  // Check for emergency save
  char* emergency_data = nullptr;
  EM_ASM(
      {
        const data = localStorage.getItem('yaze_emergency_save');
        if (data) {
          const len = lengthBytesUTF8(data) + 1;
          const ptr = _malloc(len);
          stringToUTF8(data, ptr, len);
          setValue($0, ptr, 'i32');
        }
      },
      &emergency_data);

  if (emergency_data) {
    try {
      nlohmann::json data = nlohmann::json::parse(emergency_data);
      free(emergency_data);

      nlohmann::json info;
      info["type"] = "emergency";
      info["timestamp"] = data["timestamp"];
      info["component_count"] = data["components"].size();
      return info;
    } catch (const std::exception& e) {
      free(emergency_data);
      return absl::InvalidArgumentError("Failed to parse emergency save data");
    }
  }

  return absl::NotFoundError("No recovery data found");
}

absl::Status AutoSaveManager::RecoverLastSession() {
  std::lock_guard<std::mutex> lock(mutex_);

  // First try regular auto-save data
  auto data_result = LoadFromStorage();
  if (data_result.ok()) {
    const auto& data = *data_result;
    if (data.contains("components") && data["components"].is_object()) {
      for (const auto& [id, component_data] : data["components"].items()) {
        auto it = components_.find(id);
        if (it != components_.end() && it->second.restore_fn) {
          try {
            it->second.restore_fn(component_data);
            emscripten_log(EM_LOG_INFO, "Restored component: %s", id.c_str());
          } catch (const std::exception& e) {
            emscripten_log(EM_LOG_ERROR, "Failed to restore component %s: %s",
                           id.c_str(), e.what());
          }
        }
      }
      recovery_count_++;
      set_recovery_flag(0);  // Clear recovery flag
      return absl::OkStatus();
    }
  }

  // Try emergency save data
  char* emergency_data = nullptr;
  EM_ASM(
      {
        const data = localStorage.getItem('yaze_emergency_save');
        if (data) {
          const len = lengthBytesUTF8(data) + 1;
          const ptr = _malloc(len);
          stringToUTF8(data, ptr, len);
          setValue($0, ptr, 'i32');
        }
      },
      &emergency_data);

  if (emergency_data) {
    try {
      nlohmann::json data = nlohmann::json::parse(emergency_data);
      free(emergency_data);

      if (data.contains("components") && data["components"].is_object()) {
        for (const auto& [id, component_data] : data["components"].items()) {
          auto it = components_.find(id);
          if (it != components_.end() && it->second.restore_fn) {
            try {
              it->second.restore_fn(component_data);
              emscripten_log(EM_LOG_INFO,
                             "Restored component from emergency save: %s",
                             id.c_str());
            } catch (const std::exception& e) {
              emscripten_log(EM_LOG_ERROR, "Failed to restore component %s: %s",
                             id.c_str(), e.what());
            }
          }
        }

        // Clear emergency save data
        EM_ASM({ localStorage.removeItem('yaze_emergency_save'); });

        recovery_count_++;
        set_recovery_flag(0);  // Clear recovery flag
        return absl::OkStatus();
      }
    } catch (const std::exception& e) {
      return absl::InvalidArgumentError(
          absl::StrFormat("Failed to recover session: %s", e.what()));
    }
  }

  return absl::NotFoundError("No recovery data available");
}

absl::Status AutoSaveManager::ClearRecoveryData() {
  // Clear regular auto-save data
  WasmStorage::DeleteProject(kAutoSaveDataKey);
  WasmStorage::DeleteProject(kAutoSaveMetaKey);

  // Clear emergency save data
  EM_ASM({ localStorage.removeItem('yaze_emergency_save'); });

  // Clear recovery flag
  set_recovery_flag(0);

  emscripten_log(EM_LOG_INFO, "Recovery data cleared");
  return absl::OkStatus();
}

void AutoSaveManager::SetInterval(int seconds) {
  bool was_running = IsRunning();
  if (was_running) {
    Stop();
  }

  interval_seconds_ = seconds;

  if (was_running && enabled_) {
    Start(seconds);
  }
}

void AutoSaveManager::SetEnabled(bool enabled) {
  enabled_ = enabled;
  if (!enabled && IsRunning()) {
    Stop();
  } else if (enabled && !IsRunning()) {
    Start(interval_seconds_);
  }
}

nlohmann::json AutoSaveManager::GetStatistics() const {
  std::lock_guard<std::mutex> lock(mutex_);

  nlohmann::json stats;
  stats["save_count"] = save_count_;
  stats["error_count"] = error_count_;
  stats["recovery_count"] = recovery_count_;
  stats["components_registered"] = components_.size();
  stats["is_running"] = running_;
  stats["interval_seconds"] = interval_seconds_;
  stats["enabled"] = enabled_;

  if (last_save_time_.time_since_epoch().count() > 0) {
    stats["last_save_timestamp"] =
        std::chrono::duration_cast<std::chrono::milliseconds>(
            last_save_time_.time_since_epoch())
            .count();
  }

  return stats;
}

}  // namespace platform
}  // namespace yaze

#endif  // __EMSCRIPTEN__
