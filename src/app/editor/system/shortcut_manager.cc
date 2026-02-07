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
struct ParsedChord {
  int required_mods = 0;               // ImGuiMod_* mask
  std::vector<ImGuiKey> main_keys;     // Non-modifier keys
};

ParsedChord DecomposeChord(const std::vector<ImGuiKey>& keys) {
  ParsedChord out;
  out.main_keys.reserve(keys.size());

  for (ImGuiKey key : keys) {
    const int key_value = static_cast<int>(key);
    if (key_value & ImGuiMod_Mask_) {
      out.required_mods |= key_value & ImGuiMod_Mask_;
      continue;
    }
    out.main_keys.push_back(key);
  }

  return out;
}

int CountMods(int mods) {
  int count = 0;
  if (mods & ImGuiMod_Ctrl) ++count;
  if (mods & ImGuiMod_Shift) ++count;
  if (mods & ImGuiMod_Alt) ++count;
  if (mods & ImGuiMod_Super) ++count;
  return count;
}

int ScopePriority(Shortcut::Scope scope) {
  // Higher wins.
  switch (scope) {
    case Shortcut::Scope::kGlobal:
      return 3;
    case Shortcut::Scope::kEditor:
      return 2;
    case Shortcut::Scope::kPanel:
      return 1;
  }
  return 0;
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
  // Check for keyboard shortcuts using the shortcut manager.
  //
  // Note: we intentionally do NOT gate on io.WantCaptureKeyboard here. In an
  // ImGui-first app it is frequently true (focused windows, menus, etc) and
  // would incorrectly disable shortcuts globally.
  const ImGuiIO& io = ImGui::GetIO();

  struct Candidate {
    const Shortcut* shortcut = nullptr;
    int scope_priority = 0;
    int key_count = 0;
    int mod_count = 0;
    std::string name;
  };

  auto better = [](const Candidate& a, const Candidate& b) -> bool {
    if (a.scope_priority != b.scope_priority)
      return a.scope_priority > b.scope_priority;
    if (a.key_count != b.key_count) return a.key_count > b.key_count;
    if (a.mod_count != b.mod_count) return a.mod_count > b.mod_count;
    return a.name < b.name;
  };

  Candidate best;
  bool have_best = false;

  for (const auto& [name, shortcut] : shortcut_manager.GetShortcuts()) {
    if (!shortcut.callback) {
      continue;
    }
    if (shortcut.keys.empty()) {
      continue;  // command palette only
    }

    const ParsedChord chord = DecomposeChord(shortcut.keys);
    if (chord.main_keys.empty()) {
      continue;
    }

    // When typing in an InputText, don't steal plain keys (Space, letters, etc).
    if (io.WantTextInput && chord.required_mods == 0) {
      continue;
    }

    // Modifier satisfaction (ImGui handles Cmd/Ctrl swap on macOS internally).
    if ((io.KeyMods & chord.required_mods) != chord.required_mods) {
      continue;
    }

    // Require all non-mod keys, with the last key triggering on press.
    bool chord_pressed = true;
    for (size_t i = 0; i + 1 < chord.main_keys.size(); ++i) {
      if (!ImGui::IsKeyDown(chord.main_keys[i])) {
        chord_pressed = false;
        break;
      }
    }
    if (!chord_pressed) {
      continue;
    }
    if (!ImGui::IsKeyPressed(chord.main_keys.back(), false /* repeat */)) {
      continue;
    }

    Candidate cand;
    cand.shortcut = &shortcut;
    cand.scope_priority = ScopePriority(shortcut.scope);
    cand.key_count = static_cast<int>(chord.main_keys.size());
    cand.mod_count = CountMods(chord.required_mods);
    cand.name = name;

    if (!have_best || better(cand, best)) {
      best = std::move(cand);
      have_best = true;
    }
  }

  if (have_best && best.shortcut && best.shortcut->callback) {
    best.shortcut->callback();
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
