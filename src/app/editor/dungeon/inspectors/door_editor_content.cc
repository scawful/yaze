#include "app/editor/dungeon/inspectors/door_editor_content.h"

#include <algorithm>
#include <array>
#include <string>

#include "absl/strings/str_format.h"
#include "app/editor/agent/agent_ui_theme.h"
#include "app/gui/core/style_guard.h"
#include "app/gui/core/ui_helpers.h"
#include "imgui/imgui.h"

namespace yaze::editor {

namespace {

ImVec4 DoorTypeAccent(const AgentUITheme& theme, zelda3::DoorType type) {
  const int type_val = static_cast<int>(type);
  if (type_val <= 0x12) {
    return theme.dungeon_object_door;
  }
  if (type_val <= 0x1E) {
    return theme.status_warning;
  }
  return theme.status_success;
}

std::string ShortDoorTypeLabel(zelda3::DoorType type) {
  switch (type) {
    case zelda3::DoorType::NormalDoor:
      return "Normal";
    case zelda3::DoorType::NormalDoorLower:
      return "Lower";
    case zelda3::DoorType::CaveExit:
      return "Cave";
    case zelda3::DoorType::DoubleSidedShutter:
      return "2-Side";
    case zelda3::DoorType::EyeWatchDoor:
      return "Eye";
    case zelda3::DoorType::SmallKeyDoor:
      return "S-Key";
    case zelda3::DoorType::BigKeyDoor:
      return "B-Key";
    case zelda3::DoorType::SmallKeyStairsUp:
      return "Up";
    case zelda3::DoorType::SmallKeyStairsDown:
      return "Down";
    case zelda3::DoorType::DashWall:
      return "Dash";
    case zelda3::DoorType::BombableDoor:
      return "Bomb";
    case zelda3::DoorType::ExplodingWall:
      return "Blast";
    case zelda3::DoorType::CurtainDoor:
      return "Curtain";
    case zelda3::DoorType::BottomSidedShutter:
      return "Bottom";
    case zelda3::DoorType::TopSidedShutter:
      return "Top";
    case zelda3::DoorType::FancyDungeonExit:
      return "Exit";
    case zelda3::DoorType::WaterfallDoor:
      return "Water";
    case zelda3::DoorType::ExitMarker:
      return "Marker";
    case zelda3::DoorType::LayerSwapMarker:
      return "Layer";
    case zelda3::DoorType::DungeonSwapMarker:
      return "Swap";
  }
  return "Door";
}

}  // namespace

DungeonCanvasViewer* DoorEditorContent::ResolveCanvasViewer() {
  if (canvas_viewer_provider_) {
    canvas_viewer_ = canvas_viewer_provider_();
  }
  return canvas_viewer_;
}

void DoorEditorContent::CancelPlacement() {
  door_placement_mode_ = false;
  if (ResolveCanvasViewer()) {
    canvas_viewer_->object_interaction().SetDoorPlacementMode(
        false, zelda3::DoorType::NormalDoor);
  }
}

void DoorEditorContent::OnClose() {
  CancelPlacement();
}

void DoorEditorContent::Draw(bool* p_open) {
  (void)p_open;
  ResolveCanvasViewer();

  const auto& theme = AgentUI::GetTheme();
  static constexpr std::array<zelda3::DoorType, 20> kDoorTypes = {{
      zelda3::DoorType::NormalDoor,
      zelda3::DoorType::NormalDoorLower,
      zelda3::DoorType::CaveExit,
      zelda3::DoorType::DoubleSidedShutter,
      zelda3::DoorType::EyeWatchDoor,
      zelda3::DoorType::SmallKeyDoor,
      zelda3::DoorType::BigKeyDoor,
      zelda3::DoorType::SmallKeyStairsUp,
      zelda3::DoorType::SmallKeyStairsDown,
      zelda3::DoorType::DashWall,
      zelda3::DoorType::BombableDoor,
      zelda3::DoorType::ExplodingWall,
      zelda3::DoorType::CurtainDoor,
      zelda3::DoorType::BottomSidedShutter,
      zelda3::DoorType::TopSidedShutter,
      zelda3::DoorType::FancyDungeonExit,
      zelda3::DoorType::WaterfallDoor,
      zelda3::DoorType::ExitMarker,
      zelda3::DoorType::LayerSwapMarker,
      zelda3::DoorType::DungeonSwapMarker,
  }};

  gui::SectionHeader(ICON_MD_DOOR_FRONT, "Door Styles", theme.text_info);
  ImGui::TextColored(theme.text_secondary_gray,
                     "Select a door style, then click a wall in the room "
                     "canvas to place it.");

  if (door_placement_mode_) {
    ImGui::TextColored(theme.status_warning, ICON_MD_PLACE " Active: %s",
                       std::string(zelda3::GetDoorTypeName(selected_door_type_))
                           .c_str());
    ImGui::SameLine();
    if (ImGui::SmallButton(ICON_MD_CANCEL " Cancel")) {
      CancelPlacement();
    }
  }

  constexpr float kDoorCardHeight = 48.0f;
  constexpr float kDoorCardSpacing = 6.0f;
  constexpr float kMinDoorCardWidth = 92.0f;
  const float panel_width = ImGui::GetContentRegionAvail().x;
  const int items_per_row = std::max(
      2, static_cast<int>((panel_width + kDoorCardSpacing) /
                          (kMinDoorCardWidth + kDoorCardSpacing)));
  const float card_width =
      std::max(kMinDoorCardWidth,
               (panel_width - (items_per_row - 1) * kDoorCardSpacing) /
                   items_per_row);

  ImGui::BeginChild("##DoorTypeGrid", ImVec2(0, 150), true,
                    ImGuiWindowFlags_AlwaysVerticalScrollbar);

  int col = 0;
  for (size_t i = 0; i < kDoorTypes.size(); ++i) {
    const auto door_type = kDoorTypes[i];
    const bool is_selected = selected_door_type_ == door_type;
    const int type_val = static_cast<int>(door_type);
    ImGui::PushID(static_cast<int>(i));

    ImVec4 button_color = DoorTypeAccent(theme, door_type);
    if (is_selected) {
      button_color.x = std::min(1.0f, button_color.x + 0.2f);
      button_color.y = std::min(1.0f, button_color.y + 0.2f);
      button_color.z = std::min(1.0f, button_color.z + 0.2f);
    }

    gui::StyleColorGuard btn_colors({
        {ImGuiCol_Button, button_color},
        {ImGuiCol_ButtonHovered,
         ImVec4(button_color.x + 0.1f, button_color.y + 0.1f,
                button_color.z + 0.1f, 1.0f)},
        {ImGuiCol_ButtonActive,
         ImVec4(button_color.x + 0.2f, button_color.y + 0.2f,
                button_color.z + 0.2f, 1.0f)},
    });

    const std::string label =
        absl::StrFormat("%02X\n%s", type_val,
                        ShortDoorTypeLabel(door_type).c_str());
    if (ImGui::Button(label.c_str(), ImVec2(card_width, kDoorCardHeight))) {
      selected_door_type_ = door_type;
      door_placement_mode_ = true;
      if (ResolveCanvasViewer()) {
        canvas_viewer_->object_interaction().SetDoorPlacementMode(
            true, selected_door_type_);
      }
    }

    if (ImGui::IsItemHovered()) {
      ImGui::SetTooltip("%s (0x%02X)\nClick to select for placement",
                        std::string(zelda3::GetDoorTypeName(door_type)).c_str(),
                        type_val);
    }

    if (is_selected) {
      ImVec2 min = ImGui::GetItemRectMin();
      ImVec2 max = ImGui::GetItemRectMax();
      ImGui::GetWindowDrawList()->AddRect(min, max, IM_COL32(255, 255, 0, 255),
                                          0.0f, 0, 2.0f);
    }

    ImGui::PopID();
    col++;
    if (col < items_per_row && i < kDoorTypes.size() - 1) {
      ImGui::SameLine();
    } else {
      col = 0;
    }
  }

  ImGui::EndChild();

  if (!rooms_ || current_room_id_ < 0 || current_room_id_ >= 296) {
    return;
  }

  const auto& room = (*rooms_)[current_room_id_];
  const auto& doors = room.GetDoors();

  ImGui::Spacing();
  gui::SectionHeader(ICON_MD_LIST, "Doors In Room", theme.text_info);
  if (doors.empty()) {
    ImGui::TextColored(theme.text_secondary_gray,
                       ICON_MD_INFO " No doors in this room");
    return;
  }

  ImGui::BeginChild("##DoorList", ImVec2(0, 0), true);
  for (size_t i = 0; i < doors.size(); ++i) {
    const auto& door = doors[i];
    const auto [tile_x, tile_y] = door.GetTileCoords();
    const std::string type_name(zelda3::GetDoorTypeName(door.type));
    const std::string dir_name(zelda3::GetDoorDirectionName(door.direction));

    ImGui::PushID(static_cast<int>(i));
    ImGui::Text("[%zu] %s", i, type_name.c_str());
    ImGui::SameLine();
    ImGui::TextColored(theme.text_secondary_gray, "(%s @ %d,%d)",
                       dir_name.c_str(), tile_x, tile_y);
    ImGui::SameLine();
    if (ImGui::SmallButton(ICON_MD_DELETE "##DeleteDoor")) {
      auto& mutable_room = (*rooms_)[current_room_id_];
      mutable_room.RemoveDoor(i);
    }
    ImGui::PopID();
  }
  ImGui::EndChild();
}

}  // namespace yaze::editor
