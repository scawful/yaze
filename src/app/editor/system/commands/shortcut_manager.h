#ifndef YAZE_APP_EDITOR_SYSTEM_SHORTCUT_MANAGER_H
#define YAZE_APP_EDITOR_SYSTEM_SHORTCUT_MANAGER_H

#include <functional>
#include <optional>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include "absl/strings/string_view.h"

// Must define before including imgui.h
#ifndef IMGUI_DEFINE_MATH_OPERATORS
#define IMGUI_DEFINE_MATH_OPERATORS
#endif

#include "imgui/imgui.h"

namespace yaze {
namespace editor {

enum class EditorType;

struct Shortcut {
  enum class Scope { kGlobal, kEditor, kPanel };
  std::string name;
  Scope scope = Scope::kGlobal;
  std::vector<ImGuiKey> keys;
  std::function<void()> callback;
  // When set, the shortcut only competes for its chord while this editor is
  // the active editor (see ShortcutManager::SetActiveEditorProvider).
  std::optional<EditorType> editor_type;
  // Optional extra applicability check evaluated before arbitration.
  std::function<bool()> enabled;
  // Keys as first registered; ResetShortcutKeys() restores them.
  std::vector<ImGuiKey> default_keys;
};

std::vector<ImGuiKey> ParseShortcut(const std::string& shortcut);

std::string PrintShortcut(const std::vector<ImGuiKey>& keys);

/**
 * @brief Menu-IA group for a shortcut/command name (File, View, Drawers, …).
 *
 * Used by the shortcuts browser and any UI that wants the same buckets as the
 * menu bar without hard-coding every action.
 */
std::string InferShortcutGroup(absl::string_view name);

class ShortcutManager {
 public:
  using ActiveEditorProvider = std::function<std::optional<EditorType>()>;

  void RegisterShortcut(const std::string& name,
                        const std::vector<ImGuiKey>& keys,
                        Shortcut::Scope scope = Shortcut::Scope::kGlobal) {
    Store({name, scope, keys});
  }
  void RegisterShortcut(const std::string& name,
                        const std::vector<ImGuiKey>& keys,
                        std::function<void()> callback,
                        Shortcut::Scope scope = Shortcut::Scope::kGlobal) {
    Store({name, scope, keys, std::move(callback)});
  }

  void RegisterShortcut(const std::string& name, ImGuiKey key,
                        std::function<void()> callback,
                        Shortcut::Scope scope = Shortcut::Scope::kGlobal) {
    Store({name, scope, {key}, std::move(callback)});
  }

  /**
   * @brief Register an editor-owned shortcut (Scope::kEditor).
   *
   * The shortcut is ignored during arbitration unless @p editor_type is the
   * active editor, so another editor's binding for the same chord can win.
   */
  void RegisterEditorShortcut(const std::string& name,
                              const std::vector<ImGuiKey>& keys,
                              std::function<void()> callback,
                              EditorType editor_type) {
    Shortcut shortcut{name, Shortcut::Scope::kEditor, keys,
                      std::move(callback)};
    shortcut.editor_type = editor_type;
    Store(std::move(shortcut));
  }

  /**
   * @brief Register a command without keyboard shortcut (command palette only)
   *
   * These commands appear in the command palette but have no keyboard binding.
   * Useful for layout presets and other infrequently used commands.
   */
  void RegisterCommand(const std::string& name, std::function<void()> callback,
                       Shortcut::Scope scope = Shortcut::Scope::kGlobal) {
    Store({name, scope, {}, std::move(callback)});  // Empty key vector
  }

  /// Restrict an existing shortcut to one editor. Returns false if unknown.
  bool SetShortcutEditor(const std::string& name,
                         std::optional<EditorType> editor_type);
  /// Attach an applicability predicate. Returns false if unknown.
  bool SetShortcutEnabled(const std::string& name,
                          std::function<bool()> enabled);

  /// Supplies the active editor used to filter editor-owned shortcuts. When
  /// unset, editor-owned shortcuts are treated as applicable.
  void SetActiveEditorProvider(ActiveEditorProvider provider) {
    active_editor_provider_ = std::move(provider);
  }
  std::optional<EditorType> GetActiveEditorType() const {
    return active_editor_provider_ ? active_editor_provider_() : std::nullopt;
  }

  /// True when the shortcut may fire now (editor owner + enabled predicate).
  bool IsShortcutApplicable(const Shortcut& shortcut) const;

  void ExecuteShortcut(const std::string& name) const {
    shortcuts_.at(name).callback();
  }

  // Access the shortcut and print the readable name of the shortcut for menus
  const Shortcut& GetShortcut(const std::string& name) const {
    return shortcuts_.at(name);
  }

  const Shortcut* FindShortcut(const std::string& name) const {
    auto it = shortcuts_.find(name);
    return it != shortcuts_.end() ? &it->second : nullptr;
  }

  // Get shortcut callback function
  std::function<void()> GetCallback(const std::string& name) const {
    return shortcuts_.at(name).callback;
  }

  const std::string GetKeys(const std::string& name) const {
    return PrintShortcut(shortcuts_.at(name).keys);
  }

  /**
   * @brief Live, platform-formatted binding for a shortcut name.
   *
   * Returns e.g. "Cmd+Shift+Z" on macOS or "Ctrl+Shift+Z" elsewhere, reflecting
   * user rebinds. Returns an empty string for unknown or unbound names, so
   * menus and the command palette can omit the hint.
   */
  std::string GetDisplayString(const std::string& name) const;

  const std::unordered_map<std::string, Shortcut>& GetShortcuts() const {
    return shortcuts_;
  }
  bool UpdateShortcutKeys(const std::string& name,
                          const std::vector<ImGuiKey>& keys);
  /// Restore the keys the shortcut was registered with.
  bool ResetShortcutKeys(const std::string& name);

  /**
   * @brief Names of other shortcuts bound to the same chord that can be
   * active at the same time (same editor owner, or either one unowned).
   * Sorted by name. Keyless shortcuts never conflict.
   */
  std::vector<std::string> FindConflicts(const std::string& name) const;
  std::vector<std::string> FindConflicts(
      const std::vector<ImGuiKey>& keys, std::optional<EditorType> editor_type,
      const std::string& exclude_name = "") const;

  std::vector<Shortcut> GetShortcutsByScope(Shortcut::Scope scope) const {
    std::vector<Shortcut> result;
    result.reserve(shortcuts_.size());
    for (const auto& [_, sc] : shortcuts_) {
      if (sc.scope == scope)
        result.push_back(sc);
    }
    return result;
  }

  // Convenience methods for registering common shortcuts
  void RegisterStandardShortcuts(std::function<void()> save_callback,
                                 std::function<void()> open_callback,
                                 std::function<void()> close_callback,
                                 std::function<void()> find_callback,
                                 std::function<void()> settings_callback);

  void RegisterWindowNavigationShortcuts(std::function<void()> focus_left,
                                         std::function<void()> focus_right,
                                         std::function<void()> focus_up,
                                         std::function<void()> focus_down,
                                         std::function<void()> close_window,
                                         std::function<void()> split_horizontal,
                                         std::function<void()> split_vertical);

 private:
  void Store(Shortcut shortcut) {
    shortcut.default_keys = shortcut.keys;
    std::string name = shortcut.name;
    shortcuts_[name] = std::move(shortcut);
  }

  std::unordered_map<std::string, Shortcut> shortcuts_;
  ActiveEditorProvider active_editor_provider_;
};

/// True when two key lists describe the same chord (order-insensitive).
bool SameChord(const std::vector<ImGuiKey>& a, const std::vector<ImGuiKey>& b);

/**
 * @brief Fire at most one shortcut for this frame's key press.
 *
 * Arbitration:
 * 1. Drop shortcuts that are keyless, have no callback, or are not applicable
 *    (IsShortcutApplicable: wrong active editor or `enabled` false).
 * 2. While an ImGui text field wants input, drop plain-key shortcuts and
 *    text-editing chords (primary modifier + A/C/V/X/Y/Z, arrows, Home/End,
 *    Backspace/Delete) so the field handles them instead of the editor.
 * 3. Prefer Panel > Editor > Global scope, then editor-owned over unowned,
 *    then more keys, then more modifiers, then name (for determinism).
 */
void ExecuteShortcuts(const ShortcutManager& shortcut_manager);

}  // namespace editor
}  // namespace yaze

#endif  // YAZE_APP_EDITOR_SYSTEM_SHORTCUT_MANAGER_H
