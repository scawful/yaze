#ifndef YAZE_APP_PLATFORM_WASM_WASM_LOADING_MANAGER_H_
#define YAZE_APP_PLATFORM_WASM_WASM_LOADING_MANAGER_H_

#ifdef __EMSCRIPTEN__

#include <atomic>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>

#include "absl/status/status.h"
#include "absl/strings/str_format.h"

namespace yaze {
namespace app {
namespace platform {

/**
 * @class WasmLoadingManager
 * @brief Manages loading operations with progress tracking for WASM builds
 *
 * This class provides a centralized loading manager for long-running operations
 * in the browser environment. It integrates with JavaScript UI to show progress
 * indicators with cancel capability.
 *
 * Handle Design:
 * The LoadingHandle is a 64-bit value composed of:
 * - High 32 bits: Generation counter (prevents handle reuse after EndLoading)
 * - Low 32 bits: Unique ID for JS interop
 *
 * This design eliminates race conditions where a new operation could reuse
 * the same ID as a recently-ended operation. Operations are deleted
 * synchronously in EndLoading() rather than using async callbacks.
 *
 * Example usage:
 * @code
 * auto handle = WasmLoadingManager::BeginLoading("Loading Graphics");
 * for (int i = 0; i < total; i++) {
 *   if (WasmLoadingManager::IsCancelled(handle)) {
 *     WasmLoadingManager::EndLoading(handle);
 *     return absl::CancelledError("User cancelled loading");
 *   }
 *   // Do work...
 *   WasmLoadingManager::UpdateProgress(handle, static_cast<float>(i) / total);
 *   WasmLoadingManager::UpdateMessage(handle, absl::StrFormat("Sheet %d/%d", i, total));
 * }
 * WasmLoadingManager::EndLoading(handle);
 * @endcode
 */
class WasmLoadingManager {
 public:
  /**
   * @brief Handle for tracking a loading operation
   *
   * 64-bit handle with generation counter to prevent reuse race conditions:
   * - High 32 bits: Generation counter
   * - Low 32 bits: JS-visible ID
   */
  using LoadingHandle = uint64_t;

  /**
   * @brief Invalid handle value
   */
  static constexpr LoadingHandle kInvalidHandle = 0;

  /**
   * @brief Extract the JS-visible ID from a handle (low 32 bits)
   * @param handle The full 64-bit handle
   * @return The 32-bit JS ID
   */
  static uint32_t GetJsId(LoadingHandle handle) {
    return static_cast<uint32_t>(handle & 0xFFFFFFFF);
  }

  /**
   * @brief Begin a new loading operation
   * @param task_name The name of the task to display in the UI
   * @return Handle to track this loading operation
   */
  static LoadingHandle BeginLoading(const std::string& task_name);

  /**
   * @brief Update the progress of a loading operation
   * @param handle The loading operation handle
   * @param progress Progress value between 0.0 and 1.0
   */
  static void UpdateProgress(LoadingHandle handle, float progress);

  /**
   * @brief Update the status message for a loading operation
   * @param handle The loading operation handle
   * @param message Status message to display
   */
  static void UpdateMessage(LoadingHandle handle, const std::string& message);

  /**
   * @brief Check if the user has requested cancellation
   * @param handle The loading operation handle
   * @return true if the operation should be cancelled
   */
  static bool IsCancelled(LoadingHandle handle);

  /**
   * @brief End a loading operation and remove UI
   * @param handle The loading operation handle
   */
  static void EndLoading(LoadingHandle handle);

  /**
   * @brief Integration point for gfx::Arena progressive loading
   *
   * This method can be called by gfx::Arena during texture loading
   * to report progress without modifying Arena's core logic.
   *
   * @param current Current item being processed
   * @param total Total items to process
   * @param item_name Name of the current item (e.g., "Graphics Sheet 42")
   * @return true if loading should continue, false if cancelled
   */
  static bool ReportArenaProgress(int current, int total,
                                  const std::string& item_name);

  /**
   * @brief Set the global loading handle for Arena operations
   *
   * This allows Arena to use a pre-existing loading operation
   * instead of creating its own.
   *
   * @param handle The loading operation handle to use for Arena progress
   */
  static void SetArenaHandle(LoadingHandle handle);

  /**
   * @brief Clear the global Arena loading handle
   */
  static void ClearArenaHandle();

 private:
  /**
   * @brief Internal structure to track loading operations
   */
  struct LoadingOperation {
    std::string task_name;
    float progress = 0.0f;
    std::string message;
    std::atomic<bool> cancelled{false};
    bool active = true;
    uint32_t generation = 0;  // Generation counter for validation
  };

  /**
   * @brief Get the singleton instance
   */
  static WasmLoadingManager& GetInstance();

  /**
   * @brief Constructor (private for singleton)
   */
  WasmLoadingManager();

  /**
   * @brief Destructor
   */
  ~WasmLoadingManager();

  // Disable copy and move
  WasmLoadingManager(const WasmLoadingManager&) = delete;
  WasmLoadingManager& operator=(const WasmLoadingManager&) = delete;
  WasmLoadingManager(WasmLoadingManager&&) = delete;
  WasmLoadingManager& operator=(WasmLoadingManager&&) = delete;

  /**
   * @brief Create a handle from JS ID and generation
   */
  static LoadingHandle MakeHandle(uint32_t js_id, uint32_t generation) {
    return (static_cast<uint64_t>(generation) << 32) | js_id;
  }

  /**
   * @brief Extract generation from handle (high 32 bits)
   */
  static uint32_t GetGeneration(LoadingHandle handle) {
    return static_cast<uint32_t>(handle >> 32);
  }

  /**
   * @brief Next available JS ID (low 32 bits of handle)
   *
   * This counter only increments, never wraps. With 32 bits, even at
   * 1000 operations/second, it would take ~50 days to wrap.
   */
  std::atomic<uint32_t> next_js_id_{1};

  /**
   * @brief Generation counter to prevent handle reuse
   *
   * Incremented each time BeginLoading is called. Combined with js_id
   * to create unique 64-bit handles that cannot be accidentally reused.
   */
  std::atomic<uint32_t> generation_counter_{1};

  /**
   * @brief Active loading operations, keyed by full 64-bit handle
   */
  std::unordered_map<LoadingHandle, std::unique_ptr<LoadingOperation>>
      operations_;

  /**
   * @brief Mutex for thread safety
   */
  std::mutex mutex_;

  /**
   * @brief Global handle for Arena operations (protected by mutex_)
   *
   * NOTE: This is a non-static member protected by mutex_ to prevent
   * race conditions between ReportArenaProgress() and ClearArenaHandle().
   */
  LoadingHandle arena_handle_ = kInvalidHandle;
};

}  // namespace platform
}  // namespace app
}  // namespace yaze

#endif  // __EMSCRIPTEN__

#endif  // YAZE_APP_PLATFORM_WASM_WASM_LOADING_MANAGER_H_
