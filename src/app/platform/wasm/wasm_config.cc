#ifdef __EMSCRIPTEN__

#include "app/platform/wasm/wasm_config.h"

#include <cstdlib>
#include <emscripten.h>

namespace yaze {
namespace app {
namespace platform {

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

void WasmConfig::LoadFromJavaScript() {
  // Prevent concurrent loading
  bool expected = false;
  if (!loading_.compare_exchange_strong(expected, true,
                                         std::memory_order_acq_rel)) {
    // Already loading, wait for completion
    return;
  }

  // Lock for writing config values
  std::lock_guard<std::mutex> lock(config_mutex_);

  // Collaboration settings
  char* server_url = WasmConfig_GetString("collaboration.serverUrl", "");
  collaboration.server_url = std::string(server_url);
  free(server_url);

  collaboration.user_timeout_seconds =
      WasmConfig_GetNumber("collaboration.userTimeoutSeconds", 30.0);
  collaboration.cursor_send_interval_seconds =
      WasmConfig_GetNumber("collaboration.cursorSendIntervalMs", 100.0) / 1000.0;
  collaboration.max_change_size_bytes = static_cast<size_t>(
      WasmConfig_GetInt("collaboration.maxChangeSizeBytes", 1024));

  // Autosave settings
  autosave.interval_seconds =
      WasmConfig_GetInt("autosave.intervalSeconds", 60);
  autosave.max_recovery_slots =
      WasmConfig_GetInt("autosave.maxRecoverySlots", 5);

  // Terminal settings
  terminal.max_history_items =
      WasmConfig_GetInt("terminal.maxHistoryItems", 50);
  terminal.max_output_lines =
      WasmConfig_GetInt("terminal.maxOutputLines", 1000);

  // UI settings
  ui.min_zoom = static_cast<float>(WasmConfig_GetNumber("ui.minZoom", 0.25));
  ui.max_zoom = static_cast<float>(WasmConfig_GetNumber("ui.maxZoom", 4.0));
  ui.touch_gesture_threshold =
      WasmConfig_GetInt("ui.touchGestureThreshold", 10);

  // Cache settings
  char* cache_version = WasmConfig_GetString("cache.version", "v1");
  cache.version = std::string(cache_version);
  free(cache_version);

  cache.max_rom_cache_size_mb =
      WasmConfig_GetInt("cache.maxRomCacheSizeMb", 100);

  // AI settings
  ai.enabled = WasmConfig_GetInt("ai.enabled", 1) != 0;
  char* ai_model = WasmConfig_GetString("ai.model", "gemini-2.5-flash");
  ai.model = std::string(ai_model);
  free(ai_model);

  char* ai_endpoint = WasmConfig_GetString("ai.endpoint", "");
  ai.endpoint = std::string(ai_endpoint);
  free(ai_endpoint);

  ai.max_response_length = WasmConfig_GetInt("ai.maxResponseLength", 4096);

  // Deployment info (read-only defaults, but can be overridden)
  char* server_repo = WasmConfig_GetString("deployment.serverRepo",
      "https://github.com/scawful/yaze-server");
  deployment.server_repo = std::string(server_repo);
  free(server_repo);

  deployment.default_port = WasmConfig_GetInt("deployment.defaultPort", 8765);

  char* protocol_version = WasmConfig_GetString("deployment.protocolVersion", "2.0");
  deployment.protocol_version = std::string(protocol_version);
  free(protocol_version);

  loaded_.store(true, std::memory_order_release);
  loading_.store(false, std::memory_order_release);
}

WasmConfig& WasmConfig::Get() {
  static WasmConfig instance;
  return instance;
}

void WasmConfig::FetchServerStatus() {
  // Server status fetching is handled via JavaScript in the web shell.
  // The web shell calls fetch() on /health and populates window.YAZE_SERVER_STATUS.
  // This C++ function is a stub - actual status is read from JS config on next LoadFromJavaScript().
  // TODO: Implement async status fetching via emscripten_async_call if needed.
}

}  // namespace platform
}  // namespace app
}  // namespace yaze

#endif  // __EMSCRIPTEN__
