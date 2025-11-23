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
   */
  using LoadingHandle = uint32_t;

  /**
   * @brief Invalid handle value
   */
  static constexpr LoadingHandle kInvalidHandle = 0;

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
   * @brief Next available handle ID
   */
  std::atomic<LoadingHandle> next_handle_{1};

  /**
   * @brief Active loading operations
   */
  std::unordered_map<LoadingHandle, std::unique_ptr<LoadingOperation>>
      operations_;

  /**
   * @brief Mutex for thread safety
   */
  std::mutex mutex_;

  /**
   * @brief Global handle for Arena operations
   */
  static std::atomic<LoadingHandle> arena_handle_;
};

}  // namespace platform
}  // namespace app
}  // namespace yaze

#endif  // __EMSCRIPTEN__

#endif  // YAZE_APP_PLATFORM_WASM_WASM_LOADING_MANAGER_H_