#ifndef YAZE_APP_EDITOR_OVERWORLD_TILE16_EDITOR_SHORTCUTS_H
#define YAZE_APP_EDITOR_OVERWORLD_TILE16_EDITOR_SHORTCUTS_H

#include <cstdint>
#include <optional>

namespace yaze {
namespace editor {

struct Tile16NumericShortcutResult {
  std::optional<int> quadrant_focus;
  std::optional<uint8_t> palette_id;
};

inline Tile16NumericShortcutResult ResolveTile16NumericShortcut(
    bool ctrl_held, int number_index) {
  Tile16NumericShortcutResult result;
  if (number_index < 0 || number_index > 7) {
    return result;
  }

  if (ctrl_held) {
    result.palette_id = static_cast<uint8_t>(number_index);
    return result;
  }

  if (number_index <= 3) {
    result.quadrant_focus = number_index;
  }
  return result;
}

}  // namespace editor
}  // namespace yaze

#endif  // YAZE_APP_EDITOR_OVERWORLD_TILE16_EDITOR_SHORTCUTS_H
