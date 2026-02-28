#ifndef YAZE_APP_EDITOR_DUNGEON_WIDGETS_DUNGEON_STATUS_BAR_H
#define YAZE_APP_EDITOR_DUNGEON_WIDGETS_DUNGEON_STATUS_BAR_H

#include <string>

namespace yaze::editor {

class DungeonCanvasViewer;

// Data source for the status bar — caller populates fields each frame.
struct DungeonStatusBarState {
  // Current placement/edit mode label (e.g., "Object", "Sprite", "Select")
  const char* tool_mode = "Select";

  // Number of selected objects and which layer they're on
  int selection_count = 0;
  int selection_layer = -1;  // -1 = mixed/none

  // Zoom level as a percentage (100 = 1x)
  int zoom_percent = 100;

  // Whether the current room has unsaved changes
  bool room_dirty = false;

  // Cursor position in tile coordinates (-1 = not hovering)
  int cursor_tile_x = -1;
  int cursor_tile_y = -1;

  // Current room ID for display
  int room_id = -1;

  // Undo/Redo state
  bool can_undo = false;
  bool can_redo = false;
  const char* undo_desc = nullptr;  // Description of undo action
  const char* redo_desc = nullptr;  // Description of redo action
  int undo_depth = 0;               // Number of undo actions available

  // Callbacks for undo/redo buttons (set by the host panel)
  using UndoCallback = void (*)();
  UndoCallback on_undo = nullptr;
  UndoCallback on_redo = nullptr;
};

// Thin persistent bar drawn at the bottom of the dungeon editor canvas area.
// Displays tool mode, selection summary, zoom level, dirty indicator, and
// cursor coordinates at a glance.
class DungeonStatusBar {
 public:
  // Draws the status bar using the provided state. Should be called once per
  // frame, below the canvas area.
  static void Draw(const DungeonStatusBarState& state);

  // Helper: populate state from a DungeonCanvasViewer's current frame data.
  static DungeonStatusBarState BuildState(const DungeonCanvasViewer& viewer,
                                          const char* tool_mode,
                                          bool room_dirty);
};

}  // namespace yaze::editor

#endif  // YAZE_APP_EDITOR_DUNGEON_WIDGETS_DUNGEON_STATUS_BAR_H
