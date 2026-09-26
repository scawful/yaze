#include "app/editor/menu/menu_shortcut_labels.h"

#include <string>

#include "app/editor/system/commands/shortcut_manager.h"

namespace yaze {
namespace editor {

std::string ShortcutLabelForAction(const ShortcutManager* shortcut_manager,
                                   absl::string_view action) {
  if (shortcut_manager == nullptr || action.empty()) {
    return "";
  }
  const Shortcut* shortcut =
      shortcut_manager->FindShortcut(std::string(action));
  if (shortcut == nullptr || shortcut->keys.empty()) {
    return "";
  }
  return PrintShortcut(shortcut->keys);
}

}  // namespace editor
}  // namespace yaze
