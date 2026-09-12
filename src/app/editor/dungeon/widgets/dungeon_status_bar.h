#ifndef YAZE_APP_EDITOR_DUNGEON_WIDGETS_DUNGEON_STATUS_BAR_H
#define YAZE_APP_EDITOR_DUNGEON_WIDGETS_DUNGEON_STATUS_BAR_H

#include <functional>
#include <string>

namespace yaze::editor {

class DungeonCanvasViewer;

// Data source for the status bar — caller populates fields each frame.
struct DungeonStatusBarState {
  // Current placement/edit mode label (e.g., "Object", "Sprite", "Select")
  const char* tool_mode = "Select";

  // Workflow badge (e.g., "Workbench", "Standalone")
  const char* workflow_mode = nullptr;
  bool workflow_primary = false;

  // Number of selected objects and which layer they're on
  int selection_count = 0;
  int selection_layer = -1;  // -1 = mixed/none
  std::string selection_summary = "No selection";

  // Zoom level as a percentage (100 = 1x)
  int zoom_percent = 100;

  // Whether the current room has unsaved changes
  bool room_dirty = false;

  // Current room ID for display
  int room_id = -1;

  // Undo/Redo state
  bool can_undo = false;
  bool can_redo = false;
  const char* undo_desc = nullptr;  // Description of undo action
  const char* redo_desc = nullptr;  // Description of redo action
  int undo_depth = 0;               // Number of undo actions available

  // Callbacks for stable status-bar actions (set by the host panel).
  // Selection opens the detailed inspector without adding transient canvas
  // chrome above the drawing surface.
  std::function<void()> on_undo;
  std::function<void()> on_redo;
  std::function<void()> on_selection;
};

// Thin persistent bar drawn at the bottom of the dungeon editor canvas area.
// Displays tool mode, selection summary, zoom level, and dirty state at a
// glance.
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
