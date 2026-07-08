#ifndef YAZE_APP_EDITOR_DUNGEON_DUNGEON_WORKBENCH_STATE_H
#define YAZE_APP_EDITOR_DUNGEON_DUNGEON_WORKBENCH_STATE_H

namespace yaze::editor {

// UI-only state for the Dungeon Workbench layout. This is intentionally kept
// separate from room data so we can reset it cleanly on ROM changes.
struct DungeonWorkbenchLayoutState {
  bool show_left_sidebar = true;
  bool show_right_inspector = true;

  // Remembered widths for the collapsible panes when expanded. The right
  // inspector defaults narrower than the left room browser because the
  // inspector content (selection details + bulk actions) fits comfortably at
  // ~280 px, whereas the room matrix on the left wants a little more room.
  float left_width = 280.0f;
  float right_width = 280.0f;

  // Split/compare quality-of-life.
  bool sync_split_view = false;

  // Read-only stitched room matrix view for browsing adjacent rooms.
  bool show_connected_canvas_view = false;
};

}  // namespace yaze::editor

#endif  // YAZE_APP_EDITOR_DUNGEON_DUNGEON_WORKBENCH_STATE_H
