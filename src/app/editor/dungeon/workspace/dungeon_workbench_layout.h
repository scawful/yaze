#ifndef YAZE_APP_EDITOR_DUNGEON_WORKSPACE_DUNGEON_WORKBENCH_LAYOUT_H
#define YAZE_APP_EDITOR_DUNGEON_WORKSPACE_DUNGEON_WORKBENCH_LAYOUT_H

#include "imgui/imgui.h"

namespace yaze::editor {

struct DungeonWorkbenchToolDrawerLayout {
  bool show_drawer = false;
  bool compact = false;
  float canvas_height = 0.0f;
  float drawer_height = 0.0f;
  float min_drawer_height = 0.0f;
  float max_drawer_height = 0.0f;
};

// Resolve the vertical canvas/tool split without touching ImGui state. When
// height is constrained, both regions remain visible and use a stable 60/40
// split instead of hiding the requested tool.
DungeonWorkbenchToolDrawerLayout ResolveDungeonWorkbenchToolDrawerLayout(
    float total_height, float splitter_height, float stored_drawer_height,
    float min_canvas_height, float min_drawer_height, bool want_drawer);

// Draw a workbench-standard vertical splitter that mutates pane_width.
// Returns true when the drag moves past collapse_threshold.
bool DrawDungeonWorkbenchVerticalSplitter(const char* id, float height,
                                          float* pane_width, float min_width,
                                          float max_width,
                                          bool resize_from_left_edge,
                                          float collapse_threshold = 100.0f);

// Draw a horizontal splitter above a bottom pane. Returns true when the drag
// continues below the pane's minimum size (or an explicitly larger collapse
// threshold), requesting that the bottom pane collapse.
bool DrawDungeonWorkbenchHorizontalSplitter(const char* id, float width,
                                            float* pane_height,
                                            float min_height, float max_height,
                                            float collapse_threshold = 100.0f);

}  // namespace yaze::editor

#endif  // YAZE_APP_EDITOR_DUNGEON_WORKSPACE_DUNGEON_WORKBENCH_LAYOUT_H
