#ifndef YAZE_APP_EDITOR_MENU_MENU_SHORTCUT_LABELS_H_
#define YAZE_APP_EDITOR_MENU_MENU_SHORTCUT_LABELS_H_

#include <string>

#include "absl/strings/string_view.h"

namespace yaze {
namespace editor {

class ShortcutManager;

// Returns the display label ("Ctrl+Shift+Z", "Cmd+O", ...) for the live
// binding registered under `action` in `shortcut_manager`, or an empty string
// when the manager is null, the action is unknown, or the action has no keys.
//
// Menus must use this instead of hard-coded key hints so a label can never
// drift from the binding that actually fires. This is the single place that
// reads ShortcutManager for menu labels. After claude/ui-shortcuts (ece5e6d9d)
// merges, switch the body to ShortcutManager::GetDisplayString(name), which
// has the same contract (platform-formatted, rebind-aware, "" when unknown or
// unbound). Menus must reference primary names ("Redo", "Window Browser"),
// not palette-only aliases ("Redo (Alt)", legacy "Panel ..." names).
std::string ShortcutLabelForAction(const ShortcutManager* shortcut_manager,
                                   absl::string_view action);

}  // namespace editor
}  // namespace yaze

#endif  // YAZE_APP_EDITOR_MENU_MENU_SHORTCUT_LABELS_H_
