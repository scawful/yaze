#include "app/editor/dungeon/inspectors/dungeon_destination_editor.h"

#include <algorithm>

#include "absl/strings/str_format.h"
#include "app/editor/dungeon/dungeon_canvas_viewer.h"
#include "app/editor/dungeon/dungeon_project_labels.h"
#include "app/editor/dungeon/dungeon_room_edit.h"
#include "app/gui/automation/widget_auto_register.h"
#include "app/gui/core/input.h"
#include "imgui/imgui.h"
#include "zelda3/dungeon/object_layer_semantics.h"
#include "zelda3/dungeon/room_collision.h"

namespace yaze::editor {

void DrawDungeonDestinationEditor(DungeonCanvasViewer& viewer) {
  const int room_id = viewer.current_room_id();
  const auto* room =
      viewer.rooms() ? viewer.rooms()->GetIfLoaded(room_id) : nullptr;
  if (!room || room->rom() != viewer.rom()) {
    ImGui::TextWrapped("Open a room to edit its destinations.");
    return;
  }
  gui::AutoWidgetScope scope("Dungeon/Destinations");
  ImGui::PushID(&viewer);
  ImGui::PushID(room_id);
  auto* storage = ImGui::GetStateStorage();
  const auto route_key = ImGui::GetID("DestinationRoute");
  int route = storage->GetInt(route_key, 0);
  const auto selection = viewer.object_interaction().GetSelectedObjectIndices();
  const auto inspect_key = ImGui::GetID("StaircaseInspectResult");
  const auto selected_key = ImGui::GetID("StaircaseInspectSelection");
  const int selected =
      selection.size() == 1 ? static_cast<int>(selection.front()) : -1;
  if (storage->GetInt(selected_key, -1) != selected) {
    storage->SetInt(selected_key, selected);
    storage->SetInt(inspect_key, 0);
  }
  ImGui::BeginDisabled(selection.size() != 1 || !room->rom() ||
                       room->has_custom_collision());
  const bool inspect = ImGui::Button("Find selected stair slot", ImVec2(-1, 0));
  gui::AutoRegisterLastItem("button", "FindStairSlot");
  ImGui::EndDisabled();
  if (inspect) {
    // Explicit inspection avoids replaying the room on every ImGui frame.
    auto input = zelda3::MakeRoomCollisionInput(*room);
    // The loaded layout is a primary-list prefix ($018834), before the room
    // stream ($01884A). Preserve that order and the selected object's identity.
    const auto& layout = room->GetLayout().GetObjects();
    input.objects.insert(input.objects.begin(), layout.begin(), layout.end());
    const bool custom_objects =
        std::any_of(input.objects.begin(), input.objects.end(),
                    zelda3::HasActiveCustomObjectOverride);
    const auto mappings =
        custom_objects
            ? std::vector<zelda3::StaircaseSlotResolution>{}
            : zelda3::ResolveVanillaStaircaseSlots(*room->rom(), input);
    int found = -1;
    for (const auto& mapping : mappings) {
      if (mapping.object_index == selection.front() + layout.size() &&
          mapping.slot) {
        found = *mapping.slot;
        break;
      }
    }
    storage->SetInt(inspect_key, custom_objects ? 3 : (found >= 0 ? 1 : 2));
    if (found >= 0) {
      route = found + 1;
      storage->SetInt(route_key, route);
    }
  }
  if (room->has_custom_collision()) {
    ImGui::TextWrapped("Stair lookup is unavailable with custom collision.");
  } else if (storage->GetInt(inspect_key, 0) == 3) {
    ImGui::TextWrapped(
        "Stair lookup is unavailable with active custom object overrides.");
  } else if (storage->GetInt(inspect_key, 0) == 1) {
    ImGui::TextWrapped(
        "Opened the selected stair's vanilla collision slot. "
        "Preview only; verify custom engines in-game.");
  } else if (storage->GetInt(inspect_key, 0) == 2) {
    ImGui::TextWrapped(
        "No unambiguous staircase trigger was found. The selected "
        "object may not be a supported stair, or its trigger may "
        "be overwritten, overlapping, outside the room or overflowing.");
  }
  constexpr const char* routes[] = {"Pit / warp", "Stair slot 1",
                                    "Stair slot 2", "Stair slot 3",
                                    "Stair slot 4"};
  ImGui::SetNextItemWidth(-1);
  const bool route_open = ImGui::BeginCombo("##Route", routes[route]);
  gui::AutoRegisterLastItem("combo", "Route");
  if (route_open) {
    for (int i = 0; i < 5; ++i) {
      if (ImGui::Selectable(routes[i], route == i)) {
        route = i;
        storage->SetInt(route_key, route);
      }
      gui::AutoRegisterLastItem("selectable", absl::StrFormat("Route/%d", i));
    }
    ImGui::EndCombo();
  }
  const bool pit = route == 0;
  const int index = pit ? 0 : route - 1;
  const auto room_field =
      pit ? RoomMetadataField::kHolewarp : RoomMetadataField::kStaircaseRoom;
  const auto plane_field =
      pit ? RoomMetadataField::kPitPlane : RoomMetadataField::kStaircasePlane;
  int destination = pit ? room->holewarp() : room->staircase_room(index);
  int plane = pit ? room->CaptureMetadataSnapshot().pit_target_layer
                  : room->staircase_plane(index);
  const auto error_key = ImGui::GetID("DestinationError");
  auto edit = [&](RoomMetadataField field, int value) {
    const auto status = viewer.EditRoomMetadata(room_id, {field, value, index});
    storage->SetBool(error_key, !status.ok());
    return status.ok();
  };
  const bool editable =
      !viewer.header_read_only() && viewer.IsObjectInteractionEnabled();
  ImGui::BeginDisabled(!editable);
  ImGui::TextUnformatted("Destination room (hex 00–FF)");
  ImGui::SetNextItemWidth(-1);
  int requested = destination;
  if (gui::InputScalarDeferred(
          "##DestinationRoom", ImGuiDataType_S32, &requested, "%02X",
          ImGuiInputTextFlags_CharsHexadecimal,
          {reinterpret_cast<uintptr_t>(viewer.rooms()),
           static_cast<uint64_t>(room_id), static_cast<uint64_t>(route)})) {
    if (edit(room_field, requested))
      destination = requested;
  }
  gui::AutoRegisterLastItem("input_hex", "Room");
  // $01C31F: 0476 = {0,1,1}; $01C322: EE = {0,0,1}.
  constexpr const char* planes[] = {"00 · Upper (BG1)", "01 · Lower (BG1)",
                                    "02 · Lower (BG2)"};
  const auto plane_label =
      plane >= 0 && plane < 3
          ? std::string(planes[plane])
          : absl::StrFormat("%02X · Unmapped vanilla plane", plane);
  ImGui::TextUnformatted("Arrival layer");
  ImGui::SetNextItemWidth(-1);
  const bool open =
      ImGui::BeginCombo("##DestinationPlane", plane_label.c_str());
  gui::AutoRegisterLastItem("combo", "Plane");
  if (open) {
    for (int i = 0; i < 3; ++i) {
      if (ImGui::Selectable(planes[i], plane == i))
        edit(plane_field, i);
      gui::AutoRegisterLastItem("selectable", absl::StrFormat("Plane/%d", i));
    }
    ImGui::EndCombo();
  }
  ImGui::EndDisabled();
  ImGui::TextWrapped(
      "%03X · %s", destination,
      dungeon_project_labels::GetRoomLabel(viewer.project(), destination)
          .c_str());
  ImGui::BeginDisabled(!viewer.CanNavigateRooms());
  const bool navigate = ImGui::Button("Open destination", ImVec2(-1, 0));
  gui::AutoRegisterLastItem("button", "Open");
  ImGui::EndDisabled();
  if (!pit) {
    ImGui::TextWrapped(
        "Edits header slot %d. Find selected stair slot previews the vanilla "
        "mapping; editing does not create a return link.",
        index + 1);
  } else {
    ImGui::TextWrapped(
        "Pit damage and warp behavior depend on the room and game logic. This "
        "edits the stored destination only.");
  }
  if (storage->GetBool(error_key, false)) {
    ImGui::TextWrapped(
        "Destination edit rejected. Use room 00–FF and a supported arrival "
        "layer.");
  }
  if (!editable)
    ImGui::TextWrapped("This view is read-only.");
  ImGui::PopID();
  ImGui::PopID();
  // Navigation may change the current room/view. Do not access room state after it.
  if (navigate)
    viewer.NavigateToRoom(destination);
}

}  // namespace yaze::editor
