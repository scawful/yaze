# WASM Web Features Roadmap

This document captures planned features for the browser-based yaze editor.

## Foundation (Completed)

The following infrastructure is in place:

- **Phase 1**: File System Layer (WasmStorage, WasmFileDialog) - IndexedDB storage
- **Phase 2**: Error Handling (WasmErrorHandler) - Browser UI integration
- **Phase 3**: Progressive Loading (WasmLoadingManager) - Cancellable loading with progress
- **Phase 4**: Offline Support (service-worker.js, manifest.json) - PWA capabilities
- **Phase 5**: AI Integration (BrowserAIService, WasmSecureStorage) - WebLLM ready
- **Phase 6**: Local Storage (WasmSettings, WasmAutosave) - Preferences persistence
- **Phase 7**: Web Workers (WasmWorkerPool) - Background task processing
- **Phase 8**: WebAudio (WasmAudio) - SPC700 audio playback

---

## Phase 9: Enhanced File Handling

### 9.1 Drag & Drop ROM Loading
- **Status**: Planned
- **Priority**: High
- **Description**: Enhanced drag-and-drop interface for ROM files
- **Features**:
  - Visual drop zone with hover effects
  - File type validation before processing
  - Preview panel showing ROM metadata
  - Multiple file support (load ROM + patch together)
- **Files**: `src/app/platform/wasm/wasm_drop_handler.{h,cc}`, `src/web/drop_zone.{js,css}`

### 9.2 Export Options
- **Status**: Planned
- **Priority**: High
- **Description**: Export modifications as patches instead of full ROMs
- **Features**:
  - BPS patch generation (standard format)
  - IPS patch generation (legacy support)
  - UPS patch generation (alternative)
  - Patch preview showing changed bytes
  - Direct download or save to IndexedDB
- **Files**: `src/app/platform/wasm/wasm_patch_export.{h,cc}`

---

## Phase 10: Collaboration

### 10.1 Real-time Collaboration
- **Status**: Planned
- **Priority**: High
- **Description**: Multi-user editing via WebSocket
- **Features**:
  - Session creation/joining with room codes
  - User presence indicators (cursors, selections)
  - Change synchronization via operational transforms
  - Chat/comments sidebar
  - Permission levels (owner, editor, viewer)
- **Files**: `src/app/platform/wasm/wasm_collaboration.{h,cc}`, `src/web/collaboration_ui.{js,css}`
- **Dependencies**: EmscriptenWebSocket (completed)

### 10.2 ShareLink Generation
- **Status**: Future
- **Priority**: Medium
- **Description**: Create shareable URLs with embedded patches
- **Features**:
  - Base64-encoded diff in URL hash
  - Short URL generation via service
  - QR code generation for mobile sharing

### 10.3 Comment Annotations
- **Status**: Future
- **Priority**: Low
- **Description**: Add notes to map locations/rooms
- **Features**:
  - Pin comments to coordinates
  - Threaded discussions
  - Export annotations as JSON

---

## Phase 11: Browser-Specific Enhancements

### 11.1 Keyboard Shortcut Overlay
- **Status**: Future
- **Priority**: Medium
- **Description**: Help panel showing all keyboard shortcuts
- **Features**:
  - Toggle with `?` key
  - Context-aware (shows relevant shortcuts for current editor)
  - Searchable

### 11.2 Touch Support
- **Status**: Future
- **Priority**: Medium
- **Description**: Touch gestures for tablet/mobile browsers
- **Features**:
  - Pinch to zoom
  - Two-finger pan
  - Long press for context menu
  - Touch-friendly toolbar

### 11.3 Fullscreen Mode
- **Status**: Future
- **Priority**: Low
- **Description**: Dedicated fullscreen API integration
- **Features**:
  - F11 toggle
  - Auto-hide toolbar in fullscreen
  - Escape to exit

### 11.4 Browser Notifications
- **Status**: Future
- **Priority**: Low
- **Description**: Alert when long operations complete
- **Features**:
  - Permission request flow
  - Build/export completion notifications
  - Background tab awareness

---

## Phase 12: AI Integration Enhancements

### 12.1 Browser-Local LLM
- **Status**: Future
- **Priority**: Medium
- **Description**: In-browser AI using WebLLM
- **Features**:
  - Model download/caching
  - Chat interface for ROM hacking questions
  - Code generation for ASM patches
- **Dependencies**: BrowserAIService (completed)

### 12.2 ROM Analysis Reports
- **Status**: Future
- **Priority**: Low
- **Description**: Generate sharable HTML reports
- **Features**:
  - Modification summary
  - Changed areas visualization
  - Exportable as standalone HTML

---

## Phase 13: Performance Optimizations

### 13.1 WebGPU Rendering
- **Status**: Future
- **Priority**: Medium
- **Description**: Modern GPU acceleration
- **Features**:
  - Feature detection with fallback to WebGL
  - Hardware-accelerated tile rendering
  - Shader-based effects

### 13.2 Lazy Tile Loading
- **Status**: Future
- **Priority**: Medium
- **Description**: Load only visible map sections
- **Features**:
  - Virtual scrolling for large maps
  - Tile cache management
  - Preload adjacent areas

---

## Phase 14: Cloud Features

### 14.1 Cloud ROM Storage
- **Status**: Future
- **Priority**: Low
- **Description**: Optional cloud sync for projects
- **Features**:
  - User accounts
  - Project backup/restore
  - Cross-device sync

### 14.2 Screenshot Gallery
- **Status**: Future
- **Priority**: Low
- **Description**: Save emulator screenshots
- **Features**:
  - Auto-capture on emulator test
  - Gallery view in IndexedDB
  - Share to social media

---

## Implementation Notes

### Technology Stack
- **Storage**: IndexedDB via WasmStorage
- **Networking**: EmscriptenWebSocket for real-time features
- **Background Processing**: WasmWorkerPool (pthread-based)
- **Audio**: WebAudio API via WasmAudio
- **UI**: ImGui with HTML overlays for browser-specific elements

### File Organization
```
src/app/platform/wasm/
├── wasm_storage.{h,cc}         # Phase 1 ✓
├── wasm_file_dialog.{h,cc}     # Phase 1 ✓
├── wasm_error_handler.{h,cc}   # Phase 2 ✓
├── wasm_loading_manager.{h,cc} # Phase 3 ✓
├── wasm_settings.{h,cc}        # Phase 6 ✓
├── wasm_autosave.{h,cc}        # Phase 6 ✓
├── wasm_browser_storage.{h,cc} # Phase 5 ✓
├── wasm_worker_pool.{h,cc}     # Phase 7 ✓
├── wasm_drop_handler.{h,cc}    # Phase 9 (planned)
├── wasm_patch_export.{h,cc}    # Phase 9 (planned)
└── wasm_collaboration.{h,cc}   # Phase 10 (planned)

src/app/emu/platform/wasm/
└── wasm_audio.{h,cc}           # Phase 8 ✓

src/web/
├── error_handler.{js,css}      # Phase 2 ✓
├── loading_indicator.{js,css}  # Phase 3 ✓
├── service-worker.js           # Phase 4 ✓
├── manifest.json               # Phase 4 ✓
├── drop_zone.{js,css}          # Phase 9 (planned)
└── collaboration_ui.{js,css}   # Phase 10 (planned)
```
