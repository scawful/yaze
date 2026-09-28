#ifndef YAZE_APP_EDITOR_DUNGEON_WORKSPACE_DUNGEON_WORKBENCH_INSPECTOR_HELPERS_H
#define YAZE_APP_EDITOR_DUNGEON_WORKSPACE_DUNGEON_WORKBENCH_INSPECTOR_HELPERS_H

#include <cstddef>
#include <span>
#include <string>

#include "imgui/imgui.h"
#include "zelda3/dungeon/room_object.h"

namespace yaze::editor::workbench {

bool HasEditableRoomObjectSize(std::span<const zelda3::RoomObject> objects,
                               std::span<const size_t> selected_indices);

enum class ObjectSizeControlKind { kFixed, kLength, kArea, kSize, kVariant };

struct ObjectSizeDescription {
  ObjectSizeControlKind kind = ObjectSizeControlKind::kFixed;
  int width_tiles = 1;
  int height_tiles = 1;
  bool length_horizontal = true;
  std::string footprint;
  std::string wheel_hint;
};

// Uses the same geometry service as canvas selection; does not mutate the object.
ObjectSizeDescription DescribeObjectSize(const zelda3::RoomObject& object);

// Draw size/variant rows inside the caller's two-column property table.
// Returns a requested encoded value; the caller owns mutation and undo.
bool DrawObjectSizeControls(const zelda3::RoomObject& object,
                            uint8_t* requested_size);

void DrawInspectorSectionHeader(const char* label);

bool BeginInspectorSection(const char* label, bool default_open);

bool DrawActionButton(const char* label, const ImVec2& size);

}  // namespace yaze::editor::workbench

#endif  // YAZE_APP_EDITOR_DUNGEON_WORKSPACE_DUNGEON_WORKBENCH_INSPECTOR_HELPERS_H
