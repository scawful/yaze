#ifndef YAZE_APP_PLATFORM_WASM_ERROR_HANDLER_H_
#define YAZE_APP_PLATFORM_WASM_ERROR_HANDLER_H_

#ifdef __EMSCRIPTEN__

#include <atomic>
#include <functional>
#include <string>

namespace yaze {
namespace platform {

/**
 * @enum ToastType
 * @brief Type of toast notification
 */
enum class ToastType { kInfo, kSuccess, kWarning, kError };

/**
 * @class WasmErrorHandler
 * @brief Browser-based error handling and notification system for WASM builds
 *
 * This class provides user-friendly error display, toast notifications,
 * progress indicators, and confirmation dialogs using browser UI elements.
 * All methods are static and thread-safe.
 */
class WasmErrorHandler {
 public:
  /**
   * @brief Display an error dialog in the browser
   * @param title Dialog title
   * @param message Error message to display
   */
  static void ShowError(const std::string& title, const std::string& message);

  /**
   * @brief Display a warning dialog in the browser
   * @param title Dialog title
   * @param message Warning message to display
   */
  static void ShowWarning(const std::string& title, const std::string& message);

  /**
   * @brief Display an info dialog in the browser
   * @param title Dialog title
   * @param message Info message to display
   */
  static void ShowInfo(const std::string& title, const std::string& message);

  /**
   * @brief Show a non-blocking toast notification
   * @param message Message to display
   * @param type Toast type (affects styling)
   * @param duration_ms Duration in milliseconds (default 3000)
   */
  static void Toast(const std::string& message,
                    ToastType type = ToastType::kInfo, int duration_ms = 3000);

  /**
   * @brief Show a progress indicator
   * @param task Task description
   * @param progress Progress value (0.0 to 1.0)
   */
  static void ShowProgress(const std::string& task, float progress);

  /**
   * @brief Hide the progress indicator
   */
  static void HideProgress();

  /**
   * @brief Show a confirmation dialog with callback
   * @param message Confirmation message
   * @param callback Function to call with user's choice (true = confirmed)
   */
  static void Confirm(const std::string& message,
                      std::function<void(bool)> callback);

  /**
   * @brief Initialize error handler (called once on startup)
   * This injects the necessary CSS styles and prepares the DOM
   */
  static void Initialize();

 private:
  // Prevent instantiation
  WasmErrorHandler() = delete;
  ~WasmErrorHandler() = delete;

  // Track if handler is initialized (thread-safe)
  static std::atomic<bool> initialized_;

  // Counter for generating unique callback IDs (thread-safe)
  static std::atomic<int> callback_counter_;
};

}  // namespace platform
}  // namespace yaze

#endif  // __EMSCRIPTEN__

#endif  // YAZE_APP_PLATFORM_WASM_ERROR_HANDLER_H_