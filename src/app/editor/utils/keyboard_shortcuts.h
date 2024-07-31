#ifndef YAZE_APP_EDITOR_UTILS_KEYBOARD_SHORTCUTS_H
#define YAZE_APP_EDITOR_UTILS_KEYBOARD_SHORTCUTS_H

#include "imgui/imgui.h"

namespace yaze {
namespace app {
namespace editor {

struct KeyboardShortcuts {
  enum class ShortcutType {
    kCut,
    kCopy,
    kPaste,
    kUndo,
    kRedo,
    kFind,
  };
};

}  // namespace editor
}  // namespace app
}  // namespace yaze

#endif  // YAZE_APP_EDITOR_UTILS_KEYBOARD_SHORTCUTS_H_
