#include "app/editor/dungeon/widgets/dungeon_room_nav_widget.h"

#include <charconv>
#include <cstring>
#include <string>

#include "imgui/imgui.h"
#include "zelda3/dungeon/room.h"
#include "zelda3/resource_labels.h"

namespace yaze::editor {

namespace {

constexpr int kRoomMatrixCols = 16;
constexpr int kRoomMatrixRows = 19;
std::optional<int> RoomIfValid(int candidate) {
  if (candidate < 0 || candidate >= zelda3::kNumberOfRooms) {
    return std::nullopt;
  }
  return candidate;
}

std::string MakeTooltip(const std::optional<int>& target,
                        const char* direction) {
  if (!target.has_value()) {
    return "";
  }
  const auto label = zelda3::GetRoomLabel(*target);
  char buf[192];
  snprintf(buf, sizeof(buf),
           "Room grid %s: [%03X] %s\nGrid position does not confirm a door "
           "connection.",
           direction, *target, label.c_str());
  return buf;
}

bool ArrowButtonWithTooltip(const char* id, ImGuiDir dir,
                            const std::optional<int>& target,
                            const std::string& tooltip,
                            const std::function<void(int)>& on_navigate) {
  const bool enabled = target.has_value();
  if (!enabled) {
    ImGui::BeginDisabled();
  }
  const bool pressed = ImGui::ArrowButton(id, dir);
  if (!enabled) {
    ImGui::EndDisabled();
  }
  if (enabled && ImGui::IsItemHovered() && !tooltip.empty()) {
    ImGui::SetTooltip("%s", tooltip.c_str());
  }
  if (pressed && enabled && on_navigate) {
    on_navigate(*target);
    return true;
  }
  return false;
}

}  // namespace

DungeonRoomNavWidget::Neighbors DungeonRoomNavWidget::GetNeighbors(
    int room_id) {
  if (room_id < 0 || room_id >= zelda3::kNumberOfRooms) {
    return {};
  }

  const int col = room_id % kRoomMatrixCols;
  const int row = room_id / kRoomMatrixCols;

  // Note: the matrix has empty slots in the final row; RoomIfValid handles it.
  Neighbors out;
  out.west = RoomIfValid(col > 0 ? room_id - 1 : -1);
  out.east = RoomIfValid(col < (kRoomMatrixCols - 1) ? room_id + 1 : -1);
  out.north = RoomIfValid(row > 0 ? room_id - kRoomMatrixCols : -1);
  out.south =
      RoomIfValid(row < (kRoomMatrixRows - 1) ? room_id + kRoomMatrixCols : -1);
  return out;
}

bool DungeonRoomNavWidget::Draw(const char* id, int room_id,
                                const std::function<void(int)>& on_navigate) {
  ImGui::PushID(id);

  char room_text[9];
  snprintf(room_text, sizeof(room_text), "%03X", room_id);
  ImGui::SetNextItemWidth(ImGui::CalcTextSize("00000").x +
                          ImGui::GetStyle().FramePadding.x * 2.0f);
  const bool submitted =
      ImGui::InputText("##RoomId", room_text, sizeof(room_text),
                       ImGuiInputTextFlags_CharsHexadecimal |
                           ImGuiInputTextFlags_EnterReturnsTrue);
  if (ImGui::IsItemHovered()) {
    ImGui::SetTooltip("Room ID (hexadecimal, 000-127). Press Enter to open.");
  }
  bool navigated = false;
  if (submitted && on_navigate) {
    int requested_room = -1;
    const auto parsed = std::from_chars(
        room_text, room_text + std::strlen(room_text), requested_room, 16);
    if (parsed.ec == std::errc{} &&
        parsed.ptr == room_text + std::strlen(room_text) &&
        requested_room != room_id && RoomIfValid(requested_room)) {
      on_navigate(requested_room);
      navigated = true;
    }
  }
  ImGui::SameLine();

  const Neighbors n = GetNeighbors(room_id);
  const std::string tip_w = MakeTooltip(n.west, "West");
  const std::string tip_n = MakeTooltip(n.north, "North");
  const std::string tip_s = MakeTooltip(n.south, "South");
  const std::string tip_e = MakeTooltip(n.east, "East");

  navigated |=
      ArrowButtonWithTooltip("West", ImGuiDir_Left, n.west, tip_w, on_navigate);
  ImGui::SameLine();
  navigated |=
      ArrowButtonWithTooltip("North", ImGuiDir_Up, n.north, tip_n, on_navigate);
  ImGui::SameLine();
  navigated |= ArrowButtonWithTooltip("South", ImGuiDir_Down, n.south, tip_s,
                                      on_navigate);
  ImGui::SameLine();
  navigated |= ArrowButtonWithTooltip("East", ImGuiDir_Right, n.east, tip_e,
                                      on_navigate);

  ImGui::PopID();
  return navigated;
}

}  // namespace yaze::editor
