#include "app/platform/native_menu_bridge.h"

#include <utility>

#include "app/editor/system/commands/shortcut_manager.h"

namespace yaze {
namespace platform {
namespace native_menu {

namespace {

constexpr KeyChord Cmd(char key) {
  KeyChord c;
  c.key = key;
  c.command = true;
  return c;
}

constexpr KeyChord CmdShift(char key) {
  KeyChord c = Cmd(key);
  c.shift = true;
  return c;
}

constexpr KeyChord CmdOpt(char key) {
  KeyChord c = Cmd(key);
  c.option = true;
  return c;
}

constexpr KeyChord CmdCtrl(char key) {
  KeyChord c = Cmd(key);
  c.control = true;
  return c;
}

constexpr KeyChord kNoChord{};

char ChordKeyFromImGuiKey(ImGuiKey key) {
  if (key >= ImGuiKey_A && key <= ImGuiKey_Z) {
    return static_cast<char>('a' + (key - ImGuiKey_A));
  }
  if (key >= ImGuiKey_0 && key <= ImGuiKey_9) {
    return static_cast<char>('0' + (key - ImGuiKey_0));
  }
  switch (key) {
    case ImGuiKey_Comma:
      return ',';
    case ImGuiKey_Period:
      return '.';
    case ImGuiKey_Slash:
      return '/';
    case ImGuiKey_Semicolon:
      return ';';
    case ImGuiKey_Apostrophe:
      return '\'';
    case ImGuiKey_Minus:
      return '-';
    case ImGuiKey_Equal:
      return '=';
    case ImGuiKey_LeftBracket:
      return '[';
    case ImGuiKey_RightBracket:
      return ']';
    case ImGuiKey_Backslash:
      return '\\';
    case ImGuiKey_GraveAccent:
      return '`';
    default:
      return 0;
  }
}

bool HasCallback(const editor::Shortcut* shortcut) {
  return shortcut != nullptr && static_cast<bool>(shortcut->callback);
}

// A ShortcutManager binding with a different action already uses this exact
// chord; the native item must then not claim it.
bool ExactShortcutConflict(const editor::ShortcutManager* shortcuts,
                           const KeyChord& chord) {
  if (shortcuts == nullptr || !chord.valid()) {
    return false;
  }
  for (const auto& [name, shortcut] : shortcuts->GetShortcuts()) {
    if (!shortcut.callback || shortcut.keys.empty()) {
      continue;
    }
    if (ChordFromImGuiKeys(shortcut.keys) == chord) {
      return true;
    }
  }
  return false;
}

}  // namespace

const std::vector<ActionSpec>& ActionSpecs() {
  // clang-format off
  static const std::vector<ActionSpec> kSpecs = {
    // App menu
    {MenuAction::kAbout, "About yaze", "Show About", kNoChord,
     Dispatch::kShortcut, Dispatch::kHost, false},
    // ShortcutManager may bind Cmd+, to the Settings editor (claude/ui-shortcuts);
    // then ImGui owns the chord and this item only displays it.
    {MenuAction::kSettings, "Settings\xE2\x80\xA6", nullptr, Cmd(','),
     Dispatch::kHost, Dispatch::kHost, false, true},
    {MenuAction::kHide, "Hide yaze", nullptr, Cmd('h'),
     Dispatch::kCocoa, Dispatch::kCocoa, false},
    {MenuAction::kHideOthers, "Hide Others", nullptr, CmdOpt('h'),
     Dispatch::kCocoa, Dispatch::kCocoa, false},
    {MenuAction::kShowAll, "Show All", nullptr, kNoChord,
     Dispatch::kCocoa, Dispatch::kCocoa, false},
    {MenuAction::kQuit, "Quit yaze", "Quit", Cmd('q'),
     Dispatch::kShortcut, Dispatch::kHost, false},
    // File
    {MenuAction::kOpenRom, "Open ROM\xE2\x80\xA6", "Open", Cmd('o'),
     Dispatch::kShortcut, Dispatch::kHost, false},
    {MenuAction::kOpenProject, "Open Project\xE2\x80\xA6", nullptr, kNoChord,
     Dispatch::kHost, Dispatch::kHost, false},
    {MenuAction::kSave, "Save", "Save", Cmd('s'),
     Dispatch::kShortcut, Dispatch::kHost, false},
    {MenuAction::kSaveAs, "Save As\xE2\x80\xA6", "Save As", CmdShift('s'),
     Dispatch::kShortcut, Dispatch::kHost, false},
    {MenuAction::kCloseRom, "Close ROM", "Close ROM", Cmd('w'),
     Dispatch::kShortcut, Dispatch::kHost, false},
    // Edit
    {MenuAction::kUndo, "Undo", "Undo", Cmd('z'),
     Dispatch::kShortcut, Dispatch::kHost, false},
    {MenuAction::kRedo, "Redo", "Redo", CmdShift('z'),
     Dispatch::kShortcut, Dispatch::kHost, false},
    // ImGui text fields handle Cmd+X/C/V/A themselves, so these chords always
    // belong to ImGui even without a ShortcutManager entry.
    {MenuAction::kCut, "Cut", "Cut", Cmd('x'),
     Dispatch::kTextOrShortcut, Dispatch::kTextOrShortcut, true},
    {MenuAction::kCopy, "Copy", "Copy", Cmd('c'),
     Dispatch::kTextOrShortcut, Dispatch::kTextOrShortcut, true},
    {MenuAction::kPaste, "Paste", "Paste", Cmd('v'),
     Dispatch::kTextOrShortcut, Dispatch::kTextOrShortcut, true},
    {MenuAction::kSelectAll, "Select All", nullptr, Cmd('a'),
     Dispatch::kTextOrShortcut, Dispatch::kTextOrShortcut, true},
    // View
    {MenuAction::kToggleSidebar, "Toggle Sidebar", "view.toggle_activity_bar",
     kNoChord, Dispatch::kShortcut, Dispatch::kHost, false},
    {MenuAction::kToggleFullScreen, "Enter Full Screen", nullptr, CmdCtrl('f'),
     Dispatch::kCocoa, Dispatch::kCocoa, false},
    // Window
    {MenuAction::kMinimize, "Minimize", nullptr, Cmd('m'),
     Dispatch::kCocoa, Dispatch::kCocoa, false},
    {MenuAction::kZoom, "Zoom", nullptr, kNoChord,
     Dispatch::kCocoa, Dispatch::kCocoa, false},
    {MenuAction::kBringAllToFront, "Bring All to Front", nullptr, kNoChord,
     Dispatch::kCocoa, Dispatch::kCocoa, false},
    // Help
    {MenuAction::kKeyboardShortcuts, "Keyboard Shortcuts", "Keyboard Shortcuts",
     kNoChord, Dispatch::kShortcut, Dispatch::kHost, false},
  };
  // clang-format on
  return kSpecs;
}

const ActionSpec* FindSpec(MenuAction action) {
  for (const auto& spec : ActionSpecs()) {
    if (spec.action == action) {
      return &spec;
    }
  }
  return nullptr;
}

KeyChord ChordFromImGuiKeys(const std::vector<ImGuiKey>& keys) {
  KeyChord chord;
  char main_key = 0;
  int main_count = 0;
  for (ImGuiKey key : keys) {
    const int value = static_cast<int>(key);
    if (value == ImGuiMod_Ctrl) {
      // ShortcutManager's Ctrl is Command on macOS (ImGui swaps Cmd/Ctrl).
      chord.command = true;
    } else if (value == ImGuiMod_Super) {
      chord.control = true;
    } else if (value == ImGuiMod_Shift) {
      chord.shift = true;
    } else if (value == ImGuiMod_Alt) {
      chord.option = true;
    } else {
      ++main_count;
      main_key = ChordKeyFromImGuiKey(key);
    }
  }
  if (main_count != 1 || main_key == 0) {
    return KeyChord{};
  }
  // A menu key equivalent needs Command or Control; plain keys stay in ImGui.
  if (!chord.command && !chord.control) {
    return KeyChord{};
  }
  chord.key = main_key;
  return chord;
}

ImGuiKey ImGuiKeyFromChordKey(char key) {
  if (key >= 'a' && key <= 'z') {
    return static_cast<ImGuiKey>(ImGuiKey_A + (key - 'a'));
  }
  return ImGuiKey_None;
}

ResolvedItem ResolveItem(const editor::ShortcutManager* shortcuts,
                         MenuAction action) {
  ResolvedItem result;
  const ActionSpec* spec = FindSpec(action);
  if (spec == nullptr) {
    return result;
  }

  if (spec->shortcut_name != nullptr && shortcuts != nullptr) {
    const editor::Shortcut* shortcut =
        shortcuts->FindShortcut(spec->shortcut_name);
    if (HasCallback(shortcut)) {
      result.dispatch = spec->dispatch;
      result.chord = ChordFromImGuiKeys(shortcut->keys);
      result.owner =
          result.chord.valid() ? ChordOwner::kImGui : ChordOwner::kNone;
      return result;
    }
  }

  result.dispatch =
      spec->shortcut_name != nullptr ? spec->fallback : spec->dispatch;
  result.chord = spec->default_chord;
  if (!result.chord.valid()) {
    result.owner = ChordOwner::kNone;
  } else if (spec->imgui_owns_default_chord) {
    result.owner = ChordOwner::kImGui;
  } else if (ExactShortcutConflict(shortcuts, result.chord) &&
             spec->binding_on_default_chord_is_same_action) {
    // ShortcutManager runs the same action on this chord: display only.
    result.owner = ChordOwner::kImGui;
  } else if (ExactShortcutConflict(shortcuts, result.chord)) {
    // e.g. Cmd+H is the agent sidebar: Hide yaze keeps its item, loses ⌘H.
    result.chord = KeyChord{};
    result.owner = ChordOwner::kNone;
  } else {
    result.owner = ChordOwner::kNative;
  }
  return result;
}

bool IsNativeOwnedChord(const editor::ShortcutManager* shortcuts,
                        const KeyChord& chord) {
  if (!chord.valid()) {
    return false;
  }
  for (const auto& spec : ActionSpecs()) {
    if (spec.default_chord != chord) {
      // Fast path: native ownership only ever comes from a default chord.
      continue;
    }
    const ResolvedItem item = ResolveItem(shortcuts, spec.action);
    if (item.owner == ChordOwner::kNative && item.chord == chord) {
      return true;
    }
  }
  return false;
}

bool ShouldMenuPerformKeyEquivalent(const editor::ShortcutManager* shortcuts,
                                    const KeyChord& chord) {
  if (!chord.valid()) {
    // Function keys, arrows, etc. never map to a ShortcutManager chord here.
    return true;
  }
  if (IsNativeOwnedChord(shortcuts, chord)) {
    return true;
  }
  if (ExactShortcutConflict(shortcuts, chord)) {
    return false;
  }
  for (const auto& spec : ActionSpecs()) {
    const ResolvedItem item = ResolveItem(shortcuts, spec.action);
    if (item.owner == ChordOwner::kImGui && item.chord == chord) {
      return false;
    }
  }
  return true;
}

bool IsActionEnabled(NativeMenuHost& host, MenuAction action) {
  switch (action) {
    case MenuAction::kSave:
    case MenuAction::kSaveAs:
    case MenuAction::kCloseRom:
      return host.HasLoadedRom();
    case MenuAction::kUndo:
      return host.HasCurrentEditor() && host.CanUndo();
    case MenuAction::kRedo:
      return host.HasCurrentEditor() && host.CanRedo();
    case MenuAction::kCut:
    case MenuAction::kCopy:
    case MenuAction::kPaste:
      return host.WantsTextInput() || host.HasCurrentEditor();
    case MenuAction::kSelectAll:
      return host.WantsTextInput();
    default:
      return true;
  }
}

bool PerformAction(NativeMenuHost& host, MenuAction action) {
  const ActionSpec* spec = FindSpec(action);
  if (spec == nullptr) {
    return false;
  }
  editor::ShortcutManager* shortcuts = host.shortcut_manager();
  const ResolvedItem item = ResolveItem(shortcuts, action);

  auto run_shortcut = [&host, shortcuts](const char* name) {
    if (shortcuts == nullptr || name == nullptr) {
      return false;
    }
    if (!HasCallback(shortcuts->FindShortcut(name))) {
      return false;
    }
    std::string shortcut_name = name;
    host.Defer([shortcuts, shortcut_name]() {
      const editor::Shortcut* shortcut = shortcuts->FindShortcut(shortcut_name);
      if (shortcut != nullptr && shortcut->callback) {
        shortcut->callback();
      }
    });
    return true;
  };

  switch (item.dispatch) {
    case Dispatch::kCocoa:
      return false;
    case Dispatch::kShortcut:
      if (!run_shortcut(spec->shortcut_name)) {
        host.RunHostAction(action);
      }
      return true;
    case Dispatch::kTextOrShortcut:
      if (host.WantsTextInput()) {
        const ImGuiKey key = ImGuiKeyFromChordKey(spec->default_chord.key);
        if (key != ImGuiKey_None) {
          host.InjectCommandChord(key);
        }
        return true;
      }
      run_shortcut(spec->shortcut_name);
      return true;
    case Dispatch::kHost:
      host.RunHostAction(action);
      return true;
  }
  return false;
}

std::string RecentFileLabel(const std::string& path) {
  const size_t slash = path.find_last_of("/\\");
  if (slash == std::string::npos || slash + 1 >= path.size()) {
    return path;
  }
  return path.substr(slash + 1);
}

}  // namespace native_menu
}  // namespace platform
}  // namespace yaze
