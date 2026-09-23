#include "app/editor/dungeon/inspectors/dungeon_connection_editor.h"

#include <algorithm>
#include <string>

#include "app/editor/dungeon/dungeon_canvas_viewer.h"
#include "app/editor/dungeon/dungeon_connection_edit.h"
#include "app/gui/automation/widget_auto_register.h"
#include "imgui/imgui.h"

namespace yaze::editor {
namespace {

void DrawEndpoint(const char* label, int room_id,
                  const zelda3::Room::Door& proposed,
                  const zelda3::Room::Door* existing) {
  ImGui::PushID(label);
  ImGui::TextWrapped("%s · room %03X", label, room_id);
  if (ImGui::BeginTable(
          "##Endpoint", 2,
          ImGuiTableFlags_SizingStretchProp | ImGuiTableFlags_NoPadOuterX)) {
    constexpr float kSize = 52.0f;
    ImGui::TableSetupColumn("Slot preview", ImGuiTableColumnFlags_WidthFixed,
                            kSize);
    ImGui::TableSetupColumn("Endpoint", ImGuiTableColumnFlags_WidthStretch);
    ImGui::TableNextRow();
    ImGui::TableNextColumn();
    const ImVec2 origin = ImGui::GetCursorScreenPos();
    auto* draw = ImGui::GetWindowDrawList();
    const ImVec2 corner(origin.x + kSize, origin.y + kSize);
    draw->AddRectFilled(origin, corner, ImGui::GetColorU32(ImGuiCol_FrameBg),
                        3.0f);
    draw->AddRect(origin, corner, ImGui::GetColorU32(ImGuiCol_Border), 3.0f);
    const auto [x, y, width, height] = proposed.GetEditorBounds();
    constexpr float kScale = kSize / 512.0f;
    const ImVec2 slot_min(origin.x + std::clamp(x * kScale, 0.0f, kSize - 3),
                          origin.y + std::clamp(y * kScale, 0.0f, kSize - 3));
    const ImVec2 slot_max(
        std::min(corner.x, slot_min.x + std::max(3.0f, width * kScale)),
        std::min(corner.y, slot_min.y + std::max(3.0f, height * kScale)));
    draw->AddRectFilled(slot_min, slot_max,
                        ImGui::GetColorU32(ImGuiCol_PlotHistogram));
    ImGui::Dummy(ImVec2(kSize, kSize));
    if (ImGui::IsItemHovered()) {
      ImGui::SetTooltip("Door slot preview in the room's 64 by 64 tile grid");
    }
    ImGui::TableNextColumn();
    ImGui::TextWrapped(
        "%s · slot %02X",
        std::string(zelda3::GetDoorDirectionName(proposed.direction)).c_str(),
        proposed.position);
    if (existing && existing->type != proposed.type) {
      ImGui::TextWrapped(
          "%s → %s",
          std::string(zelda3::GetDoorTypeName(existing->type)).c_str(),
          std::string(zelda3::GetDoorTypeName(proposed.type)).c_str());
    } else {
      ImGui::TextWrapped(
          "%s%s", existing ? "" : "New: ",
          std::string(zelda3::GetDoorTypeName(proposed.type)).c_str());
    }
    ImGui::EndTable();
  }
  ImGui::PopID();
}

void OpenTarget(DungeonCanvasViewer& viewer, const DungeonConnectionPlan& plan,
                const std::function<void(int, size_t)>& jump_to_reciprocal) {
  if (!plan.creates_return && jump_to_reciprocal) {
    jump_to_reciprocal(plan.target_room_id, plan.target_door_index);
    return;
  }
  if (!plan.creates_return &&
      viewer.NavigateToDoorConnectionTarget(plan.target_room_id,
                                            plan.target_door_index)) {
    return;
  }
  viewer.NavigateToRoom(plan.target_room_id);
  if (viewer.current_room_id() != plan.target_room_id) {
    return;
  }
  const auto& door = plan.target_after[plan.target_door_index];
  const auto [x, y, width, height] = door.GetEditorBounds();
  if (!plan.creates_return) {
    viewer.object_interaction().SelectEntity(EntityType::Door,
                                             plan.target_door_index);
  }
  viewer.ScrollToTile((x + width / 2) / 8, (y + height / 2) / 8);
  viewer.TriggerCanvasPingRect(x, y, width, height);
}

}  // namespace

void DrawDungeonConnectionEditor(
    DungeonCanvasViewer& viewer, size_t door_index,
    const std::function<void(int, size_t)>& jump_to_reciprocal) {
  const auto* room = viewer.rooms()
                         ? viewer.rooms()->GetIfLoaded(viewer.current_room_id())
                         : nullptr;
  if (!room || door_index >= room->GetDoors().size()) {
    return;
  }
  const auto source = room->GetDoors()[door_index];
  const std::array<uint8_t, 3> source_identity{
      source.position, static_cast<uint8_t>(source.type),
      static_cast<uint8_t>(source.direction)};
  auto& state = viewer.connection_editor_state();
  if (state.room_id != viewer.current_room_id() ||
      state.door_index != door_index || state.rom != viewer.rom() ||
      state.source != source_identity) {
    state = {};
    state.room_id = viewer.current_room_id();
    state.door_index = door_index;
    state.rom = viewer.rom();
    state.source = source_identity;
    state.layer = source.type == zelda3::DoorType::NormalDoorLower
                      ? DungeonConnectionLayer::kLower
                      : DungeonConnectionLayer::kUpper;
  }

  gui::AutoWidgetScope scope("Dungeon/ConnectionEditor");
  ImGui::PushID("DungeonConnectionEditor");
  ImGui::PushID(state.room_id);
  ImGui::PushID(static_cast<int>(door_index));
  ImGui::Spacing();
  ImGui::Separator();
  ImGui::TextUnformatted("Room connection");
  ImGui::TextWrapped("Preview both endpoints before applying the pair.");

  const bool normal = source.type == zelda3::DoorType::NormalDoor ||
                      source.type == zelda3::DoorType::NormalDoorLower;
  ImGui::BeginDisabled(viewer.header_read_only() || !normal);
  ImGui::TextUnformatted("Pair layer");
  ImGui::SetNextItemWidth(-1);
  const bool lower = state.layer == DungeonConnectionLayer::kLower;
  const bool open =
      ImGui::BeginCombo("##ConnectionLayer", lower ? "Lower" : "Upper");
  gui::AutoRegisterLastItem("combo", "ConnectionLayer");
  if (open) {
    if (ImGui::Selectable("Upper", !lower)) {
      state.layer = DungeonConnectionLayer::kUpper;
      state.error.clear();
    }
    gui::AutoRegisterLastItem("selectable", "ConnectionUpper");
    if (ImGui::Selectable("Lower", lower)) {
      state.layer = DungeonConnectionLayer::kLower;
      state.error.clear();
    }
    gui::AutoRegisterLastItem("selectable", "ConnectionLower");
    ImGui::EndCombo();
  }
  ImGui::EndDisabled();

  const DungeonConnectionRequest request{state.room_id, door_index,
                                         state.layer};
  const auto preview = viewer.PreviewDoorConnection(request);
  if (!preview.ok()) {
    DrawEndpoint("Source", state.room_id, source, &source);
    ImGui::TextWrapped("Connection unavailable: %s",
                       std::string(preview.status().message()).c_str());
  } else {
    const auto& plan = *preview;
    const auto* target_before =
        plan.creates_return ? nullptr
                            : &plan.target_before[plan.target_door_index];
    DrawEndpoint("Source", state.room_id, plan.source_after[door_index],
                 &plan.source_before[door_index]);
    DrawEndpoint("Return", plan.target_room_id,
                 plan.target_after[plan.target_door_index], target_before);
    ImGui::TextWrapped(
        "%s", plan.creates_return
                  ? "Missing return door. Apply creates the shown endpoint."
              : plan.changed()
                  ? "Return door found. Apply updates the shown pair."
                  : "Both doors already match.");
    if (viewer.header_read_only()) {
      ImGui::TextWrapped("This view is read-only.");
    }
    ImGui::BeginDisabled(viewer.header_read_only() || !plan.changed());
    const bool apply = ImGui::Button(
        plan.creates_return ? "Create Return Door" : "Update Pair",
        ImVec2(-1, 0));
    gui::AutoRegisterLastItem("button", "ConnectionApply");
    ImGui::EndDisabled();
    if (apply) {
      const auto status = viewer.ApplyDoorConnection(plan);
      state.error = status.ok() ? "" : std::string(status.message());
    }
    const bool can_open =
        viewer.CanNavigateRooms() ||
        (!plan.creates_return &&
         (jump_to_reciprocal || viewer.CanNavigateDoorConnectionTarget()));
    ImGui::BeginDisabled(!can_open);
    const bool open_target = ImGui::Button("Open Target Room", ImVec2(-1, 0));
    gui::AutoRegisterLastItem("button", "ConnectionOpenTarget");
    ImGui::EndDisabled();
    if (open_target) {
      OpenTarget(viewer, plan, jump_to_reciprocal);
    }
  }
  if (!state.error.empty()) {
    ImGui::TextWrapped("Edit not applied: %s", state.error.c_str());
  }
  ImGui::PopID();
  ImGui::PopID();
  ImGui::PopID();
}

}  // namespace yaze::editor
