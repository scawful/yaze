#ifndef YAZE_APP_PLATFORM_WASM_CONFIG_H_
#define YAZE_APP_PLATFORM_WASM_CONFIG_H_

#ifdef __EMSCRIPTEN__

#include <emscripten.h>
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

  /**
   * @brief Load configuration from JavaScript window.YAZE_CONFIG
   *
   * Call this once during initialization to populate all config values
   * from the JavaScript environment.
   */
  void LoadFromJavaScript();

  /**
   * @brief Get the singleton configuration instance
   * @return Reference to the global config
   */
  static WasmConfig& Get();

  /**
   * @brief Check if config was loaded from JavaScript
   * @return true if LoadFromJavaScript() was called successfully
   */
  bool IsLoaded() const { return loaded_; }

 private:
  bool loaded_ = false;
};

// clang-format off

// Helper to read string from JS config
EM_JS(char*, WasmConfig_GetString, (const char* path, const char* defaultVal), {
  try {
    var config = window.YAZE_CONFIG || {};
    var parts = UTF8ToString(path).split('.');
    var value = config;
    for (var i = 0; i < parts.length; i++) {
      if (value && typeof value === 'object' && parts[i] in value) {
        value = value[parts[i]];
      } else {
        value = UTF8ToString(defaultVal);
        break;
      }
    }
    if (typeof value !== 'string') {
      value = UTF8ToString(defaultVal);
    }
    var lengthBytes = lengthBytesUTF8(value) + 1;
    var stringOnWasmHeap = _malloc(lengthBytes);
    stringToUTF8(value, stringOnWasmHeap, lengthBytes);
    return stringOnWasmHeap;
  } catch (e) {
    console.error('[WasmConfig] Error reading string:', e);
    var def = UTF8ToString(defaultVal);
    var len = lengthBytesUTF8(def) + 1;
    var ptr = _malloc(len);
    stringToUTF8(def, ptr, len);
    return ptr;
  }
});

// Helper to read number from JS config
EM_JS(double, WasmConfig_GetNumber, (const char* path, double defaultVal), {
  try {
    var config = window.YAZE_CONFIG || {};
    var parts = UTF8ToString(path).split('.');
    var value = config;
    for (var i = 0; i < parts.length; i++) {
      if (value && typeof value === 'object' && parts[i] in value) {
        value = value[parts[i]];
      } else {
        return defaultVal;
      }
    }
    return typeof value === 'number' ? value : defaultVal;
  } catch (e) {
    console.error('[WasmConfig] Error reading number:', e);
    return defaultVal;
  }
});

// Helper to read int from JS config
EM_JS(int, WasmConfig_GetInt, (const char* path, int defaultVal), {
  try {
    var config = window.YAZE_CONFIG || {};
    var parts = UTF8ToString(path).split('.');
    var value = config;
    for (var i = 0; i < parts.length; i++) {
      if (value && typeof value === 'object' && parts[i] in value) {
        value = value[parts[i]];
      } else {
        return defaultVal;
      }
    }
    return typeof value === 'number' ? Math.floor(value) : defaultVal;
  } catch (e) {
    console.error('[WasmConfig] Error reading int:', e);
    return defaultVal;
  }
});

// clang-format on

}  // namespace platform
}  // namespace app
}  // namespace yaze

#else  // !__EMSCRIPTEN__

// Stub for non-WASM builds - provides defaults
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

  void LoadFromJavaScript() {}
  static WasmConfig& Get() {
    static WasmConfig instance;
    return instance;
  }
  bool IsLoaded() const { return true; }
};

}  // namespace platform
}  // namespace app
}  // namespace yaze

#endif  // __EMSCRIPTEN__

#endif  // YAZE_APP_PLATFORM_WASM_CONFIG_H_
