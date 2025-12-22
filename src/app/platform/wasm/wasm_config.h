#ifndef YAZE_APP_PLATFORM_WASM_CONFIG_H_
#define YAZE_APP_PLATFORM_WASM_CONFIG_H_

#ifdef __EMSCRIPTEN__

#include <emscripten.h>

#include <atomic>
#include <mutex>
#include <string>

namespace yaze {
namespace app {
namespace platform {

/**
 * @brief Centralized configuration for WASM platform features
 *
 * All configurable values are loaded from JavaScript's window.YAZE_CONFIG
 * object, allowing deployment-specific customization without recompiling.
 *
 * Usage in JavaScript (before WASM loads):
 * @code
 * window.YAZE_CONFIG = {
 *   collaboration: {
 *     serverUrl: "wss://your-server.com/ws",
 *     userTimeoutSeconds: 30.0,
 *     cursorSendIntervalMs: 100,
 *     maxChangeSizeBytes: 1024
 *   },
 *   autosave: {
 *     intervalSeconds: 60,
 *     maxRecoverySlots: 5
 *   },
 *   terminal: {
 *     maxHistoryItems: 50,
 *     maxOutputLines: 1000
 *   },
 *   ui: {
 *     minZoom: 0.25,
 *     maxZoom: 4.0,
 *     touchGestureThreshold: 10
 *   },
 *   cache: {
 *     version: "v1",
 *     maxRomCacheSizeMb: 100
 *   }
 * };
 * @endcode
 */
struct WasmConfig {
  // Collaboration settings
  struct Collaboration {
    std::string server_url;
    double user_timeout_seconds = 30.0;
    double cursor_send_interval_seconds = 0.1;  // 100ms
    size_t max_change_size_bytes = 1024;
  } collaboration;

  // Autosave settings
  struct Autosave {
    int interval_seconds = 60;
    int max_recovery_slots = 5;
  } autosave;

  // Terminal settings
  struct Terminal {
    int max_history_items = 50;
    int max_output_lines = 1000;
  } terminal;

  // UI settings
  struct UI {
    float min_zoom = 0.25f;
    float max_zoom = 4.0f;
    int touch_gesture_threshold = 10;
  } ui;

  // Cache settings
  struct Cache {
    std::string version = "v1";
    int max_rom_cache_size_mb = 100;
  } cache;

  // AI service settings (for terminal AI commands)
  struct AI {
    bool enabled = true;
    std::string model = "gemini-2.5-flash";
    std::string endpoint;  // Empty = use collaboration server
    int max_response_length = 4096;
  } ai;

  // Server deployment info
  struct Deployment {
    std::string server_repo = "https://github.com/scawful/yaze-server";
    int default_port = 8765;
    std::string protocol_version = "2.0";
  } deployment;

  // Server status (populated by FetchServerStatus)
  struct ServerStatus {
    bool fetched = false;
    bool reachable = false;
    bool ai_enabled = false;
    bool ai_configured = false;
    std::string ai_provider;  // "gemini", "external", "none"
    bool tls_detected = false;
    std::string persistence_type;  // "memory", "file"
    int active_sessions = 0;
    int total_connections = 0;
    std::string server_version;
    std::string error_message;
  } server_status;

  /**
   * @brief Load configuration from JavaScript window.YAZE_CONFIG
   *
   * Call this once during initialization to populate all config values
   * from the JavaScript environment.
   */
  void LoadFromJavaScript();

  /**
   * @brief Fetch server status from /health endpoint asynchronously
   *
   * Populates server_status struct with reachability, AI status, TLS info.
   * Safe to call multiple times; will update server_status on each call.
   */
  void FetchServerStatus();

  /**
   * @brief Get the singleton configuration instance
   * @return Reference to the global config
   */
  static WasmConfig& Get();

  /**
   * @brief Check if config was loaded from JavaScript
   * @return true if LoadFromJavaScript() was called successfully
   */
  bool IsLoaded() const { return loaded_.load(std::memory_order_acquire); }

  /**
   * @brief Check if config is currently being loaded
   * @return true if LoadFromJavaScript() is in progress
   */
  bool IsLoading() const { return loading_.load(std::memory_order_acquire); }

  /**
   * @brief Get read access to config (thread-safe)
   * @return Lock guard for read operations
   *
   * Use this when reading multiple config values to ensure consistency.
   * Example:
   * @code
   * auto lock = WasmConfig::Get().GetReadLock();
   * auto url = WasmConfig::Get().collaboration.server_url;
   * auto timeout = WasmConfig::Get().collaboration.user_timeout_seconds;
   * @endcode
   */
  std::unique_lock<std::mutex> GetReadLock() const {
    return std::unique_lock<std::mutex>(config_mutex_);
  }

 private:
  std::atomic<bool> loaded_{false};
  std::atomic<bool> loading_{false};
  mutable std::mutex config_mutex_;
};

// External C declarations for functions implemented in .cc
extern "C" {
  char* WasmConfig_GetString(const char* path, const char* defaultVal);
  double WasmConfig_GetNumber(const char* path, double defaultVal);
  int WasmConfig_GetInt(const char* path, int defaultVal);
}

}  // namespace platform
}  // namespace app
}  // namespace yaze

#else  // !__EMSCRIPTEN__

// Stub for non-WASM builds - provides defaults
#include <mutex>
#include <string>

namespace yaze {
namespace app {
namespace platform {

struct WasmConfig {
  struct Collaboration {
    std::string server_url;
    double user_timeout_seconds = 30.0;
    double cursor_send_interval_seconds = 0.1;
    size_t max_change_size_bytes = 1024;
  } collaboration;

  struct Autosave {
    int interval_seconds = 60;
    int max_recovery_slots = 5;
  } autosave;

  struct Terminal {
    int max_history_items = 50;
    int max_output_lines = 1000;
  } terminal;

  struct UI {
    float min_zoom = 0.25f;
    float max_zoom = 4.0f;
    int touch_gesture_threshold = 10;
  } ui;

  struct Cache {
    std::string version = "v1";
    int max_rom_cache_size_mb = 100;
  } cache;

  struct AI {
    bool enabled = true;
    std::string model = "gemini-2.5-flash";
    std::string endpoint;
    int max_response_length = 4096;
  } ai;

  struct Deployment {
    std::string server_repo = "https://github.com/scawful/yaze-server";
    int default_port = 8765;
    std::string protocol_version = "2.0";
  } deployment;

  struct ServerStatus {
    bool fetched = false;
    bool reachable = false;
    bool ai_enabled = false;
    bool ai_configured = false;
    std::string ai_provider;
    bool tls_detected = false;
    std::string persistence_type;
    int active_sessions = 0;
    int total_connections = 0;
    std::string server_version;
    std::string error_message;
  } server_status;

  void LoadFromJavaScript() {}
  void FetchServerStatus() {}
  static WasmConfig& Get() {
    static WasmConfig instance;
    return instance;
  }
  bool IsLoaded() const { return true; }
  bool IsLoading() const { return false; }
  std::unique_lock<std::mutex> GetReadLock() const {
    return std::unique_lock<std::mutex>(config_mutex_);
  }

 private:
  mutable std::mutex config_mutex_;
};

}  // namespace platform
}  // namespace app
}  // namespace yaze

#endif  // __EMSCRIPTEN__

#endif  // YAZE_APP_PLATFORM_WASM_CONFIG_H_
