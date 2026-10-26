#include "app/editor/dungeon/inspectors/dungeon_destination_editor.h"
#include "dungeon_canvas_viewer.h"
#include "util/i18n/tr.h"

#include <algorithm>
#include <cstdint>
#include <initializer_list>
#include <optional>
#include <string>

#include "absl/status/status.h"
#include "absl/strings/str_format.h"
#include "app/editor/dungeon/dungeon_overlay_controls.h"
#include "app/editor/dungeon/dungeon_project_labels.h"
#include "app/editor/dungeon/dungeon_room_edit.h"
#include "app/editor/dungeon/dungeon_room_property_controls.h"
#include "app/editor/dungeon/dungeon_room_selector.h"
#include "app/editor/dungeon/inspectors/dungeon_chest_editor.h"
#include "app/gui/animation/animator.h"
#include "app/gui/automation/widget_auto_register.h"
#include "app/gui/core/icons.h"
#include "app/gui/core/input.h"
#include "app/gui/core/layout_helpers.h"
#include "app/gui/core/style_guard.h"
#include "app/gui/core/theme_manager.h"
#include "app/gui/core/ui_config.h"
#include "app/gui/widgets/themed_widgets.h"
#include "imgui/imgui.h"
#include "util/log.h"
#include "util/macro.h"
#include "zelda3/dungeon/dungeon_rom_addresses.h"
#include "zelda3/zelda3_labels.h"

namespace yaze::editor {

namespace {

const char* GetBg2ModeName(int value) {
  static constexpr const char* kNames[] = {
      "Off",      "Parallax", "Dark",        "On top",   "Translucent",
      "Addition", "Normal",   "Transparent", "Dark room"};
  constexpr int kNameCount = sizeof(kNames) / sizeof(kNames[0]);
  return (value >= 0 && value < kNameCount) ? kNames[value] : "Unknown";
}

const char* GetCollisionName(int value) {
  static constexpr const char* kNames[] = {"One", "Both", "Both + Scroll",
                                           "Moving Floor", "Moving Water"};
  constexpr int kNameCount = sizeof(kNames) / sizeof(kNames[0]);
  return (value >= 0 && value < kNameCount) ? kNames[value] : "Unknown";
}

std::string GetLayer2ModeName(int value) {
  // ZScream distinguishes the reserved mode 1 from Nothing (mode 0).
  if (value == 1)
    return "One";
  return value >= 0 && value < 8 ? zelda3::RoomEffect[value] : "Unknown";
}

}  // namespace

void DrawDungeonRoomPropertyFields(DungeonCanvasViewer& viewer,
                                   zelda3::Room& room, int room_id,
                                   RoomPropertyLayout layout) {
  auto* rooms = viewer.rooms();
  if (!rooms || room_id < 0 || room_id >= static_cast<int>(rooms->size()))
    return;
  const bool compact = layout == RoomPropertyLayout::kCompact;
  const bool stack_choices =
      ImGui::GetContentRegionAvail().x < ImGui::GetFontSize() * 25.0f;
  const ImGuiID error_id =
      ImGui::GetID(absl::StrFormat("RoomMetadataError/%d", room_id).c_str());
  // Combo selections run inside a popup window. Keep errors in the host pane.
  auto* inspector_storage = ImGui::GetStateStorage();
  const gui::InputScalarTargetIdentity identity{
      reinterpret_cast<uintptr_t>(rooms), static_cast<uint64_t>(room_id)};
  auto apply = [&](RoomMetadataField field, int value, int index = 0) {
    const auto status = viewer.EditRoomMetadata(
        room_id, {.field = field, .value = value, .index = index});
    inspector_storage->SetBool(error_id, !status.ok());
  };
  auto begin_properties = [](const char* id, float label_width = 104.0f) {
    if (!ImGui::BeginTable(
            id, 2, ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingStretchProp)) {
      return false;
    }
    ImGui::TableSetupColumn("Property", ImGuiTableColumnFlags_WidthFixed,
                            label_width);
    ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthStretch, 1.0f);
    return true;
  };
  auto property_row = [](const char* label) {
    // Advance by one label/value pair; four-column tables pair short fields.
    ImGui::TableNextColumn();
    ImGui::AlignTextToFramePadding();
    ImGui::TextUnformatted(label);
    ImGui::TableNextColumn();
    ImGui::SetNextItemWidth(-1);
  };
  auto draw_hex = [&](const char* label, const char* id, int value,
                      int max_value, RoomMetadataField field, int index = 0) {
    property_row(label);
    auto target = identity;
    target.element = static_cast<uint64_t>(field);
    target.component = static_cast<uint64_t>(index);
    if (gui::InputScalarDeferred(
            id, ImGuiDataType_S32, &value, max_value > 0xFF ? "%03X" : "%02X",
            ImGuiInputTextFlags_CharsHexadecimal, target)) {
      apply(field, value, index);
    }
    {
      gui::AutoWidgetScope automation_scope(compact ? "Dungeon/RoomProperties"
                                                    : "Dungeon/Workbench");
      gui::AutoRegisterLastItem("input_hex", id + 2, label);
    }
    if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) {
      const char* description = label;
      if (field == RoomMetadataField::kSpriteset)
        description = "Sprite graphics set";
      else if (field == RoomMetadataField::kFloor1)
        description = "Background 1 floor pattern";
      else if (field == RoomMetadataField::kFloor2)
        description = "Background 2 floor pattern";
      ImGui::SetTooltip("%s (hex 00-%X). Enter or leave the field to apply.",
                        description, max_value);
    }
  };
  auto draw_choice = [&](const char* label, const char* id, int value,
                         int count, RoomMetadataField field, auto name) {
    if (stack_choices) {
      ImGui::TextUnformatted(label);
      ImGui::SetNextItemWidth(-1);
    } else {
      property_row(label);
    }
    const std::string preview = name(value);
    const bool open = ImGui::BeginCombo(id, preview.c_str());
    {
      gui::AutoWidgetScope automation_scope(compact ? "Dungeon/RoomProperties"
                                                    : "Dungeon/Workbench");
      gui::AutoRegisterLastItem("combo", id + 2, label);
    }
    if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) {
      ImGui::SetTooltip("%s: %s (0x%02X)", label, preview.c_str(), value);
    }
    if (open) {
      for (int option = 0; option < count; ++option) {
        const std::string option_label = name(option);
        ImGui::PushID(option);
        if (ImGui::Selectable(option_label.c_str(), value == option)) {
          apply(field, option);
        }
        {
          gui::AutoWidgetScope automation_scope(
              compact ? "Dungeon/RoomProperties" : "Dungeon/Workbench");
          gui::AutoRegisterLastItem("selectable",
                                    absl::StrFormat("%s/%d", id + 2, option));
        }
        if (value == option) {
          ImGui::SetItemDefaultFocus();
        }
        if (ImGui::IsItemHovered())
          ImGui::SetTooltip("%s (0x%02X)", option_label.c_str(), option);
        ImGui::PopID();
      }
      ImGui::EndCombo();
    }
  };
  auto choice_group = [&](const char* id, float label_width, auto draw) {
    if (stack_choices) {
      draw();
    } else if (begin_properties(id, label_width)) {
      draw();
      ImGui::EndTable();
    }
  };
  auto label_width = [](std::initializer_list<const char*> labels) {
    float width = 0.0f;
    for (const auto* label : labels)
      width = std::max(width, ImGui::CalcTextSize(label).x);
    return width;
  };

  ImGui::BeginDisabled(viewer.header_read_only() ||
                       !viewer.IsObjectInteractionEnabled());
  if (compact) {
    if (ImGui::BeginTable("##CompactRoomGraphics", 4,
                          ImGuiTableFlags_SizingStretchProp)) {
      ImGui::TableSetupColumn("Property 1", ImGuiTableColumnFlags_WidthFixed,
                              label_width({"Layout", "Palette"}));
      ImGui::TableSetupColumn("Value 1", ImGuiTableColumnFlags_WidthStretch);
      ImGui::TableSetupColumn("Property 2", ImGuiTableColumnFlags_WidthFixed,
                              label_width({"Blockset", "Spriteset"}));
      ImGui::TableSetupColumn("Value 2", ImGuiTableColumnFlags_WidthStretch);
      draw_hex("Layout", "##Lyr", room.layout_id(), 0x07,
               RoomMetadataField::kLayout);
      draw_hex("Blockset", "##BS", room.blockset(), 0x51,
               RoomMetadataField::kBlockset);
      draw_hex("Palette", "##Pal", room.palette(), 0x47,
               RoomMetadataField::kPalette);
      draw_hex("Spriteset", "##SS", room.spriteset(),
               zelda3::kMaxDungeonSpriteset, RoomMetadataField::kSpriteset);
      ImGui::EndTable();
    }
    ImGui::EndDisabled();
    if (inspector_storage->GetBool(error_id))
      ImGui::TextWrapped(
          "Edit not applied. Hover the field for its supported range.");
    return;
  }

  const bool show_all = (layout == RoomPropertyLayout::kFull);
  const bool show_core =
      show_all || (layout == RoomPropertyLayout::kCoreGraphics);
  const bool show_mechanics =
      show_all || (layout == RoomPropertyLayout::kMechanics);
  const bool show_tags =
      show_all || (layout == RoomPropertyLayout::kTags) || show_mechanics;

  if (show_tags) {
    choice_group("##WorkbenchRoomTags", label_width({"Tag 1", "Tag 2"}), [&] {
      const auto tag_name = [&](int value) {
        return dungeon_project_labels::GetRoomTagLabel(viewer.project(), value);
      };
      for (int index = 0; index < 2; ++index) {
        const int tag = index == 0 ? room.tag1() : room.tag2();
        draw_choice(
            index == 0 ? "Tag 1" : "Tag 2",
            index == 0 ? "##RoomHeaderTag1" : "##RoomHeaderTag2", tag, 0x40,
            index == 0 ? RoomMetadataField::kTag1 : RoomMetadataField::kTag2,
            tag_name);
      }
    });
  }

  if (inspector_storage->GetBool(error_id)) {
    ImGui::TextWrapped(
        "Edit not applied. Check that this room is editable and the value "
        "is within the field's supported range. Hover the field for its "
        "range.");
  }

  if (show_core) {
    if (show_all) {
      ImGui::SeparatorText("Room properties");
    }
    if (ImGui::BeginTable(
            "##WorkbenchRoomGraphics", 4,
            ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingStretchProp)) {
      ImGui::TableSetupColumn("Property 1", ImGuiTableColumnFlags_WidthFixed,
                              label_width({"Layout", "Floor 1", "Palette"}));
      ImGui::TableSetupColumn("Value 1", ImGuiTableColumnFlags_WidthStretch,
                              1.0f);
      ImGui::TableSetupColumn(
          "Property 2", ImGuiTableColumnFlags_WidthFixed,
          label_width({"Blockset", "Floor 2", "Spriteset"}));
      ImGui::TableSetupColumn("Value 2", ImGuiTableColumnFlags_WidthStretch,
                              1.0f);
      draw_hex("Layout", "##RoomHeaderLayout", room.layout_id(), 0x07,
               RoomMetadataField::kLayout);
      draw_hex("Blockset", "##RoomHeaderBlockset", room.blockset(), 0x51,
               RoomMetadataField::kBlockset);
      draw_hex("Floor 1", "##RoomHeaderFloor1", room.floor1(), 0x0F,
               RoomMetadataField::kFloor1);
      draw_hex("Floor 2", "##RoomHeaderFloor2", room.floor2(), 0x0F,
               RoomMetadataField::kFloor2);
      draw_hex("Palette", "##RoomHeaderPalette", room.palette(), 0x47,
               RoomMetadataField::kPalette);
      draw_hex("Spriteset", "##RoomHeaderSpriteset", room.spriteset(),
               zelda3::kMaxDungeonSpriteset, RoomMetadataField::kSpriteset);
      ImGui::EndTable();
    }
  }

  if (show_mechanics) {
    if (begin_properties("##WorkbenchRoomMessage", label_width({"Message"}))) {
      draw_hex("Message", "##RoomHeaderMessage", room.message_id(), 0x0FFF,
               RoomMetadataField::kMessage);
      ImGui::EndTable();
    }
    choice_group(
        "##WorkbenchRoomHeader",
        label_width({"Layer 2 type", "Layer 2 mode", "Collision"}), [&] {
          draw_choice("Layer 2 type", "##RoomHeaderBg2",
                      static_cast<int>(room.bg2()), 9, RoomMetadataField::kBg2,
                      GetBg2ModeName);
          draw_choice("Layer 2 mode", "##RoomHeaderEffect", room.effect(), 8,
                      RoomMetadataField::kEffect, GetLayer2ModeName);
          draw_choice("Collision", "##RoomHeaderCollision", room.collision(), 5,
                      RoomMetadataField::kCollision, GetCollisionName);
        });
  }
  ImGui::EndDisabled();
}

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

  auto* room_ptr = rooms_->TryEnsureRoom(room_id);
  if (!room_ptr) {
    LOG_DEBUG("[LoadAndRender]", "ERROR: Room data unavailable");
    return absl::InvalidArgumentError("Invalid room ID");
  }
  auto& room = *room_ptr;
  LOG_DEBUG("[LoadAndRender]", "Got room reference");

  if (!game_data_) {
    LOG_ERROR("[LoadAndRender]", "GameData not available");
    return absl::FailedPreconditionError("GameData not available");
  }
  const auto& dungeon_main = game_data_->palette_groups.dungeon_main;
  if (!dungeon_main.empty()) {
    current_palette_group_id_ =
        static_cast<uint64_t>(room.ResolveDungeonPaletteId());

    const auto& full_palette = dungeon_main[current_palette_group_id_];
    current_palette_group_ = zelda3::BuildDungeonRenderPaletteGroupFromGameData(
        full_palette, game_data_);
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
  if ((!compact_header_mode_ || show_room_details_) &&
      ImGui::CollapsingHeader("Destinations##RoomDestinations")) {
    DrawDungeonDestinationEditor(*this);
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
      ImGui::SetTooltip(tr(
          "Room changes are pending in the editor. Apply Room writes them to "
          "the loaded ROM buffer."));
    }
  }
  ImGui::SameLine();

  if (pin_callback_) {
    if (gui::ThemedIconButton(ICON_MD_PUSH_PIN,
                              is_pinned_ ? "Unpin Room" : "Pin Room",
                              gui::ScaledSize(30.0f, 30.0f), is_pinned_)) {
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
  DrawRecentRoomBreadcrumbs(room_id);

  DrawDungeonRoomPropertyFields(*this, room, room_id,
                                RoomPropertyLayout::kCompact);

  if (show_room_details_) {
    ImGui::TextDisabled(tr("Floor: %d | Effect: %d | Tag1: %d | Tag2: %d"),
                        room.floor1(), room.effect(), room.tag1(), room.tag2());
    if (ImGui::CollapsingHeader(tr("Chest contents"))) {
      DrawDungeonChestEditor(room_id, room, *this);
    }
  }
}

void DungeonCanvasViewer::DrawRecentRoomBreadcrumbs(int room_id) {
  if (!CanNavigateRooms() || recently_visited_rooms_.size() <= 1) {
    return;
  }

  ImGui::PushID("RecentRooms");
  ImGui::TextDisabled(ICON_MD_HISTORY);
  if (ImGui::IsItemHovered()) {
    ImGui::SetTooltip(tr("Recently visited rooms"));
  }

  int rendered = 0;
  for (int recent_room : recently_visited_rooms_) {
    if (recent_room == room_id) {
      continue;
    }
    ImGui::SameLine(0, 3);
    const std::string button_label =
        absl::StrFormat("%03X##RecentRoom%d", recent_room, rendered);
    if (ImGui::SmallButton(button_label.c_str())) {
      NavigateToRoom(recent_room);
      TriggerChangePing();
    }
    if (ImGui::IsItemHovered()) {
      ImGui::SetTooltip(
          "[%03X] %s", recent_room,
          dungeon_project_labels::GetRoomLabel(project_, recent_room).c_str());
    }
    ++rendered;
  }
  ImGui::PopID();
  if (rendered > 0) {
    ImGui::SameLine();
  }
}

void DungeonCanvasViewer::DrawCompactLayerToggles(int room_id) {
  DrawDungeonLayerControls(*this, room_id, ImGui::GetFrameHeight());
  ImGui::SameLine();
  if (ImGui::Button(tr("View")))
    ImGui::OpenPopup("##RoomViewOptions");
  if (ImGui::BeginPopup("##RoomViewOptions")) {
    DrawDungeonOverlayControls(*this);
    ImGui::EndPopup();
  }
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
