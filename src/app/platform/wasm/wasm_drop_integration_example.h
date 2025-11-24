#ifndef YAZE_APP_PLATFORM_WASM_WASM_DROP_INTEGRATION_EXAMPLE_H_
#define YAZE_APP_PLATFORM_WASM_WASM_DROP_INTEGRATION_EXAMPLE_H_

/**
 * @file wasm_drop_integration_example.h
 * @brief Example integration of WasmDropHandler with EditorManager
 *
 * This file demonstrates how to integrate the drag & drop ROM loading
 * functionality into the main yaze application when building for WASM.
 *
 * INTEGRATION STEPS:
 *
 * 1. In your Controller or EditorManager initialization:
 * @code
 * #ifdef __EMSCRIPTEN__
 * #include "app/platform/wasm/wasm_drop_handler.h"
 *
 * absl::Status InitializeWasmFeatures() {
 *   // Initialize drop zone with callbacks
 *   auto& drop_handler = yaze::platform::WasmDropHandler::GetInstance();
 *
 *   return drop_handler.Initialize(
 *     "",  // Use document body as drop zone
 *     [this](const std::string& filename, const std::vector<uint8_t>& data) {
 *       // Handle dropped ROM file
 *       HandleDroppedRom(filename, data);
 *     },
 *     [](const std::string& error) {
 *       // Handle drop errors
 *       LOG_ERROR("Drop error: %s", error.c_str());
 *     }
 *   );
 * }
 * @endcode
 *
 * 2. Implement the ROM loading handler:
 * @code
 * void HandleDroppedRom(const std::string& filename,
 *                      const std::vector<uint8_t>& data) {
 *   // Create a new ROM instance
 *   auto rom = std::make_unique<Rom>();
 *
 *   // Load from data instead of file
 *   auto status = rom->LoadFromData(data);
 *   if (!status.ok()) {
 *     toast_manager_.Show("Failed to load ROM: " + status.ToString(),
 *                        ToastType::kError);
 *     return;
 *   }
 *
 *   // Set the filename for display
 *   rom->set_filename(filename);
 *
 *   // Find or create a session
 *   auto session_id = session_coordinator_->FindEmptySession();
 *   if (session_id == -1) {
 *     session_id = session_coordinator_->CreateNewSession();
 *   }
 *
 *   // Set the ROM in the session
 *   session_coordinator_->SetSessionRom(session_id, std::move(rom));
 *   session_coordinator_->SetCurrentSession(session_id);
 *
 *   // Load editor assets
 *   LoadAssets();
 *
 *   // Update UI
 *   ui_coordinator_->SetWelcomeScreenVisible(false);
 *   ui_coordinator_->SetEditorSelectionVisible(true);
 *
 *   toast_manager_.Show("ROM loaded via drag & drop: " + filename,
 *                      ToastType::kSuccess);
 * }
 * @endcode
 *
 * 3. Optional: Customize the drop zone appearance:
 * @code
 * drop_handler.SetOverlayText("Drop your A Link to the Past ROM here!");
 * @endcode
 *
 * 4. Optional: Enable/disable drop zone based on application state:
 * @code
 * // Disable during ROM operations
 * drop_handler.SetEnabled(false);
 * PerformRomOperation();
 * drop_handler.SetEnabled(true);
 * @endcode
 *
 * HTML INTEGRATION:
 *
 * Include the CSS in your HTML file:
 * @code{.html}
 * <link rel="stylesheet" href="drop_zone.css">
 * @endcode
 *
 * The JavaScript is automatically initialized when the Module is ready.
 * You can also manually initialize it:
 * @code{.html}
 * <script src="drop_zone.js"></script>
 * <script>
 *   // Optional: Custom initialization after Module is ready
 *   Module.onRuntimeInitialized = function() {
 *     YazeDropZone.init({
 *       config: {
 *         maxFileSize: 8 * 1024 * 1024,  // 8MB max
 *         messages: {
 *           dropHere: 'Drop SNES ROM here',
 *           loading: 'Loading ROM...'
 *         }
 *       }
 *     });
 *   };
 * </script>
 * @endcode
 *
 * TESTING:
 *
 * To test the drag & drop functionality:
 * 1. Build with Emscripten: cmake --preset wasm-dbg && cmake --build build_wasm
 * 2. Serve the files: python3 -m http.server 8000 -d build_wasm
 * 3. Open browser: http://localhost:8000/yaze.html
 * 4. Drag a .sfc/.smc ROM file onto the page
 * 5. The overlay should appear and the ROM should load
 *
 * TROUBLESHOOTING:
 *
 * - If overlay doesn't appear: Check browser console for errors
 * - If ROM doesn't load: Verify Rom::LoadFromData() implementation
 * - If styles are missing: Ensure drop_zone.css is included
 * - For debugging: Check Module._yazeHandleDroppedFile in console
 */

#endif  // YAZE_APP_PLATFORM_WASM_WASM_DROP_INTEGRATION_EXAMPLE_H_