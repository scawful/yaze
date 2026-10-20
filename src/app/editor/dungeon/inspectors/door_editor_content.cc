#include "app/editor/dungeon/inspectors/door_editor_content.h"
#include "util/i18n/tr.h"

#include <algorithm>
#include <string>

#include "absl/strings/str_format.h"
#include "app/editor/agent/agent_ui_theme.h"
#include "app/editor/dungeon/dungeon_rendering_helpers.h"
#include "app/gfx/render/background_buffer.h"
#include "app/gui/automation/widget_auto_register.h"
#include "app/gui/core/input.h"
#include "app/gui/core/style_guard.h"
#include "app/gui/core/ui_helpers.h"
#include "imgui/imgui.h"
#include "rom/rom.h"
#include "zelda3/dungeon/object_drawer.h"
#include "zelda3/dungeon/room.h"

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

// Semantic icon prefix used by both the placement picker and the "Selected
// Door" swap combo. Keeps supported types scannable by category.
const char* DoorTypeIcon(zelda3::DoorType type) {
  switch (type) {
    case zelda3::DoorType::NormalDoor:
    case zelda3::DoorType::NormalDoorLower:
    case zelda3::DoorType::NormalDoorOneSidedShutter:
    case zelda3::DoorType::ExplicitRoomDoor:
      return ICON_MD_DOOR_FRONT;
    case zelda3::DoorType::ExitLower:
    case zelda3::DoorType::CaveExit:
    case zelda3::DoorType::FancyDungeonExit:
    case zelda3::DoorType::FancyDungeonExitLower:
    case zelda3::DoorType::LitCaveExitLower:
      return ICON_MD_EXIT_TO_APP;
    case zelda3::DoorType::DoubleSidedShutter:
    case zelda3::DoorType::DoubleSidedShutterLower:
    case zelda3::DoorType::BottomSidedShutter:
    case zelda3::DoorType::BottomShutterLower:
    case zelda3::DoorType::TopSidedShutter:
    case zelda3::DoorType::TopShutterLower:
    case zelda3::DoorType::CurtainDoor:
      return ICON_MD_VIEW_DAY;
    case zelda3::DoorType::EyeWatchDoor:
      return ICON_MD_VISIBILITY;
    case zelda3::DoorType::SmallKeyDoor:
    case zelda3::DoorType::BigKeyDoor:
    case zelda3::DoorType::UnopenableBigKeyDoor:
      return ICON_MD_KEY;
    case zelda3::DoorType::SmallKeyStairsUp:
    case zelda3::DoorType::SmallKeyStairsDown:
    case zelda3::DoorType::SmallKeyStairsUpLower:
    case zelda3::DoorType::SmallKeyStairsDownLower:
      return ICON_MD_STAIRS;
    case zelda3::DoorType::DashWall:
      return ICON_MD_BOLT;
    case zelda3::DoorType::BombableDoor:
    case zelda3::DoorType::BombableCaveExit:
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
    case zelda3::DoorType::ExitLower:
      return "Exit Low";
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
    case zelda3::DoorType::SmallKeyStairsUpLower:
      return "Up Low";
    case zelda3::DoorType::SmallKeyStairsDownLower:
      return "Down Low";
    case zelda3::DoorType::DashWall:
      return "Dash";
    case zelda3::DoorType::BombableDoor:
      return "Bomb";
    case zelda3::DoorType::BombableCaveExit:
      return "Bomb Exit";
    case zelda3::DoorType::UnopenableBigKeyDoor:
      return "B-Key Lock";
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
    case zelda3::DoorType::FancyDungeonExitLower:
      return "Exit Low";
    case zelda3::DoorType::LitCaveExitLower:
      return "Lit Low";
    case zelda3::DoorType::WaterfallDoor:
      return "Water";
    case zelda3::DoorType::ExitMarker:
      return "Marker";
    case zelda3::DoorType::LayerSwapMarker:
      return "Layer";
    case zelda3::DoorType::DungeonSwapMarker:
      return "Swap";
    case zelda3::DoorType::NormalDoorOneSidedShutter:
      return "1-Side";
    case zelda3::DoorType::DoubleSidedShutterLower:
      return "2-Side L";
    case zelda3::DoorType::ExplicitRoomDoor:
      return "Explicit";
    case zelda3::DoorType::BottomShutterLower:
      return "Bottom L";
    case zelda3::DoorType::TopShutterLower:
      return "Top L";
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

void DoorEditorContent::Draw(bool* /*p_open*/) {
  DrawContent(true);
}

void DoorEditorContent::DrawInWorkbench() {
  DrawContent(false);
}

bool DoorEditorContent::HasMatchingViewer() const {
  if (!canvas_viewer_ || !rooms_ || canvas_viewer_->rooms() != rooms_ ||
      current_room_id_ < 0 ||
      current_room_id_ >= static_cast<int>(rooms_->size()) ||
      canvas_viewer_->current_room_id() != current_room_id_) {
    return false;
  }
  const auto* context = canvas_viewer_->object_interaction()
                            .entity_coordinator()
                            .tile_handler()
                            .context();
  return context && context->current_room_id == current_room_id_;
}

bool DoorEditorContent::CanEditRoom() const {
  return HasMatchingViewer() && canvas_viewer_->IsObjectInteractionEnabled() &&
         !canvas_viewer_->header_read_only();
}

const DoorEditorContent::CachedDoorPreview&
DoorEditorContent::GetOrCreateDoorPreview(zelda3::DoorType type,
                                          const zelda3::Room& room) {
  if (cached_door_graphics_revision_ != room.graphics_revision()) {
    door_preview_cache_.clear();
    cached_door_graphics_revision_ = room.graphics_revision();
  }
  const int type_key = static_cast<int>(type);
  auto it = door_preview_cache_.find(type_key);
  if (it != door_preview_cache_.end()) {
    return it->second;
  }

  CachedDoorPreview cached;
  Rom* rom = canvas_viewer_ ? canvas_viewer_->rom() : nullptr;
  const auto& gfx_buffer = room.get_gfx_buffer();
  if (rom && rom->is_loaded() && !gfx_buffer.empty()) {
    zelda3::ObjectDrawer drawer(rom, current_room_id_, gfx_buffer.data());
    gfx::BackgroundBuffer bg1(512, 512);
    gfx::BackgroundBuffer bg2(512, 512);
    bg1.EnsureBitmapInitialized();
    bg2.EnsureBitmapInitialized();

    zelda3::ObjectDrawer::DoorDef door_def;
    door_def.type = type;
    door_def.direction = zelda3::DoorDirection::North;
    door_def.position = 0;

    drawer.DrawDoor(door_def, 0, bg1, bg2, nullptr, nullptr, nullptr);

    auto [tile_x, tile_y] = door_def.GetTileCoords();
    auto dims = door_def.GetDimensions();
    const int start_x = tile_x * 8;
    const int start_y = tile_y * 8;
    const int w = std::max(1, dims.width_tiles * 8);
    const int h = std::max(1, dims.height_tiles * 8);

    cached.width = w;
    cached.height = h;
    cached.pixels.assign(w * h, 255);

    const auto& bg1_bmp = bg1.bitmap();
    const auto& bg2_bmp = bg2.bitmap();
    const auto& bg1_data = bg1_bmp.vector();
    const auto& bg2_data = bg2_bmp.vector();

    for (int y = 0; y < h; ++y) {
      for (int x = 0; x < w; ++x) {
        const int bmp_x = start_x + x;
        const int bmp_y = start_y + y;
        if (bmp_x >= 0 && bmp_x < 512 && bmp_y >= 0 && bmp_y < 512) {
          const int idx = bmp_y * 512 + bmp_x;
          uint8_t px = 255;
          if (idx < static_cast<int>(bg1_data.size()) && bg1_data[idx] != 255) {
            px = bg1_data[idx];
          } else if (idx < static_cast<int>(bg2_data.size()) &&
                     bg2_data[idx] != 255) {
            px = bg2_data[idx];
          }
          cached.pixels[y * w + x] = px;
        }
      }
    }
  }

  auto [inserted_it, _] =
      door_preview_cache_.emplace(type_key, std::move(cached));
  return inserted_it->second;
}

void DoorEditorContent::DrawContent(bool show_room_doors) {
  ResolveCanvasViewer();
  const bool can_edit = CanEditRoom();
  door_placement_mode_ =
      HasMatchingViewer() &&
      canvas_viewer_->object_interaction().IsDoorPlacementActive();
  if (door_placement_mode_) {
    selected_door_type_ = canvas_viewer_->object_interaction()
                              .entity_coordinator()
                              .door_handler()
                              .GetDoorType();
  }
  gui::AutoWidgetScope automation_scope("Dungeon/DoorEditor");

  const auto& theme = AgentUI::GetTheme();
  static constexpr auto kDoorTypes = zelda3::GetPlaceableDoorTypes();

  const zelda3::Room* active_room =
      (rooms_ && current_room_id_ >= 0 &&
       current_room_id_ < static_cast<int>(rooms_->size()))
          ? &(*rooms_)[current_room_id_]
          : nullptr;

  std::array<SDL_Color, 256> color_table{};
  if (active_room) {
    const auto& pal = active_room->rendered_palette();
    for (size_t p_idx = 0; p_idx < std::min<size_t>(pal.size(), 256); ++p_idx) {
      color_table[p_idx] = pal[p_idx];
    }
  }

  if (show_room_doors && HasMatchingViewer() &&
      canvas_viewer_->object_interaction().HasEntitySelection() &&
      canvas_viewer_->object_interaction().GetSelectedEntity().type ==
          EntityType::Door) {
    gui::SectionHeader(ICON_MD_SELECT_ALL, "Selected Door", theme.text_info);
    if (open_selection_inspector_callback_ &&
        ImGui::Button(ICON_MD_OPEN_IN_NEW " Inspect Selected", ImVec2(-1, 0))) {
      open_selection_inspector_callback_();
    }
    ImGui::Spacing();
  }

  gui::SectionHeader(ICON_MD_DOOR_FRONT, "Door Styles", theme.text_info);
  ImGui::TextWrapped("Select a door style, then click a wall to place it.");

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
  constexpr float kMinDoorCardWidth = 94.0f;

  const float grid_height =
      show_room_doors ? 150.0f
                      : std::max(1.0f, ImGui::GetContentRegionAvail().y);
  ImGui::BeginChild("##DoorTypeGrid", ImVec2(0, grid_height), true,
                    ImGuiWindowFlags_AlwaysVerticalScrollbar);
  ImGui::BeginDisabled(!can_edit);
  const float panel_width = ImGui::GetContentRegionAvail().x;
  const int items_per_row =
      std::max(1, static_cast<int>((panel_width + kDoorCardSpacing) /
                                   (kMinDoorCardWidth + kDoorCardSpacing)));
  const float card_width = std::max(
      kMinDoorCardWidth,
      (panel_width - (items_per_row - 1) * kDoorCardSpacing) / items_per_row);

  ImDrawList* draw_list = ImGui::GetWindowDrawList();

  int col = 0;
  for (size_t i = 0; i < kDoorTypes.size(); ++i) {
    const auto door_type = kDoorTypes[i];
    const bool is_selected = selected_door_type_ == door_type;
    const int type_val = static_cast<int>(door_type);
    ImGui::PushID(static_cast<int>(i));

    const ImVec2 card_pos = ImGui::GetCursorScreenPos();

    gui::StyleColorGuard btn_colors({
        {ImGuiCol_Button,
         is_selected ? theme.selection_pulsing : theme.panel_bg_darker},
    });

    const bool picked =
        ImGui::Button("##door_card", ImVec2(card_width, kDoorCardHeight));
    gui::AutoRegisterLastItem("button",
                              absl::StrFormat("place_door_%02X", type_val));
    if (picked && can_edit) {
      selected_door_type_ = door_type;
      door_placement_mode_ = true;
      if (ResolveCanvasViewer()) {
        canvas_viewer_->object_interaction().SetDoorPlacementMode(
            true, selected_door_type_);
      }
    }

    const ImVec2 preview_box_min(card_pos.x + 4.0f, card_pos.y + 4.0f);
    const ImVec2 preview_box_max(card_pos.x + 36.0f,
                                 card_pos.y + kDoorCardHeight - 4.0f);
    bool drew = false;
    if (active_room) {
      const auto& preview = GetOrCreateDoorPreview(door_type, *active_room);
      SDL_Rect bounds{0, 0, preview.width, preview.height};
      if (bounds.w > 0 && bounds.h > 0 && !preview.pixels.empty()) {
        drew = DungeonRenderingHelpers::DrawSpritePreview(
            draw_list, preview.pixels, bounds, preview_box_min, preview_box_max,
            color_table,
            DungeonRenderingHelpers::kBackgroundBufferTransparentIndex);
      }
    }
    if (!drew) {
      draw_list->AddRect(preview_box_min, preview_box_max,
                         ImGui::GetColorU32(theme.status_inactive), 2.0f);
      draw_list->AddText(
          ImVec2(preview_box_min.x + 7.0f, preview_box_min.y + 8.0f),
          ImGui::GetColorU32(theme.status_inactive), DoorTypeIcon(door_type));
    }

    const std::string id_text = absl::StrFormat("%02X", type_val);
    const std::string name_text = ShortDoorTypeLabel(door_type);
    draw_list->AddText(ImVec2(card_pos.x + 40.0f, card_pos.y + 6.0f),
                       ImGui::GetColorU32(theme.text_info), id_text.c_str());
    draw_list->AddText(
        ImVec2(card_pos.x + 40.0f, card_pos.y + 24.0f),
        ImGui::GetColorU32(is_selected ? theme.text_primary
                                       : theme.text_secondary_gray),
        name_text.c_str());

    if (is_selected) {
      draw_list->AddRect(
          card_pos,
          ImVec2(card_pos.x + card_width, card_pos.y + kDoorCardHeight),
          ImGui::GetColorU32(theme.selection_primary), 4.0f, 0, 2.0f);
    }

    if (ImGui::IsItemHovered()) {
      ImGui::SetTooltip(tr("%s (0x%02X)\nClick to select for placement"),
                        std::string(zelda3::GetDoorTypeName(door_type)).c_str(),
                        type_val);
    }

    ImGui::PopID();
    col++;
    if (col < items_per_row && i < kDoorTypes.size() - 1) {
      ImGui::SameLine(0, kDoorCardSpacing);
    } else {
      col = 0;
    }
  }

  ImGui::EndDisabled();
  ImGui::EndChild();

  if (!show_room_doors || !rooms_ || current_room_id_ < 0 ||
      current_room_id_ >= static_cast<int>(rooms_->size())) {
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

  int selected_door_index = -1;
  if (HasMatchingViewer() &&
      canvas_viewer_->object_interaction().HasEntitySelection()) {
    const auto selected_entity =
        canvas_viewer_->object_interaction().GetSelectedEntity();
    if (selected_entity.type == EntityType::Door) {
      selected_door_index = static_cast<int>(selected_entity.index);
    }
  }

  if (selected_door_index >= 0 &&
      selected_door_index < static_cast<int>(doors.size())) {
    ImGui::BeginDisabled(!can_edit);
    uint8_t raw_type = static_cast<uint8_t>(doors[selected_door_index].type);
    ImGui::AlignTextToFramePadding();
    ImGui::TextUnformatted(tr("Selected raw type"));
    ImGui::SameLine();
    ImGui::SetNextItemWidth(72.0f);
    const bool type_changed = gui::InputScalarDeferred(
        "##SelectedDoorRawType", ImGuiDataType_U8, &raw_type, "%02X",
        ImGuiInputTextFlags_CharsHexadecimal,
        {reinterpret_cast<uintptr_t>(rooms_),
         static_cast<uint64_t>(current_room_id_),
         static_cast<uint64_t>(selected_door_index)});
    gui::AutoRegisterLastItem("input_scalar", "selected_door_raw_type",
                              "Selected door raw type byte");
    ImGui::EndDisabled();
    if (type_changed && can_edit) {
      canvas_viewer_->object_interaction()
          .entity_coordinator()
          .door_handler()
          .MutateDoorType(selected_door_index,
                          static_cast<zelda3::DoorType>(raw_type));
    }
  }

  ImGui::BeginChild("##DoorList", ImVec2(0, 0), true);
  for (size_t i = 0; i < doors.size(); ++i) {
    const auto& door = doors[i];
    const auto [tile_x, tile_y] = door.GetTileCoords();
    const std::string type_name(zelda3::GetDoorTypeName(door.type));
    const std::string dir_name(zelda3::GetDoorDirectionName(door.direction));

    ImGui::PushID(static_cast<int>(i));
    const std::string row_label = absl::StrFormat("[%zu] %s", i, type_name);
    const bool row_selected = ImGui::Selectable(
        row_label.c_str(), selected_door_index == static_cast<int>(i));
    gui::AutoRegisterLastItem("selectable", absl::StrFormat("door_row_%zu", i),
                              "Select a door in the current dungeon room");
    if (row_selected) {
      if (HasMatchingViewer()) {
        canvas_viewer_->object_interaction().SelectEntity(EntityType::Door, i);
      }
    }
    ImGui::SameLine();
    ImGui::TextColored(theme.text_secondary_gray, "(%s @ %d,%d)",
                       dir_name.c_str(), tile_x, tile_y);
    ImGui::PopID();
  }
  ImGui::EndChild();
}

}  // namespace yaze::editor
