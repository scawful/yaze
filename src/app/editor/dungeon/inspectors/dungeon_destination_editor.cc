#include "app/editor/dungeon/inspectors/dungeon_destination_editor.h"

#include <algorithm>
#include <cstdint>

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

namespace {

// Fingerprint the authored inputs to the vanilla stair resolver. Rendering,
// palette refreshes, and destination header writes must not invalidate a
// selected stair's mapping; object/door/layout changes can.
uint64_t StairLookupSourceFingerprint(const zelda3::Room& room) {
  uint64_t hash = 1469598103934665603ULL;
  const auto mix = [&](uint64_t value) {
    hash ^= value;
    hash *= 1099511628211ULL;
  };
  mix(reinterpret_cast<uintptr_t>(room.rom()));
  mix(static_cast<uint64_t>(room.id()));
  mix(core::FeatureFlags::get().kEnableCustomObjects);
  mix(zelda3::CustomObjectManager::Get().asset_generation());
  const auto metadata = room.CaptureMetadataSnapshot();
  mix(metadata.blockset);
  mix(metadata.layout);
  mix(metadata.floor1);
  mix(metadata.floor2);
  mix(static_cast<uint8_t>(metadata.bg2));
  mix(metadata.layer2_mode);
  mix(metadata.layer_merging.ID);
  mix(static_cast<uint8_t>(metadata.collision));
  mix(static_cast<uint8_t>(metadata.effect));
  mix(static_cast<uint8_t>(metadata.tag1));
  mix(static_cast<uint8_t>(metadata.tag2));
  mix(room.has_custom_collision());
  const auto mix_objects = [&](const std::vector<zelda3::RoomObject>& objects) {
    mix(objects.size());
    for (const auto& object : objects) {
      mix(static_cast<uint16_t>(object.id_));
      mix(object.x());
      mix(object.y());
      mix(object.size());
      mix(object.GetLayerValue());
      mix(static_cast<uint64_t>(object.options()));
      mix(object.block_behavior_layer());
      mix(object.torch_reserved_bit());
      mix(object.lit_);
    }
  };
  mix_objects(room.GetLayout().GetObjects());
  mix_objects(room.GetTileObjects());
  mix(room.GetDoors().size());
  for (const auto& door : room.GetDoors()) {
    mix(door.position);
    mix(static_cast<uint8_t>(door.direction));
    mix(static_cast<uint8_t>(door.type));
    mix(door.byte1);
    mix(door.byte2);
  }
  // ComposeYazeRoomTilemaps supplies the resolver's basic attribute pass.
  // Only tile number and flip bits influence that pass; palette and priority
  // bits can change during harmless presentation refreshes.
  const auto mix_tilemap = [&](const gfx::BackgroundBuffer& layout,
                               const gfx::BackgroundBuffer& objects) {
    const auto& layout_words = layout.buffer();
    const auto& object_words = objects.buffer();
    for (size_t i = 0; i < zelda3::kRoomTilemapWords; ++i) {
      const uint16_t object_word =
          i < object_words.size() ? object_words[i] : 0;
      const uint16_t layout_word =
          i < layout_words.size() ? layout_words[i] : 0;
      mix((object_word != 0 ? object_word : layout_word) & 0xC3FF);
    }
  };
  mix_tilemap(room.bg1_buffer(), room.object_bg1_buffer());
  mix_tilemap(room.bg2_buffer(), room.object_bg2_buffer());
  return hash;
}

}  // namespace

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
  const auto revision_low_key = ImGui::GetID("StaircaseInspectRevisionLow");
  const auto revision_high_key = ImGui::GetID("StaircaseInspectRevisionHigh");
  const auto auto_route_key = ImGui::GetID("StaircaseInspectAutoRoute");
  const int selected =
      selection.size() == 1 ? static_cast<int>(selection.front()) : -1;
  const uint64_t source = StairLookupSourceFingerprint(*room);
  const int revision_low = static_cast<int>(static_cast<uint32_t>(source));
  const int revision_high = static_cast<int>(source >> 32);
  if (storage->GetInt(selected_key, -1) != selected ||
      storage->GetInt(revision_low_key, -1) != revision_low ||
      storage->GetInt(revision_high_key, -1) != revision_high) {
    storage->SetInt(selected_key, selected);
    storage->SetInt(revision_low_key, revision_low);
    storage->SetInt(revision_high_key, revision_high);
    storage->SetInt(inspect_key,
                    storage->GetBool(auto_route_key, false) ? 4 : 0);
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
      storage->SetBool(auto_route_key, true);
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
  } else if (storage->GetInt(inspect_key, 0) == 4) {
    ImGui::TextWrapped(
        "Selected stair mapping is stale. Find the slot again, or choose a "
        "header slot explicitly before editing its destination.");
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
        storage->SetBool(auto_route_key, false);
        storage->SetInt(inspect_key, 0);
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
  const bool auto_route_stale =
      storage->GetBool(auto_route_key, false) &&
      (storage->GetInt(inspect_key, 0) != 1 || room->has_custom_collision());
  const bool editable = !auto_route_stale && !viewer.header_read_only() &&
                        viewer.IsObjectInteractionEnabled();
  ImGui::BeginDisabled(!editable);
  ImGui::TextUnformatted(room_id >= 0x100
                             ? "Destination room (low byte; page 1xx)"
                             : "Destination room (hex 00–FF)");
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
  // The header stores one byte; the game keeps this room's high byte ($A1),
  // so from room 119 the byte 1D leads to 11D.
  const int destination_room =
      zelda3::ResolveHeaderDestinationRoom(room_id, destination);
  ImGui::TextWrapped(
      "%03X · %s", destination_room,
      dungeon_project_labels::GetRoomLabel(viewer.project(), destination_room)
          .c_str());
  ImGui::BeginDisabled(auto_route_stale || !viewer.CanNavigateRooms());
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
  if (!editable && !auto_route_stale)
    ImGui::TextWrapped("This view is read-only.");
  ImGui::PopID();
  ImGui::PopID();
  // Navigation may change the current room/view. Do not access room state after it.
  if (navigate)
    viewer.NavigateToRoom(destination_room);
}

}  // namespace yaze::editor
