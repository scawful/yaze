#include "app/editor/dungeon/workspace/dungeon_workbench_content.h"

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <string>

#include "absl/strings/str_format.h"
#include "app/editor/agent/agent_ui_theme.h"
#include "app/editor/dungeon/dungeon_canvas_viewer.h"
#include "app/editor/dungeon/dungeon_project_labels.h"
#include "app/editor/dungeon/dungeon_room_property_controls.h"
#include "app/editor/dungeon/dungeon_room_selector.h"
#include "app/editor/dungeon/inspectors/dungeon_chest_editor.h"
#include "app/editor/dungeon/inspectors/dungeon_destination_editor.h"
#include "app/editor/dungeon/inspectors/dungeon_room_transfer_editor.h"
#include "app/editor/dungeon/ui/window/dungeon_entrances_panel.h"
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

  bool room_dirty = false;
  if (auto* rooms = viewer.rooms(); rooms && room_id >= 0) {
    if (const auto* room = rooms->GetIfMaterialized(room_id)) {
      room_dirty = room->HasUnsavedChanges();
    }
  }

  // Room badge: hex ID + copy button (only for valid room IDs).
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
    if (room_dirty) {
      ImGui::SameLine();
      ImGui::TextColored(theme.status_warning, "Unsaved");
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
          FocusSelectionInspector();
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

  // Dungeon group context: prefer ROM entrance-based lookup (accurate for
  // custom Oracle dungeons); fall back to blockset-derived name.
  if (!room_dungeon_cache_built_ && rom_ && rom_->is_loaded()) {
    BuildRoomDungeonCache();
  }
  ImGui::PushTextWrapPos(0.0f);
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
  ImGui::PopTextWrapPos();

  // Room properties share the editor's validated metadata edit boundary. Raw
  // graphics IDs remain available alongside named behavior choices.
  if (auto* rooms = viewer.rooms();
      rooms && room_id >= 0 && room_id < static_cast<int>(rooms->size())) {
    auto& room = (*rooms)[room_id];
    DrawDungeonRoomPropertyFields(viewer, room, room_id,
                                  RoomPropertyLayout::kFull);

    DrawPitDamageControls(room_id);
    workbench::DrawInspectorSectionHeader("Destinations");
    DrawDungeonDestinationEditor(viewer);
    if (viewer.current_room_id() != room_id)
      return;

    if (entrance_navigator_content_) {
      ImGui::PushID("InspectedEntrance");
      workbench::DrawInspectorSectionHeader("Entrance");
      const std::string id = entrance_navigator_content_->GetId();
      if (is_standalone_tool_open_ && is_standalone_tool_open_(id)) {
        ImGui::TextDisabled(
            "Entrance properties are open in a separate window.");
        if (ImGui::Button("Show entrance window") &&
            open_and_focus_standalone_tool_)
          open_and_focus_standalone_tool_(id);
      } else {
        ImGui::BeginDisabled(viewer.header_read_only() ||
                             !viewer.IsObjectInteractionEnabled());
        entrance_navigator_content_->DrawPropertiesInWorkbench();
        ImGui::EndDisabled();
      }
      ImGui::PopID();
      if (viewer.current_room_id() != room_id)
        return;
    }

    if (workbench::BeginInspectorSection(ICON_MD_INVENTORY_2 " Chest contents",
                                         false)) {
      DrawDungeonChestEditor(room_id, room, viewer);
    }
  } else {
    ImGui::TextDisabled(tr("Room properties unavailable"));
  }

  if (workbench::BeginInspectorSection(
          ICON_MD_CONTENT_COPY " Clone / Import Room", false)) {
    if (viewer.room_transfer_state().popup_open) {
      ImGui::TextWrapped("Room transfer is open in the canvas dialog.");
    } else {
      DrawDungeonRoomTransferEditor(viewer);
    }
  }
}

}  // namespace yaze::editor
