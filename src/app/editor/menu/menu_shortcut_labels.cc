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
  return shortcut_manager->GetDisplayString(std::string(action));
}

}  // namespace editor
}  // namespace yaze
