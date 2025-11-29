#ifndef YAZE_APP_PLATFORM_WASM_WASM_ASYNC_GUARD_H_
#define YAZE_APP_PLATFORM_WASM_WASM_ASYNC_GUARD_H_

#ifdef __EMSCRIPTEN__

#include <atomic>

#include <emscripten.h>

namespace yaze {
namespace platform {

/**
 * @class WasmAsyncGuard
 * @brief Global guard to prevent concurrent Asyncify operations
 *
 * Emscripten's Asyncify only supports one async operation at a time.
 * This guard provides C++ side tracking to complement the JavaScript
 * async queue (async_queue.js).
 *
 * Usage:
 *   // At the start of any function that calls Asyncify-wrapped code:
 *   WasmAsyncGuard::ScopedGuard guard;
 *   if (!guard.acquired()) {
 *     // Another async operation is in progress - the JS queue will handle it
 *   }
 *   // ... call async operations
 */
class WasmAsyncGuard {
 public:
  /**
   * @brief Try to acquire the async operation lock
   * @return true if acquired, false if another operation is in progress
   */
  static bool TryAcquire() {
    bool expected = false;
    return in_async_op_.compare_exchange_strong(expected, true);
  }

  /**
   * @brief Release the async operation lock
   */
  static void Release() { in_async_op_.store(false); }

  /**
   * @brief Check if an async operation is in progress
   * @return true if an operation is currently running
   */
  static bool IsInProgress() { return in_async_op_.load(); }

  /**
   * @class ScopedGuard
   * @brief RAII wrapper for async operation tracking
   *
   * Automatically acquires the guard on construction and releases on
   * destruction. Logs a warning if another operation was already in progress.
   */
  class ScopedGuard {
   public:
    ScopedGuard() : acquired_(TryAcquire()) {
      if (!acquired_) {
        emscripten_log(
            EM_LOG_WARN,
            "[WasmAsyncGuard] Async operation already in progress, "
            "request will be queued by JS async queue");
      }
    }

    ~ScopedGuard() {
      if (acquired_) {
        Release();
      }
    }

    // Non-copyable
    ScopedGuard(const ScopedGuard&) = delete;
    ScopedGuard& operator=(const ScopedGuard&) = delete;

    /**
     * @brief Check if this guard acquired the lock
     * @return true if this guard holds the lock
     */
    bool acquired() const { return acquired_; }

   private:
    bool acquired_;
  };

 private:
  static std::atomic<bool> in_async_op_;
};

}  // namespace platform
}  // namespace yaze

#endif  // __EMSCRIPTEN__

#endif  // YAZE_APP_PLATFORM_WASM_WASM_ASYNC_GUARD_H_
