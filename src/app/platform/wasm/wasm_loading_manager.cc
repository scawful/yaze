#ifdef __EMSCRIPTEN__

#include "app/platform/wasm/wasm_loading_manager.h"

#include <emscripten.h>
#include <emscripten/html5.h>

namespace yaze {
namespace app {
namespace platform {

// Static member initialization
std::atomic<WasmLoadingManager::LoadingHandle>
    WasmLoadingManager::arena_handle_{kInvalidHandle};

// JavaScript interface functions
EM_JS(void, js_create_loading_indicator, (uint32_t id, const char* task_name), {
  if (typeof window.createLoadingIndicator == = 'function') {
    window.createLoadingIndicator(id, UTF8ToString(task_name));
  } else {
    console.warn(
        'createLoadingIndicator not defined. Include loading_indicator.js');
  }
});

EM_JS(void, js_update_loading_progress,
      (uint32_t id, float progress, const char* message), {
        if (typeof window.updateLoadingProgress == = 'function') {
          window.updateLoadingProgress(id, progress, UTF8ToString(message));
        }
      });

EM_JS(void, js_remove_loading_indicator, (uint32_t id), {
  if (typeof window.removeLoadingIndicator == = 'function') {
    window.removeLoadingIndicator(id);
  }
});

EM_JS(bool, js_check_loading_cancelled, (uint32_t id), {
  if (typeof window.isLoadingCancelled == = 'function') {
    return window.isLoadingCancelled(id);
  }
  return false;
});

EM_JS(void, js_show_cancel_button, (uint32_t id), {
  if (typeof window.showCancelButton == = 'function') {
    window.showCancelButton(
        id, function(){
                // Callback will be handled by js_check_loading_cancelled
            });
  }
});

// WasmLoadingManager implementation
WasmLoadingManager& WasmLoadingManager::GetInstance() {
  static WasmLoadingManager instance;
  return instance;
}

WasmLoadingManager::WasmLoadingManager() {
  // Constructor
}

WasmLoadingManager::~WasmLoadingManager() {
  // Clean up any remaining operations
  std::lock_guard<std::mutex> lock(mutex_);
  for (const auto& [handle, op] : operations_) {
    if (op && op->active) {
      js_remove_loading_indicator(handle);
    }
  }
}

WasmLoadingManager::LoadingHandle WasmLoadingManager::BeginLoading(
    const std::string& task_name) {
  auto& instance = GetInstance();

  LoadingHandle handle = instance.next_handle_.fetch_add(1);

  auto operation = std::make_unique<LoadingOperation>();
  operation->task_name = task_name;
  operation->active = true;

  {
    std::lock_guard<std::mutex> lock(instance.mutex_);
    instance.operations_[handle] = std::move(operation);
  }

  // Create the UI element
  js_create_loading_indicator(handle, task_name.c_str());

  // Show cancel button for long operations
  js_show_cancel_button(handle);

  return handle;
}

void WasmLoadingManager::UpdateProgress(LoadingHandle handle, float progress) {
  if (handle == kInvalidHandle)
    return;

  auto& instance = GetInstance();

  std::string message;
  {
    std::lock_guard<std::mutex> lock(instance.mutex_);
    auto it = instance.operations_.find(handle);
    if (it == instance.operations_.end() || !it->second->active) {
      return;
    }
    it->second->progress = progress;
    message = it->second->message;
  }

  // Update the UI
  js_update_loading_progress(handle, progress, message.c_str());
}

void WasmLoadingManager::UpdateMessage(LoadingHandle handle,
                                       const std::string& message) {
  if (handle == kInvalidHandle)
    return;

  auto& instance = GetInstance();

  float progress = 0.0f;
  {
    std::lock_guard<std::mutex> lock(instance.mutex_);
    auto it = instance.operations_.find(handle);
    if (it == instance.operations_.end() || !it->second->active) {
      return;
    }
    it->second->message = message;
    progress = it->second->progress;
  }

  // Update the UI
  js_update_loading_progress(handle, progress, message.c_str());
}

bool WasmLoadingManager::IsCancelled(LoadingHandle handle) {
  if (handle == kInvalidHandle)
    return false;

  auto& instance = GetInstance();

  // Check JavaScript for cancellation
  bool js_cancelled = js_check_loading_cancelled(handle);

  {
    std::lock_guard<std::mutex> lock(instance.mutex_);
    auto it = instance.operations_.find(handle);
    if (it == instance.operations_.end() || !it->second->active) {
      return true;  // Operation no longer exists, treat as cancelled
    }

    // Update our state if JS reports cancellation
    if (js_cancelled && !it->second->cancelled.load()) {
      it->second->cancelled.store(true);
    }

    return it->second->cancelled.load();
  }
}

void WasmLoadingManager::EndLoading(LoadingHandle handle) {
  if (handle == kInvalidHandle)
    return;

  auto& instance = GetInstance();

  {
    std::lock_guard<std::mutex> lock(instance.mutex_);
    auto it = instance.operations_.find(handle);
    if (it != instance.operations_.end()) {
      it->second->active = false;
    }
  }

  // Remove the UI element
  js_remove_loading_indicator(handle);

  // Clean up the operation after a delay to allow any pending checks
  emscripten_async_call(
      [](void* arg) {
        LoadingHandle handle =
            static_cast<LoadingHandle>(reinterpret_cast<uintptr_t>(arg));
        auto& instance = GetInstance();
        std::lock_guard<std::mutex> lock(instance.mutex_);
        instance.operations_.erase(handle);
      },
      reinterpret_cast<void*>(static_cast<uintptr_t>(handle)), 100);
}

bool WasmLoadingManager::ReportArenaProgress(int current, int total,
                                             const std::string& item_name) {
  LoadingHandle handle = arena_handle_.load();
  if (handle == kInvalidHandle) {
    // No active Arena loading operation
    return true;
  }

  if (total > 0) {
    float progress = static_cast<float>(current) / static_cast<float>(total);
    UpdateProgress(handle, progress);
  }

  if (!item_name.empty()) {
    UpdateMessage(handle, item_name);
  }

  // Check for cancellation
  return !IsCancelled(handle);
}

void WasmLoadingManager::SetArenaHandle(LoadingHandle handle) {
  arena_handle_.store(handle);
}

void WasmLoadingManager::ClearArenaHandle() {
  arena_handle_.store(kInvalidHandle);
}

}  // namespace platform
}  // namespace app
}  // namespace yaze

#endif  // __EMSCRIPTEN__