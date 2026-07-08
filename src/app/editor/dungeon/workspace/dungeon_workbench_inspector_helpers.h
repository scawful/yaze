#ifndef YAZE_APP_EDITOR_DUNGEON_WORKSPACE_DUNGEON_WORKBENCH_INSPECTOR_HELPERS_H
#define YAZE_APP_EDITOR_DUNGEON_WORKSPACE_DUNGEON_WORKBENCH_INSPECTOR_HELPERS_H

#include "imgui/imgui.h"

namespace yaze::editor::workbench {

void DrawInspectorSectionHeader(const char* label);

bool BeginInspectorSection(const char* label, bool default_open);

bool DrawActionButton(const char* label, const ImVec2& size);

}  // namespace yaze::editor::workbench

#endif  // YAZE_APP_EDITOR_DUNGEON_WORKSPACE_DUNGEON_WORKBENCH_INSPECTOR_HELPERS_H
