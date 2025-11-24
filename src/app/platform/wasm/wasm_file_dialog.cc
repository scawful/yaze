// clang-format off
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
std::unordered_map<int, WasmFileDialog::PendingOperation> WasmFileDialog::pending_operations_;
std::mutex WasmFileDialog::operations_mutex_;

// JavaScript interop for file operations
EM_JS(void, openFileDialog_impl, (const char* accept, int callback_id, bool is_text), {
  var input = document.createElement('input');
  input.type = 'file';
  input.accept = UTF8ToString(accept);
  input.style.display = 'none';
  input.onchange = function(e) {
    var file = e.target.files[0];
    if (!file) {
      var errPtr = allocateUTF8("No file selected");
      Module._yazeHandleFileError(callback_id, errPtr);
      _free(errPtr);
      return;
    }
    var reader = new FileReader();
    reader.onload = function() {
      var filename = file.name;
      var filenamePtr = allocateUTF8(filename);
      if (is_text) {
        var contentPtr = allocateUTF8(reader.result);
        Module._yazeHandleTextFileLoaded(callback_id, filenamePtr, contentPtr);
        _free(contentPtr);
      } else {
        var data = new Uint8Array(reader.result);
        var dataPtr = Module._malloc(data.length);
        Module.HEAPU8.set(data, dataPtr);
        Module._yazeHandleFileLoaded(callback_id, filenamePtr, dataPtr, data.length);
        Module._free(dataPtr);
      }
      _free(filenamePtr);
    };
    reader.onerror = function() {
      var errPtr = allocateUTF8("Failed to read file");
      Module._yazeHandleFileError(callback_id, errPtr);
      _free(errPtr);
    };
    if (is_text) {
      reader.readAsText(file);
    } else {
      reader.readAsArrayBuffer(file);
    }
  };
  document.body.appendChild(input);
  input.click();
  setTimeout(function() { document.body.removeChild(input); }, 100);
});

EM_JS(void, downloadFile_impl, (const char* filename, const uint8_t* data, size_t size, const char* mime_type), {
  var dataArray = HEAPU8.subarray(data, data + size);
  var blob = new Blob([dataArray], { type: UTF8ToString(mime_type) });
  var url = URL.createObjectURL(blob);
  var a = document.createElement('a');
  a.href = url;
  a.download = UTF8ToString(filename);
  a.style.display = 'none';
  document.body.appendChild(a);
  a.click();
  setTimeout(function() {
    document.body.removeChild(a);
    URL.revokeObjectURL(url);
  }, 100);
});

EM_JS(void, downloadTextFile_impl, (const char* filename, const char* content, const char* mime_type), {
  var blob = new Blob([UTF8ToString(content)], { type: UTF8ToString(mime_type) });
  var url = URL.createObjectURL(blob);
  var a = document.createElement('a');
  a.href = url;
  a.download = UTF8ToString(filename);
  a.style.display = 'none';
  document.body.appendChild(a);
  a.click();
  setTimeout(function() {
    document.body.removeChild(a);
    URL.revokeObjectURL(url);
  }, 100);
});

EM_JS(bool, isFileApiSupported, (), {
  return (typeof File !== 'undefined' && typeof FileReader !== 'undefined' && typeof Blob !== 'undefined' && typeof URL !== 'undefined' && typeof URL.createObjectURL !== 'undefined');
});

// Implementation of public methods
void WasmFileDialog::OpenFileDialog(const std::string& accept, FileLoadCallback on_load, ErrorCallback on_error) {
  PendingOperation op;
  op.binary_callback = on_load;
  op.error_callback = on_error;
  op.is_text = false;
  int callback_id = RegisterCallback(std::move(op));
  openFileDialog_impl(accept.c_str(), callback_id, false);
}

void WasmFileDialog::OpenTextFileDialog(const std::string& accept,
    std::function<void(const std::string&, const std::string&)> on_load,
    ErrorCallback on_error) {
  PendingOperation op;
  op.text_callback = on_load;
  op.error_callback = on_error;
  op.is_text = true;
  int callback_id = RegisterCallback(std::move(op));
  openFileDialog_impl(accept.c_str(), callback_id, true);
}

absl::Status WasmFileDialog::DownloadFile(const std::string& filename, const std::vector<uint8_t>& data) {
  if (!IsSupported()) {
    return absl::FailedPreconditionError("File API not supported in this browser");
  }
  if (data.empty()) {
    return absl::InvalidArgumentError("Cannot download empty file");
  }
  downloadFile_impl(filename.c_str(), data.data(), data.size(), "application/octet-stream");
  return absl::OkStatus();
}

absl::Status WasmFileDialog::DownloadTextFile(const std::string& filename, const std::string& content, const std::string& mime_type) {
  if (!IsSupported()) {
    return absl::FailedPreconditionError("File API not supported in this browser");
  }
  downloadTextFile_impl(filename.c_str(), content.c_str(), mime_type.c_str());
  return absl::OkStatus();
}

bool WasmFileDialog::IsSupported() {
  return isFileApiSupported();
}

// Private methods
int WasmFileDialog::RegisterCallback(PendingOperation operation) {
  std::lock_guard<std::mutex> lock(operations_mutex_);
  int id = next_callback_id_++;
  operation.id = id;
  pending_operations_[id] = std::move(operation);
  return id;
}

std::unique_ptr<WasmFileDialog::PendingOperation> WasmFileDialog::GetPendingOperation(int callback_id) {
  std::lock_guard<std::mutex> lock(operations_mutex_);
  auto it = pending_operations_.find(callback_id);
  if (it == pending_operations_.end()) {
    return nullptr;
  }
  auto op = std::make_unique<PendingOperation>(std::move(it->second));
  pending_operations_.erase(it);
  return op;
}

void WasmFileDialog::HandleFileLoaded(int callback_id, const char* filename, const uint8_t* data, size_t size) {
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

void WasmFileDialog::HandleTextFileLoaded(int callback_id, const char* filename, const char* content) {
  auto op = GetPendingOperation(callback_id);
  if (!op) {
    emscripten_log(EM_LOG_WARN, "Unknown callback ID: %d", callback_id);
    return;
  }
  if (op->text_callback) {
    op->text_callback(filename, content);
  }
}

void WasmFileDialog::HandleFileError(int callback_id, const char* error_message) {
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

// C-style callbacks for JavaScript interop - must be extern "C" with EMSCRIPTEN_KEEPALIVE
extern "C" {

EMSCRIPTEN_KEEPALIVE
void yazeHandleFileLoaded(int callback_id, const char* filename, const uint8_t* data, size_t size) {
  yaze::platform::WasmFileDialog::HandleFileLoaded(callback_id, filename, data, size);
}

EMSCRIPTEN_KEEPALIVE
void yazeHandleTextFileLoaded(int callback_id, const char* filename, const char* content) {
  yaze::platform::WasmFileDialog::HandleTextFileLoaded(callback_id, filename, content);
}

EMSCRIPTEN_KEEPALIVE
void yazeHandleFileError(int callback_id, const char* error_message) {
  yaze::platform::WasmFileDialog::HandleFileError(callback_id, error_message);
}

}  // extern "C"

#endif  // __EMSCRIPTEN__
// clang-format on
