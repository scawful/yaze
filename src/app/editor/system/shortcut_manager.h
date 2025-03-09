#ifndef YAZE_APP_EDITOR_SYSTEM_SHORTCUT_MANAGER_H
#define YAZE_APP_EDITOR_SYSTEM_SHORTCUT_MANAGER_H

#include <functional>
#include <string>
#include <unordered_map>

#include "imgui/imgui.h"

namespace yaze {
namespace editor {

struct Shortcut {
  std::string name;
  std::vector<ImGuiKey> keys;
  std::function<void()> callback;
};

std::vector<ImGuiKey> ParseShortcut(const std::string &shortcut);

std::string PrintShortcut(const std::vector<ImGuiKey> &keys);

class ShortcutManager {
 public:
  void RegisterShortcut(const std::string &name,
                        const std::vector<ImGuiKey> &keys) {
    shortcuts_[name] = {name, keys};
  }
  void RegisterShortcut(const std::string &name,
                        const std::vector<ImGuiKey> &keys,
                        std::function<void()> callback) {
    shortcuts_[name] = {name, keys, callback};
  }

  void RegisterShortcut(const std::string &name, ImGuiKey key,
                        std::function<void()> callback) {
    shortcuts_[name] = {name, {key}, callback};
  }

  void ExecuteShortcut(const std::string &name) const {
    shortcuts_.at(name).callback();
  }

  // Access the shortcut and print the readable name of the shortcut for menus
  const Shortcut &GetShortcut(const std::string &name) const {
    return shortcuts_.at(name);
  }

  // Get shortcut callback function
  std::function<void()> GetCallback(const std::string &name) const {
    return shortcuts_.at(name).callback;
  }

  const std::string GetKeys(const std::string &name) const {
    return PrintShortcut(shortcuts_.at(name).keys);
  }

  auto GetShortcuts() const { return shortcuts_; }

 private:
  std::unordered_map<std::string, Shortcut> shortcuts_;
};

void ExecuteShortcuts(const ShortcutManager &shortcut_manager);

}  // namespace editor
}  // namespace yaze

#endif  // YAZE_APP_EDITOR_SYSTEM_SHORTCUT_MANAGER_H