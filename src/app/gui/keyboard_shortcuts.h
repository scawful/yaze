// SPDX-License-Identifier: MIT
// Keyboard shortcut overlay system for yaze
// Provides a centralized UI for discovering and managing keyboard shortcuts

#ifndef YAZE_APP_GUI_KEYBOARD_SHORTCUTS_H
#define YAZE_APP_GUI_KEYBOARD_SHORTCUTS_H

#include <functional>
#include <map>
#include <string>
#include <vector>

#ifndef IMGUI_DEFINE_MATH_OPERATORS
#define IMGUI_DEFINE_MATH_OPERATORS
#endif

#include "imgui/imgui.h"

namespace yaze {
namespace gui {

/**
 * @enum ShortcutContext
 * @brief Defines the context in which a shortcut is active
 */
enum class ShortcutContext {
  kGlobal,     // Active everywhere
  kOverworld,  // Only active in Overworld Editor
  kDungeon,    // Only active in Dungeon Editor
  kGraphics,   // Only active in Graphics Editor
  kPalette,    // Only active in Palette Editor
  kSprite,     // Only active in Sprite Editor
  kMusic,      // Only active in Music Editor
  kMessage,    // Only active in Message Editor
  kEmulator,   // Only active in Emulator
  kCode,       // Only active in Code/Assembly Editor
};

/**
 * @struct Shortcut
 * @brief Represents a keyboard shortcut with its associated action
 */
struct Shortcut {
  std::string id;           // Unique identifier
  std::string description;  // Human-readable description
  ImGuiKey key;             // Primary key
  bool requires_ctrl = false;
  bool requires_shift = false;
  bool requires_alt = false;
  std::string category;                 // "File", "Edit", "View", "Navigation", etc.
  ShortcutContext context;              // When this shortcut is active
  std::function<void()> action;         // Action to execute
  bool enabled = true;                  // Whether the shortcut is currently enabled

  // Display the shortcut as a string (e.g., "Ctrl+S")
  std::string GetDisplayString() const;

  // Check if this shortcut matches the current key state
  bool Matches(ImGuiKey pressed_key, bool ctrl, bool shift, bool alt) const;
};

/**
 * @class KeyboardShortcuts
 * @brief Manages keyboard shortcuts and provides an overlay UI
 *
 * This class provides a centralized system for registering, displaying,
 * and processing keyboard shortcuts. It includes an overlay UI that can
 * be toggled with the '?' key to show all available shortcuts.
 *
 * Usage:
 *
 *   KeyboardShortcuts::Get().RegisterShortcut({
 *     .id = "file.save",
 *     .description = "Save Project",
 *     .key = ImGuiKey_S,
 *     .requires_ctrl = true,
 *     .category = "File",
 *     .context = ShortcutContext::kGlobal,
 *     .action = []() { SaveProject(); }
 *   });
 *
 *   KeyboardShortcuts::Get().ProcessInput();
 *   KeyboardShortcuts::Get().DrawOverlay();
 */
class KeyboardShortcuts {
 public:
  // Singleton access
  static KeyboardShortcuts& Get();

  // Register a new shortcut
  void RegisterShortcut(const Shortcut& shortcut);

  // Register a shortcut using parameters (convenience method)
  void RegisterShortcut(const std::string& id,
                        const std::string& description,
                        ImGuiKey key,
                        bool ctrl, bool shift, bool alt,
                        const std::string& category,
                        ShortcutContext context,
                        std::function<void()> action);

  // Unregister a shortcut by ID
  void UnregisterShortcut(const std::string& id);

  // Enable/disable a shortcut
  void SetShortcutEnabled(const std::string& id, bool enabled);

  // Process keyboard input and execute matching shortcuts
  void ProcessInput();

  // Draw the overlay UI (call every frame, handles visibility internally)
  void DrawOverlay();

  // Toggle overlay visibility
  void ToggleOverlay();

  // Show/hide overlay
  void ShowOverlay() { show_overlay_ = true; }
  void HideOverlay() { show_overlay_ = false; }
  bool IsOverlayVisible() const { return show_overlay_; }

  // Set the current editor context (called by editors when they become active)
  void SetCurrentContext(ShortcutContext context) { current_context_ = context; }
  ShortcutContext GetCurrentContext() const { return current_context_; }

  // Get all shortcuts in a category
  std::vector<const Shortcut*> GetShortcutsInCategory(const std::string& category) const;

  // Get all shortcuts for the current context
  std::vector<const Shortcut*> GetContextShortcuts() const;

  // Get all registered categories
  std::vector<std::string> GetCategories() const;

  // Register default application shortcuts
  void RegisterDefaultShortcuts(
      std::function<void()> open_callback,
      std::function<void()> save_callback,
      std::function<void()> save_as_callback,
      std::function<void()> close_callback,
      std::function<void()> undo_callback,
      std::function<void()> redo_callback,
      std::function<void()> copy_callback,
      std::function<void()> paste_callback,
      std::function<void()> cut_callback,
      std::function<void()> find_callback);

  // Check if any text input is active (to avoid triggering shortcuts while typing)
  static bool IsTextInputActive();

 private:
  KeyboardShortcuts() = default;
  ~KeyboardShortcuts() = default;
  KeyboardShortcuts(const KeyboardShortcuts&) = delete;
  KeyboardShortcuts& operator=(const KeyboardShortcuts&) = delete;

  // Draw the overlay content
  void DrawOverlayContent();

  // Draw a single shortcut row
  void DrawShortcutRow(const Shortcut& shortcut);

  // Draw category section
  void DrawCategorySection(const std::string& category,
                           const std::vector<const Shortcut*>& shortcuts);

  // Check if a shortcut should be active in the current context
  bool IsShortcutActiveInContext(const Shortcut& shortcut) const;

  // Registered shortcuts (id -> shortcut)
  std::map<std::string, Shortcut> shortcuts_;

  // Category order for display
  std::vector<std::string> category_order_ = {
    "File", "Edit", "View", "Navigation", "Tools", "Editor", "Other"
  };

  // UI State
  bool show_overlay_ = false;
  char search_filter_[256] = {};
  std::string expanded_category_;
  ShortcutContext current_context_ = ShortcutContext::kGlobal;

  // Prevent repeated triggers while key is held
  bool toggle_key_was_pressed_ = false;
};

/**
 * @brief Convert ShortcutContext to display string
 */
const char* ShortcutContextToString(ShortcutContext context);

/**
 * @brief Convert editor type name to ShortcutContext
 */
ShortcutContext EditorNameToContext(const std::string& editor_name);

}  // namespace gui
}  // namespace yaze

#endif  // YAZE_APP_GUI_KEYBOARD_SHORTCUTS_H
