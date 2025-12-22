// clang-format off
#ifdef __EMSCRIPTEN__

#include "app/platform/wasm/wasm_loading_manager.h"

#include <emscripten.h>
#include <emscripten/html5.h>

namespace yaze {
namespace app {
namespace platform {

// JavaScript interface functions
// Note: These functions take uint32_t js_id, not the full 64-bit handle.
// The JS layer only sees the low 32 bits which are unique IDs for UI elements.
EM_JS(void, js_create_loading_indicator, (uint32_t id, const char* task_name), {
  if (typeof window.createLoadingIndicator === 'function') {
    window.createLoadingIndicator(id, UTF8ToString(task_name));
  } else {
    console.warn('createLoadingIndicator not defined. Include loading_indicator.js');
  }
});

EM_JS(void, js_update_loading_progress, (uint32_t id, float progress, const char* message), {
  if (typeof window.updateLoadingProgress === 'function') {
    window.updateLoadingProgress(id, progress, UTF8ToString(message));
  }
});

EM_JS(void, js_remove_loading_indicator, (uint32_t id), {
  if (typeof window.removeLoadingIndicator === 'function') {
    window.removeLoadingIndicator(id);
  }
});

EM_JS(bool, js_check_loading_cancelled, (uint32_t id), {
  if (typeof window.isLoadingCancelled === 'function') {
    return window.isLoadingCancelled(id);
  }
  return false;
});

EM_JS(void, js_show_cancel_button, (uint32_t id), {
  if (typeof window.showCancelButton === 'function') {
    window.showCancelButton(id, function() {});
  }
});

// WasmLoadingManager implementation
WasmLoadingManager& WasmLoadingManager::GetInstance() {
  static WasmLoadingManager instance;
  return instance;
}

WasmLoadingManager::WasmLoadingManager() {}

WasmLoadingManager::~WasmLoadingManager() {
  std::lock_guard<std::mutex> lock(mutex_);
  for (const auto& [handle, op] : operations_) {
    if (op && op->active) {
      js_remove_loading_indicator(GetJsId(handle));
    }
  }
}

WasmLoadingManager::LoadingHandle WasmLoadingManager::BeginLoading(const std::string& task_name) {
  auto& instance = GetInstance();

  // Generate unique JS ID and generation counter atomically
  uint32_t js_id = instance.next_js_id_.fetch_add(1);
  uint32_t generation = instance.generation_counter_.fetch_add(1);

  // Create the full 64-bit handle
  LoadingHandle handle = MakeHandle(js_id, generation);

  auto operation = std::make_unique<LoadingOperation>();
  operation->task_name = task_name;
  operation->active = true;
  operation->generation = generation;

  {
    std::lock_guard<std::mutex> lock(instance.mutex_);
    instance.operations_[handle] = std::move(operation);
  }

  // JS functions receive only the 32-bit ID
  js_create_loading_indicator(js_id, task_name.c_str());
  js_show_cancel_button(js_id);
  return handle;
}

void WasmLoadingManager::UpdateProgress(LoadingHandle handle, float progress) {
  if (handle == kInvalidHandle) return;
  auto& instance = GetInstance();
  std::string message;
  uint32_t js_id = GetJsId(handle);

  {
    std::lock_guard<std::mutex> lock(instance.mutex_);
    auto it = instance.operations_.find(handle);
    if (it == instance.operations_.end() || !it->second->active) return;
    it->second->progress = progress;
    message = it->second->message;
  }

  js_update_loading_progress(js_id, progress, message.c_str());
}

void WasmLoadingManager::UpdateMessage(LoadingHandle handle, const std::string& message) {
  if (handle == kInvalidHandle) return;
  auto& instance = GetInstance();
  float progress = 0.0f;
  uint32_t js_id = GetJsId(handle);

  {
    std::lock_guard<std::mutex> lock(instance.mutex_);
    auto it = instance.operations_.find(handle);
    if (it == instance.operations_.end() || !it->second->active) return;
    it->second->message = message;
    progress = it->second->progress;
  }

  js_update_loading_progress(js_id, progress, message.c_str());
}

bool WasmLoadingManager::IsCancelled(LoadingHandle handle) {
  if (handle == kInvalidHandle) return false;
  auto& instance = GetInstance();
  uint32_t js_id = GetJsId(handle);

  // Check JS cancellation state first (outside lock)
  bool js_cancelled = js_check_loading_cancelled(js_id);

  {
    std::lock_guard<std::mutex> lock(instance.mutex_);
    auto it = instance.operations_.find(handle);
    if (it == instance.operations_.end() || !it->second->active) {
      return true;
    }
    if (js_cancelled && !it->second->cancelled.load()) {
      it->second->cancelled.store(true);
    }
    return it->second->cancelled.load();
  }
}

void WasmLoadingManager::EndLoading(LoadingHandle handle) {
  if (handle == kInvalidHandle) return;
  auto& instance = GetInstance();
  uint32_t js_id = GetJsId(handle);

  {
    std::lock_guard<std::mutex> lock(instance.mutex_);
    auto it = instance.operations_.find(handle);
    if (it != instance.operations_.end()) {
      // Mark inactive and erase immediately - no async delay.
      // The 64-bit handle with generation counter ensures that even if
      // a new operation starts immediately with the same js_id, it will
      // have a different generation and thus a different full handle.
      it->second->active = false;

      // Clear arena handle if it matches this handle
      if (instance.arena_handle_ == handle) {
        instance.arena_handle_ = kInvalidHandle;
      }

      // Erase synchronously - safe because generation counter prevents reuse
      instance.operations_.erase(it);
    }
  }

  // Remove the JS UI element after releasing the lock
  js_remove_loading_indicator(js_id);
}

bool WasmLoadingManager::ReportArenaProgress(int current, int total, const std::string& item_name) {
  auto& instance = GetInstance();
  LoadingHandle handle;
  uint32_t js_id;
  float progress = 0.0f;
  bool should_update_progress = false;
  bool should_update_message = false;
  bool is_cancelled = false;

  {
    std::lock_guard<std::mutex> lock(instance.mutex_);
    handle = instance.arena_handle_;
    if (handle == kInvalidHandle) return true;

    js_id = GetJsId(handle);

    // Check if the operation still exists and is active
    auto it = instance.operations_.find(handle);
    if (it == instance.operations_.end() || !it->second->active) {
      // Handle is stale, clear it and return
      instance.arena_handle_ = kInvalidHandle;
      return true;
    }

    // Update progress if applicable
    if (total > 0) {
      progress = static_cast<float>(current) / static_cast<float>(total);
      it->second->progress = progress;
      should_update_progress = true;
    } else {
      progress = it->second->progress;
    }

    // Update message if applicable
    if (!item_name.empty()) {
      it->second->message = item_name;
      should_update_message = true;
    }

    // Check cancellation status
    is_cancelled = it->second->cancelled.load();
  }

  // Perform JS calls outside the lock to avoid blocking
  if (should_update_progress || should_update_message) {
    js_update_loading_progress(js_id, progress,
                               should_update_message ? item_name.c_str() : "");
  }

  return !is_cancelled;
}

void WasmLoadingManager::SetArenaHandle(LoadingHandle handle) {
  auto& instance = GetInstance();
  std::lock_guard<std::mutex> lock(instance.mutex_);
  instance.arena_handle_ = handle;
}

void WasmLoadingManager::ClearArenaHandle() {
  auto& instance = GetInstance();
  std::lock_guard<std::mutex> lock(instance.mutex_);
  instance.arena_handle_ = kInvalidHandle;
}

}  // namespace platform
}  // namespace app
}  // namespace yaze

#endif  // __EMSCRIPTEN__
// clang-format on
