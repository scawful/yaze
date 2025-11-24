#ifdef __EMSCRIPTEN__

#include "app/platform/wasm/wasm_config.h"

#include <cstdlib>

namespace yaze {
namespace app {
namespace platform {

void WasmConfig::LoadFromJavaScript() {
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

  loaded_ = true;
}

WasmConfig& WasmConfig::Get() {
  static WasmConfig instance;
  return instance;
}

}  // namespace platform
}  // namespace app
}  // namespace yaze

#endif  // __EMSCRIPTEN__
