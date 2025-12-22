#ifndef YAZE_APP_PLATFORM_WASM_WASM_FILE_DIALOG_H_
#define YAZE_APP_PLATFORM_WASM_WASM_FILE_DIALOG_H_

#ifdef __EMSCRIPTEN__

#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

#include "absl/status/status.h"
#include "absl/status/statusor.h"

namespace yaze {
namespace platform {

/**
 * @class WasmFileDialog
 * @brief File dialog implementation for WASM/browser environment
 *
 * This class provides file input/output functionality in the browser
 * using HTML5 File API and Blob downloads.
 */
class WasmFileDialog {
 public:
  /**
   * @brief Callback type for file load operations
   * @param filename Name of the loaded file
   * @param data Binary data of the file
   */
  using FileLoadCallback = std::function<void(
      const std::string& filename, const std::vector<uint8_t>& data)>;

  /**
   * @brief Callback type for error handling
   * @param error_message Description of the error
   */
  using ErrorCallback = std::function<void(const std::string& error_message)>;

  /**
   * @brief Open a file selection dialog
   * @param accept File type filter (e.g., ".sfc,.smc" for ROM files)
   * @param on_load Callback to invoke when file is loaded
   * @param on_error Optional callback for error handling
   */
  static void OpenFileDialog(const std::string& accept,
                             FileLoadCallback on_load,
                             ErrorCallback on_error = nullptr);

  /**
   * @brief Open a file selection dialog for text files
   * @param accept File type filter (e.g., ".json,.txt")
   * @param on_load Callback to invoke with file content as string
   * @param on_error Optional callback for error handling
   */
  static void OpenTextFileDialog(const std::string& accept,
                                 std::function<void(const std::string& filename,
                                                    const std::string& content)>
                                     on_load,
                                 ErrorCallback on_error = nullptr);

  /**
   * @brief Download a file to the user's downloads folder
   * @param filename Suggested filename for the download
   * @param data Binary data to download
   * @return Status indicating success or failure
   */
  static absl::Status DownloadFile(const std::string& filename,
                                   const std::vector<uint8_t>& data);

  /**
   * @brief Download a text file to the user's downloads folder
   * @param filename Suggested filename for the download
   * @param content Text content to download
   * @param mime_type MIME type (default: "text/plain")
   * @return Status indicating success or failure
   */
  static absl::Status DownloadTextFile(
      const std::string& filename, const std::string& content,
      const std::string& mime_type = "text/plain");

  /**
   * @brief Check if file dialogs are supported in the current environment
   * @return true if file operations are available
   */
  static bool IsSupported();

  /**
   * @brief Structure to hold pending file operation data
   */
  struct PendingOperation {
    int id;
    FileLoadCallback binary_callback;
    std::function<void(const std::string&, const std::string&)> text_callback;
    ErrorCallback error_callback;
    bool is_text;
  };

 private:
  // Callback management (thread-safe)
  static int next_callback_id_;
  static std::unordered_map<int, PendingOperation> pending_operations_;
  static std::mutex operations_mutex_;

  /**
   * @brief Register a callback for async file operations
   * @param operation The pending operation to register
   * @return Unique callback ID
   */
  static int RegisterCallback(PendingOperation operation);

  /**
   * @brief Get and remove a pending operation
   * @param callback_id The callback ID to retrieve
   * @return The pending operation or nullptr if not found
   */
  static std::unique_ptr<PendingOperation> GetPendingOperation(int callback_id);

 public:
  // These must be public to be called from extern "C" functions
  /**
   * @brief Handle file load completion (called from JavaScript)
   * @param callback_id The callback ID
   * @param filename The loaded filename
   * @param data Pointer to file data
   * @param size Size of file data
   */
  static void HandleFileLoaded(int callback_id, const char* filename,
                               const uint8_t* data, size_t size);

  /**
   * @brief Handle text file load completion (called from JavaScript)
   * @param callback_id The callback ID
   * @param filename The loaded filename
   * @param content The text content
   */
  static void HandleTextFileLoaded(int callback_id, const char* filename,
                                   const char* content);

  /**
   * @brief Handle file load error (called from JavaScript)
   * @param callback_id The callback ID
   * @param error_message Error description
   */
  static void HandleFileError(int callback_id, const char* error_message);
};

}  // namespace platform
}  // namespace yaze

// C-style callbacks for JavaScript interop
extern "C" {
void yazeHandleFileLoaded(int callback_id, const char* filename,
                          const uint8_t* data, size_t size);
void yazeHandleTextFileLoaded(int callback_id, const char* filename,
                              const char* content);
void yazeHandleFileError(int callback_id, const char* error_message);
}

#endif  // __EMSCRIPTEN__

#endif  // YAZE_APP_PLATFORM_WASM_WASM_FILE_DIALOG_H_