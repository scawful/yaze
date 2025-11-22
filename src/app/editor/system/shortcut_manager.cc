#include "shortcut_manager.h"

#include <algorithm>
#include <cstddef>
#include <functional>
#include <string>
#include <utility>
#include <vector>

#include "app/gui/core/input.h"
#include "imgui/imgui.h"

namespace yaze {
namespace editor {

namespace {
constexpr std::pair<ImGuiKey, const char*> kKeyNames[] = {
    {ImGuiKey_Tab, "Tab"},
    {ImGuiKey_LeftArrow, "Left"},
    {ImGuiKey_RightArrow, "Right"},
    {ImGuiKey_UpArrow, "Up"},
    {ImGuiKey_DownArrow, "Down"},
    {ImGuiKey_PageUp, "PageUp"},
    {ImGuiKey_PageDown, "PageDown"},
    {ImGuiKey_Home, "Home"},
    {ImGuiKey_End, "End"},
    {ImGuiKey_Insert, "Insert"},
    {ImGuiKey_Delete, "Delete"},
    {ImGuiKey_Backspace, "Backspace"},
    {ImGuiKey_Space, "Space"},
    {ImGuiKey_Enter, "Enter"},
    {ImGuiKey_Escape, "Escape"},
    {ImGuiMod_Ctrl, "Ctrl"},
    {ImGuiMod_Shift, "Shift"},
    {ImGuiMod_Alt, "Alt"},
    {ImGuiMod_Super, "Super"},
    {ImGuiKey_A, "A"},
    {ImGuiKey_B, "B"},
    {ImGuiKey_C, "C"},
    {ImGuiKey_D, "D"},
    {ImGuiKey_E, "E"},
    {ImGuiKey_F, "F"},
    {ImGuiKey_G, "G"},
    {ImGuiKey_H, "H"},
    {ImGuiKey_I, "I"},
    {ImGuiKey_J, "J"},
    {ImGuiKey_K, "K"},
    {ImGuiKey_L, "L"},
    {ImGuiKey_M, "M"},
    {ImGuiKey_N, "N"},
    {ImGuiKey_O, "O"},
    {ImGuiKey_P, "P"},
    {ImGuiKey_Q, "Q"},
    {ImGuiKey_R, "R"},
    {ImGuiKey_S, "S"},
    {ImGuiKey_T, "T"},
    {ImGuiKey_U, "U"},
    {ImGuiKey_V, "V"},
    {ImGuiKey_W, "W"},
    {ImGuiKey_X, "X"},
    {ImGuiKey_Y, "Y"},
    {ImGuiKey_Z, "Z"},
    {ImGuiKey_F1, "F1"},
    {ImGuiKey_F2, "F2"},
    {ImGuiKey_F3, "F3"},
    {ImGuiKey_F4, "F4"},
    {ImGuiKey_F5, "F5"},
    {ImGuiKey_F6, "F6"},
    {ImGuiKey_F7, "F7"},
    {ImGuiKey_F8, "F8"},
    {ImGuiKey_F9, "F9"},
    {ImGuiKey_F10, "F10"},
    {ImGuiKey_F11, "F11"},
    {ImGuiKey_F12, "F12"},
    {ImGuiKey_F13, "F13"},
    {ImGuiKey_F14, "F14"},
    {ImGuiKey_F15, "F15"},
};

constexpr const char* GetKeyName(ImGuiKey key) {
  for (const auto& pair : kKeyNames) {
    if (pair.first == key) {
      return pair.second;
    }
  }
  return "";
}
}  // namespace

std::string PrintShortcut(const std::vector<ImGuiKey>& keys) {
  std::string shortcut;
  for (size_t i = keys.size(); i > 0; --i) {
    shortcut += GetKeyName(keys[i - 1]);
    if (i > 1) {
      shortcut += "+";
    }
  }
  return shortcut;
}

const static std::string kCtrlKey = "Ctrl";
const static std::string kAltKey = "Alt";
const static std::string kShiftKey = "Shift";
const static std::string kSuperKey = "Super";

std::vector<ImGuiKey> ParseShortcut(const std::string& shortcut) {
  std::vector<ImGuiKey> shortcuts;
  // Search for special keys and the + symbol to combine with the
  // MapKeyToImGuiKey function
  size_t start = 0;
  size_t end = shortcut.find(kCtrlKey);
  if (end != std::string::npos) {
    shortcuts.push_back(ImGuiMod_Ctrl);
    start = end + kCtrlKey.size();
  }

  end = shortcut.find(kAltKey, start);
  if (end != std::string::npos) {
    shortcuts.push_back(ImGuiMod_Alt);
    start = end + kAltKey.size();
  }

  end = shortcut.find(kShiftKey, start);
  if (end != std::string::npos) {
    shortcuts.push_back(ImGuiMod_Shift);
    start = end + kShiftKey.size();
  }

  end = shortcut.find(kSuperKey, start);
  if (end != std::string::npos) {
    shortcuts.push_back(ImGuiMod_Super);
    start = end + kSuperKey.size();
  }

  // Parse the rest of the keys
  while (start < shortcut.size()) {
    shortcuts.push_back(gui::MapKeyToImGuiKey(shortcut[start]));
    start++;
  }

  return shortcuts;
}

void ExecuteShortcuts(const ShortcutManager& shortcut_manager) {
  // Check for keyboard shortcuts using the shortcut manager
  for (const auto& shortcut : shortcut_manager.GetShortcuts()) {
    bool modifiers_held = true;
    bool key_pressed = false;
    ImGuiKey main_key = ImGuiKey_None;

    // Separate modifiers from main key
    for (const auto& key : shortcut.second.keys) {
      if (key == ImGuiMod_Ctrl || key == ImGuiMod_Alt ||
          key == ImGuiMod_Shift || key == ImGuiMod_Super) {
        // Check if modifier is held
        if (key == ImGuiMod_Ctrl) {
          modifiers_held &= ImGui::GetIO().KeyCtrl;
        } else if (key == ImGuiMod_Alt) {
          modifiers_held &= ImGui::GetIO().KeyAlt;
        } else if (key == ImGuiMod_Shift) {
          modifiers_held &= ImGui::GetIO().KeyShift;
        } else if (key == ImGuiMod_Super) {
          modifiers_held &= ImGui::GetIO().KeySuper;
        }
      } else {
        // This is the main key - use IsKeyPressed for single trigger
        main_key = key;
      }
    }

    // Check if main key was pressed (not just held)
    if (main_key != ImGuiKey_None) {
      key_pressed = ImGui::IsKeyPressed(main_key, false);  // false = no repeat
    }

    // Execute if modifiers held and key pressed
    if (modifiers_held && key_pressed && shortcut.second.callback) {
      shortcut.second.callback();
    }
  }
}

}  // namespace editor
}  // namespace yaze

// Implementation in header file (inline methods)
namespace yaze {
namespace editor {

void ShortcutManager::RegisterStandardShortcuts(
    std::function<void()> save_callback, std::function<void()> open_callback,
    std::function<void()> close_callback, std::function<void()> find_callback,
    std::function<void()> settings_callback) {
  // Ctrl+S - Save
  if (save_callback) {
    RegisterShortcut("save", {ImGuiMod_Ctrl, ImGuiKey_S}, save_callback);
  }

  // Ctrl+O - Open
  if (open_callback) {
    RegisterShortcut("open", {ImGuiMod_Ctrl, ImGuiKey_O}, open_callback);
  }

  // Ctrl+W - Close
  if (close_callback) {
    RegisterShortcut("close", {ImGuiMod_Ctrl, ImGuiKey_W}, close_callback);
  }

  // Ctrl+F - Find
  if (find_callback) {
    RegisterShortcut("find", {ImGuiMod_Ctrl, ImGuiKey_F}, find_callback);
  }

  // Ctrl+, - Settings
  if (settings_callback) {
    RegisterShortcut("settings", {ImGuiMod_Ctrl, ImGuiKey_Comma},
                     settings_callback);
  }

  // Ctrl+Tab - Next tab (placeholder for now)
  // Ctrl+Shift+Tab - Previous tab (placeholder for now)
}

void ShortcutManager::RegisterWindowNavigationShortcuts(
    std::function<void()> focus_left, std::function<void()> focus_right,
    std::function<void()> focus_up, std::function<void()> focus_down,
    std::function<void()> close_window, std::function<void()> split_horizontal,
    std::function<void()> split_vertical) {
  // Ctrl+Arrow keys for window navigation
  if (focus_left) {
    RegisterShortcut("focus_left", {ImGuiMod_Ctrl, ImGuiKey_LeftArrow},
                     focus_left);
  }

  if (focus_right) {
    RegisterShortcut("focus_right", {ImGuiMod_Ctrl, ImGuiKey_RightArrow},
                     focus_right);
  }

  if (focus_up) {
    RegisterShortcut("focus_up", {ImGuiMod_Ctrl, ImGuiKey_UpArrow}, focus_up);
  }

  if (focus_down) {
    RegisterShortcut("focus_down", {ImGuiMod_Ctrl, ImGuiKey_DownArrow},
                     focus_down);
  }

  // Ctrl+W, C - Close current window
  if (close_window) {
    RegisterShortcut("close_window", {ImGuiMod_Ctrl, ImGuiKey_W, ImGuiKey_C},
                     close_window);
  }

  // Ctrl+W, S - Split horizontal
  if (split_horizontal) {
    RegisterShortcut("split_horizontal",
                     {ImGuiMod_Ctrl, ImGuiKey_W, ImGuiKey_S}, split_horizontal);
  }

  // Ctrl+W, V - Split vertical
  if (split_vertical) {
    RegisterShortcut("split_vertical", {ImGuiMod_Ctrl, ImGuiKey_W, ImGuiKey_V},
                     split_vertical);
  }
}

}  // namespace editor
}  // namespace yaze