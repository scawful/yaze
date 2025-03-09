#include "shortcut_manager.h"

#include <algorithm>
#include <string>

#include "app/gui/input.h"
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

constexpr std::string kCtrlKey = "Ctrl";
constexpr std::string kAltKey = "Alt";
constexpr std::string kShiftKey = "Shift";
constexpr std::string kSuperKey = "Super";

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
    bool keys_pressed = true;
    // Check for all the keys in the shortcut
    for (const auto& key : shortcut.second.keys) {
      if (key == ImGuiMod_Ctrl) {
        keys_pressed &= ImGui::GetIO().KeyCtrl;
      } else if (key == ImGuiMod_Alt) {
        keys_pressed &= ImGui::GetIO().KeyAlt;
      } else if (key == ImGuiMod_Shift) {
        keys_pressed &= ImGui::GetIO().KeyShift;
      } else if (key == ImGuiMod_Super) {
        keys_pressed &= ImGui::GetIO().KeySuper;
      } else {
        keys_pressed &= ImGui::IsKeyDown(key);
      }
      if (!keys_pressed) {
        break;
      }
    }
    if (keys_pressed) {
      shortcut.second.callback();
    }
  }
}

}  // namespace editor
}  // namespace yaze