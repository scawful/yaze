#ifdef __EMSCRIPTEN__

#include "app/platform/wasm/wasm_file_dialog.h"

#include <emscripten.h>
#include <emscripten/html5.h>
#include <memory>
#include <unordered_map>

#include "absl/strings/str_format.h"

namespace yaze {
namespace platform {

// Static member initialization
int WasmFileDialog::next_callback_id_ = 1;
std::unordered_map<int, WasmFileDialog::PendingOperation>
    WasmFileDialog::pending_operations_;

// JavaScript interop for file operations
EM_JS(void, openFileDialog_impl,
      (const char* accept, int callback_id, bool is_text), {
        // Create a hidden file input element
        const input = document.createElement('input');
        input.type = 'file';
        input.accept = UTF8ToString(accept);
        input.style.display = 'none';

        input.onchange = (e) = > {
          const file = e.target.files[0];
          if (!file) {
            Module.__handleFileError(callback_id, "No file selected");
            return;
          }

          const reader = new FileReader();

          reader.onload = () = > {
            const filename = file.name;

            if (is_text) {
              // For text files, pass the content as string
              Module.__handleTextFileLoaded(callback_id, filename,
                                            reader.result);
            } else {
              // For binary files, pass as Uint8Array
              const data = new Uint8Array(reader.result);
              const dataPtr = Module._malloc(data.length);
              Module.HEAPU8.set(data, dataPtr);
              Module.__handleFileLoaded(callback_id, filename, dataPtr,
                                        data.length);
              Module._free(dataPtr);
            }
          };

          reader.onerror = () = > {
            Module.__handleFileError(callback_id,
                                     "Failed to read file: " + reader.error);
          };

          // Read file based on type
          if (is_text) {
            reader.readAsText(file);
          } else {
            reader.readAsArrayBuffer(file);
          }
        };

        // Add to DOM temporarily and trigger click
        document.body.appendChild(input);
        input.click();

        // Remove from DOM after a short delay
        setTimeout(() = > { document.body.removeChild(input); }, 100);
      });

EM_JS(void, downloadFile_impl,
      (const char* filename, const uint8_t* data, size_t size,
       const char* mime_type),
      {
        // Create a Blob from the data
        const dataArray = HEAPU8.subarray(data, data + size);
        const blob = new Blob([dataArray], {
          type:
            UTF8ToString(mime_type)
        });

        // Create download link
        const url = URL.createObjectURL(blob);
        const a = document.createElement('a');
        a.href = url;
        a.download = UTF8ToString(filename);
        a.style.display = 'none';

        // Trigger download
        document.body.appendChild(a);
        a.click();

        // Cleanup
        setTimeout(() = >
                        {
                          document.body.removeChild(a);
                          URL.revokeObjectURL(url);
                        },
                   100);
      });

EM_JS(void, downloadTextFile_impl,
      (const char* filename, const char* content, const char* mime_type), {
        // Create a Blob from the text content
        const blob = new Blob([UTF8ToString(content)], {
          type:
            UTF8ToString(mime_type)
        });

        // Create download link
        const url = URL.createObjectURL(blob);
        const a = document.createElement('a');
        a.href = url;
        a.download = UTF8ToString(filename);
        a.style.display = 'none';

        // Trigger download
        document.body.appendChild(a);
        a.click();

        // Cleanup
        setTimeout(() = >
                        {
                          document.body.removeChild(a);
                          URL.revokeObjectURL(url);
                        },
                   100);
      });

EM_JS(bool, isFileApiSupported, (), {
  return (typeof File != = 'undefined' && typeof FileReader !=
          = 'undefined' && typeof Blob != = 'undefined' && typeof URL !=
          = 'undefined' && typeof URL.createObjectURL != = 'undefined');
});

// Implementation of public methods

void WasmFileDialog::OpenFileDialog(const std::string& accept,
                                    FileLoadCallback on_load,
                                    ErrorCallback on_error) {
  PendingOperation op;
  op.binary_callback = on_load;
  op.error_callback = on_error;
  op.is_text = false;

  int callback_id = RegisterCallback(std::move(op));
  openFileDialog_impl(accept.c_str(), callback_id, false);
}

void WasmFileDialog::OpenTextFileDialog(
    const std::string& accept,
    std::function<void(const std::string&, const std::string&)> on_load,
    ErrorCallback on_error) {
  PendingOperation op;
  op.text_callback = on_load;
  op.error_callback = on_error;
  op.is_text = true;

  int callback_id = RegisterCallback(std::move(op));
  openFileDialog_impl(accept.c_str(), callback_id, true);
}

absl::Status WasmFileDialog::DownloadFile(const std::string& filename,
                                          const std::vector<uint8_t>& data) {
  if (!IsSupported()) {
    return absl::FailedPreconditionError(
        "File API not supported in this browser");
  }

  if (data.empty()) {
    return absl::InvalidArgumentError("Cannot download empty file");
  }

  downloadFile_impl(filename.c_str(), data.data(), data.size(),
                    "application/octet-stream");

  return absl::OkStatus();
}

absl::Status WasmFileDialog::DownloadTextFile(const std::string& filename,
                                              const std::string& content,
                                              const std::string& mime_type) {
  if (!IsSupported()) {
    return absl::FailedPreconditionError(
        "File API not supported in this browser");
  }

  downloadTextFile_impl(filename.c_str(), content.c_str(), mime_type.c_str());

  return absl::OkStatus();
}

bool WasmFileDialog::IsSupported() {
  return isFileApiSupported();
}

// Private methods

int WasmFileDialog::RegisterCallback(PendingOperation operation) {
  int id = next_callback_id_++;
  operation.id = id;
  pending_operations_[id] = std::move(operation);
  return id;
}

std::unique_ptr<WasmFileDialog::PendingOperation>
WasmFileDialog::GetPendingOperation(int callback_id) {
  auto it = pending_operations_.find(callback_id);
  if (it == pending_operations_.end()) {
    return nullptr;
  }

  auto op = std::make_unique<PendingOperation>(std::move(it->second));
  pending_operations_.erase(it);
  return op;
}

void WasmFileDialog::HandleFileLoaded(int callback_id, const char* filename,
                                      const uint8_t* data, size_t size) {
  auto op = GetPendingOperation(callback_id);
  if (!op) {
    emscripten_log(EM_LOG_WARN, "Unknown callback ID: %d", callback_id);
    return;
  }

  if (op->binary_callback) {
    std::vector<uint8_t> file_data(data, data + size);
    op->binary_callback(filename, file_data);
  }
}

void WasmFileDialog::HandleTextFileLoaded(int callback_id, const char* filename,
                                          const char* content) {
  auto op = GetPendingOperation(callback_id);
  if (!op) {
    emscripten_log(EM_LOG_WARN, "Unknown callback ID: %d", callback_id);
    return;
  }

  if (op->text_callback) {
    op->text_callback(filename, content);
  }
}

void WasmFileDialog::HandleFileError(int callback_id,
                                     const char* error_message) {
  auto op = GetPendingOperation(callback_id);
  if (!op) {
    emscripten_log(EM_LOG_WARN, "Unknown callback ID: %d", callback_id);
    return;
  }

  if (op->error_callback) {
    op->error_callback(error_message);
  } else {
    emscripten_log(EM_LOG_ERROR, "File operation error: %s", error_message);
  }
}

}  // namespace platform
}  // namespace yaze

// C-style callbacks for JavaScript interop
extern "C" {

EMSCRIPTEN_KEEPALIVE
void _handleFileLoaded(int callback_id, const char* filename,
                       const uint8_t* data, size_t size) {
  yaze::platform::WasmFileDialog::HandleFileLoaded(callback_id, filename, data,
                                                   size);
}

EMSCRIPTEN_KEEPALIVE
void _handleTextFileLoaded(int callback_id, const char* filename,
                           const char* content) {
  yaze::platform::WasmFileDialog::HandleTextFileLoaded(callback_id, filename,
                                                       content);
}

EMSCRIPTEN_KEEPALIVE
void _handleFileError(int callback_id, const char* error_message) {
  yaze::platform::WasmFileDialog::HandleFileError(callback_id, error_message);
}

}  // extern "C"

// Register JavaScript-callable functions with Module object
EM_JS(void, registerFileCallbacks, (), {
  Module.__handleFileLoaded = Module._handleFileLoaded;
  Module.__handleTextFileLoaded = Module._handleTextFileLoaded;
  Module.__handleFileError = Module._handleFileError;
});

// Initialize callbacks on module load
EMSCRIPTEN_BINDINGS(file_dialog_callbacks) {
  emscripten::function("initFileCallbacks", registerFileCallbacks);
}

// Auto-initialize on module startup
struct FileDialogInitializer {
  FileDialogInitializer() {
    EM_ASM({
      // Ensure callbacks are registered when module is ready
      if (Module.onRuntimeInitialized) {
        const originalInit = Module.onRuntimeInitialized;
        Module.onRuntimeInitialized = function() {
          originalInit();
          Module.initFileCallbacks();
        };
      } else {
        Module.onRuntimeInitialized = function() {
          Module.initFileCallbacks();
        };
      }
    });
  }
};

static FileDialogInitializer g_file_dialog_initializer;

#endif  // __EMSCRIPTEN__