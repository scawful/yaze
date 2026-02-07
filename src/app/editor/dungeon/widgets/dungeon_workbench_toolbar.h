#ifndef YAZE_APP_EDITOR_DUNGEON_WIDGETS_DUNGEON_WORKBENCH_TOOLBAR_H
#define YAZE_APP_EDITOR_DUNGEON_WIDGETS_DUNGEON_WORKBENCH_TOOLBAR_H

#include <cstddef>
#include <deque>
#include <functional>

#include "app/editor/dungeon/dungeon_workbench_state.h"

namespace yaze::editor {

class DungeonCanvasViewer;

struct DungeonWorkbenchToolbarParams {
  DungeonWorkbenchLayoutState* layout = nullptr;

  int* current_room_id = nullptr;
  int* previous_room_id = nullptr;
  bool* split_view_enabled = nullptr;
  int* compare_room_id = nullptr;

  DungeonCanvasViewer* primary_viewer = nullptr;
  DungeonCanvasViewer* compare_viewer = nullptr;

  std::function<void(int)> on_room_selected;
  std::function<const std::deque<int>&()> get_recent_rooms;

  char* compare_search_buf = nullptr;
  size_t compare_search_buf_size = 0;
};

// Draws the stitched "Workbench" toolbar (room nav + compare + key view
// toggles). Intended to replace per-canvas header chrome in Workbench mode.
class DungeonWorkbenchToolbar {
 public:
  static void Draw(const DungeonWorkbenchToolbarParams& params);
};

}  // namespace yaze::editor

#endif  // YAZE_APP_EDITOR_DUNGEON_WIDGETS_DUNGEON_WORKBENCH_TOOLBAR_H

