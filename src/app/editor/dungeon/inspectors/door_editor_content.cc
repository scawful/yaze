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

// Semantic icon prefix used by both the 20-type placement picker and the
// "Selected Door" swap combo. Keeps the 20 core types scannable by category.
const char* DoorTypeIcon(zelda3::DoorType type) {
  switch (type) {
    case zelda3::DoorType::NormalDoor:
    case zelda3::DoorType::NormalDoorLower:
      return ICON_MD_DOOR_FRONT;
    case zelda3::DoorType::CaveExit:
    case zelda3::DoorType::FancyDungeonExit:
      return ICON_MD_EXIT_TO_APP;
    case zelda3::DoorType::DoubleSidedShutter:
    case zelda3::DoorType::BottomSidedShutter:
    case zelda3::DoorType::TopSidedShutter:
    case zelda3::DoorType::CurtainDoor:
      return ICON_MD_VIEW_DAY;
    case zelda3::DoorType::EyeWatchDoor:
      return ICON_MD_VISIBILITY;
    case zelda3::DoorType::SmallKeyDoor:
    case zelda3::DoorType::BigKeyDoor:
      return ICON_MD_KEY;
    case zelda3::DoorType::SmallKeyStairsUp:
    case zelda3::DoorType::SmallKeyStairsDown:
      return ICON_MD_STAIRS;
    case zelda3::DoorType::DashWall:
      return ICON_MD_BOLT;
    case zelda3::DoorType::BombableDoor:
    case zelda3::DoorType::ExplodingWall:
      return ICON_MD_WHATSHOT;
    case zelda3::DoorType::WaterfallDoor:
      return ICON_MD_WATER_DROP;
    case zelda3::DoorType::ExitMarker:
      return ICON_MD_FLAG;
    case zelda3::DoorType::LayerSwapMarker:
    case zelda3::DoorType::DungeonSwapMarker:
      return ICON_MD_SWAP_HORIZ;
    default:
      return ICON_MD_DOOR_SLIDING;
  }
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

  if (ResolveCanvasViewer() &&
      canvas_viewer_->object_interaction().HasEntitySelection()) {
    const auto selected_entity =
        canvas_viewer_->object_interaction().GetSelectedEntity();
    if (selected_entity.type == EntityType::Door && rooms_ &&
        current_room_id_ >= 0 && current_room_id_ < 296) {
      const auto& room = (*rooms_)[current_room_id_];
      const auto& doors = room.GetDoors();
      if (selected_entity.index < doors.size()) {
        const auto& door = doors[selected_entity.index];
        const auto [tile_x, tile_y] = door.GetTileCoords();
        const auto [pixel_x, pixel_y] = door.GetPixelCoords();
        selected_door_type_ = door.type;

        gui::SectionHeader(ICON_MD_SELECT_ALL, "Selected Door",
                           theme.text_info);

        // Door-type combo: swap in place via DoorInteractionHandler, which
        // preserves the door's position/direction and re-encodes ROM bytes.
        const std::string current_type_name(zelda3::GetDoorTypeName(door.type));
        ImGui::SetNextItemWidth(-1);
        if (ImGui::BeginCombo("##SelectedDoorType",
                              current_type_name.c_str())) {
          for (const auto door_type : kDoorTypes) {
            const bool is_current = door.type == door_type;
            const std::string entry_label = absl::StrFormat(
                "%s  0x%02X  %s", DoorTypeIcon(door_type),
                static_cast<int>(door_type),
                std::string(zelda3::GetDoorTypeName(door_type)).c_str());
            if (ImGui::Selectable(entry_label.c_str(), is_current) &&
                !is_current) {
              canvas_viewer_->object_interaction()
                  .entity_coordinator()
                  .door_handler()
                  .MutateDoorType(selected_entity.index, door_type);
            }
            if (is_current) {
              ImGui::SetItemDefaultFocus();
            }
          }
          ImGui::EndCombo();
        }

        ImGui::TextDisabled(
            "Direction: %s  Position: 0x%02X",
            std::string(zelda3::GetDoorDirectionName(door.direction)).c_str(),
            door.position);
        ImGui::TextDisabled("Tile: (%d, %d)  Pixel: (%d, %d)", tile_x, tile_y,
                            pixel_x, pixel_y);

        // Jump-to-reciprocal navigation. Enabled when the neighbor room exists
        // and contains a door on the opposite wall at the same position.
        const int neighbor_id =
            NeighborRoomId(current_room_id_, door.direction);
        std::optional<size_t> reciprocal_index;
        if (neighbor_id >= 0 && rooms_ && neighbor_id < 296) {
          const auto opposite = OppositeDir(door.direction);
          const auto& neighbor_doors = (*rooms_)[neighbor_id].GetDoors();
          for (size_t i = 0; i < neighbor_doors.size(); ++i) {
            const auto& nd = neighbor_doors[i];
            if (nd.direction == opposite && nd.position == door.position) {
              reciprocal_index = i;
              break;
            }
          }
          // Fallback: any door on the opposite wall (handles paired doors that
          // weren't saved at matching positions).
          if (!reciprocal_index) {
            for (size_t i = 0; i < neighbor_doors.size(); ++i) {
              if (neighbor_doors[i].direction == opposite) {
                reciprocal_index = i;
                break;
              }
            }
          }
        }

        const bool jump_enabled =
            reciprocal_index.has_value() &&
            static_cast<bool>(on_jump_to_reciprocal_door_);
        if (!jump_enabled)
          ImGui::BeginDisabled();
        if (ImGui::Button(ICON_MD_ARROW_FORWARD " Jump to Reciprocal",
                          ImVec2(-1, 0))) {
          if (jump_enabled) {
            on_jump_to_reciprocal_door_(neighbor_id, *reciprocal_index);
          }
        }
        if (!jump_enabled)
          ImGui::EndDisabled();
        if (ImGui::IsItemHovered()) {
          if (neighbor_id < 0) {
            ImGui::SetTooltip("No adjacent room in this direction.");
          } else if (!reciprocal_index) {
            ImGui::SetTooltip("Room 0x%03X has no door on the opposite wall.",
                              neighbor_id);
          } else {
            ImGui::SetTooltip("Open room 0x%03X and select the paired door.",
                              neighbor_id);
          }
        }

        if (ImGui::Button(ICON_MD_PLACE " Use Selected Door Style",
                          ImVec2(-1, 0))) {
          door_placement_mode_ = true;
          canvas_viewer_->object_interaction().SetDoorPlacementMode(
              true, selected_door_type_);
        }
        ImGui::Spacing();
      }
    }
  }

  gui::SectionHeader(ICON_MD_DOOR_FRONT, "Door Styles", theme.text_info);
  ImGui::TextColored(theme.text_secondary_gray,
                     "Select a door style, then click a wall in the room "
                     "canvas to place it.");

  if (door_placement_mode_) {
    ImGui::TextColored(
        theme.status_warning, ICON_MD_PLACE " Active: %s",
        std::string(zelda3::GetDoorTypeName(selected_door_type_)).c_str());
    ImGui::SameLine();
    if (ImGui::SmallButton(ICON_MD_CANCEL " Cancel")) {
      CancelPlacement();
    }
  }

  constexpr float kDoorCardHeight = 48.0f;
  constexpr float kDoorCardSpacing = 6.0f;
  constexpr float kMinDoorCardWidth = 92.0f;
  const float panel_width = ImGui::GetContentRegionAvail().x;
  const int items_per_row =
      std::max(2, static_cast<int>((panel_width + kDoorCardSpacing) /
                                   (kMinDoorCardWidth + kDoorCardSpacing)));
  const float card_width = std::max(
      kMinDoorCardWidth,
      (panel_width - (items_per_row - 1) * kDoorCardSpacing) / items_per_row);

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
        absl::StrFormat("%s %02X\n%s", DoorTypeIcon(door_type), type_val,
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
