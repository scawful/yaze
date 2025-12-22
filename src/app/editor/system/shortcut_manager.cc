#include "shortcut_manager.h"

#include <algorithm>
#include <cstddef>
#include <functional>
#include <string>
#include <utility>
#include <vector>

#include "absl/strings/str_split.h"
#include "app/gui/core/input.h"
#include "app/gui/core/platform_keys.h"
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

struct ModifierState {
  int mods = 0;
  bool shortcut = false;  // Primary modifier (Cmd on macOS, Ctrl elsewhere)
  bool alt = false;
  bool shift = false;
  bool super = false;     // Physical Super/Cmd key
};

ModifierState BuildModifierState(const ImGuiIO& io) {
  ModifierState state;
  state.mods = io.KeyMods;
  state.super =
      io.KeySuper || ((state.mods & ImGuiMod_Super) == ImGuiMod_Super);
  state.alt = io.KeyAlt || ((state.mods & ImGuiMod_Alt) == ImGuiMod_Alt);
  state.shift =
      io.KeyShift || ((state.mods & ImGuiMod_Shift) == ImGuiMod_Shift);
  state.shortcut =
      io.KeyCtrl || io.KeySuper ||
      ((io.KeyMods & ImGuiMod_Shortcut) == ImGuiMod_Shortcut);
  return state;
}

bool ModifiersSatisfied(int required_mods, const ModifierState& state) {
  // Primary modifier: allow either Ctrl or Cmd/Super to satisfy "Ctrl"
  if ((required_mods & ImGuiMod_Ctrl) && !state.shortcut) {
    return false;
  }
  if ((required_mods & ImGuiMod_Alt) && !state.alt) {
    return false;
  }
  if ((required_mods & ImGuiMod_Shift) && !state.shift) {
    return false;
  }
  if ((required_mods & ImGuiMod_Super) && !state.super) {
    return false;
  }
  return true;
}
}  // namespace

std::string PrintShortcut(const std::vector<ImGuiKey>& keys) {
  // Use the platform-aware FormatShortcut from platform_keys.h
  // This handles Ctrl→Cmd and Alt→Opt conversions for macOS/WASM
  return gui::FormatShortcut(keys);
}

std::vector<ImGuiKey> ParseShortcut(const std::string& shortcut) {
  std::vector<ImGuiKey> keys;
  if (shortcut.empty()) {
    return keys;
  }

  // Split on '+' and trim whitespace
  std::vector<std::string> parts = absl::StrSplit(shortcut, '+');
  for (auto& part : parts) {
    // Trim leading/trailing spaces
    while (!part.empty() && (part.front() == ' ' || part.front() == '\t')) {
      part.erase(part.begin());
    }
    while (!part.empty() && (part.back() == ' ' || part.back() == '\t')) {
      part.pop_back();
    }
    if (part.empty()) continue;

    std::string lower;
    lower.reserve(part.size());
    for (char c : part) lower.push_back(static_cast<char>(std::tolower(c)));

    // Modifiers (support platform aliases)
    if (lower == "ctrl" || lower == "control") {
      keys.push_back(ImGuiMod_Ctrl);
      continue;
    }
    if (lower == "cmd" || lower == "command" || lower == "win" ||
        lower == "super") {
      keys.push_back(ImGuiMod_Super);
      continue;
    }
    if (lower == "alt" || lower == "opt" || lower == "option") {
      keys.push_back(ImGuiMod_Alt);
      continue;
    }
    if (lower == "shift") {
      keys.push_back(ImGuiMod_Shift);
      continue;
    }

    // Function keys
    if (lower.size() >= 2 && lower[0] == 'f') {
      int fnum = 0;
      try {
        fnum = std::stoi(lower.substr(1));
      } catch (...) {
        fnum = 0;
      }
      if (fnum >= 1 && fnum <= 24) {
        keys.push_back(static_cast<ImGuiKey>(ImGuiKey_F1 + (fnum - 1)));
        continue;
      }
    }

    // Single character keys
    if (part.size() == 1) {
      ImGuiKey mapped = gui::MapKeyToImGuiKey(part[0]);
      if (mapped != ImGuiKey_COUNT) {
        keys.push_back(mapped);
        continue;
      }
    }
  }

  return keys;
}

void ExecuteShortcuts(const ShortcutManager& shortcut_manager) {
  // Check for keyboard shortcuts using the shortcut manager. Modifier handling
  // is normalized so Cmd (macOS) and Ctrl (other platforms) map to the same
  // registered shortcuts.
  const ImGuiIO& io = ImGui::GetIO();

  // Skip shortcut processing when ImGui wants keyboard input (typing in text fields)
  if (io.WantCaptureKeyboard || io.WantTextInput) {
    return;
  }

  const ModifierState mod_state = BuildModifierState(io);

  for (const auto& shortcut : shortcut_manager.GetShortcuts()) {
    int required_mods = 0;
    std::vector<ImGuiKey> main_keys;

    // Decompose the shortcut into modifier mask + main keys
    for (const auto& key : shortcut.second.keys) {
      // Handle combined modifier entries (e.g., ImGuiMod_Ctrl | ImGuiMod_Shift)
      int key_value = static_cast<int>(key);
      if (key_value & ImGuiMod_Mask_) {
        required_mods |= key_value & ImGuiMod_Mask_;
        continue;
      }
      // Treat ImGuiMod_Shortcut (alias of Ctrl) the same way
      if (key == ImGuiMod_Shortcut || key == ImGuiMod_Ctrl ||
          key == ImGuiMod_Alt || key == ImGuiMod_Shift ||
          key == ImGuiMod_Super) {
        required_mods |= key_value;
        continue;
      }

      main_keys.push_back(key);
    }

    // Fast path: single-key chords leverage ImGui's chord helper, which
    // already accounts for macOS Cmd/Ctrl translation.
    if (main_keys.size() == 1) {
      ImGuiKeyChord chord = static_cast<ImGuiKeyChord>(required_mods) | main_keys.back();
      if (ImGui::IsKeyChordPressed(chord) && shortcut.second.callback) {
        shortcut.second.callback();
      }
      continue;
    }

    // Require modifiers first for multi-key chords (e.g., Ctrl+W then C)
    if (!ModifiersSatisfied(required_mods, mod_state)) {
      continue;
    }

    // Require all non-mod keys, with the last key triggering on press
    bool chord_pressed = !main_keys.empty();
    for (size_t i = 0; i + 1 < main_keys.size(); ++i) {
      if (!ImGui::IsKeyDown(main_keys[i])) {
        chord_pressed = false;
        break;
      }
    }

    if (chord_pressed && !main_keys.empty()) {
      chord_pressed =
          ImGui::IsKeyPressed(main_keys.back(), false /* repeat */);
    }

    if (chord_pressed && shortcut.second.callback) {
      shortcut.second.callback();
    }
  }
}

bool ShortcutManager::UpdateShortcutKeys(const std::string& name,
                                         const std::vector<ImGuiKey>& keys) {
  auto it = shortcuts_.find(name);
  if (it == shortcuts_.end()) {
    return false;
  }
  it->second.keys = keys;
  return true;
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
