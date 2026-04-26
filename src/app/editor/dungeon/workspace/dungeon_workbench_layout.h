#ifndef YAZE_APP_EDITOR_DUNGEON_WORKSPACE_DUNGEON_WORKBENCH_LAYOUT_H
#define YAZE_APP_EDITOR_DUNGEON_WORKSPACE_DUNGEON_WORKBENCH_LAYOUT_H

#include "imgui/imgui.h"

namespace yaze::editor {

// Draw a workbench-standard vertical splitter that mutates pane_width.
// Returns true when the drag moves past collapse_threshold.
bool DrawDungeonWorkbenchVerticalSplitter(const char* id, float height,
                                          float* pane_width, float min_width,
                                          float max_width,
                                          bool resize_from_left_edge,
                                          float collapse_threshold = 100.0f);

}  // namespace yaze::editor

#endif  // YAZE_APP_EDITOR_DUNGEON_WORKSPACE_DUNGEON_WORKBENCH_LAYOUT_H
