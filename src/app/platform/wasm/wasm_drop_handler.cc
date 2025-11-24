// clang-format off
#ifdef __EMSCRIPTEN__

#include "app/platform/wasm/wasm_drop_handler.h"

#include <emscripten.h>
#include <emscripten/html5.h>
#include <algorithm>
#include <cctype>
#include <memory>

#include "absl/strings/str_format.h"

namespace yaze {
namespace platform {

// Static member initialization
std::unique_ptr<WasmDropHandler> WasmDropHandler::instance_ = nullptr;

// JavaScript interop for drag and drop operations
EM_JS(void, setupDropZone_impl, (const char* element_id), {
  var targetElement = document.body;
  if (element_id && UTF8ToString(element_id).length > 0) {
    var el = document.getElementById(UTF8ToString(element_id));
    if (el) {
      targetElement = el;
    }
  }

  // Remove existing event listeners if any
  if (window.yazeDropListeners) {
    window.yazeDropListeners.forEach(function(listener) {
      document.removeEventListener(listener.event, listener.handler);
    });
  }
  window.yazeDropListeners = [];

  // Create drop zone overlay if it doesn't exist
  var overlay = document.getElementById('yaze-drop-overlay');
  if (!overlay) {
    overlay = document.createElement('div');
    overlay.id = 'yaze-drop-overlay';
    overlay.className = 'yaze-drop-overlay';
    overlay.innerHTML = '<div class="yaze-drop-content"><div class="yaze-drop-icon">📁</div><div class="yaze-drop-text">Drop file here</div><div class="yaze-drop-info">Supported: .sfc, .smc, .zip, .pal, .tpl</div></div>';
    document.body.appendChild(overlay);
  }

  // Helper function to check if file is a ROM or supported asset
  function isSupportedFile(filename) {
    var ext = filename.toLowerCase().split('.').pop();
    return ext === 'sfc' || ext === 'smc' || ext === 'zip' || 
           ext === 'pal' || ext === 'tpl';
  }

  // Helper function to check if dragged items contain files
  function containsFiles(e) {
    if (e.dataTransfer.types) {
      for (var i = 0; i < e.dataTransfer.types.length; i++) {
        if (e.dataTransfer.types[i] === "Files") {
          return true;
        }
      }
    }
    return false;
  }

  // Drag enter handler
  function handleDragEnter(e) {
    if (containsFiles(e)) {
      e.preventDefault();
      e.stopPropagation();
      Module._yazeHandleDragEnter();
      overlay.classList.add('yaze-drop-active');
    }
  }

  // Drag over handler
  function handleDragOver(e) {
    if (containsFiles(e)) {
      e.preventDefault();
      e.stopPropagation();
      e.dataTransfer.dropEffect = 'copy';
    }
  }

  // Drag leave handler
  function handleDragLeave(e) {
    if (e.target === document || e.target === overlay) {
      e.preventDefault();
      e.stopPropagation();
      Module._yazeHandleDragLeave();
      overlay.classList.remove('yaze-drop-active');
    }
  }

  // Drop handler
  function handleDrop(e) {
    e.preventDefault();
    e.stopPropagation();

    overlay.classList.remove('yaze-drop-active');

    var files = e.dataTransfer.files;
    if (!files || files.length === 0) {
      var errPtr = allocateUTF8("No files dropped");
      Module._yazeHandleDropError(errPtr);
      _free(errPtr);
      return;
    }

    var file = files[0];  // Only handle first file

    if (!isSupportedFile(file.name)) {
      var errPtr = allocateUTF8("Invalid file type. Please drop a ROM (.sfc) or Palette (.pal, .tpl)");
      Module._yazeHandleDropError(errPtr);
      _free(errPtr);
      return;
    }

    // Show loading state in overlay
    overlay.classList.add('yaze-drop-loading');
    overlay.querySelector('.yaze-drop-text').textContent = 'Loading file...';
    overlay.querySelector('.yaze-drop-info').textContent = file.name + ' (' + (file.size / 1024).toFixed(1) + ' KB)';

    var reader = new FileReader();
    reader.onload = function() {
      var filename = file.name;
      var filenamePtr = allocateUTF8(filename);
      var data = new Uint8Array(reader.result);
      var dataPtr = Module._malloc(data.length);
      Module.HEAPU8.set(data, dataPtr);
      Module._yazeHandleDroppedFile(filenamePtr, dataPtr, data.length);
      Module._free(dataPtr);
      _free(filenamePtr);

      // Hide loading state
      setTimeout(function() {
        overlay.classList.remove('yaze-drop-loading');
        overlay.querySelector('.yaze-drop-text').textContent = 'Drop file here';
        overlay.querySelector('.yaze-drop-info').textContent = 'Supported: .sfc, .smc, .zip, .pal, .tpl';
      }, 500);
    };

    reader.onerror = function() {
      var errPtr = allocateUTF8("Failed to read file: " + file.name);
      Module._yazeHandleDropError(errPtr);
      _free(errPtr);
      overlay.classList.remove('yaze-drop-loading');
    };

    reader.readAsArrayBuffer(file);
  }

  // Register event listeners
  var dragEnterHandler = handleDragEnter;
  var dragOverHandler = handleDragOver;
  var dragLeaveHandler = handleDragLeave;
  var dropHandler = handleDrop;

  document.addEventListener('dragenter', dragEnterHandler, false);
  document.addEventListener('dragover', dragOverHandler, false);
  document.addEventListener('dragleave', dragLeaveHandler, false);
  document.addEventListener('drop', dropHandler, false);

  // Store listeners for cleanup
  window.yazeDropListeners = [
    { event: 'dragenter', handler: dragEnterHandler },
    { event: 'dragover', handler: dragOverHandler },
    { event: 'dragleave', handler: dragLeaveHandler },
    { event: 'drop', handler: dropHandler }
  ];
});

EM_JS(void, disableDropZone_impl, (), {
  if (window.yazeDropListeners) {
    window.yazeDropListeners.forEach(function(listener) {
      document.removeEventListener(listener.event, listener.handler);
    });
    window.yazeDropListeners = [];
  }
  var overlay = document.getElementById('yaze-drop-overlay');
  if (overlay) {
    overlay.classList.remove('yaze-drop-active');
    overlay.classList.remove('yaze-drop-loading');
  }
});

EM_JS(void, setOverlayVisible_impl, (bool visible), {
  var overlay = document.getElementById('yaze-drop-overlay');
  if (overlay) {
    overlay.style.display = visible ? 'flex' : 'none';
  }
});

EM_JS(void, setOverlayText_impl, (const char* text), {
  var overlay = document.getElementById('yaze-drop-overlay');
  if (overlay) {
    var textElement = overlay.querySelector('.yaze-drop-text');
    if (textElement) {
      textElement.textContent = UTF8ToString(text);
    }
  }
});

EM_JS(bool, isDragDropSupported, (), {
  return (typeof FileReader !== 'undefined' && typeof DataTransfer !== 'undefined' && 'draggable' in document.createElement('div'));
});

EM_JS(void, injectDropZoneStyles, (), {
  if (document.getElementById('yaze-drop-styles')) {
    return;  // Already injected
  }

  var style = document.createElement('style');
  style.id = 'yaze-drop-styles';
  style.textContent = `
    .yaze-drop-overlay {
      display: none;
      position: fixed;
      top: 0;
      left: 0;
      right: 0;
      bottom: 0;
      background: rgba(0, 0, 0, 0.85);
      z-index: 10000;
      align-items: center;
      justify-content: center;
      pointer-events: none;
      transition: all 0.3s ease;
    }

    .yaze-drop-overlay.yaze-drop-active {
      display: flex;
      pointer-events: all;
      background: rgba(0, 0, 0, 0.9);
    }

    .yaze-drop-overlay.yaze-drop-loading {
      display: flex;
      pointer-events: all;
    }

    .yaze-drop-content {
      text-align: center;
      color: white;
      font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', Roboto, sans-serif;
      padding: 60px;
      border-radius: 20px;
      background: rgba(255, 255, 255, 0.1);
      border: 3px dashed rgba(255, 255, 255, 0.5);
      animation: pulse 2s infinite;
    }

    .yaze-drop-overlay.yaze-drop-active .yaze-drop-content {
      border-color: #4CAF50;
      background: rgba(76, 175, 80, 0.2);
      animation: pulse-active 1s infinite;
    }

    .yaze-drop-overlay.yaze-drop-loading .yaze-drop-content {
      border-color: #2196F3;
      background: rgba(33, 150, 243, 0.2);
      border-style: solid;
      animation: none;
    }

    .yaze-drop-icon {
      font-size: 72px;
      margin-bottom: 20px;
      filter: drop-shadow(0 4px 6px rgba(0, 0, 0, 0.3));
    }

    .yaze-drop-text {
      font-size: 28px;
      font-weight: 600;
      margin-bottom: 10px;
      text-shadow: 0 2px 4px rgba(0, 0, 0, 0.5);
    }

    .yaze-drop-info {
      font-size: 16px;
      opacity: 0.8;
      font-weight: 400;
    }

    @keyframes pulse {
      0%, 100% { transform: scale(1); opacity: 1; }
      50% { transform: scale(1.02); opacity: 0.9; }
    }

    @keyframes pulse-active {
      0%, 100% { transform: scale(1); }
      50% { transform: scale(1.05); }
    }
  `;
  document.head.appendChild(style);
});

// WasmDropHandler implementation
WasmDropHandler& WasmDropHandler::GetInstance() {
  if (!instance_) {
    instance_ = std::unique_ptr<WasmDropHandler>(new WasmDropHandler());
  }
  return *instance_;
}

WasmDropHandler::WasmDropHandler() = default;
WasmDropHandler::~WasmDropHandler() {
  if (initialized_ && enabled_) {
    disableDropZone_impl();
  }
}

absl::Status WasmDropHandler::Initialize(const std::string& element_id,
                                         DropCallback on_drop,
                                         ErrorCallback on_error) {
  if (!IsSupported()) {
    return absl::FailedPreconditionError(
        "Drag and drop not supported in this browser");
  }

  // Inject CSS styles
  injectDropZoneStyles();

  // Set callbacks
  if (on_drop) {
    drop_callback_ = on_drop;
  }
  if (on_error) {
    error_callback_ = on_error;
  }

  // Setup drop zone
  element_id_ = element_id;
  setupDropZone_impl(element_id.c_str());

  initialized_ = true;
  enabled_ = true;

  return absl::OkStatus();
}

void WasmDropHandler::SetDropCallback(DropCallback on_drop) {
  drop_callback_ = on_drop;
}

void WasmDropHandler::SetErrorCallback(ErrorCallback on_error) {
  error_callback_ = on_error;
}

void WasmDropHandler::SetEnabled(bool enabled) {
  if (enabled_ != enabled) {
    enabled_ = enabled;
    if (!enabled) {
      disableDropZone_impl();
    } else if (initialized_) {
      setupDropZone_impl(element_id_.c_str());
    }
  }
}

void WasmDropHandler::SetOverlayVisible(bool visible) {
  setOverlayVisible_impl(visible);
}

void WasmDropHandler::SetOverlayText(const std::string& text) {
  setOverlayText_impl(text.c_str());
}

bool WasmDropHandler::IsSupported() {
  return isDragDropSupported();
}

bool WasmDropHandler::IsValidRomFile(const std::string& filename) {
  // Get file extension
  size_t dot_pos = filename.find_last_of('.');
  if (dot_pos == std::string::npos) {
    return false;
  }

  std::string ext = filename.substr(dot_pos + 1);

  // Convert to lowercase for comparison
  std::transform(ext.begin(), ext.end(), ext.begin(),
                 [](unsigned char c) { return std::tolower(c); });

  return ext == "sfc" || ext == "smc" || ext == "zip" ||
         ext == "pal" || ext == "tpl";
}

void WasmDropHandler::HandleDroppedFile(const char* filename,
                                        const uint8_t* data, size_t size) {
  auto& instance = GetInstance();

  // Validate file
  if (!IsValidRomFile(filename)) {
    HandleDropError("Invalid file format");
    return;
  }

  // Call the drop callback
  if (instance.drop_callback_) {
    std::vector<uint8_t> file_data(data, data + size);
    instance.drop_callback_(filename, file_data);
  } else {
    emscripten_log(EM_LOG_WARN, "No drop callback registered for file: %s",
                   filename);
  }

  // Reset drag counter
  instance.drag_counter_ = 0;
}

void WasmDropHandler::HandleDropError(const char* error_message) {
  auto& instance = GetInstance();

  if (instance.error_callback_) {
    instance.error_callback_(error_message);
  } else {
    emscripten_log(EM_LOG_ERROR, "Drop error: %s", error_message);
  }

  // Reset drag counter
  instance.drag_counter_ = 0;
}

void WasmDropHandler::HandleDragEnter() {
  auto& instance = GetInstance();
  instance.drag_counter_++;
}

void WasmDropHandler::HandleDragLeave() {
  auto& instance = GetInstance();
  instance.drag_counter_--;

  // Only truly left when counter reaches 0
  if (instance.drag_counter_ <= 0) {
    instance.drag_counter_ = 0;
  }
}

}  // namespace platform
}  // namespace yaze

// C-style callbacks for JavaScript interop - must be extern "C" with EMSCRIPTEN_KEEPALIVE
extern "C" {

EMSCRIPTEN_KEEPALIVE
void yazeHandleDroppedFile(const char* filename, const uint8_t* data,
                           size_t size) {
  yaze::platform::WasmDropHandler::HandleDroppedFile(filename, data, size);
}

EMSCRIPTEN_KEEPALIVE
void yazeHandleDropError(const char* error_message) {
  yaze::platform::WasmDropHandler::HandleDropError(error_message);
}

EMSCRIPTEN_KEEPALIVE
void yazeHandleDragEnter() {
  yaze::platform::WasmDropHandler::HandleDragEnter();
}

EMSCRIPTEN_KEEPALIVE
void yazeHandleDragLeave() {
  yaze::platform::WasmDropHandler::HandleDragLeave();
}

}  // extern "C"

#endif  // __EMSCRIPTEN__
// clang-format on