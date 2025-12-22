#ifndef YAZE_APP_EDITOR_SYSTEM_SHORTCUT_MANAGER_H
#define YAZE_APP_EDITOR_SYSTEM_SHORTCUT_MANAGER_H

#include <functional>
#include <string>
#include <unordered_map>
#include <vector>

// Must define before including imgui.h
#ifndef IMGUI_DEFINE_MATH_OPERATORS
#define IMGUI_DEFINE_MATH_OPERATORS
#endif

#include "imgui/imgui.h"

namespace yaze {
namespace editor {

struct Shortcut {
  enum class Scope {
    kGlobal,
    kEditor,
    kPanel
  };
  std::string name;
  Scope scope = Scope::kGlobal;
  std::vector<ImGuiKey> keys;
  std::function<void()> callback;
};

std::vector<ImGuiKey> ParseShortcut(const std::string& shortcut);

std::string PrintShortcut(const std::vector<ImGuiKey>& keys);

class ShortcutManager {
 public:
  void RegisterShortcut(const std::string& name,
                        const std::vector<ImGuiKey>& keys,
                        Shortcut::Scope scope = Shortcut::Scope::kGlobal) {
    shortcuts_[name] = {name, scope, keys};
  }
  void RegisterShortcut(const std::string& name,
                        const std::vector<ImGuiKey>& keys,
                        std::function<void()> callback,
                        Shortcut::Scope scope = Shortcut::Scope::kGlobal) {
    shortcuts_[name] = {name, scope, keys, callback};
  }

  void RegisterShortcut(const std::string& name, ImGuiKey key,
                        std::function<void()> callback,
                        Shortcut::Scope scope = Shortcut::Scope::kGlobal) {
    shortcuts_[name] = {name, scope, {key}, callback};
  }

  /**
   * @brief Register a command without keyboard shortcut (command palette only)
   *
   * These commands appear in the command palette but have no keyboard binding.
   * Useful for layout presets and other infrequently used commands.
   */
  void RegisterCommand(const std::string& name,
                       std::function<void()> callback,
                       Shortcut::Scope scope = Shortcut::Scope::kGlobal) {
    shortcuts_[name] = {name, scope, {}, callback};  // Empty key vector
  }

  void ExecuteShortcut(const std::string& name) const {
    shortcuts_.at(name).callback();
  }

  // Access the shortcut and print the readable name of the shortcut for menus
  const Shortcut& GetShortcut(const std::string& name) const {
    return shortcuts_.at(name);
  }

  // Get shortcut callback function
  std::function<void()> GetCallback(const std::string& name) const {
    return shortcuts_.at(name).callback;
  }

  const std::string GetKeys(const std::string& name) const {
    return PrintShortcut(shortcuts_.at(name).keys);
  }

  auto GetShortcuts() const { return shortcuts_; }
  bool UpdateShortcutKeys(const std::string& name,
                          const std::vector<ImGuiKey>& keys);
  std::vector<Shortcut> GetShortcutsByScope(Shortcut::Scope scope) const {
    std::vector<Shortcut> result;
    result.reserve(shortcuts_.size());
    for (const auto& [_, sc] : shortcuts_) {
      if (sc.scope == scope) result.push_back(sc);
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
  std::unordered_map<std::string, Shortcut> shortcuts_;
};

void ExecuteShortcuts(const ShortcutManager& shortcut_manager);

}  // namespace editor
}  // namespace yaze

#endif  // YAZE_APP_EDITOR_SYSTEM_SHORTCUT_MANAGER_H
