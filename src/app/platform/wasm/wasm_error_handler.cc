// clang-format off
#ifdef __EMSCRIPTEN__

#include "app/platform/wasm/wasm_error_handler.h"

#include <emscripten.h>
#include <emscripten/html5.h>
#include <map>
#include <mutex>

namespace yaze {
namespace platform {

// Static member initialization
std::atomic<bool> WasmErrorHandler::initialized_{false};
std::atomic<int> WasmErrorHandler::callback_counter_{0};

// Store confirmation callbacks with timestamps for timeout cleanup
struct CallbackEntry {
  std::function<void(bool)> callback;
  double timestamp;  // Time when callback was registered (ms since epoch)
};
static std::map<int, CallbackEntry> g_confirm_callbacks;
static std::mutex g_callback_mutex;

// Callback timeout in milliseconds (5 minutes)
constexpr double kCallbackTimeoutMs = 5.0 * 60.0 * 1000.0;

// Helper to get current time in milliseconds
static double GetCurrentTimeMs() {
  return EM_ASM_DOUBLE({ return Date.now(); });
}

// Cleanup stale callbacks that have exceeded the timeout
static void CleanupStaleCallbacks() {
  std::lock_guard<std::mutex> lock(g_callback_mutex);
  const double now = GetCurrentTimeMs();

  for (auto it = g_confirm_callbacks.begin(); it != g_confirm_callbacks.end();) {
    if (now - it->second.timestamp > kCallbackTimeoutMs) {
      it = g_confirm_callbacks.erase(it);
    } else {
      ++it;
    }
  }
}

// JavaScript function to register cleanup handler for page unload
EM_JS(void, js_register_cleanup_handler, (), {
  window.addEventListener('beforeunload', function() {
    // Signal C++ to cleanup stale callbacks
    if (Module._cleanupConfirmCallbacks) {
      Module._cleanupConfirmCallbacks();
    }
  });
});

// C++ cleanup function called from JavaScript on page unload
extern "C" EMSCRIPTEN_KEEPALIVE void cleanupConfirmCallbacks() {
  std::lock_guard<std::mutex> lock(g_callback_mutex);
  g_confirm_callbacks.clear();
}

// JavaScript functions for browser UI interaction
EM_JS(void, js_show_modal, (const char* title, const char* message, const char* type), {
  var titleStr = UTF8ToString(title);
  var messageStr = UTF8ToString(message);
  var typeStr = UTF8ToString(type);
  if (typeof window.showYazeModal === 'function') {
    window.showYazeModal(titleStr, messageStr, typeStr);
  } else {
    alert(titleStr + '\n\n' + messageStr);
  }
});

EM_JS(void, js_show_toast, (const char* message, const char* type, int duration_ms), {
  var messageStr = UTF8ToString(message);
  var typeStr = UTF8ToString(type);
  if (typeof window.showYazeToast === 'function') {
    window.showYazeToast(messageStr, typeStr, duration_ms);
  } else {
    console.log('[' + typeStr + '] ' + messageStr);
  }
});

EM_JS(void, js_show_progress, (const char* task, float progress), {
  var taskStr = UTF8ToString(task);
  if (typeof window.showYazeProgress === 'function') {
    window.showYazeProgress(taskStr, progress);
  } else {
    console.log('Progress: ' + taskStr + ' - ' + (progress * 100).toFixed(0) + '%');
  }
});

EM_JS(void, js_hide_progress, (), {
  if (typeof window.hideYazeProgress === 'function') {
    window.hideYazeProgress();
  }
});

EM_JS(void, js_show_confirm, (const char* message, int callback_id), {
  var messageStr = UTF8ToString(message);
  if (typeof window.showYazeConfirm === 'function') {
    window.showYazeConfirm(messageStr, function(result) {
      Module._handleConfirmCallback(callback_id, result ? 1 : 0);
    });
  } else {
    var result = confirm(messageStr);
    Module._handleConfirmCallback(callback_id, result ? 1 : 0);
  }
});

EM_JS(void, js_inject_styles, (), {
  if (document.getElementById('yaze-error-handler-styles')) {
    return;
  }
  var link = document.createElement('link');
  link.id = 'yaze-error-handler-styles';
  link.rel = 'stylesheet';
  link.href = 'error_handler.css';
  link.onerror = function() {
    var style = document.createElement('style');
    style.textContent = '.yaze-modal-overlay{position:fixed;top:0;left:0;right:0;bottom:0;background:rgba(0,0,0,0.5);display:flex;align-items:center;justify-content:center;z-index:10000}.yaze-modal-content{background:white;border-radius:8px;padding:24px;max-width:500px;box-shadow:0 4px 6px rgba(0,0,0,0.1)}.yaze-toast{position:fixed;bottom:20px;right:20px;padding:12px 20px;border-radius:4px;color:white;z-index:10001}.yaze-toast-info{background:#3498db}.yaze-toast-success{background:#2ecc71}.yaze-toast-warning{background:#f39c12}.yaze-toast-error{background:#e74c3c}';
    document.head.appendChild(style);
  };
  document.head.appendChild(link);
});

// C++ callback handler for confirmation dialogs
extern "C" EMSCRIPTEN_KEEPALIVE void handleConfirmCallback(int callback_id, int result) {
  std::function<void(bool)> callback;
  {
    std::lock_guard<std::mutex> lock(g_callback_mutex);
    auto it = g_confirm_callbacks.find(callback_id);
    if (it != g_confirm_callbacks.end()) {
      callback = it->second.callback;
      g_confirm_callbacks.erase(it);
    }
  }
  if (callback) {
    callback(result != 0);
  }
}

void WasmErrorHandler::Initialize() {
  // Use compare_exchange for thread-safe initialization
  bool expected = false;
  if (!initialized_.compare_exchange_strong(expected, true)) {
    return;  // Already initialized by another thread
  }
  js_inject_styles();
  EM_ASM({
    Module._handleConfirmCallback = Module.cwrap('handleConfirmCallback', null, ['number', 'number']);
    Module._cleanupConfirmCallbacks = Module.cwrap('cleanupConfirmCallbacks', null, []);
  });
  js_register_cleanup_handler();
}

void WasmErrorHandler::ShowError(const std::string& title, const std::string& message) {
  if (!initialized_.load()) Initialize();
  js_show_modal(title.c_str(), message.c_str(), "error");
}

void WasmErrorHandler::ShowWarning(const std::string& title, const std::string& message) {
  if (!initialized_.load()) Initialize();
  js_show_modal(title.c_str(), message.c_str(), "warning");
}

void WasmErrorHandler::ShowInfo(const std::string& title, const std::string& message) {
  if (!initialized_.load()) Initialize();
  js_show_modal(title.c_str(), message.c_str(), "info");
}

void WasmErrorHandler::Toast(const std::string& message, ToastType type, int duration_ms) {
  if (!initialized_.load()) Initialize();
  const char* type_str = "info";
  switch (type) {
    case ToastType::kSuccess: type_str = "success"; break;
    case ToastType::kWarning: type_str = "warning"; break;
    case ToastType::kError: type_str = "error"; break;
    case ToastType::kInfo:
    default: type_str = "info"; break;
  }
  js_show_toast(message.c_str(), type_str, duration_ms);
}

void WasmErrorHandler::ShowProgress(const std::string& task, float progress) {
  if (!initialized_.load()) Initialize();
  if (progress < 0.0f) progress = 0.0f;
  if (progress > 1.0f) progress = 1.0f;
  js_show_progress(task.c_str(), progress);
}

void WasmErrorHandler::HideProgress() {
  if (!initialized_.load()) Initialize();
  js_hide_progress();
}

void WasmErrorHandler::Confirm(const std::string& message, std::function<void(bool)> callback) {
  if (!initialized_.load()) Initialize();

  // Cleanup any stale callbacks before adding new one
  CleanupStaleCallbacks();

  int callback_id = callback_counter_.fetch_add(1) + 1;
  {
    std::lock_guard<std::mutex> lock(g_callback_mutex);
    g_confirm_callbacks[callback_id] = CallbackEntry{callback, GetCurrentTimeMs()};
  }
  js_show_confirm(message.c_str(), callback_id);
}

}  // namespace platform
}  // namespace yaze

#endif  // __EMSCRIPTEN__
// clang-format on
