# WASM / Web Agent Integration Status

**Last Updated:** November 25, 2025
**Status:** Functional MVP with Agent APIs (ROM loading fixed, loading progress added, control APIs implemented, performance optimizations applied)

## Overview
This document tracks the development state of the `yaze` WASM web application, specifically focusing on the AI Agent integration (`z3ed` console) and the modern UI overhaul.

## 1. Completed Features

### ROM Loading & Initialization (November 2025 Fixes)
*   **ROM File Validation (`rom_file_manager.cc`):**
    *   Fixed minimum ROM size check from 1MB to 512KB (was rejecting valid 1MB Zelda 3 ROMs)
*   **CMake WASM Configuration (`app.cmake`):**
    *   Added `MODULARIZE=1` and `EXPORT_NAME='createYazeModule'` to match `app.js` expectations
    *   Added missing exports: `_yazeHandleDroppedFile`, `_yazeHandleDropError`, `_yazeHandleDragEnter`, `_yazeHandleDragLeave`, `_malloc`, `_free`
    *   Added missing runtime methods: `lengthBytesUTF8`, `IDBFS`, `allocateUTF8`
*   **JavaScript Fixes (`filesystem_manager.js`):**
    *   Fixed `Module.ccall` return type from `'null'` (string) to `null`
    *   Fixed direct function fallback to properly allocate/free memory for string parameters
*   **Drop Zone (`drop_zone.js`):**
    *   Disabled duplicate auto-initialization (conflicted with C++ handler)
    *   Now delegates to `FilesystemManager.handleRomUpload` instead of calling non-existent function
*   **Loading Progress (`editor_manager.cc`):**
    *   Added `WasmLoadingManager` integration to `LoadAssets()`
    *   Shows progress for each editor: "Loading overworld...", "Loading dungeons...", etc.
*   **UI Streamlining (`shell.html`, `app.js`):**
    *   Removed HTML welcome screen - canvas is always visible
    *   Loading overlay shows during initialization with status messages

### AI Agent Integration
*   **Core Bridge (`wasm_terminal_bridge.cc`):**
    *   Exposes `Z3edProcessCommand` to JavaScript.
    *   Exposes `GetGlobalBrowserAIService()` and `GetGlobalRom()` to C++ handlers.
*   **Browser Agent (`browser_agent.cc`):**
    *   **`agent chat`**: Fully functional with conversation history. Uses `std::thread` for non-blocking AI calls.
    *   **`agent plan`**: Generates text-based implementation plans (asynchronous).
    *   **`agent diff`**: Shows the "pending plan" (conceptual diff).
    *   **`agent list/describe`**: Introspects ROM resources via `ResourceCatalog`.
    *   **`agent todo`**: Fully implemented with persistent storage.
*   **Browser AI Service** (`src/cli/service/ai/browser_ai_service.cc`):
    *   Implements `AIService` interface for browser-based AI calls
    *   Uses `IHttpClient` from network abstraction layer (CORS-compatible)
    *   Supports Gemini API (text and vision models)
    *   Secures API keys via sessionStorage (cleared on tab close)
    *   Comprehensive error handling with `absl::Status`
*   **Browser Storage** (`src/app/platform/wasm/wasm_browser_storage.cc`):
    *   Non-hardcoded API key management via sessionStorage/localStorage
    *   User-provided keys, never embedded in binary
    *   Namespaced storage to avoid conflicts
*   **Persistence (`todo_manager.cc`):**
    *   Updated to use `WasmStorage` (IndexedDB) when compiled for Emscripten. TODOs persist across reloads.

### UI & UX
*   **Drag & Drop (`wasm_drop_handler.cc`):**
    *   Supports `.sfc`, `.smc`, `.zip`.
    *   Automatically writes to `/roms/` in MEMFS and loads the ROM.
    *   Stubbed support for `.pal` / `.tpl`.
*   **Modern Interface:**
    *   **`main.css`**: Unified design system (VS Code dark theme variables).
    *   **`app.js`**: Extracted logic from `shell.html`. Handles terminal resize, zoom, and PWA updates.
    *   **Components**: `terminal.css`, `collab_console.css`, etc., updated to use CSS variables.

### WASM Control APIs (November 2025)

The WASM build now exposes comprehensive JavaScript APIs for programmatic control, enabling LLM agents with DOM access to interact with the editor.

#### Editor State APIs (`window.yaze.editor`)
*   **`getSnapshot()`**: Get current editor state (type, ROM status, active data)
*   **`getCurrentRoom()`**: Get dungeon room info (room_id, active_rooms, visible_cards)
*   **`getCurrentMap()`**: Get overworld map info (map_id, world, world_name)
*   **`getSelection()`**: Get current selection in active editor

#### Read-only Data APIs (`window.yaze.data`)
*   **Dungeon Data:**
    *   `getRoomTiles(roomId)` - Get room tile data (layer1, layer2)
    *   `getRoomObjects(roomId)` - Get objects in a room
    *   `getRoomProperties(roomId)` - Get room properties (music, palette, tileset)
*   **Overworld Data:**
    *   `getMapTiles(mapId)` - Get map tile data
    *   `getMapEntities(mapId)` - Get entities (entrances, exits, items, sprites)
    *   `getMapProperties(mapId)` - Get map properties (gfx_group, palette, area_size)
*   **Palette Data:**
    *   `getPalette(group, id)` - Get palette colors
    *   `getPaletteGroups()` - List available palette groups

#### GUI Automation APIs (`window.yaze.gui`)
*   **Element Discovery:**
    *   `discover()` - List all interactive UI elements with metadata
    *   `getElementBounds(id)` - Get element position and dimensions (backed by `WidgetIdRegistry`)
    *   `waitForElement(id, timeout)` - Async wait for element to appear
*   **Interaction:**
    *   `click(target)` - Click by element ID or {x, y} coordinates
    *   `doubleClick(target)` - Double-click
    *   `drag(from, to, steps)` - Drag operation
    *   `pressKey(key, modifiers)` - Send keyboard input
    *   `type(text, delay)` - Type text string
    *   `scroll(dx, dy)` - Scroll canvas
*   **Utility:**
    *   `takeScreenshot(format)` - Capture canvas as base64
    *   `getCanvasInfo()` - Get canvas dimensions
    *   `isReady()` - Check if GUI API is ready

**Widget Tracking Infrastructure** (November 2025):
The `WidgetIdRegistry` system tracks all ImGui widget bounds in real-time:
-   **Real-time Bounds**: `GetUIElementTree()` and `GetUIElementBounds()` query live widget positions via `WidgetIdRegistry`
-   **Frame Lifecycle**: Integrated into `Controller::OnLoad()` with `BeginFrame()` and `EndFrame()` hooks
-   **Bounds Data**: Includes `min_x`, `min_y`, `max_x`, `max_y` for accurate GUI automation
-   **Metadata**: Returns `imgui_id`, `last_seen_frame`, widget type, visibility, enabled state
-   **Key Files**: `src/app/gui/automation/widget_id_registry.h`, `src/app/gui/automation/widget_measurement.h`

#### Control APIs (`window.yaze.control`)
*   **Editor Control:** `switchEditor()`, `getCurrentEditor()`, `getAvailableEditors()`
*   **Card Control:** `openCard()`, `closeCard()`, `toggleCard()`, `getVisibleCards()`
*   **Layout Control:** `setCardLayout()`, `getAvailableLayouts()`, `saveCurrentLayout()`
*   **Menu Actions:** `triggerMenuAction()`, `getAvailableMenuActions()`
*   **Session Control:** `getSessionInfo()`, `createSession()`, `switchSession()`
*   **ROM Control:** `getRomStatus()`, `readRomBytes()`, `writeRomBytes()`, `saveRom()`

#### Extended UI Control APIs (November 2025)

**Async Editor Switching (`yazeDebug.switchToEditorAsync`)**:
Promise-based editor switching with operation tracking for reliable LLM automation.
*   Returns `Promise<{success, editor, session_id, error}>` after editor transition completes
*   Supports all 14 editor types: Assembly, Dungeon, Graphics, Music, Overworld, Palette, Screen, Sprite, Message, Hex, Agent, Settings, World, Map
*   5-second timeout with proper error reporting

**Card Control API (`yazeDebug.cards`)**:
*   `show(cardId)` - Show a specific card by ID (e.g., "dungeon.room_selector")
*   `hide(cardId)` - Hide a specific card
*   `toggle(cardId)` - Toggle card visibility
*   `getState()` - Get visibility state of all cards
*   `getInCategory(category)` - List cards in a category (dungeon, overworld, etc.)
*   `showGroup(groupName)` - Show predefined card groups (dungeon_editing, overworld_editing, etc.)
*   `hideGroup(groupName)` - Hide predefined card groups
*   `getGroups()` - List available card groups

**Sidebar Control API (`yazeDebug.sidebar`)**:
*   `isTreeView()` - Check if tree view mode is active
*   `setTreeView(enabled)` - Switch between tree view (200px) and icon mode (48px)
*   `toggle()` - Toggle between view modes
*   `getState()` - Get sidebar state (mode, width, collapsed)

**Right Panel Control API (`yazeDebug.rightPanel`)**:
*   `open(panelName)` - Open specific panel: properties, agent, proposals, settings, help
*   `close()` - Close current panel
*   `toggle(panelName)` - Toggle panel visibility
*   `getState()` - Get panel state (active, expanded, width)
*   `openProperties()` - Convenience method for properties panel
*   `openAgent()` - Convenience method for agent chat panel

**Tree View Sidebar**:
New hierarchical sidebar mode (200px wide) with:
*   Category icons and expandable tree nodes
*   Checkboxes for each card with visibility toggles
*   Visible count badges per category
*   "Show All" / "Hide All" buttons per category
*   Toggle button to switch to icon mode

**Selection Properties Panel**:
New right-side panel for editing selected entities:
*   Context-aware property display based on selection type
*   Supports dungeon rooms, objects, sprites, entrances
*   Supports overworld maps, tiles, sprites, entrances, exits, items
*   Supports graphics sheets and palettes
*   Position/size editors with clamping
*   Byte/word property editors with hex display
*   Flag property editors with checkboxes
*   Advanced and raw data toggles

**Key Files:**
*   `src/app/platform/wasm/wasm_control_api.cc` - C++ implementation
*   `src/app/platform/wasm/wasm_control_api.h` - API declarations
*   `src/web/core/agent_automation.js` - GUI automation layer
*   `src/web/debug/yaze_debug_inspector.cc` - Extended WASM bindings
*   `src/app/editor/system/editor_card_registry.cc` - Tree view sidebar implementation
*   `src/app/editor/ui/right_panel_manager.cc` - Right panel management
*   `src/app/editor/ui/selection_properties_panel.cc` - Properties panel implementation

### Performance Optimizations & Bug Fixes (November 2025)

A comprehensive audit and fix of the WASM web layer was performed to address performance issues, memory leaks, and race conditions.

#### JavaScript Performance Fixes (`app.js`)
*   **Event Sanitization Optimization:**
    *   Removed redundant document-level event listeners (canvas-only now)
    *   Added WeakMap caching to avoid re-sanitizing the same event objects
    *   Optimized to check only relevant properties per event type category
    *   ~50% reduction in sanitization overhead
*   **Console Log Buffer:**
    *   Replaced O(n) `Array.shift()` with O(1) circular buffer implementation
    *   Uses modulo arithmetic for constant-time log rotation
*   **Polling Cleanup:**
    *   Added timeout tracking and max retry limits for module initialization
    *   Proper interval cleanup when components are destroyed
    *   Added `window.YAZE_MODULE_READY` flag for reliable initialization detection

#### Memory Leak Fixes
*   **Service Worker Cache (`service-worker.js`):**
    *   Added `MAX_RUNTIME_CACHE_SIZE` (50 entries) with LRU eviction
    *   New `trimRuntimeCache()` function enforces size limits
    *   `addToRuntimeCacheWithEviction()` wrapper for cache operations
*   **Confirmation Callbacks (`wasm_error_handler.cc`):**
    *   Added `CallbackEntry` struct with timestamps for timeout tracking
    *   Auto-cleanup of callbacks older than 5 minutes
    *   Page unload handler via `js_register_cleanup_handler()`
*   **Loading Indicators (`loading_indicator.js`):**
    *   Added try-catch error handling to ensure cleanup on errors
    *   Stale indicator cleanup (5-minute timeout)
    *   Periodic cleanup interval with proper lifecycle management

#### Race Condition Fixes
*   **Module Initialization (`app.js`):**
    *   Added `window.YAZE_MODULE_READY` flag set AFTER promise resolves
    *   Updated `waitForModule()` to check both Module existence AND ready flag
    *   Prevents code from seeing incomplete Module state
*   **FS Ready State (`filesystem_manager.js`):**
    *   Restructured `initPersistentFS()` with synchronous lock pattern
    *   Promise created immediately before async operations
    *   Eliminates race where two calls could create duplicate promises
*   **Redundant FS Exposure:**
    *   Added `fsExposed` flag to prevent wasteful redundant calls
    *   Reduced from 3 setTimeout calls to 1 conditional retry

#### C++ WASM Fixes
*   **Memory Safety (`wasm_storage.cc`):**
    *   Added `free(data_ptr)` in error paths of `LoadRom()` to prevent memory leaks
    *   Ensures allocated memory is freed even when operations fail
*   **Cleanup Handlers (`wasm_error_handler.cc`):**
    *   Added `cleanupConfirmCallbacks()` function for page unload
    *   Registered via `js_register_cleanup_handler()` in `Initialize()`

#### Drop Zone Optimization (`drop_zone.js`, `filesystem_manager.js`)
*   **Eliminated Double File Reading:**
    *   Added new `FilesystemManager.handleRomData(filename, data)` method
    *   Accepts pre-read `Uint8Array` instead of `File` object
    *   Drop zone now passes already-read data instead of re-reading
    *   Reduces CPU and memory usage for ROM uploads

**Key Files Modified:**
*   `src/web/app.js` - Event sanitization, console buffer, module init
*   `src/web/core/filesystem_manager.js` - FS init race fix, handleRomData
*   `src/web/core/loading_indicator.js` - Stale cleanup, error handling
*   `src/web/components/drop_zone.js` - Use handleRomData
*   `src/web/pwa/service-worker.js` - Cache eviction
*   `src/app/platform/wasm/wasm_storage.cc` - Memory free on error
*   `src/app/platform/wasm/wasm_error_handler.cc` - Callback cleanup

## 2. Technical Debt & Known Issues

*   **`SimpleChatSession`**: This C++ class relies on `VimMode` and raw TTY input, which is incompatible with WASM. We bypassed this by implementing a custom `HandleChatCommand` in `browser_agent.cc`. The original `SimpleChatSession` remains unused in the browser build.
*   **Emscripten Fetch Blocking**: The `EmscriptenHttpClient` implementation contains a `cv.wait()` which blocks the main thread. We worked around this by spawning `std::thread` in the command handlers, but the HTTP client itself remains synchronous-blocking if called directly on the main thread.
*   **Single-Threaded Rendering**: Dungeon graphics loading happens on the main thread (`DungeonEditorV2::DrawRoomTab`), causing UI freezes on large ROMs.

## 3. Next Steps / Roadmap

### Short Term
1.  **Palette Import**: Implement the logic in `wasm_drop_handler.cc` (or `main.cc` callback) to parse `.pal` files and apply them to `PaletteManager`.
2.  **Deep Linking**: Add logic to `app.js` and `main.cc` to parse URL query parameters (e.g., `?rom=url`) for easy sharing.

### Medium Term
1.  **In-Memory Proposal Registry**:
    *   Implement a `WasmProposalRegistry` that mimics the file-based `ProposalRegistry`.
    *   Store "sandboxes" as `Rom` copies in memory (or IndexedDB blobs).
    *   Enable `agent apply` to execute the plans generated by `agent plan`.
2.  **Multithreaded Graphics**:
    *   Refactor `DungeonEditorV2` to use `WasmWorkerPool` for `LoadRoomGraphics`.
    *   Requires decoupling `Room` data structures from the loading logic to pass data across threads safely.

## 4. Key Files

*   **C++ Logic**:
    *   `src/cli/handlers/agent/browser_agent.cc` (Agent commands)
    *   `src/cli/wasm_terminal_bridge.cc` (JS <-> C++ Bridge)
    *   `src/app/platform/wasm/wasm_drop_handler.cc` (File drag & drop)
    *   `src/app/platform/wasm/wasm_control_api.cc` (Control API implementation)
    *   `src/app/platform/wasm/wasm_control_api.h` (Control API declarations)
    *   `src/cli/service/agent/todo_manager.cc` (Persistence logic)

*   **Web Frontend**:
    *   `src/web/shell.html` (Entry point)
    *   `src/web/app.js` (Main UI logic)
    *   `src/web/core/agent_automation.js` (GUI Automation layer)
    *   `src/web/styles/main.css` (Theme definitions)
    *   `src/web/components/terminal.js` (Console UI component)
    *   `src/web/components/collaboration_ui.js` (Collaboration UI)
