#ifndef YAZE_APP_EDITOR_DUNGEON_DUNGEON_WORKBENCH_STATE_H
#define YAZE_APP_EDITOR_DUNGEON_DUNGEON_WORKBENCH_STATE_H

namespace yaze::editor {

// UI-only state for the Dungeon Workbench layout. This is intentionally kept
// separate from room data so we can reset it cleanly on ROM changes.
struct DungeonWorkbenchLayoutState {
  bool show_left_sidebar = true;
  bool show_right_inspector = true;

  // The browser includes a narrow navigation rail. Selection and the room
  // matrix share the right pane, independently of the active browser.
  float left_width = 304.0f;
  float right_width = 280.0f;

  // Split/compare quality-of-life.
  bool sync_split_view = false;

  // Read-only stitched room matrix view for browsing adjacent rooms.
  bool show_connected_canvas_view = false;

  // Session-only presentation options, adjustable without rebuilding the app.
  bool show_room_context_row = true;
  bool show_nearby_rooms = true;
  float nearby_rooms_height = 120.0f;
};

}  // namespace yaze::editor

#endif  // YAZE_APP_EDITOR_DUNGEON_DUNGEON_WORKBENCH_STATE_H
