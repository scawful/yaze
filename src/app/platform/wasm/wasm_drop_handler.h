#ifndef YAZE_APP_PLATFORM_WASM_WASM_DROP_HANDLER_H_
#define YAZE_APP_PLATFORM_WASM_WASM_DROP_HANDLER_H_

#ifdef __EMSCRIPTEN__

#include <functional>
#include <memory>
#include <string>
#include <vector>

#include "absl/status/status.h"
#include "absl/status/statusor.h"

namespace yaze {
namespace platform {

/**
 * @class WasmDropHandler
 * @brief Handles drag and drop file operations in WASM/browser environment
 *
 * This class provides drag and drop functionality for ROM files in the browser,
 * allowing users to drag ROM files directly onto the web page to load them.
 * It supports .sfc, .smc, and .zip files containing ROMs.
 */
class WasmDropHandler {
 public:
  /**
   * @brief Callback type for when ROM data is received via drop
   * @param filename Name of the dropped file
   * @param data Binary data of the file
   */
  using DropCallback = std::function<void(const std::string& filename,
                                          const std::vector<uint8_t>& data)>;

  /**
   * @brief Callback type for error handling
   * @param error_message Description of the error
   */
  using ErrorCallback = std::function<void(const std::string& error_message)>;

  /**
   * @brief Get the singleton instance of WasmDropHandler
   * @return Reference to the singleton instance
   */
  static WasmDropHandler& GetInstance();

  /**
   * @brief Initialize the drop zone on a specific DOM element
   * @param element_id ID of the DOM element to use as drop zone (default: document body)
   * @param on_drop Callback to invoke when a valid file is dropped
   * @param on_error Optional callback for error handling
   * @return Status indicating success or failure
   */
  absl::Status Initialize(const std::string& element_id = "",
                          DropCallback on_drop = nullptr,
                          ErrorCallback on_error = nullptr);

  /**
   * @brief Register or update the drop callback
   * @param on_drop Callback to invoke when a file is dropped
   */
  void SetDropCallback(DropCallback on_drop);

  /**
   * @brief Register or update the error callback
   * @param on_error Callback to invoke on error
   */
  void SetErrorCallback(ErrorCallback on_error);

  /**
   * @brief Enable or disable the drop zone
   * @param enabled true to enable drop zone, false to disable
   */
  void SetEnabled(bool enabled);

  /**
   * @brief Check if drop zone is currently enabled
   * @return true if drop zone is active
   */
  bool IsEnabled() const { return enabled_; }

  /**
   * @brief Show or hide the drop zone overlay
   * @param visible true to show overlay, false to hide
   */
  void SetOverlayVisible(bool visible);

  /**
   * @brief Update the overlay text displayed when dragging
   * @param text Text to display in the overlay
   */
  void SetOverlayText(const std::string& text);

  /**
   * @brief Check if drag and drop is supported in the current browser
   * @return true if drag and drop is available
   */
  static bool IsSupported();

  /**
   * @brief Validate if a file is a supported ROM format
   * @param filename Name of the file to validate
   * @return true if file has valid ROM extension
   */
  static bool IsValidRomFile(const std::string& filename);

  /**
   * @brief Handle a dropped file (called from JavaScript)
   * @param filename The dropped filename
   * @param data Pointer to file data
   * @param size Size of file data
   */
  static void HandleDroppedFile(const char* filename, const uint8_t* data,
                                size_t size);

  /**
   * @brief Handle drop error (called from JavaScript)
   * @param error_message Error description
   */
  static void HandleDropError(const char* error_message);

  /**
   * @brief Handle drag enter event (called from JavaScript)
   */
  static void HandleDragEnter();

  /**
   * @brief Handle drag leave event (called from JavaScript)
   */
  static void HandleDragLeave();

 public:
  ~WasmDropHandler();

 private:
  // Singleton pattern - private constructor
  WasmDropHandler();

  // Delete copy constructor and assignment operator
  WasmDropHandler(const WasmDropHandler&) = delete;
  WasmDropHandler& operator=(const WasmDropHandler&) = delete;

  // Instance data
  bool initialized_ = false;
  bool enabled_ = false;
  DropCallback drop_callback_;
  ErrorCallback error_callback_;
  std::string element_id_;
  int drag_counter_ = 0;  // Track nested drag enter/leave events

  // Static singleton instance
  static std::unique_ptr<WasmDropHandler> instance_;
};

}  // namespace platform
}  // namespace yaze

// C-style callbacks for JavaScript interop
extern "C" {
void yazeHandleDroppedFile(const char* filename, const uint8_t* data,
                           size_t size);
void yazeHandleDropError(const char* error_message);
void yazeHandleDragEnter();
void yazeHandleDragLeave();
}

#endif  // __EMSCRIPTEN__

#endif  // YAZE_APP_PLATFORM_WASM_WASM_DROP_HANDLER_H_