# WASM Web App Enhancements Plan

## Executive Summary

This document outlines the comprehensive plan to make yaze's WASM web build fully featured, robust, and user-friendly. The goal is to achieve feature parity with the native desktop application where technically feasible, while leveraging browser-specific capabilities where appropriate.

## Current Status

### Completed
- [x] Basic WASM build configuration
- [x] pthread support for Emscripten
- [x] Network abstraction layer (IHttpClient, IWebSocket)
- [x] Emscripten HTTP client using `emscripten_fetch()`
- [x] Emscripten WebSocket using browser API
- [x] **Phase 1**: File System Layer (WasmStorage, WasmFileDialog)
- [x] **Phase 2**: Error Handling Infrastructure (WasmErrorHandler)
- [x] **Phase 3**: Progressive Loading UI (WasmLoadingManager)
- [x] **Phase 4**: Offline Support (Service Workers, PWA manifest)
- [x] **Phase 5**: AI Service Integration (BrowserAIService, WasmSecureStorage)
- [x] **Phase 6**: Local Storage Persistence (WasmSettings, AutoSaveManager)

- [x] **Phase 7**: Web Workers for heavy processing (WasmWorkerPool)
- [x] **Phase 8**: Emulator Audio (WebAudio, WasmAudioBackend)

### In Progress
- [ ] WASM CI build verification
- [ ] Integration of loading manager with gfx::Arena
- [ ] Integration testing across all phases

---

## Phase 1: File System Layer

### Overview
WASM builds cannot access the local filesystem directly. We need a virtualized file system layer that uses browser storage APIs.

### Implementation

#### 1.1 IndexedDB Storage Backend
```cpp
// src/app/platform/wasm/wasm_storage.h
class WasmStorage {
 public:
  // Store ROM data
  static absl::Status SaveRom(const std::string& name, const std::vector<uint8_t>& data);
  static absl::StatusOr<std::vector<uint8_t>> LoadRom(const std::string& name);
  static absl::Status DeleteRom(const std::string& name);
  static std::vector<std::string> ListRoms();

  // Store project files (JSON, palettes, patches)
  static absl::Status SaveProject(const std::string& name, const std::string& json);
  static absl::StatusOr<std::string> LoadProject(const std::string& name);

  // Store user preferences
  static absl::Status SavePreferences(const nlohmann::json& prefs);
  static absl::StatusOr<nlohmann::json> LoadPreferences();
};
```

#### 1.2 File Upload Handler
```cpp
// JavaScript interop for file input
EM_JS(void, openFileDialog, (const char* accept, int callback_id), {
  const input = document.createElement('input');
  input.type = 'file';
  input.accept = UTF8ToString(accept);
  input.onchange = (e) => {
    const file = e.target.files[0];
    const reader = new FileReader();
    reader.onload = () => {
      const data = new Uint8Array(reader.result);
      Module._handleFileLoaded(callback_id, data);
    };
    reader.readAsArrayBuffer(file);
  };
  input.click();
});
```

#### 1.3 File Download Handler
```cpp
// Download ROM or project file
EM_JS(void, downloadFile, (const char* filename, const uint8_t* data, size_t size), {
  const blob = new Blob([HEAPU8.subarray(data, data + size)], {type: 'application/octet-stream'});
  const url = URL.createObjectURL(blob);
  const a = document.createElement('a');
  a.href = url;
  a.download = UTF8ToString(filename);
  a.click();
  URL.revokeObjectURL(url);
});
```

### Files to Create
- `src/app/platform/wasm/wasm_storage.h`
- `src/app/platform/wasm/wasm_storage.cc`
- `src/app/platform/wasm/wasm_file_dialog.h`
- `src/app/platform/wasm/wasm_file_dialog.cc`

---

## Phase 2: Error Handling Infrastructure

### Overview
Browser-based applications need specialized error handling that integrates with the web UI and provides user-friendly feedback.

### Implementation

#### 2.1 Browser Error Handler
```cpp
// src/app/platform/wasm/wasm_error_handler.h
class WasmErrorHandler {
 public:
  // Display error in browser UI
  static void ShowError(const std::string& title, const std::string& message);
  static void ShowWarning(const std::string& title, const std::string& message);
  static void ShowInfo(const std::string& title, const std::string& message);

  // Toast notifications (non-blocking)
  static void Toast(const std::string& message, ToastType type, int duration_ms = 3000);

  // Progress indicators
  static void ShowProgress(const std::string& task, float progress);
  static void HideProgress();

  // Confirmation dialogs
  static void Confirm(const std::string& message, std::function<void(bool)> callback);
};
```

#### 2.2 JavaScript Integration
```javascript
// src/web/error_handler.js
window.showYazeError = function(title, message) {
  // Create styled error modal
  const modal = document.createElement('div');
  modal.className = 'yaze-error-modal';
  modal.innerHTML = `
    <div class="yaze-error-content">
      <h2>${escapeHtml(title)}</h2>
      <p>${escapeHtml(message)}</p>
      <button onclick="this.parentElement.parentElement.remove()">OK</button>
    </div>
  `;
  document.body.appendChild(modal);
};

window.showYazeToast = function(message, type, duration) {
  const toast = document.createElement('div');
  toast.className = `yaze-toast yaze-toast-${type}`;
  toast.textContent = message;
  document.body.appendChild(toast);
  setTimeout(() => toast.remove(), duration);
};
```

### Files to Create
- `src/app/platform/wasm/wasm_error_handler.h`
- `src/app/platform/wasm/wasm_error_handler.cc`
- `src/web/error_handler.js`
- `src/web/error_handler.css`

---

## Phase 3: Progressive Loading UI

### Overview
ROM loading and graphics processing can take significant time. Users need visual feedback and the ability to cancel long operations.

### Implementation

#### 3.1 Loading Manager
```cpp
// src/app/platform/wasm/wasm_loading_manager.h
class WasmLoadingManager {
 public:
  // Start a loading operation with progress tracking
  static LoadingHandle BeginLoading(const std::string& task_name);

  // Update progress (0.0 to 1.0)
  static void UpdateProgress(LoadingHandle handle, float progress);
  static void UpdateMessage(LoadingHandle handle, const std::string& message);

  // Check if user requested cancel
  static bool IsCancelled(LoadingHandle handle);

  // Complete the loading operation
  static void EndLoading(LoadingHandle handle);
};
```

#### 3.2 Integration with gfx::Arena
```cpp
// Modified graphics loading to report progress
void Arena::LoadGraphicsWithProgress(Rom* rom) {
  auto handle = WasmLoadingManager::BeginLoading("Loading Graphics");

  for (int i = 0; i < kNumGraphicsSheets; i++) {
    if (WasmLoadingManager::IsCancelled(handle)) {
      WasmLoadingManager::EndLoading(handle);
      return;  // User cancelled
    }

    LoadGraphicsSheet(rom, i);
    WasmLoadingManager::UpdateProgress(handle, static_cast<float>(i) / kNumGraphicsSheets);
    WasmLoadingManager::UpdateMessage(handle, absl::StrFormat("Sheet %d/%d", i, kNumGraphicsSheets));

    // Yield to browser event loop periodically
    emscripten_sleep(0);
  }

  WasmLoadingManager::EndLoading(handle);
}
```

### Files to Create
- `src/app/platform/wasm/wasm_loading_manager.h`
- `src/app/platform/wasm/wasm_loading_manager.cc`
- `src/web/loading_indicator.js`
- `src/web/loading_indicator.css`

---

## Phase 4: Offline Support (Service Workers)

### Overview
Cache the WASM binary and assets for offline use, enabling users to work without an internet connection.

### Implementation

#### 4.1 Service Worker
```javascript
// src/web/service-worker.js
const CACHE_NAME = 'yaze-cache-v1';
const ASSETS = [
  '/yaze.wasm',
  '/yaze.js',
  '/yaze.data',
  '/index.html',
  '/style.css',
  '/fonts/',
];

self.addEventListener('install', (event) => {
  event.waitUntil(
    caches.open(CACHE_NAME).then((cache) => cache.addAll(ASSETS))
  );
});

self.addEventListener('fetch', (event) => {
  event.respondWith(
    caches.match(event.request).then((response) => {
      return response || fetch(event.request);
    })
  );
});
```

#### 4.2 Registration
```javascript
// src/web/index.html
if ('serviceWorker' in navigator) {
  navigator.serviceWorker.register('/service-worker.js')
    .then((registration) => {
      console.log('Service Worker registered:', registration.scope);
    });
}
```

### Files to Create
- `src/web/service-worker.js`
- `src/web/manifest.json` (PWA manifest)

---

## Phase 5: AI Service Integration

### Overview
Integrate the network abstraction layer with AI services (Gemini, Ollama) for browser-based AI assistance.

### Implementation

#### 5.1 Browser AI Client
```cpp
// src/cli/service/ai/browser_ai_service.h
#ifdef __EMSCRIPTEN__
class BrowserAIService : public IAIService {
 public:
  explicit BrowserAIService(const AIConfig& config);

  absl::StatusOr<std::string> GenerateResponse(const std::string& prompt) override;
  absl::StatusOr<std::string> AnalyzeImage(const gfx::Bitmap& image,
                                            const std::string& prompt) override;

 private:
  std::unique_ptr<IHttpClient> http_client_;
  std::string api_key_;  // User-provided, never stored in binary
  std::string model_;
};
#endif
```

#### 5.2 API Key Management
```cpp
// Secure API key storage in browser
EM_JS(void, storeApiKey, (const char* service, const char* key), {
  // Use sessionStorage for temporary storage (cleared on tab close)
  // or encrypted localStorage for persistent storage
  sessionStorage.setItem('yaze_' + UTF8ToString(service) + '_key', UTF8ToString(key));
});

EM_JS(char*, retrieveApiKey, (const char* service), {
  const key = sessionStorage.getItem('yaze_' + UTF8ToString(service) + '_key');
  if (!key) return null;
  const len = lengthBytesUTF8(key) + 1;
  const ptr = _malloc(len);
  stringToUTF8(key, ptr, len);
  return ptr;
});
```

### CORS Considerations
- Gemini API: Should work with browser fetch (Google APIs support CORS)
- Ollama: Requires `--cors` flag or proxy server for local instances

### Files to Create
- `src/cli/service/ai/browser_ai_service.h`
- `src/cli/service/ai/browser_ai_service.cc`
- `src/app/platform/wasm/wasm_browser_storage.h`
- `src/app/platform/wasm/wasm_browser_storage.cc`

---

## Phase 6: Local Storage Persistence

### Overview
Persist user settings, recent files, undo history, and workspace layouts in browser storage.

### Implementation

#### 6.1 Settings Persistence
```cpp
// src/app/platform/wasm/wasm_settings.h
class WasmSettings {
 public:
  // User preferences
  static void SaveTheme(const std::string& theme);
  static std::string LoadTheme();

  // Recent files (stored as IndexedDB references)
  static void AddRecentFile(const std::string& name);
  static std::vector<std::string> GetRecentFiles();

  // Workspace layouts
  static void SaveWorkspace(const std::string& name, const std::string& layout_json);
  static std::string LoadWorkspace(const std::string& name);

  // Undo history (for crash recovery)
  static void SaveUndoHistory(const std::string& editor, const std::vector<uint8_t>& history);
  static std::vector<uint8_t> LoadUndoHistory(const std::string& editor);
};
```

#### 6.2 Auto-Save & Recovery
```cpp
// Periodic auto-save
class AutoSaveManager {
 public:
  void Start(int interval_seconds = 60);
  void Stop();

  // Called on page unload
  void EmergencySave();

  // Called on startup
  bool HasRecoveryData();
  void RecoverLastSession();
  void ClearRecoveryData();
};
```

### Files to Create
- `src/app/platform/wasm/wasm_settings.h`
- `src/app/platform/wasm/wasm_settings.cc`
- `src/app/platform/wasm/wasm_autosave.h`
- `src/app/platform/wasm/wasm_autosave.cc`

---

## Phase 7: Web Workers for Heavy Processing

### Overview
Offload CPU-intensive operations to Web Workers to prevent UI freezing.

### Implementation

#### 7.1 Background Processing Worker
```cpp
// Operations to run in Web Worker:
// - ROM decompression (LC-LZ2)
// - Graphics sheet decoding
// - Palette calculations
// - Asar assembly compilation

class WasmWorkerPool {
 public:
  using TaskCallback = std::function<void(const std::vector<uint8_t>&)>;

  // Submit work to background thread
  void SubmitTask(const std::string& type,
                  const std::vector<uint8_t>& input,
                  TaskCallback callback);

  // Wait for all tasks
  void WaitAll();
};
```

### Files to Create
- `src/app/platform/wasm/wasm_worker_pool.h`
- `src/app/platform/wasm/wasm_worker_pool.cc`
- `src/web/worker.js`

---

## Phase 8: Emulator Integration

### Overview
The SNES emulator can run in WASM with WebAudio for sound output.

### Implementation

#### 8.1 WebAudio Backend
```cpp
// src/app/emu/platform/wasm/wasm_audio.h
class WasmAudioBackend : public IAudioBackend {
 public:
  void Initialize(int sample_rate, int buffer_size) override;
  void QueueSamples(const int16_t* samples, size_t count) override;
  void Shutdown() override;
};
```

#### 8.2 Canvas Rendering
```cpp
// Use EM_ASM for direct canvas manipulation if needed
// Or use existing ImGui/SDL2 rendering (already WASM compatible)
```

### Files to Create
- `src/app/emu/platform/wasm/wasm_audio.h`
- `src/app/emu/platform/wasm/wasm_audio.cc`

---

## Feature Availability Matrix

| Feature | Native | WASM | Notes |
|---------|--------|------|-------|
| ROM Loading | File dialog | File input + IndexedDB | Full support |
| ROM Saving | Direct write | Blob download | Full support |
| Overworld Editor | Full | Full | No limitations |
| Dungeon Editor | Full | Full | No limitations |
| Graphics Editor | Full | Full | No limitations |
| Palette Editor | Full | Full | No limitations |
| Emulator | Full | Full | WebAudio for sound |
| AI (Gemini) | HTTP | Browser Fetch | Requires API key |
| AI (Ollama) | HTTP | Requires proxy | CORS limitation |
| Asar Assembly | Full | In-memory | No file I/O |
| Collaboration | gRPC | WebSocket | Different protocol |
| Crash Recovery | File-based | IndexedDB | Equivalent |
| Offline Mode | N/A | Service Worker | WASM-only feature |

---

## CMake Configuration

### Recommended Preset Updates
```json
{
  "name": "wasm-release",
  "cacheVariables": {
    "YAZE_BUILD_WASM_PLATFORM": "ON",
    "YAZE_WASM_ENABLE_WORKERS": "ON",
    "YAZE_WASM_ENABLE_OFFLINE": "ON",
    "YAZE_WASM_ENABLE_AI": "ON"
  }
}
```

### Source Organization
```
src/app/platform/
  wasm/
    wasm_storage.h/.cc
    wasm_file_dialog.h/.cc
    wasm_error_handler.h/.cc
    wasm_loading_manager.h/.cc
    wasm_settings.h/.cc
    wasm_autosave.h/.cc
    wasm_worker_pool.h/.cc
    wasm_browser_storage.h/.cc
src/web/
  index.html
  shell.html
  style.css
  error_handler.js
  loading_indicator.js
  service-worker.js
  worker.js
  manifest.json
```

---

## Implementation Priority

### High Priority (Required for MVP)
1. File System Layer (Phase 1)
2. Error Handling (Phase 2)
3. Progressive Loading (Phase 3)

### Medium Priority (Enhanced Experience)
4. Local Storage Persistence (Phase 6)
5. Offline Support (Phase 4)

### Lower Priority (Advanced Features)
6. AI Integration (Phase 5)
7. Web Workers (Phase 7)
8. Emulator Audio (Phase 8)

---

## Success Criteria

- [ ] User can load ROM from local file system
- [ ] User can save modified ROM to downloads
- [ ] Loading progress is visible with cancel option
- [ ] Errors display user-friendly messages
- [ ] Settings persist across sessions
- [ ] App works offline after first load
- [ ] AI assistance available via Gemini API
- [ ] No UI freezes during heavy operations
- [ ] Emulator runs with sound

---

## References

- [Emscripten Documentation](https://emscripten.org/docs/)
- [IndexedDB API](https://developer.mozilla.org/en-US/docs/Web/API/IndexedDB_API)
- [Service Workers](https://developer.mozilla.org/en-US/docs/Web/API/Service_Worker_API)
- [Web Workers](https://developer.mozilla.org/en-US/docs/Web/API/Web_Workers_API)
- [WebAudio API](https://developer.mozilla.org/en-US/docs/Web/API/Web_Audio_API)
