#ifndef YAZE_APP_EDITOR_DUNGEON_WORKSPACE_DUNGEON_WORKBENCH_INSPECTOR_HELPERS_H
#define YAZE_APP_EDITOR_DUNGEON_WORKSPACE_DUNGEON_WORKBENCH_INSPECTOR_HELPERS_H

#include <cstddef>
#include <span>

#include "imgui/imgui.h"
#include "zelda3/dungeon/room_object.h"

namespace yaze::editor::workbench {

bool HasEditableRoomObjectSize(std::span<const zelda3::RoomObject> objects,
                               std::span<const size_t> selected_indices);

void DrawInspectorSectionHeader(const char* label);

bool BeginInspectorSection(const char* label, bool default_open);

bool DrawActionButton(const char* label, const ImVec2& size);

}  // namespace yaze::editor::workbench

#endif  // YAZE_APP_EDITOR_DUNGEON_WORKSPACE_DUNGEON_WORKBENCH_INSPECTOR_HELPERS_H
