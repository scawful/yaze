#ifndef YAZE_APP_EDITOR_OVERWORLD_TILE16_EDITOR_ACTION_STATE_H
#define YAZE_APP_EDITOR_OVERWORLD_TILE16_EDITOR_ACTION_STATE_H

namespace yaze {
namespace editor {

struct Tile16ActionControlState {
  bool can_write_pending = false;
  bool can_discard_all = false;
  bool can_discard_current = false;
  bool can_undo = false;
};

inline Tile16ActionControlState ComputeTile16ActionControlState(
    bool has_pending, bool current_tile_pending, bool can_undo) {
  Tile16ActionControlState state;
  state.can_write_pending = has_pending;
  state.can_discard_all = has_pending;
  state.can_discard_current = current_tile_pending;
  state.can_undo = can_undo;
  return state;
}

}  // namespace editor
}  // namespace yaze

#endif  // YAZE_APP_EDITOR_OVERWORLD_TILE16_EDITOR_ACTION_STATE_H
