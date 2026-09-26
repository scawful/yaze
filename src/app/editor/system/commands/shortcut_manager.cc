#include "shortcut_manager.h"

#include <algorithm>
#include <cctype>
#include <cstddef>
#include <functional>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "absl/strings/match.h"
#include "absl/strings/str_split.h"
#include "absl/strings/string_view.h"
#include "app/gui/core/input.h"
#include "app/gui/core/platform_keys.h"
#include "imgui/imgui.h"

namespace yaze {
namespace editor {

namespace {
struct ParsedChord {
  int required_mods = 0;            // ImGuiMod_* mask
  std::vector<ImGuiKey> main_keys;  // Non-modifier keys
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
  if (mods & ImGuiMod_Ctrl)
    ++count;
  if (mods & ImGuiMod_Shift)
    ++count;
  if (mods & ImGuiMod_Alt)
    ++count;
  if (mods & ImGuiMod_Super)
    ++count;
  return count;
}

int ScopePriority(Shortcut::Scope scope) {
  // Higher wins: the more specific scope takes the chord.
  switch (scope) {
    case Shortcut::Scope::kPanel:
      return 3;
    case Shortcut::Scope::kEditor:
      return 2;
    case Shortcut::Scope::kGlobal:
      return 1;
  }
  return 0;
}

// Chords that an ImGui text field consumes itself (clipboard, undo/redo,
// select-all, word navigation/deletion). Shortcuts bound to them must yield
// while io.WantTextInput is set, or the field and the editor both act.
bool IsTextEditingChord(const ParsedChord& chord) {
  if (chord.main_keys.size() != 1) {
    return false;
  }
  const int primary = chord.required_mods & (ImGuiMod_Ctrl | ImGuiMod_Super);
  if (primary == 0 || (chord.required_mods & ImGuiMod_Alt) != 0) {
    return false;
  }
  switch (chord.main_keys.front()) {
    case ImGuiKey_A:
    case ImGuiKey_C:
    case ImGuiKey_V:
    case ImGuiKey_X:
    case ImGuiKey_Y:
    case ImGuiKey_Z:
    case ImGuiKey_LeftArrow:
    case ImGuiKey_RightArrow:
    case ImGuiKey_Home:
    case ImGuiKey_End:
    case ImGuiKey_Backspace:
    case ImGuiKey_Delete:
      return true;
    default:
      return false;
  }
}

struct NormalizedChord {
  int mods = 0;
  std::vector<int> main_keys;
  bool operator==(const NormalizedChord& other) const {
    return mods == other.mods && main_keys == other.main_keys;
  }
};

NormalizedChord Normalize(const std::vector<ImGuiKey>& keys) {
  ParsedChord parsed = DecomposeChord(keys);
  NormalizedChord out;
  out.mods = parsed.required_mods;
  // ModsSatisfied() treats Ctrl and Super as the same requirement on macOS,
  // so they collide there.
  if (gui::IsMacPlatform() && (out.mods & (ImGuiMod_Ctrl | ImGuiMod_Super))) {
    out.mods = (out.mods & ~ImGuiMod_Super) | ImGuiMod_Ctrl;
  }
  for (ImGuiKey key : parsed.main_keys) {
    out.main_keys.push_back(static_cast<int>(key));
  }
  std::sort(out.main_keys.begin(), out.main_keys.end());
  return out;
}

// Reverse lookup for key names produced by gui::GetKeyName ("0", "=", "[",
// "Space", "PageDown", ...), case-insensitive, so PrintShortcut() output
// parses back to the same keys.
ImGuiKey LookupNamedKey(const std::string& lower) {
  static const std::pair<const char*, ImGuiKey> kAliases[] = {
      {"escape", ImGuiKey_Escape},
      {"return", ImGuiKey_Enter},
      {"del", ImGuiKey_Delete},
      {"keypadadd", ImGuiKey_KeypadAdd},
      {"keypadsubtract", ImGuiKey_KeypadSubtract},
      {"equal", ImGuiKey_Equal},
      {"minus", ImGuiKey_Minus},
      {"comma", ImGuiKey_Comma},
  };
  for (const auto& [alias, key] : kAliases) {
    if (lower == alias) {
      return key;
    }
  }
  for (int k = ImGuiKey_NamedKey_BEGIN; k < ImGuiKey_NamedKey_END; ++k) {
    const ImGuiKey key = static_cast<ImGuiKey>(k);
    std::string name = gui::GetKeyName(key);
    if (name == "?") {
      continue;
    }
    for (char& c : name) {
      c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    }
    if (name == lower) {
      return key;
    }
  }
  return ImGuiKey_None;
}

bool ModsSatisfied(int pressed_mods, int required_mods) {
  if (required_mods == 0) {
    // A plain-key command must not shadow a modified chord that is handled by
    // the active panel (for example D vs Ctrl/Cmd+D in the dungeon editor).
    constexpr int kRelevantMods =
        ImGuiMod_Ctrl | ImGuiMod_Shift | ImGuiMod_Alt | ImGuiMod_Super;
    return (pressed_mods & kRelevantMods) == 0;
  }

  auto has = [&](int mod) -> bool {
    return (pressed_mods & mod) != 0;
  };

  // macOS: ImGui may swap Cmd(Super) and Ctrl at the io.AddKeyEvent() layer
  // (ConfigMacOSXBehaviors). Treat Ctrl and Super as equivalent requirements so
  // shortcuts work regardless of that swap and for users who press either key.
  const bool mac = gui::IsMacPlatform();

  if (required_mods & ImGuiMod_Shift) {
    if (!has(ImGuiMod_Shift))
      return false;
  }
  if (required_mods & ImGuiMod_Alt) {
    if (!has(ImGuiMod_Alt))
      return false;
  }

  if (required_mods & ImGuiMod_Ctrl) {
    if (mac) {
      if (!(has(ImGuiMod_Ctrl) || has(ImGuiMod_Super)))
        return false;
    } else {
      if (!has(ImGuiMod_Ctrl))
        return false;
    }
  }
  if (required_mods & ImGuiMod_Super) {
    if (mac) {
      if (!(has(ImGuiMod_Super) || has(ImGuiMod_Ctrl)))
        return false;
    } else {
      if (!has(ImGuiMod_Super))
        return false;
    }
  }

  return true;
}
}  // namespace

std::string PrintShortcut(const std::vector<ImGuiKey>& keys) {
  // Use the platform-aware FormatShortcut from platform_keys.h
  // This handles Ctrl→Cmd and Alt→Opt conversions for macOS/WASM
  return gui::FormatShortcut(keys);
}

std::string InferShortcutGroup(absl::string_view name) {
  using absl::StartsWith;
  using absl::StrContains;

  if (StartsWith(name, "File")) {
    return "File";
  }
  if (StartsWith(name, "Edit")) {
    return "Edit";
  }
  if (StartsWith(name, "Help")) {
    return "Help";
  }
  if (StartsWith(name, "Tools") || name == "Command Palette" ||
      name == "Global Search" || name == "Load Last ROM" ||
      name == "Show About") {
    return "Tools";
  }
  if (StartsWith(name, "Layout") || StartsWith(name, "Apply Layout") ||
      StartsWith(name, "Apply Profile") || StartsWith(name, "Apply:") ||
      StartsWith(name, "layout:")) {
    return "Layout";
  }
  if (StartsWith(name, "drawer:") ||
      (StartsWith(name, "View: Toggle") &&
       (StrContains(name, "Drawer") || StrContains(name, "Panel") ||
        StrContains(name, "Notifications") || StrContains(name, "Agent") ||
        StrContains(name, "Proposals") || StrContains(name, "Settings") ||
        StrContains(name, "Help") || StrContains(name, "Project") ||
        StrContains(name, "Properties"))) ||
      StartsWith(name, "View: Next Right") ||
      StartsWith(name, "View: Previous Right")) {
    return "Drawers";
  }
  if (StartsWith(name, "window:") || StartsWith(name, "Window") ||
      StartsWith(name, "Panel Browser") || StartsWith(name, "Window Browser") ||
      StartsWith(name, "Window Finder") || StartsWith(name, "View: Show")) {
    return "Windows";
  }
  if (StartsWith(name, "View") || StartsWith(name, "Sidebar")) {
    return "View";
  }
  if (StrContains(name, ".")) {
    return "Editor";
  }
  return "Other";
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
    if (part.empty())
      continue;

    std::string lower;
    lower.reserve(part.size());
    for (char c : part)
      lower.push_back(static_cast<char>(std::tolower(c)));

    // Modifiers (support platform aliases)
    if (lower == "ctrl" || lower == "control") {
      keys.push_back(ImGuiMod_Ctrl);
      continue;
    }
    if (lower == "cmd" || lower == "command") {
      // ImGui's macOS behaviors swap Cmd/Super into ImGuiMod_Ctrl at the
      // time of io.AddKeyEvent(), so using ImGuiMod_Ctrl here makes "Cmd"
      // bindings work as expected and round-trip with PrintShortcut().
      keys.push_back(gui::IsMacPlatform() ? ImGuiMod_Ctrl : ImGuiMod_Super);
      continue;
    }
    if (lower == "win" || lower == "super") {
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

    // Digits, punctuation, lowercase letters and named keys (Space, Esc, ...)
    const ImGuiKey named = LookupNamedKey(lower);
    if (named != ImGuiKey_None) {
      keys.push_back(named);
      continue;
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
    bool editor_owned = false;
    int key_count = 0;
    int mod_count = 0;
    std::string name;
  };

  auto better = [](const Candidate& a, const Candidate& b) -> bool {
    if (a.scope_priority != b.scope_priority)
      return a.scope_priority > b.scope_priority;
    if (a.editor_owned != b.editor_owned)
      return a.editor_owned;
    if (a.key_count != b.key_count)
      return a.key_count > b.key_count;
    if (a.mod_count != b.mod_count)
      return a.mod_count > b.mod_count;
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

    // When typing in an InputText, don't steal plain keys (Space, letters,
    // etc) or the chords the text field implements itself (Cmd/Ctrl+Z/C/V...).
    if (io.WantTextInput &&
        (chord.required_mods == 0 || IsTextEditingChord(chord))) {
      continue;
    }

    // Modifier satisfaction (macOS Cmd/Ctrl handling is normalized by
    // ModsSatisfied()).
    if (!ModsSatisfied(io.KeyMods, chord.required_mods)) {
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

    // Only now evaluate applicability: the chord matched, so the predicate
    // cost is paid for at most a handful of shortcuts per key press.
    if (!shortcut_manager.IsShortcutApplicable(shortcut)) {
      continue;
    }

    Candidate cand;
    cand.shortcut = &shortcut;
    cand.scope_priority = ScopePriority(shortcut.scope);
    cand.editor_owned = shortcut.editor_type.has_value();
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

bool SameChord(const std::vector<ImGuiKey>& a, const std::vector<ImGuiKey>& b) {
  if (a.empty() || b.empty()) {
    return false;
  }
  return Normalize(a) == Normalize(b);
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

bool ShortcutManager::ResetShortcutKeys(const std::string& name) {
  auto it = shortcuts_.find(name);
  if (it == shortcuts_.end()) {
    return false;
  }
  it->second.keys = it->second.default_keys;
  return true;
}

bool ShortcutManager::SetShortcutEditor(const std::string& name,
                                        std::optional<EditorType> editor_type) {
  auto it = shortcuts_.find(name);
  if (it == shortcuts_.end()) {
    return false;
  }
  it->second.editor_type = editor_type;
  return true;
}

bool ShortcutManager::SetShortcutEnabled(const std::string& name,
                                         std::function<bool()> enabled) {
  auto it = shortcuts_.find(name);
  if (it == shortcuts_.end()) {
    return false;
  }
  it->second.enabled = std::move(enabled);
  return true;
}

bool ShortcutManager::IsShortcutApplicable(const Shortcut& shortcut) const {
  if (shortcut.editor_type.has_value() && active_editor_provider_) {
    const std::optional<EditorType> active = active_editor_provider_();
    if (!active.has_value() || *active != *shortcut.editor_type) {
      return false;
    }
  }
  if (shortcut.enabled && !shortcut.enabled()) {
    return false;
  }
  return true;
}

std::string ShortcutManager::GetDisplayString(const std::string& name) const {
  auto it = shortcuts_.find(name);
  if (it == shortcuts_.end() || it->second.keys.empty()) {
    return "";
  }
  return PrintShortcut(it->second.keys);
}

std::vector<std::string> ShortcutManager::FindConflicts(
    const std::string& name) const {
  auto it = shortcuts_.find(name);
  if (it == shortcuts_.end()) {
    return {};
  }
  return FindConflicts(it->second.keys, it->second.editor_type, name);
}

std::vector<std::string> ShortcutManager::FindConflicts(
    const std::vector<ImGuiKey>& keys, std::optional<EditorType> editor_type,
    const std::string& exclude_name) const {
  std::vector<std::string> conflicts;
  if (keys.empty()) {
    return conflicts;
  }
  for (const auto& [other_name, other] : shortcuts_) {
    if (other_name == exclude_name || other.keys.empty()) {
      continue;
    }
    // Shortcuts owned by two different editors are never live together.
    if (editor_type.has_value() && other.editor_type.has_value() &&
        *editor_type != *other.editor_type) {
      continue;
    }
    if (SameChord(keys, other.keys)) {
      conflicts.push_back(other_name);
    }
  }
  std::sort(conflicts.begin(), conflicts.end());
  return conflicts;
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
