#include "app/editor/dungeon/workspace/dungeon_workbench_content.h"

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <string>

#include "absl/strings/str_format.h"
#include "app/editor/agent/agent_ui_theme.h"
#include "app/editor/dungeon/dungeon_canvas_viewer.h"
#include "app/editor/dungeon/dungeon_project_labels.h"
#include "app/editor/dungeon/dungeon_room_edit.h"
#include "app/editor/dungeon/dungeon_room_selector.h"
#include "app/editor/dungeon/inspectors/dungeon_chest_editor.h"
#include "app/editor/dungeon/inspectors/dungeon_destination_editor.h"
#include "app/editor/dungeon/inspectors/dungeon_room_transfer_editor.h"
#include "app/editor/dungeon/workspace/dungeon_workbench_inspector_helpers.h"
#include "app/gui/automation/widget_auto_register.h"
#include "app/gui/core/icons.h"
#include "app/gui/core/input.h"
#include "imgui/imgui.h"
#include "rom/rom.h"
#include "util/i18n/tr.h"
#include "zelda3/resource_labels.h"
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

}  // namespace

void DungeonWorkbenchContent::DrawInspectorShelfRoom(
    DungeonCanvasViewer& viewer) {
  const auto& theme = AgentUI::GetTheme();

  int room_id = viewer.current_room_id();
  if (room_id < 0 && current_room_id_) {
    room_id = *current_room_id_;
  }

  const std::string room_label =
      (room_id >= 0)
          ? dungeon_project_labels::GetRoomLabel(viewer.project(), room_id)
          : std::string("None");

  // Room badge: hex ID + copy button (only for valid room IDs).
  workbench::DrawInspectorSectionHeader(ICON_MD_CASTLE " Room Summary");
  if (room_id >= 0) {
    ImGui::AlignTextToFramePadding();
    ImGui::Text(tr("Room 0x%03X"), room_id);
    ImGui::SameLine();
    if (ImGui::SmallButton(ICON_MD_CONTENT_COPY "##CopyRoomId")) {
      char buf[16];
      snprintf(buf, sizeof(buf), "0x%03X", room_id);
      ImGui::SetClipboardText(buf);
    }
    if (ImGui::IsItemHovered()) {
      ImGui::SetTooltip(tr("Copy room ID (0x%03X) to clipboard"), room_id);
    }

    if (auto* rooms = viewer.rooms();
        rooms && room_id < static_cast<int>(rooms->size())) {
      auto& objects = (*rooms)[room_id].GetTileObjects();
      if (!objects.empty()) {
        const auto selected_indices =
            viewer.object_interaction().GetSelectedObjectIndices();
        int requested_object_index =
            selected_indices.size() == 1
                ? static_cast<int>(selected_indices.front())
                : 0;
        ImGui::AlignTextToFramePadding();
        ImGui::TextUnformatted(tr("Object"));
        ImGui::SameLine();
        ImGui::SetNextItemWidth(74.0f);
        bool object_index_changed = false;
        {
          gui::AutoWidgetScope automation_scope("Dungeon/Workbench");
          object_index_changed = gui::InputScalarDeferred(
              "##WorkbenchObjectIndex", ImGuiDataType_S32,
              &requested_object_index, "%d", 0,
              {reinterpret_cast<uintptr_t>(rooms),
               static_cast<uint64_t>(room_id)});
          gui::AutoRegisterLastItem("input_int", "object_index",
                                    "Select and locate a room object by index");
        }
        if (object_index_changed) {
          const size_t target_index = static_cast<size_t>(std::clamp(
              requested_object_index, 0, static_cast<int>(objects.size()) - 1));
          viewer.object_interaction().SetSelectedObjects({target_index});
          viewer.ScrollToTile(objects[target_index].x(),
                              objects[target_index].y());
          inspector_mode_ = InspectorMode::Selection;
        }
        if (ImGui::IsItemHovered()) {
          ImGui::SetTooltip(tr("Select object index 0-%zu"),
                            objects.size() - 1);
        }
      }
    }
  } else {
    ImGui::TextUnformatted(tr("Room: None"));
  }

  bool room_dirty = false;
  if (auto* rooms = viewer.rooms(); rooms && room_id >= 0) {
    if (const auto* room = rooms->GetIfMaterialized(room_id)) {
      room_dirty = room->HasUnsavedChanges();
    }
  }
  if (room_dirty) {
    ImGui::TextColored(theme.status_warning,
                       ICON_MD_EDIT " Pending room changes");
  } else if (room_id >= 0) {
    ImGui::TextDisabled(ICON_MD_CHECK " Room matches ROM buffer");
  }

  // Dungeon group context: prefer ROM entrance-based lookup (accurate for
  // custom Oracle dungeons); fall back to blockset-derived name.
  if (!room_dungeon_cache_built_ && rom_ && rom_->is_loaded()) {
    BuildRoomDungeonCache();
  }
  if (room_id >= 0) {
    std::string project_group_name =
        dungeon_project_labels::GetDungeonNameForRoom(viewer.project(),
                                                      room_id);
    const char* group_name =
        project_group_name.empty() ? nullptr : project_group_name.c_str();
    if (!group_name) {
      auto cache_it = room_dungeon_cache_.find(room_id);
      if (cache_it != room_dungeon_cache_.end() && !cache_it->second.empty()) {
        group_name = cache_it->second.c_str();
      }
    }
    if (!group_name) {
      auto* rooms = viewer.rooms();
      if (rooms && room_id < static_cast<int>(rooms->size())) {
        group_name = DungeonRoomSelector::GetBlocksetGroupName(
            (*rooms)[room_id].blockset());
      }
    }
    if (group_name) {
      ImGui::TextDisabled(ICON_MD_CASTLE " %s – %s", group_name,
                          room_label.c_str());
    } else {
      ImGui::TextDisabled("%s", room_label.c_str());
    }
  } else {
    ImGui::TextDisabled("%s", room_label.c_str());
  }

  // Apply Room and Dungeon Map have moved to the canvas toolbar (Save and
  // Map icons). Apply Scope and Layer Compositing remain here as
  // collapsibles since they're rarely-touched per-room batch settings.
  if (workbench::BeginInspectorSection(ICON_MD_SAVE_ALT " Apply Scope",
                                       false)) {
    DrawApplyScopeControls(room_id);
  }

  if (workbench::BeginInspectorSection(ICON_MD_WARNING " Pit Damage", false)) {
    DrawPitDamageControls(room_id);
  }

  if (workbench::BeginInspectorSection(ICON_MD_LAYERS " Layer Compositing",
                                       false)) {
    DrawLayerCompositingControls(viewer, room_id);
  }

  if (workbench::BeginInspectorSection(
          ICON_MD_CONTENT_COPY " Clone / Import Room", false)) {
    if (viewer.room_transfer_state().popup_open) {
      ImGui::TextWrapped("Room transfer is open in the canvas dialog.");
    } else {
      DrawDungeonRoomTransferEditor(viewer);
    }
  }

  // Room properties share the editor's validated metadata edit boundary. Raw
  // graphics IDs remain available alongside named behavior choices.
  if (auto* rooms = viewer.rooms();
      rooms && room_id >= 0 && room_id < static_cast<int>(rooms->size())) {
    auto& room = (*rooms)[room_id];
    const ImGuiID error_id =
        ImGui::GetID(absl::StrFormat("RoomMetadataError/%d", room_id).c_str());
    const gui::InputScalarTargetIdentity identity{
        reinterpret_cast<uintptr_t>(rooms), static_cast<uint64_t>(room_id)};
    auto apply = [&](RoomMetadataField field, int value, int index = 0) {
      const auto status = viewer.EditRoomMetadata(
          room_id, {.field = field, .value = value, .index = index});
      ImGui::GetStateStorage()->SetBool(error_id, !status.ok());
    };
    auto begin_properties = [](const char* id) {
      if (!ImGui::BeginTable(
              id, 2,
              ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingStretchProp)) {
        return false;
      }
      ImGui::TableSetupColumn("Property", ImGuiTableColumnFlags_WidthFixed,
                              104.0f);
      ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthStretch);
      return true;
    };
    auto property_row = [](const char* label) {
      ImGui::TableNextRow();
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
        gui::AutoWidgetScope automation_scope("Dungeon/Workbench");
        gui::AutoRegisterLastItem("input_hex", id + 2, label);
      }
      if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("%s (hex 00-%X). Enter or leave the field to apply.",
                          label, max_value);
      }
    };
    auto draw_choice = [&](const char* label, const char* id, int value,
                           int count, RoomMetadataField field, auto name) {
      property_row(label);
      const auto preview = absl::StrFormat("%02X  %s", value, name(value));
      const bool open = ImGui::BeginCombo(id, preview.c_str());
      {
        gui::AutoWidgetScope automation_scope("Dungeon/Workbench");
        gui::AutoRegisterLastItem("combo", id + 2, label);
      }
      if (open) {
        for (int option = 0; option < count; ++option) {
          const auto option_label =
              absl::StrFormat("%02X  %s", option, name(option));
          if (ImGui::Selectable(option_label.c_str(), value == option)) {
            apply(field, option);
          }
          {
            gui::AutoWidgetScope automation_scope("Dungeon/Workbench");
            gui::AutoRegisterLastItem("selectable",
                                      absl::StrFormat("%s/%d", id + 2, option));
          }
          if (value == option) {
            ImGui::SetItemDefaultFocus();
          }
        }
        ImGui::EndCombo();
      }
    };

    if (workbench::BeginInspectorSection(ICON_MD_TUNE " Room Properties",
                                         false) &&
        begin_properties("##WorkbenchRoomHeader")) {
      draw_hex("Layout", "##RoomHeaderLayout", room.layout_id(), 0x07,
               RoomMetadataField::kLayout);
      draw_hex("Blockset", "##RoomHeaderBlockset", room.blockset(), 0x51,
               RoomMetadataField::kBlockset);
      draw_hex("BG1 floor", "##RoomHeaderFloor1", room.floor1(), 0x0F,
               RoomMetadataField::kFloor1);
      draw_hex("BG2 floor", "##RoomHeaderFloor2", room.floor2(), 0x0F,
               RoomMetadataField::kFloor2);
      draw_hex("Palette", "##RoomHeaderPalette", room.palette(), 0x47,
               RoomMetadataField::kPalette);
      draw_hex("Sprite graphics", "##RoomHeaderSpriteset", room.spriteset(),
               zelda3::kMaxDungeonSpriteset, RoomMetadataField::kSpriteset);
      draw_hex("Message", "##RoomHeaderMessage", room.message_id(), 0x0FFF,
               RoomMetadataField::kMessage);
      draw_choice("BG2 mode", "##RoomHeaderBg2", static_cast<int>(room.bg2()),
                  9, RoomMetadataField::kBg2, GetBg2ModeName);
      draw_choice("Effect", "##RoomHeaderEffect", room.effect(), 8,
                  RoomMetadataField::kEffect, [](int value) -> std::string {
                    return value >= 0 && value < 8 ? zelda3::RoomEffect[value]
                                                   : "Unknown";
                  });
      draw_choice("Collision", "##RoomHeaderCollision", room.collision(), 5,
                  RoomMetadataField::kCollision, GetCollisionName);
      const auto tag_name = [](int value) {
        return zelda3::GetRoomTagLabel(value);
      };
      const int tag_count =
          static_cast<int>(zelda3::Zelda3Labels::GetRoomTagNames().size());
      draw_choice("Tag 1", "##RoomHeaderTag1", room.tag1(), tag_count,
                  RoomMetadataField::kTag1, tag_name);
      draw_choice("Tag 2", "##RoomHeaderTag2", room.tag2(), tag_count,
                  RoomMetadataField::kTag2, tag_name);
      ImGui::EndTable();
    }

    if (workbench::BeginInspectorSection(ICON_MD_ALT_ROUTE " Destinations",
                                         false)) {
      DrawDungeonDestinationEditor(viewer);
      if (viewer.current_room_id() != room_id)
        return;
    }
    if (workbench::BeginInspectorSection(ICON_MD_INVENTORY_2 " Chest contents",
                                         false)) {
      DrawDungeonChestEditor(room_id, room, viewer);
    }
    if (ImGui::GetStateStorage()->GetBool(error_id)) {
      ImGui::TextWrapped(
          "Edit not applied. Check that this room is editable and the value "
          "is within the field's supported range. Hover the field for its "
          "range.");
    }
  } else {
    ImGui::TextDisabled(tr("Room properties unavailable"));
  }

  auto& interaction = viewer.object_interaction();
  const bool placing = interaction.mode_manager().IsPlacementActive();
  if (placing) {
    workbench::DrawInspectorSectionHeader(ICON_MD_BUILD " Editing Status");
    ImGui::TextColored(theme.text_info, tr("Placement active"));
    ImGui::SameLine();
    if (ImGui::SmallButton(ICON_MD_CLOSE " Cancel")) {
      interaction.mode_manager().CancelCurrentMode();
    }
  }
}

}  // namespace yaze::editor
