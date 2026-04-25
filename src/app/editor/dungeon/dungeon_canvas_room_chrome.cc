#include "dungeon_canvas_viewer.h"

#include <optional>
#include <string>

#include "absl/status/status.h"
#include "absl/strings/str_format.h"
#include "app/editor/dungeon/dungeon_project_labels.h"
#include "app/editor/dungeon/dungeon_room_selector.h"
#include "app/gui/animation/animator.h"
#include "app/gui/core/icons.h"
#include "app/gui/core/input.h"
#include "app/gui/core/layout_helpers.h"
#include "app/gui/core/style_guard.h"
#include "app/gui/core/theme_manager.h"
#include "app/gui/widgets/themed_widgets.h"
#include "imgui/imgui.h"
#include "util/log.h"
#include "util/macro.h"

namespace yaze::editor {

void DungeonCanvasViewer::DisplayObjectInfo(const gui::CanvasRuntime& rt,
                                            const zelda3::RoomObject& object,
                                            int canvas_x, int canvas_y) {
  std::string name = zelda3::GetObjectName(object.id_);
  std::string info_text;
  if (object.id_ >= 0x100) {
    info_text =
        absl::StrFormat("0x%03X %s (X:%d Y:%d S:0x%02X)", object.id_,
                        name.c_str(), object.x_, object.y_, object.size_);
  } else {
    info_text =
        absl::StrFormat("0x%02X %s (X:%d Y:%d S:0x%02X)", object.id_,
                        name.c_str(), object.x_, object.y_, object.size_);
  }

  gui::DrawText(rt, info_text, canvas_x, canvas_y - 12);
}

absl::Status DungeonCanvasViewer::LoadAndRenderRoomGraphics(int room_id) {
  LOG_DEBUG("[LoadAndRender]", "START room_id=%d", room_id);

  if (room_id < 0 || room_id >= zelda3::kNumberOfRooms) {
    LOG_DEBUG("[LoadAndRender]", "ERROR: Invalid room ID");
    return absl::InvalidArgumentError("Invalid room ID");
  }

  if (!rom_ || !rom_->is_loaded()) {
    LOG_DEBUG("[LoadAndRender]", "ERROR: ROM not loaded");
    return absl::FailedPreconditionError("ROM not loaded");
  }

  if (!rooms_) {
    LOG_DEBUG("[LoadAndRender]", "ERROR: Room data not available");
    return absl::FailedPreconditionError("Room data not available");
  }

  auto& room = (*rooms_)[room_id];
  LOG_DEBUG("[LoadAndRender]", "Got room reference");

  if (!game_data_) {
    LOG_ERROR("[LoadAndRender]", "GameData not available");
    return absl::FailedPreconditionError("GameData not available");
  }
  const auto& dungeon_main = game_data_->palette_groups.dungeon_main;
  if (!dungeon_main.empty()) {
    current_palette_group_id_ =
        static_cast<uint64_t>(room.ResolveDungeonPaletteId());

    auto full_palette = dungeon_main[current_palette_group_id_];
    ASSIGN_OR_RETURN(current_palette_group_,
                     gfx::CreatePaletteGroupFromLargePalette(full_palette, 16));
    LOG_DEBUG("[LoadAndRender]", "Palette loaded: group_id=%zu",
              current_palette_group_id_);
  }

  LOG_DEBUG("[LoadAndRender]", "Calling room.RenderRoomGraphics()...");
  room.ReloadGraphics(current_entrance_blockset_);
  LOG_DEBUG("[LoadAndRender]",
            "RenderRoomGraphics() complete - room buffers self-contained");

  LOG_DEBUG("[LoadAndRender]", "SUCCESS");
  return absl::OkStatus();
}

void DungeonCanvasViewer::DrawRoomHeader(zelda3::Room& room, int room_id) {
  ImGui::Separator();
  if (header_read_only_) {
    ImGui::BeginDisabled();
  }

  constexpr ImGuiTableFlags kPropsTableFlags =
      ImGuiTableFlags_NoPadOuterX | ImGuiTableFlags_NoBordersInBody;

  if (ImGui::BeginTable("##RoomPropsTable", 2, kPropsTableFlags)) {
    const float nav_col_width = (ImGui::GetFrameHeight() * 4.0f) +
                                (ImGui::GetStyle().ItemSpacing.x * 3.0f) +
                                (ImGui::GetStyle().FramePadding.x * 2.0f);
    ImGui::TableSetupColumn("NavCol", ImGuiTableColumnFlags_WidthFixed,
                            nav_col_width);
    ImGui::TableSetupColumn("PropsCol", ImGuiTableColumnFlags_WidthStretch);

    ImGui::TableNextRow();
    ImGui::TableNextColumn();
    DrawRoomNavigation(room_id);
    ImGui::TableNextColumn();
    DrawRoomPropertyTable(room, room_id);

    if (!compact_header_mode_ || show_room_details_) {
      ImGui::TableNextRow();
      ImGui::TableNextColumn();
      ImGui::TextDisabled(ICON_MD_SELECT_ALL " Select");
      ImGui::TableNextColumn();
      DrawLayerControls(room, room_id);
    }

    ImGui::EndTable();
  }

  if (header_read_only_) {
    ImGui::EndDisabled();
  }
}

void DungeonCanvasViewer::DrawRoomNavigation(int room_id) {
  if (!room_swap_callback_ && !room_navigation_callback_) {
    return;
  }

  auto room_if_valid = [](int candidate) -> std::optional<int> {
    if (candidate < 0 || candidate >= zelda3::kNumberOfRooms) {
      return std::nullopt;
    }
    return candidate;
  };

  const auto north =
      room_if_valid(NeighborRoomId(room_id, zelda3::DoorDirection::North));
  const auto south =
      room_if_valid(NeighborRoomId(room_id, zelda3::DoorDirection::South));
  const auto west =
      room_if_valid(NeighborRoomId(room_id, zelda3::DoorDirection::West));
  const auto east =
      room_if_valid(NeighborRoomId(room_id, zelda3::DoorDirection::East));

  auto make_tooltip = [&](const std::optional<int>& target,
                          const char* direction) -> std::string {
    if (!target.has_value()) {
      return "";
    }
    return absl::StrFormat(
        "%s: [%03X] %s", direction, *target,
        dungeon_project_labels::GetRoomLabel(project_, *target));
  };

  auto nav_button = [&](const char* id, ImGuiDir dir,
                        const std::optional<int>& target,
                        const std::string& tooltip) {
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
    if (pressed && enabled) {
      if (room_swap_callback_) {
        room_swap_callback_(room_id, *target);
      } else if (room_navigation_callback_) {
        room_navigation_callback_(*target);
      }
    }
  };

  ImGui::PushID(room_id);
  ImGui::BeginGroup();
  nav_button("##RoomNavWest", ImGuiDir_Left, west, make_tooltip(west, "West"));
  ImGui::SameLine();
  nav_button("##RoomNavNorth", ImGuiDir_Up, north,
             make_tooltip(north, "North"));
  ImGui::SameLine();
  nav_button("##RoomNavSouth", ImGuiDir_Down, south,
             make_tooltip(south, "South"));
  ImGui::SameLine();
  nav_button("##RoomNavEast", ImGuiDir_Right, east, make_tooltip(east, "East"));
  ImGui::EndGroup();
  ImGui::PopID();
}

void DungeonCanvasViewer::DrawRoomPropertyTable(zelda3::Room& room,
                                                int room_id) {
  ImGui::AlignTextToFramePadding();
  ImGui::Text(ICON_MD_TUNE " %03X", room_id);
  if (room.HasUnsavedChanges()) {
    ImGui::SameLine(0, 6);
    ImGui::TextColored(gui::ConvertColorToImVec4(
                           gui::ThemeManager::Get().GetCurrentTheme().warning),
                       ICON_MD_EDIT " Pending");
    if (ImGui::IsItemHovered()) {
      ImGui::SetTooltip(
          "Room changes are pending in the editor. Apply Room writes them to "
          "the loaded ROM buffer.");
    }
  }
  ImGui::SameLine();

  if (pin_callback_) {
    if (gui::ThemedIconButton(is_pinned_ ? ICON_MD_PUSH_PIN : ICON_MD_PIN,
                              is_pinned_ ? "Unpin Room" : "Pin Room",
                              ImVec2(0, 0), is_pinned_)) {
      pin_callback_(!is_pinned_);
    }
    ImGui::SameLine();
  }

  if (gui::ThemedIconButton(
          show_room_details_ ? ICON_MD_EXPAND_LESS : ICON_MD_EXPAND_MORE,
          show_room_details_ ? "Hide Details" : "Show Details")) {
    show_room_details_ = !show_room_details_;
  }
  ImGui::SameLine();

  auto hex_input = [&](const char* label, const char* icon, uint8_t* val,
                       uint8_t max, const char* tooltip) {
    ImGui::TextDisabled("%s", icon);
    ImGui::SameLine(0, 2);

    const std::string anim_id = std::string(label) + "_Flash";
    const ImVec4 flash_color = gui::GetAnimator().AnimateColor(
        "##RoomProps", anim_id, ImVec4(0, 0, 0, 0), 8.0f);

    if (flash_color.w > 0.01f) {
      ImGui::PushStyleColor(ImGuiCol_FrameBg, flash_color);
    }

    auto res = gui::InputHexByteEx(label, val, max, 32.f, true);
    const bool changed = res.ShouldApply();

    if (flash_color.w > 0.01f) {
      ImGui::PopStyleColor();
    }

    gui::ValueChangeFlash(changed, anim_id.c_str());

    if (changed) {
      return true;
    }
    if (ImGui::IsItemHovered()) {
      ImGui::SetTooltip("%s", tooltip);
    }
    return false;
  };

  uint8_t bs = room.blockset();
  if (hex_input("##BS", ICON_MD_VIEW_MODULE, &bs, 81, "Blockset")) {
    room.SetBlockset(bs);
    if (room.rom() && room.rom()->is_loaded()) {
      room.RenderRoomGraphics();
    }
  }
  ImGui::SameLine(0, 2);
  ImGui::TextDisabled("(%s)", DungeonRoomSelector::GetBlocksetGroupName(bs));
  ImGui::SameLine();

  uint8_t pal = room.palette();
  if (hex_input("##Pal", ICON_MD_PALETTE, &pal, 71, "Palette")) {
    room.SetPalette(pal);
    if (room.rom() && room.rom()->is_loaded()) {
      room.RenderRoomGraphics();
    }
  }
  ImGui::SameLine();

  uint8_t lyr = room.layout_id();
  if (hex_input("##Lyr", ICON_MD_GRID_VIEW, &lyr, 7, "Layout")) {
    room.SetLayoutId(lyr);
    room.MarkLayoutDirty();
    if (room.rom() && room.rom()->is_loaded()) {
      room.RenderRoomGraphics();
    }
  }
  ImGui::SameLine();

  uint8_t ss = room.spriteset();
  if (hex_input("##SS", ICON_MD_PEST_CONTROL, &ss, 143, "Spriteset")) {
    room.SetSpriteset(ss);
    if (room.rom() && room.rom()->is_loaded()) {
      room.RenderRoomGraphics();
    }
  }

  if (show_room_details_) {
    ImGui::TextDisabled("Floor: %d | Effect: %d | Tag1: %d | Tag2: %d",
                        room.floor1(), room.effect(), room.tag1(), room.tag2());
  }
}

void DungeonCanvasViewer::DrawCompactLayerToggles(int room_id) {
  if (room_id < 0 || room_id >= zelda3::kNumberOfRooms) {
    return;
  }

  const auto& theme = gui::ThemeManager::Get().GetCurrentTheme();
  const float compact_gap =
      std::max(2.0f, gui::LayoutHelpers::GetStandardSpacing() * 0.25f);
  const float compact_padding =
      std::clamp(gui::LayoutHelpers::GetButtonPadding(), 2.0f, 6.0f);

  gui::StyleVarGuard compact_style({
      {ImGuiStyleVar_FramePadding,
       ImVec2(compact_padding, compact_padding * 0.5f)},
      {ImGuiStyleVar_ItemSpacing, ImVec2(compact_gap, 0.0f)},
  });

  auto as_button_color = [](ImVec4 color, float alpha) {
    color.w = alpha;
    return color;
  };

  const ImVec4 inactive_color =
      as_button_color(gui::ConvertColorToImVec4(theme.frame_bg), 0.55f);
  const ImVec4 inactive_hover =
      as_button_color(gui::ConvertColorToImVec4(theme.frame_bg_hovered), 0.7f);
  const ImVec4 inactive_active =
      as_button_color(gui::ConvertColorToImVec4(theme.frame_bg_active), 0.85f);

  auto draw_toggle = [&](const char* label, bool enabled, ImVec4 active_color,
                         const char* tooltip, auto&& on_toggle) {
    const ImVec4 button = enabled ? active_color : inactive_color;
    const ImVec4 hovered =
        enabled ? as_button_color(
                      gui::ConvertColorToImVec4(theme.button_hovered), 0.95f)
                : inactive_hover;
    const ImVec4 pressed =
        enabled ? as_button_color(
                      gui::ConvertColorToImVec4(theme.button_active), 1.0f)
                : inactive_active;

    gui::StyleColorGuard button_colors({
        {ImGuiCol_Button, button},
        {ImGuiCol_ButtonHovered, hovered},
        {ImGuiCol_ButtonActive, pressed},
    });

    if (ImGui::SmallButton(label)) {
      on_toggle();
    }
    if (ImGui::IsItemHovered()) {
      ImGui::SetTooltip("%s", tooltip);
    }
  };

  const bool bg1_visible = IsBG1Visible(room_id);
  draw_toggle("BG1##LayerToggleBG1", bg1_visible,
              as_button_color(gui::ConvertColorToImVec4(theme.info), 0.9f),
              "Toggle BG1 (main layer) visibility",
              [&]() { SetBG1Visible(room_id, !bg1_visible); });

  ImGui::SameLine();
  const bool bg2_visible = IsBG2Visible(room_id);
  draw_toggle("BG2##LayerToggleBG2", bg2_visible,
              as_button_color(gui::ConvertColorToImVec4(theme.warning), 0.9f),
              "Toggle BG2 (overlay layer) visibility",
              [&]() { SetBG2Visible(room_id, !bg2_visible); });

  ImGui::SameLine();
  const bool sprites_visible = entity_visibility_.show_sprites;
  draw_toggle(ICON_MD_PEST_CONTROL "##LayerToggleSprites", sprites_visible,
              as_button_color(gui::ConvertColorToImVec4(theme.success), 0.9f),
              "Toggle sprite visibility", [&]() {
                entity_visibility_.show_sprites =
                    !entity_visibility_.show_sprites;
              });

  ImGui::SameLine();
  draw_toggle(ICON_MD_GRID_ON "##LayerToggleGrid", show_grid_,
              as_button_color(gui::ConvertColorToImVec4(theme.secondary), 0.9f),
              "Toggle grid overlay", [&]() { show_grid_ = !show_grid_; });

  ImGui::SameLine();
  draw_toggle(
      ICON_MD_CROP_FREE "##LayerToggleBounds", show_object_bounds_,
      as_button_color(gui::ConvertColorToImVec4(theme.selection_primary), 0.9f),
      "Toggle object bounds overlay",
      [&]() { show_object_bounds_ = !show_object_bounds_; });

  ImGui::SameLine();
  const bool pots_visible = entity_visibility_.show_pot_items;
  draw_toggle(ICON_MD_INVENTORY_2 "##LayerTogglePots", pots_visible,
              as_button_color(gui::ConvertColorToImVec4(theme.success), 0.9f),
              "Toggle pot item markers", [&]() {
                entity_visibility_.show_pot_items =
                    !entity_visibility_.show_pot_items;
              });

  ImGui::SameLine();
  draw_toggle(ICON_MD_FILTER_CENTER_FOCUS "##LayerToggleCollision",
              show_custom_collision_overlay_,
              as_button_color(gui::ConvertColorToImVec4(theme.warning), 0.9f),
              "Toggle custom collision overlay", [&]() {
                show_custom_collision_overlay_ =
                    !show_custom_collision_overlay_;
              });
}

void DungeonCanvasViewer::DrawLayerControls(zelda3::Room& /*room*/,
                                            int room_id) {
  auto& interaction = object_interaction_;

  interaction.SetLayersMerged(GetRoomLayerManager(room_id).AreLayersMerged());
  int current_filter = interaction.GetLayerFilter();

  auto radio = [&](const char* label, int filter) {
    if (ImGui::RadioButton(label, current_filter == filter)) {
      interaction.SetLayerFilter(filter);
    }
    ImGui::SameLine();
  };

  radio("All", ObjectSelection::kLayerAll);
  radio("L1", ObjectSelection::kLayer1);
  radio("L2", ObjectSelection::kLayer2);
  radio("L3", ObjectSelection::kLayer3);
}

}  // namespace yaze::editor
