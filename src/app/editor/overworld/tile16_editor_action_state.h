#ifndef YAZE_APP_EDITOR_OVERWORLD_TILE16_EDITOR_ACTION_STATE_H
#define YAZE_APP_EDITOR_OVERWORLD_TILE16_EDITOR_ACTION_STATE_H

#include <algorithm>

namespace yaze {
namespace editor {

struct Tile16ActionControlState {
  bool can_write_pending = false;
  bool can_discard_all = false;
  bool can_discard_current = false;
  bool can_undo = false;
  bool can_redo = false;
};

inline Tile16ActionControlState ComputeTile16ActionControlState(
    bool has_pending, bool current_tile_pending, bool can_undo, bool can_redo) {
  Tile16ActionControlState state;
  state.can_write_pending = has_pending;
  state.can_discard_all = has_pending;
  state.can_discard_current = current_tile_pending;
  state.can_undo = can_undo;
  state.can_redo = can_redo;
  return state;
}

inline int ComputeTile16CompactActionColumnCount(float available_width_px) {
  if (available_width_px >= 900.0f) {
    return 7;
  }
  if (available_width_px >= 620.0f) {
    return 5;
  }
  if (available_width_px >= 420.0f) {
    return 3;
  }
  if (available_width_px >= 260.0f) {
    return 2;
  }
  return 1;
}

inline int ComputeTile16ActionRowCount(int action_count, int column_count) {
  if (action_count <= 0) {
    return 0;
  }
  if (column_count <= 0) {
    column_count = 1;
  }
  return (action_count + column_count - 1) / column_count;
}

inline float ComputeTile16PaletteButtonSize(float available_width_px) {
  if (available_width_px <= 0.0f) {
    return 24.0f;
  }
  return std::clamp((available_width_px - 16.0f) / 4.0f, 24.0f, 32.0f);
}

}  // namespace editor
}  // namespace yaze

#endif  // YAZE_APP_EDITOR_OVERWORLD_TILE16_EDITOR_ACTION_STATE_H
