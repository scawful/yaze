#ifndef YAZE_APP_PLATFORM_WASM_WASM_BOOTSTRAP_H_
#define YAZE_APP_PLATFORM_WASM_WASM_BOOTSTRAP_H_

#ifdef __EMSCRIPTEN__

#include <string>
#include <functional>

namespace yaze::app::wasm {

/**
 * @brief Initialize the WASM platform layer.
 *
 * Sets up filesystem mounts, drag-and-drop handlers, and JS configuration.
 * Call this early in main().
 */
void InitializeWasmPlatform();

/**
 * @brief Check if the asynchronous filesystem sync is complete.
 * @return true if filesystems are mounted and ready.
 */
bool IsFileSystemReady();

/**
 * @brief Register a callback for when an external source (JS, Drop) requests a ROM load.
 */
void SetRomLoadHandler(std::function<void(std::string)> handler);

/**
 * @brief Trigger a ROM load from C++ code (e.g. terminal bridge).
 */
void TriggerRomLoad(const std::string& path);

} // namespace yaze::app::wasm

#endif // __EMSCRIPTEN__

#endif // YAZE_APP_PLATFORM_WASM_WASM_BOOTSTRAP_H_
