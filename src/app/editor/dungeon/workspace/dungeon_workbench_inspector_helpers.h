#ifndef YAZE_APP_EDITOR_DUNGEON_WORKSPACE_DUNGEON_WORKBENCH_INSPECTOR_HELPERS_H
#define YAZE_APP_EDITOR_DUNGEON_WORKSPACE_DUNGEON_WORKBENCH_INSPECTOR_HELPERS_H

#include <cstddef>
#include <span>

#include "imgui/imgui.h"
#include "zelda3/dungeon/room_object.h"

namespace yaze::editor::workbench {

bool HasEditableRoomObjectSize(std::span<const zelda3::RoomObject> objects,
                               std::span<const size_t> selected_indices);

// Draw size/variant rows inside the caller's two-column property table.
// Returns a requested encoded value; the caller owns mutation and undo.
bool DrawObjectSizeControls(const zelda3::RoomObject& object,
                            uint8_t* requested_size);

void DrawInspectorSectionHeader(const char* label);

bool BeginInspectorSection(const char* label, bool default_open);

bool DrawActionButton(const char* label, const ImVec2& size);

}  // namespace yaze::editor::workbench

#endif  // YAZE_APP_EDITOR_DUNGEON_WORKSPACE_DUNGEON_WORKBENCH_INSPECTOR_HELPERS_H
