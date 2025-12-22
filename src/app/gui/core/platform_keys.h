#ifndef YAZE_APP_GUI_CORE_PLATFORM_KEYS_H
#define YAZE_APP_GUI_CORE_PLATFORM_KEYS_H

#include <string>
#include <vector>

#include "imgui/imgui.h"

namespace yaze {
namespace gui {

/**
 * @enum Platform
 * @brief Runtime platform detection for display string customization
 */
enum class Platform {
  kWindows,
  kMacOS,
  kLinux,
  kWebMac,    // WASM running on macOS browser
  kWebOther,  // WASM running on non-macOS browser
};

/**
 * @brief Get the current platform at runtime
 *
 * For native builds, this is determined at compile time.
 * For WASM builds, this queries navigator.platform on first call.
 */
Platform GetCurrentPlatform();

/**
 * @brief Check if running on macOS (native or web)
 */
bool IsMacPlatform();

/**
 * @brief Get the display name for the primary modifier key
 * @return "Cmd" on macOS platforms, "Ctrl" elsewhere
 */
const char* GetCtrlDisplayName();

/**
 * @brief Get the display name for the secondary modifier key
 * @return "Opt" on macOS platforms, "Alt" elsewhere
 */
const char* GetAltDisplayName();

/**
 * @brief Format a list of ImGui keys into a human-readable shortcut string
 *
 * Handles platform-specific modifier naming:
 * - {ImGuiMod_Ctrl, ImGuiKey_S} → "Cmd+S" on macOS, "Ctrl+S" elsewhere
 * - {ImGuiMod_Ctrl, ImGuiMod_Shift, ImGuiKey_P} → "Cmd+Shift+P" on macOS
 *
 * @param keys Vector of ImGuiKey values (modifiers and key)
 * @return Formatted shortcut string for display
 */
std::string FormatShortcut(const std::vector<ImGuiKey>& keys);

/**
 * @brief Convenience function for Ctrl+key shortcuts
 *
 * @param key The main key (e.g., ImGuiKey_S)
 * @return Formatted string like "Cmd+S" or "Ctrl+S"
 */
std::string FormatCtrlShortcut(ImGuiKey key);

/**
 * @brief Convenience function for Ctrl+Shift+key shortcuts
 *
 * @param key The main key (e.g., ImGuiKey_P)
 * @return Formatted string like "Cmd+Shift+P" or "Ctrl+Shift+P"
 */
std::string FormatCtrlShiftShortcut(ImGuiKey key);

/**
 * @brief Get the ImGui key name as a string
 *
 * @param key The ImGuiKey value
 * @return String representation (e.g., "S", "Space", "F1")
 */
const char* GetKeyName(ImGuiKey key);

}  // namespace gui
}  // namespace yaze

#endif  // YAZE_APP_GUI_CORE_PLATFORM_KEYS_H
