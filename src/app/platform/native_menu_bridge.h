#ifndef YAZE_APP_PLATFORM_NATIVE_MENU_BRIDGE_H_
#define YAZE_APP_PLATFORM_NATIVE_MENU_BRIDGE_H_

// Platform-neutral model behind the macOS native main menu.
//
// The Cocoa side (app_delegate.mm) only builds NSMenu objects and forwards
// clicks, key equivalents, and validation to this layer. Keeping the rules in
// plain C++ lets unit tests prove the chord-ownership contract without AppKit.
//
// Chord ownership contract (one owner per chord, never both):
//   * kImGui:  the item mirrors a ShortcutManager binding. The native menu
//              shows the chord for discoverability but must NOT perform it as
//              a key equivalent; SDL delivers the key to ImGui and
//              ExecuteShortcuts() runs the action once. A click on the item
//              runs the same ShortcutManager callback.
//   * kNative: nothing in ShortcutManager binds the chord exactly. NSMenu
//              performs it, and the SDL event filter drops the key so the
//              chord never also reaches ImGui (ShortcutManager matches
//              modifiers loosely, e.g. Ctrl+Cmd+F would otherwise fire Find).
//   * kNone:   the item has no usable chord (unrepresentable key or an exact
//              ShortcutManager conflict with a different action).

#include <cstdint>
#include <functional>
#include <string>
#include <vector>

#ifndef IMGUI_DEFINE_MATH_OPERATORS
#define IMGUI_DEFINE_MATH_OPERATORS
#endif
#include "imgui/imgui.h"

namespace yaze {
namespace editor {
class ShortcutManager;
}  // namespace editor

namespace platform {
namespace native_menu {

enum class MenuAction : int {
  // App menu
  kAbout = 0,
  kSettings,
  kHide,
  kHideOthers,
  kShowAll,
  kQuit,
  // File
  kOpenRom,
  kOpenProject,
  kSave,
  kSaveAs,
  kCloseRom,
  // Edit
  kUndo,
  kRedo,
  kCut,
  kCopy,
  kPaste,
  kSelectAll,
  // View
  kToggleSidebar,
  kToggleFullScreen,
  // Window
  kMinimize,
  kZoom,
  kBringAllToFront,
  // Help
  kKeyboardShortcuts,
  kCount,
};

// A macOS key chord. `key` is the unshifted ASCII character ('s', ',', '/'),
// or 0 when there is no chord.
struct KeyChord {
  char key = 0;
  bool command = false;
  bool shift = false;
  bool option = false;
  bool control = false;

  bool valid() const { return key != 0; }
  bool operator==(const KeyChord& other) const {
    return key == other.key && command == other.command &&
           shift == other.shift && option == other.option &&
           control == other.control;
  }
  bool operator!=(const KeyChord& other) const { return !(*this == other); }
};

enum class ChordOwner { kNone, kImGui, kNative };

// How a menu action performs its work.
enum class Dispatch {
  kShortcut,        // ShortcutManager callback named by `shortcut_name`.
  kHost,            // NativeMenuHost::RunHostAction (EditorManager call).
  kTextOrShortcut,  // Text-field chord when ImGui wants text, else shortcut.
  kCocoa,           // Standard AppKit selector (hide:, performMiniaturize:).
};

struct ActionSpec {
  MenuAction action;
  const char* title;
  // ShortcutManager entry that owns this action's chord and behavior. When the
  // entry is missing (e.g. agent UI compiled out), the item falls back to
  // `fallback`.
  const char* shortcut_name;
  KeyChord default_chord;
  Dispatch dispatch;
  Dispatch fallback;
  // ImGui itself handles `default_chord` (text-field Cmd+X/C/V/A), so the
  // chord stays with ImGui even without a ShortcutManager entry.
  bool imgui_owns_default_chord;
  // If a ShortcutManager binding already uses `default_chord` exactly, it
  // does the same thing as this item (e.g. Cmd+, = Settings editor), so the
  // item shows the chord as ImGui-owned instead of dropping it. Items without
  // this flag lose the chord on a conflict (Cmd+H is the agent sidebar, not
  // Hide yaze).
  bool binding_on_default_chord_is_same_action = false;
};

const std::vector<ActionSpec>& ActionSpecs();
const ActionSpec* FindSpec(MenuAction action);

// Convert a ShortcutManager key list to a macOS chord. ShortcutManager's
// ImGuiMod_Ctrl means Command on macOS (ImGui swaps Cmd/Ctrl). Returns an
// invalid chord for keys a menu key equivalent cannot express (F1, arrows).
KeyChord ChordFromImGuiKeys(const std::vector<ImGuiKey>& keys);

// Map the ASCII key of a chord to an ImGui key (for synthesizing text-field
// chords). Returns ImGuiKey_None if unsupported.
ImGuiKey ImGuiKeyFromChordKey(char key);

struct ResolvedItem {
  KeyChord chord;
  ChordOwner owner = ChordOwner::kNone;
  // Effective dispatch after considering whether `shortcut_name` exists.
  Dispatch dispatch = Dispatch::kHost;
};

// Resolve one item against the live ShortcutManager (may be null).
ResolvedItem ResolveItem(const editor::ShortcutManager* shortcuts,
                         MenuAction action);

// True when the NSMenu should perform `chord` as a key equivalent. Everything
// else must fall through to SDL/ImGui.
bool IsNativeOwnedChord(const editor::ShortcutManager* shortcuts,
                        const KeyChord& chord);

// Gate for the root NSMenu's performKeyEquivalent:. False for every chord
// ImGui owns (a ShortcutManager binding or an ImGui-owned item); true for
// native-owned chords and for chords of items AppKit inserts itself
// (Emoji & Symbols, Dictation), which ImGui never sees as shortcuts.
bool ShouldMenuPerformKeyEquivalent(const editor::ShortcutManager* shortcuts,
                                    const KeyChord& chord);

// Editor state and side effects the menu needs. Implemented over
// EditorManager in app_delegate.mm and by a fake in unit tests.
class NativeMenuHost {
 public:
  virtual ~NativeMenuHost() = default;
  virtual editor::ShortcutManager* shortcut_manager() = 0;
  virtual bool HasLoadedRom() const = 0;
  virtual bool HasCurrentEditor() const = 0;
  virtual bool CanUndo() const = 0;
  virtual bool CanRedo() const = 0;
  virtual bool WantsTextInput() const = 0;
  // Run `action` at the start of the next ImGui frame (EditorManager's
  // deferred-action queue), where ShortcutManager callbacks normally run.
  virtual void Defer(std::function<void()> action) = 0;
  // Queue a Command+key press into ImGui's input queue (text fields).
  virtual void InjectCommandChord(ImGuiKey key) = 0;
  virtual void RunHostAction(MenuAction action) = 0;
  virtual std::vector<std::string> RecentFiles() const = 0;
  virtual void OpenRecentFile(const std::string& path) = 0;
  virtual void ClearRecentFiles() = 0;
};

bool IsActionEnabled(NativeMenuHost& host, MenuAction action);

// Perform a menu click. Returns false when the action is a Cocoa selector the
// caller must route through the responder chain itself.
bool PerformAction(NativeMenuHost& host, MenuAction action);

// Short label for a recent-file entry (file name, falls back to full path).
std::string RecentFileLabel(const std::string& path);

}  // namespace native_menu
}  // namespace platform
}  // namespace yaze

#endif  // YAZE_APP_PLATFORM_NATIVE_MENU_BRIDGE_H_
