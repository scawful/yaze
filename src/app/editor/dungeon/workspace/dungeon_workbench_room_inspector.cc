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
#include "app/editor/dungeon/inspectors/dungeon_entity_inspector.h"
#include "app/editor/dungeon/inspectors/dungeon_room_transfer_editor.h"
#include "app/editor/dungeon/workspace/dungeon_workbench_inspector_helpers.h"
#include "app/gui/automation/widget_auto_register.h"
#include "app/gui/core/icons.h"
#include "app/gui/core/input.h"
#include "app/gui/core/layout_helpers.h"
#include "app/gui/core/style_guard.h"
#include "core/features.h"
#include "imgui/imgui.h"
#include "rom/rom.h"
#include "util/i18n/tr.h"
#include "zelda3/dungeon/chest_edit.h"
#include "zelda3/dungeon/custom_object.h"
#include "zelda3/dungeon/object_layer_semantics.h"
#include "zelda3/resource_labels.h"
#include "zelda3/zelda3_labels.h"

namespace yaze::editor {
namespace {

const char* GetObjectCategory(int object_id) {
  if (object_id < 0x100)
    return "Standard";
  if (object_id < 0x200)
    return "Extended";
  if (object_id >= 0xF80)
    return "Special";
  return "Unknown";
}

const char* GetObjectStreamName(int layer_value) {
  switch (layer_value) {
    case 0:
      return "Primary";
    case 1:
      return "BG2 overlay";
    case 2:
      return "BG1 overlay";
    default:
      return "Unknown";
  }
}

const char* GetSpecialLayerName(int layer_value) {
  switch (layer_value) {
    case 0:
      return "Upper layer (BG1)";
    case 1:
      return "Lower layer (BG2)";
    default:
      return "Unknown";
  }
}

void DrawObjectIndexJump(DungeonCanvasViewer& viewer) {
  auto* rooms = viewer.rooms();
  const int room_id = viewer.current_room_id();
  if (!rooms || room_id < 0 || room_id >= static_cast<int>(rooms->size())) {
    return;
  }
  auto& objects = (*rooms)[room_id].GetTileObjects();
  if (objects.empty()) {
    return;
  }

  auto& interaction = viewer.object_interaction();
  const auto selected = interaction.GetSelectedObjectIndices();
  int requested_object_index =
      selected.size() == 1 ? static_cast<int>(selected.front()) : 0;

  ImGui::AlignTextToFramePadding();
  ImGui::TextUnformatted(tr("Jump to object"));
  ImGui::SameLine();
  ImGui::SetNextItemWidth(74.0f);
  bool object_index_changed = false;
  {
    gui::AutoWidgetScope automation_scope("Dungeon/Workbench");
    object_index_changed = gui::InputScalarDeferred(
        "##WorkbenchObjectIndex", ImGuiDataType_S32, &requested_object_index,
        "%d", 0,
        {reinterpret_cast<uintptr_t>(rooms), static_cast<uint64_t>(room_id)});
    gui::AutoRegisterLastItem("input_int", "object_index",
                              "Select and locate a room object by index");
  }
  if (ImGui::IsItemHovered()) {
    ImGui::SetTooltip(tr("Select object index 0-%zu"), objects.size() - 1);
  }
  if (object_index_changed) {
    const size_t target_index = static_cast<size_t>(std::clamp(
        requested_object_index, 0, static_cast<int>(objects.size()) - 1));
    interaction.SetSelectedObjects({target_index});
    viewer.ScrollToTile(objects[target_index].x(), objects[target_index].y());
  }
}

}  // namespace

void DungeonWorkbenchContent::DrawInspectorShelf(DungeonCanvasViewer& viewer,
                                                 bool /*compact*/) {
  auto& interaction = viewer.object_interaction();
  ImGui::PushID("InspectedSelection");
  const bool placing_object = DrawObjectPlacementInspector(viewer);
  if (!placing_object && interaction.mode_manager().IsPlacementActive()) {
    ImGui::TextUnformatted(interaction.mode_manager().GetModeName());
    if (ImGui::Button("Done placing (Esc)")) {
      interaction.CancelPlacement();
    }
    {
      gui::AutoWidgetScope scope("Dungeon/Workbench");
      gui::AutoRegisterLastItem("button", "finish_placement");
    }
  } else if (!placing_object) {
    DrawInspectorShelfSelection(viewer);
  }
  ImGui::PopID();
}

bool DungeonWorkbenchContent::DrawObjectPlacementInspector(
    DungeonCanvasViewer& viewer) {
  auto& interaction = viewer.object_interaction();
  const auto* active_preview = interaction.GetPlacementPreview();
  if (!active_preview) {
    return false;
  }
  // Setters may invalidate preview caches; draw from a stable value this frame.
  const auto object = *active_preview;
  const auto description = workbench::DescribeObjectSize(object);
  const auto& theme = AgentUI::GetTheme();
  ImGui::PushID("PlacementInspector");
  workbench::DrawInspectorSectionHeader(ICON_MD_ADD_CIRCLE " Placing object");
  {
    gui::StyleColorGuard name_color(ImGuiCol_Text, theme.text_primary);
    ImGui::TextWrapped("%s", zelda3::GetObjectName(object.id_).c_str());
  }
  ImGui::TextDisabled("0x%03X", object.id_);

  constexpr ImGuiTableFlags kFlags = ImGuiTableFlags_BordersInnerV |
                                     ImGuiTableFlags_RowBg |
                                     ImGuiTableFlags_NoPadOuterX;
  if (ImGui::BeginTable("Properties", 2, kFlags)) {
    ImGui::TableSetupColumn("Property", ImGuiTableColumnFlags_WidthFixed,
                            64.0f);
    ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthStretch);
    uint8_t requested_size = object.size_;
    if (workbench::DrawObjectSizeControls(object, &requested_size)) {
      interaction.SetPlacementPreviewSize(requested_size);
    }
    const bool uses_stream = zelda3::UsesRoomObjectStream(object);
    gui::LayoutHelpers::PropertyRow(uses_stream ? "Stream" : "Layer", [&]() {
      int layer = object.GetLayerValue();
      const char* stream_names[] = {"Primary", "BG2 overlay", "BG1 overlay"};
      const char* layer_names[] = {"Upper layer (BG1)", "Lower layer (BG2)"};
      ImGui::SetNextItemWidth(-1);
      if (ImGui::Combo("##Layer", &layer,
                       uses_stream ? stream_names : layer_names,
                       uses_stream ? 3 : 2)) {
        interaction.SetPlacementPreviewLayer(layer);
      }
      gui::AutoWidgetScope scope("Dungeon/Workbench");
      gui::AutoRegisterLastItem("combo", "placement_layer",
                                "Layer for the next placed object");
    });
    ImGui::EndTable();
  }

  bool repeat = interaction.GetPlacementPolicy() ==
                DungeonObjectInteraction::PlacementPolicy::kRepeat;
  if (ImGui::Checkbox(tr("Keep placing copies"), &repeat)) {
    interaction.SetPlacementPolicy(
        repeat ? DungeonObjectInteraction::PlacementPolicy::kRepeat
               : DungeonObjectInteraction::PlacementPolicy::kOnce);
  }
  {
    gui::AutoWidgetScope scope("Dungeon/Workbench");
    gui::AutoRegisterLastItem("checkbox", "repeat_placement",
                              "Keep placing copies after each click");
  }
  ImGui::TextWrapped("%s", repeat
                               ? tr("Click the canvas to place each copy.")
                               : tr("Click the canvas to place, then edit."));
  if (!description.wheel_hint.empty()) {
    ImGui::TextWrapped("%s", description.wheel_hint.c_str());
  }
  if (workbench::DrawActionButton(ICON_MD_CHECK " Done placing (Esc)",
                                  ImVec2(-1, 0))) {
    interaction.CancelPlacement();
    if (interaction.GetSelectionCount() > 0) {
      FocusSelectionInspector();
    }
  }
  {
    gui::AutoWidgetScope scope("Dungeon/Workbench");
    gui::AutoRegisterLastItem(
        "button", "finish_placement",
        "Finish placement and inspect the selected object");
  }
  ImGui::Separator();
  ImGui::PopID();
  return true;
}

void DungeonWorkbenchContent::DrawSelectedObjectActions(
    DungeonCanvasViewer& viewer, size_t index) {
  auto& interaction = viewer.object_interaction();
  auto& handler = interaction.entity_coordinator().tile_handler();
  const int room_id = viewer.current_room_id();
  const std::vector<size_t> indices = {index};
  ImGui::Spacing();
  if (workbench::DrawActionButton(ICON_MD_ADD_CIRCLE " Place another",
                                  ImVec2(-1, 0))) {
    if (interaction.BeginPlacementFromSelection()) {
      OpenObjectSelectorTool();
    }
  }
  {
    gui::AutoWidgetScope scope("Dungeon/Workbench");
    gui::AutoRegisterLastItem("button", "place_another",
                              "Place another object with this size and layer");
  }
  constexpr ImGuiTableFlags kFlags =
      ImGuiTableFlags_SizingStretchSame | ImGuiTableFlags_NoPadOuterX;
  if (ImGui::BeginTable("##SelectedObjectActions", 2, kFlags)) {
    auto action = [](const char* label, const char* id) {
      ImGui::TableNextColumn();
      const bool clicked = workbench::DrawActionButton(label, ImVec2(-1, 0));
      gui::AutoWidgetScope scope("Dungeon/Workbench");
      gui::AutoRegisterLastItem("button", id, label);
      return clicked;
    };
    if (action(ICON_MD_CONTENT_COPY " Duplicate", "duplicate_object")) {
      (void)interaction.HandleDuplicateSelected();
    }
    if (action(ICON_MD_DELETE " Delete", "delete_object")) {
      handler.DeleteObjects(room_id, indices);
    }
    if (action(ICON_MD_FLIP_TO_FRONT " To front", "object_to_front")) {
      handler.SendToFront(room_id, indices);
    }
    if (action(ICON_MD_FLIP_TO_BACK " To back", "object_to_back")) {
      handler.SendToBack(room_id, indices);
    }
    ImGui::EndTable();
  }
}

void DungeonWorkbenchContent::DrawInspectorShelfSelection(
    DungeonCanvasViewer& viewer) {
  auto& interaction = viewer.object_interaction();
  const auto& theme = AgentUI::GetTheme();

  const int room_id = viewer.current_room_id();
  const size_t obj_count = interaction.GetSelectionCount();
  const bool has_entity = interaction.HasEntitySelection();

  if (!has_entity && obj_count == 0) {
    workbench::DrawInspectorSectionHeader(ICON_MD_INFO " Selection");
    ImGui::TextDisabled(tr("Click an object or entity to inspect"));
    ImGui::Spacing();
    DrawObjectIndexJump(viewer);
    return;
  }

  // ── Tile Object Selection ──
  if (obj_count > 0) {
    const auto indices = interaction.GetSelectedObjectIndices();

    // Multi-object summary + inline bulk editor (O1)
    if (indices.size() > 1 && room_id >= 0 && viewer.rooms()) {
      auto& room = (*viewer.rooms())[room_id];
      auto& objects = room.GetTileObjects();
      workbench::DrawInspectorSectionHeader(ICON_MD_SUMMARIZE
                                            " Multi-selection");
      for (size_t i = 0; i < indices.size() && i < 8; ++i) {
        size_t idx = indices[i];
        if (idx < objects.size()) {
          auto& obj = objects[idx];
          std::string name = zelda3::GetObjectName(obj.id_);
          ImGui::BulletText(tr("0x%03X %s"), obj.id_, name.c_str());
        }
      }
      if (indices.size() > 8) {
        ImGui::TextDisabled(tr("  ... and %zu more"), indices.size() - 8);
      }

      // Bulk editor: nudge/layer/stacking/size + destructive duplicate/delete.
      // Operations route through tile_handler / interaction, so every action
      // captures its own undo snapshot.
      auto& tile_handler = interaction.entity_coordinator().tile_handler();
      const std::vector<size_t> selection_copy(indices.begin(), indices.end());
      bool has_room_stream_object = false;
      bool has_special_layer_object = false;
      for (const size_t index : selection_copy) {
        if (index >= objects.size()) {
          continue;
        }
        if (zelda3::UsesRoomObjectStream(objects[index])) {
          has_room_stream_object = true;
        } else {
          has_special_layer_object = true;
        }
      }
      const bool has_editable_size =
          workbench::HasEditableRoomObjectSize(objects, selection_copy);

      workbench::DrawInspectorSectionHeader(ICON_MD_OPEN_WITH " Bulk Edit");

      // Nudge grid (arrow buttons around a tile-delta drag).
      static int bulk_nudge_dx = 0;
      static int bulk_nudge_dy = 0;
      ImGui::TextDisabled(
          tr("Nudge selection (%d px steps)"),
          interaction.entity_coordinator().SelectionMoveStepPixels());
      ImGui::PushButtonRepeat(true);
      if (ImGui::Button(ICON_MD_ARROW_UPWARD "##BulkNudgeUp"))
        interaction.NudgeSelected(0, -1);
      ImGui::SameLine();
      if (ImGui::Button(ICON_MD_ARROW_DOWNWARD "##BulkNudgeDown"))
        interaction.NudgeSelected(0, 1);
      ImGui::SameLine();
      if (ImGui::Button(ICON_MD_ARROW_BACK "##BulkNudgeLeft"))
        interaction.NudgeSelected(-1, 0);
      ImGui::SameLine();
      if (ImGui::Button(ICON_MD_ARROW_FORWARD "##BulkNudgeRight"))
        interaction.NudgeSelected(1, 0);
      ImGui::PopButtonRepeat();

      ImGui::SetNextItemWidth(60);
      ImGui::DragInt("##BulkNudgeDx", &bulk_nudge_dx, 0.25f, -63, 63, "Δx:%d");
      ImGui::SameLine();
      ImGui::SetNextItemWidth(60);
      ImGui::DragInt("##BulkNudgeDy", &bulk_nudge_dy, 0.25f, -63, 63, "Δy:%d");
      ImGui::SameLine();
      if (ImGui::Button(tr("Apply##BulkNudgeApply")) &&
          (bulk_nudge_dx != 0 || bulk_nudge_dy != 0)) {
        interaction.NudgeSelected(bulk_nudge_dx, bulk_nudge_dy);
        bulk_nudge_dx = 0;
        bulk_nudge_dy = 0;
      }

      ImGui::Spacing();
      if (has_room_stream_object && !has_special_layer_object) {
        ImGui::TextDisabled(tr("Object stream (set all)"));
        if (ImGui::SmallButton(tr("Primary##BulkPlacement0")))
          tile_handler.UpdateObjectsLayer(room_id, selection_copy, 0);
        ImGui::SameLine();
        if (ImGui::SmallButton(tr("BG2 overlay##BulkPlacement1")))
          tile_handler.UpdateObjectsLayer(room_id, selection_copy, 1);
        ImGui::SameLine();
        if (ImGui::SmallButton(tr("BG1 overlay##BulkPlacement2")))
          tile_handler.UpdateObjectsLayer(room_id, selection_copy, 2);
      } else if (has_special_layer_object && !has_room_stream_object) {
        ImGui::TextDisabled(tr("Special layer (set all)"));
        if (ImGui::SmallButton(tr("Upper (BG1)##BulkPlacement0")))
          tile_handler.UpdateObjectsLayer(room_id, selection_copy, 0);
        ImGui::SameLine();
        if (ImGui::SmallButton(tr("Lower (BG2)##BulkPlacement1")))
          tile_handler.UpdateObjectsLayer(room_id, selection_copy, 1);
      } else {
        ImGui::TextDisabled(tr("Stored placement (mixed selection)"));
        if (ImGui::SmallButton(tr("Primary / Upper (BG1)##BulkPlacement0")))
          tile_handler.UpdateObjectsLayer(room_id, selection_copy, 0);
        ImGui::SameLine();
        if (ImGui::SmallButton(tr("BG2 overlay / Lower (BG2)##BulkPlacement1")))
          tile_handler.UpdateObjectsLayer(room_id, selection_copy, 1);
      }

      ImGui::Spacing();
      ImGui::TextDisabled(tr("Stacking"));
      if (ImGui::SmallButton(ICON_MD_FLIP_TO_FRONT " Front##BulkFront"))
        tile_handler.SendToFront(room_id, selection_copy);
      ImGui::SameLine();
      if (ImGui::SmallButton(ICON_MD_FLIP_TO_BACK " Back##BulkBack"))
        tile_handler.SendToBack(room_id, selection_copy);
      ImGui::SameLine();
      if (ImGui::SmallButton(ICON_MD_KEYBOARD_ARROW_UP "##BulkFwd"))
        tile_handler.MoveForward(room_id, selection_copy);
      ImGui::SameLine();
      if (ImGui::SmallButton(ICON_MD_KEYBOARD_ARROW_DOWN "##BulkBwd"))
        tile_handler.MoveBackward(room_id, selection_copy);

      ImGui::Spacing();
      ImGui::TextDisabled(tr("Size"));
      if (!has_editable_size) {
        ImGui::BeginDisabled();
      }
      if (ImGui::SmallButton(ICON_MD_REMOVE "##BulkSizeDec"))
        tile_handler.ResizeObjects(room_id, selection_copy, -1);
      ImGui::SameLine();
      if (ImGui::SmallButton(ICON_MD_ADD "##BulkSizeInc"))
        tile_handler.ResizeObjects(room_id, selection_copy, 1);
      if (!has_editable_size) {
        ImGui::EndDisabled();
      }

      ImGui::Spacing();
      if (ImGui::SmallButton(ICON_MD_CONTENT_COPY " Duplicate##BulkDup"))
        (void)interaction.HandleDuplicateSelected();
      ImGui::SameLine();
      {
        gui::StyleColorGuard danger_colors({
            {ImGuiCol_Button, theme.status_error},
            {ImGuiCol_ButtonHovered,
             ImVec4(theme.status_error.x + 0.1f, theme.status_error.y + 0.05f,
                    theme.status_error.z + 0.05f, 1.0f)},
        });
        if (ImGui::SmallButton(ICON_MD_DELETE " Delete##BulkDel"))
          ImGui::OpenPopup("##BulkDeleteConfirm");
      }

      if (ImGui::BeginPopup("##BulkDeleteConfirm")) {
        ImGui::TextColored(
            theme.status_error, ICON_MD_WARNING " Delete %zu object%s?",
            selection_copy.size(), selection_copy.size() == 1 ? "" : "s");
        ImGui::Separator();
        if (ImGui::Button(tr("Cancel##BulkDelCancel"))) {
          ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        {
          gui::StyleColorGuard confirm_colors({
              {ImGuiCol_Button, theme.status_error},
          });
          if (ImGui::Button(ICON_MD_DELETE " Confirm##BulkDelConfirm")) {
            tile_handler.DeleteObjects(room_id, selection_copy);
            ImGui::CloseCurrentPopup();
          }
        }
        ImGui::EndPopup();
      }
    }

    // Single-object detailed inspector
    if (indices.size() == 1 && room_id >= 0 && viewer.rooms()) {
      auto& room = (*viewer.rooms())[room_id];
      auto& objects = room.GetTileObjects();
      const size_t idx = indices.front();
      if (idx < objects.size()) {
        // Stream changes can reorder and replace the room's object vector.
        // Keep this frame's property values independent of that storage.
        const auto obj = objects[idx];
        const std::string obj_name = workbench::RoomObjectDisplayName(obj);
        const bool uses_room_stream = zelda3::UsesRoomObjectStream(obj);
        int displayed_layer = obj.GetLayerValue();
        const auto chest_index = zelda3::ChestIndexForObject(objects, idx);
        const bool editable =
            viewer.IsObjectInteractionEnabled() && !viewer.header_read_only();

        ImGui::TextWrapped("%s  ($%03X)", obj_name.c_str(), obj.id_);
        if (ImGui::IsItemHovered()) {
          ImGui::BeginTooltip();
          ImGui::Text("Object %zu, %s (Type %d)", idx,
                      GetObjectCategory(obj.id_),
                      zelda3::GetObjectSubtype(obj.id_));
          ImGui::Text("Pixel (%d, %d), %s", obj.x_ * 8, obj.y_ * 8,
                      uses_room_stream ? GetObjectStreamName(displayed_layer)
                                       : GetSpecialLayerName(displayed_layer));
          const auto description = workbench::DescribeObjectSize(obj);
          if (!description.wheel_hint.empty())
            ImGui::TextUnformatted(description.wheel_hint.c_str());
          ImGui::EndTooltip();
        }

        const auto tile_edit_status = viewer.ObjectTileEditStatus(room_id, obj);
        auto& custom_objects = zelda3::CustomObjectManager::Get();
        if (core::FeatureFlags::get().kEnableCustomObjects &&
            zelda3::CustomObjectManager::RuntimeSubtypeCountForObject(
                obj.id_, custom_objects.GetMinecartRailProfile()) > 0) {
          const auto filename =
              custom_objects.ResolveFilename(obj.id_, obj.size_);
          ImGui::TextWrapped("Asset: %s", filename.empty() ? "Unmapped variant"
                                                           : filename.c_str());
        }
        ImGui::BeginDisabled(!tile_edit_status.ok());
        if (ImGui::Button(tr("Edit tiles..."))) {
          viewer.EditObjectTiles(room_id, obj);
        }
        {
          gui::AutoWidgetScope scope("Dungeon/Workbench");
          gui::AutoRegisterLastItem(
              "button", "edit_object_tiles",
              "Edit the selected object's exact tile asset");
        }
        ImGui::EndDisabled();
        ImGui::SameLine();
        ImGui::TextDisabled(
            "%s", tile_edit_status.ok() ? "Available" : "Unavailable");
        if (!tile_edit_status.ok()) {
          ImGui::TextWrapped("%s",
                             std::string(tile_edit_status.message()).c_str());
        }

        if (chest_index) {
          const bool has_contents =
              room.AreChestsLoaded() && *chest_index < room.GetChests().size();
          if (has_contents) {
            const auto chest = room.GetChests()[*chest_index];
            ImGui::TextWrapped(
                "Contents: %s",
                GetDungeonChestItemLabel(chest.id, viewer.project()).c_str());
            ImGui::Text("Size: %s", chest.size ? "Big" : "Small");
          } else {
            ImGui::TextDisabled("Contents unavailable");
            ImGui::Text("Size: %s", obj.id_ == 0xFB1 ? "Big" : "Small");
          }
          ImGui::BeginDisabled(!editable || !has_contents);
          if (ImGui::Button("Edit Chest...", ImVec2(-1, 0))) {
            const auto status = viewer.OpenChestEditor(room_id, *chest_index);
            viewer.chest_editor_state().error =
                status.ok() ? "" : std::string(status.message());
          }
          {
            gui::AutoWidgetScope scope("Dungeon/Workbench");
            gui::AutoRegisterLastItem("button", "edit_chest");
          }
          ImGui::EndDisabled();
          if (!viewer.chest_editor_state().error.empty())
            ImGui::TextWrapped("%s", viewer.chest_editor_state().error.c_str());
        }

        const bool show_properties =
            !chest_index ||
            ImGui::CollapsingHeader("Properties##ChestProperties");
        if (chest_index) {
          gui::AutoWidgetScope scope("Dungeon/Workbench");
          gui::AutoRegisterLastItem("collapsing_header", "chest_properties");
        }
        if (show_properties) {
          ImGui::BeginDisabled(!editable);
          // Property edits retain the existing handlers and validation.
          constexpr ImGuiTableFlags kPropsFlags =
              ImGuiTableFlags_BordersInnerV | ImGuiTableFlags_RowBg |
              ImGuiTableFlags_NoPadOuterX;
          if (ImGui::BeginTable("##SelObjProps", 2, kPropsFlags)) {
            ImGui::TableSetupColumn("Prop", ImGuiTableColumnFlags_WidthFixed,
                                    56.0f);
            ImGui::TableSetupColumn("Val", ImGuiTableColumnFlags_WidthStretch);

            // ID
            gui::LayoutHelpers::PropertyRow("ID", [&]() {
              uint16_t obj_id = static_cast<uint16_t>(obj.id_ & 0x0FFF);
              if (auto res =
                      gui::InputHexWordEx("##SelObjId", &obj_id, 80.0f, true);
                  res.ShouldApply()) {
                obj_id &= 0x0FFF;
                interaction.SetObjectId(idx, static_cast<int16_t>(obj_id));
              }
            });

            // Position
            gui::LayoutHelpers::PropertyRow("Pos", [&]() {
              int pos_x = obj.x_;
              int pos_y = obj.y_;
              ImGui::SetNextItemWidth(60);
              bool x_changed =
                  ImGui::DragInt("##SelObjX", &pos_x, 0.1f, 0, 63, "X:%d");
              {
                gui::AutoWidgetScope automation_scope("Dungeon/Workbench");
                gui::AutoRegisterLastItem("drag_int", "selected_object_x",
                                          "Selected room object X coordinate");
              }
              ImGui::SameLine();
              ImGui::SetNextItemWidth(60);
              bool y_changed =
                  ImGui::DragInt("##SelObjY", &pos_y, 0.1f, 0, 63, "Y:%d");
              {
                gui::AutoWidgetScope automation_scope("Dungeon/Workbench");
                gui::AutoRegisterLastItem("drag_int", "selected_object_y",
                                          "Selected room object Y coordinate");
              }
              if (x_changed || y_changed) {
                int delta_x = pos_x - obj.x_;
                int delta_y = pos_y - obj.y_;
                interaction.entity_coordinator().tile_handler().MoveObjects(
                    room_id, {idx}, delta_x, delta_y);
              }
            });

            uint8_t requested_size = obj.size_;
            if (workbench::DrawObjectSizeControls(obj, &requested_size)) {
              interaction.SetObjectSize(idx, requested_size);
            }

            gui::LayoutHelpers::PropertyRow(
                uses_room_stream ? "Stream" : "Layer", [&]() {
                  int layer = displayed_layer;
                  const char* stream_names[] = {"Primary", "BG2 overlay",
                                                "BG1 overlay"};
                  const char* special_layer_names[] = {"Upper layer (BG1)",
                                                       "Lower layer (BG2)"};
                  ImGui::SetNextItemWidth(-1);
                  if (ImGui::Combo(
                          "##SelObjLayer", &layer,
                          uses_room_stream ? stream_names : special_layer_names,
                          uses_room_stream
                              ? IM_ARRAYSIZE(stream_names)
                              : IM_ARRAYSIZE(special_layer_names))) {
                    const int max_layer = uses_room_stream ? 2 : 1;
                    layer = std::clamp(layer, 0, max_layer);
                    if (interaction.SetObjectLayer(
                            idx, static_cast<zelda3::RoomObject::LayerType>(
                                     layer))) {
                      displayed_layer = layer;
                    }
                  }
                });
            ImGui::EndTable();
          }
          // Actions can reallocate the room's object vector; no object references
          // are used after this point.
          const auto current_indices = interaction.GetSelectedObjectIndices();
          if (current_indices.size() == 1) {
            DrawSelectedObjectActions(viewer, current_indices.front());
          }
          ImGui::EndDisabled();
          if (chest_index ||
              ImGui::CollapsingHeader("Details##SelectionDetails"))
            DrawObjectIndexJump(viewer);
        }
      }
    }
  }

  if (has_entity) {
    workbench::DrawInspectorSectionHeader(ICON_MD_SELECT_ALL
                                          " Entity Selection");
    DrawDungeonEntityInspector(viewer);
  }
}

void DungeonWorkbenchContent::DrawRoomContextHeader(
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

  // Room header: hex ID + copy button + room and dungeon name unified.
  if (room_id >= 0) {
    ImGui::AlignTextToFramePadding();
    ImGui::Text(ICON_MD_CASTLE " Room 0x%03X", room_id);
    ImGui::SameLine();
    if (ImGui::SmallButton(ICON_MD_CONTENT_COPY "##CopyRoomId")) {
      char buf[16];
      snprintf(buf, sizeof(buf), "0x%03X", room_id);
      ImGui::SetClipboardText(buf);
    }
    if (ImGui::IsItemHovered()) {
      ImGui::SetTooltip(tr("Copy room ID (0x%03X) to clipboard"), room_id);
    }

    if (room_label_edit_callback_ && viewer.project() != nullptr) {
      ImGui::SameLine();
      if (ImGui::SmallButton(ICON_MD_EDIT "##RenameRoom")) {
        snprintf(room_rename_buffer_, sizeof(room_rename_buffer_), "%s",
                 room_label.c_str());
        room_rename_error_.clear();
        ImGui::OpenPopup("RenameRoomModal");
      }
      if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip(tr("Rename Room 0x%03X for this project"), room_id);
      }
      gui::AutoRegisterLastItem("button", "rename_room_button");
    }

    if (ImGui::BeginPopup("RenameRoomModal")) {
      const auto apply = [&](std::optional<std::string> label) {
        const absl::Status status =
            room_label_edit_callback_
                ? room_label_edit_callback_(room_id, std::move(label))
                : absl::FailedPreconditionError("Room names are read-only");
        if (status.ok()) {
          ImGui::CloseCurrentPopup();
        } else {
          room_rename_error_ = std::string(status.message());
        }
      };

      ImGui::Text(ICON_MD_EDIT " Rename Room 0x%03X", room_id);
      ImGui::Separator();
      ImGui::Spacing();

      ImGui::TextDisabled(tr("Project Room Name:"));
      ImGui::SetNextItemWidth(260.0f);
      const bool enter_pressed = ImGui::InputText(
          "##RoomNameInput", room_rename_buffer_, sizeof(room_rename_buffer_),
          ImGuiInputTextFlags_EnterReturnsTrue);
      gui::AutoRegisterLastItem("input_text", "rename_room_input");

      ImGui::Spacing();
      const bool has_name = room_rename_buffer_[0] != '\0';
      ImGui::BeginDisabled(!has_name);
      const bool save_clicked = ImGui::Button(ICON_MD_SAVE " Save");
      ImGui::EndDisabled();
      gui::AutoRegisterLastItem("button", "rename_room_save");
      if (has_name && (save_clicked || enter_pressed)) {
        apply(std::string(room_rename_buffer_));
      }

      ImGui::SameLine();
      if (dungeon_project_labels::HasCustomRoomLabel(viewer.project(),
                                                     room_id)) {
        if (ImGui::Button(ICON_MD_RESTORE " Reset")) {
          apply(std::nullopt);
        }
        gui::AutoRegisterLastItem("button", "rename_room_reset");
        if (ImGui::IsItemHovered()) {
          ImGui::SetTooltip(tr("Reset to default/registry name"));
        }
        ImGui::SameLine();
      }

      if (ImGui::Button(ICON_MD_CANCEL " Cancel")) {
        ImGui::CloseCurrentPopup();
      }
      gui::AutoRegisterLastItem("button", "rename_room_cancel");

      if (!room_rename_error_.empty()) {
        ImGui::TextColored(theme.status_error, "%s",
                           room_rename_error_.c_str());
      }

      ImGui::EndPopup();
    }

    if (room_dirty) {
      ImGui::SameLine();
      ImGui::TextColored(theme.status_warning, "Unsaved");
    }

    // Dungeon group context: prefer ROM entrance-based lookup; fall back to blockset.
    if (!room_dungeon_cache_built_ && rom_ && rom_->is_loaded()) {
      BuildRoomDungeonCache();
    }
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
    const bool is_custom_name =
        dungeon_project_labels::HasCustomRoomLabel(viewer.project(), room_id);
    ImGui::PushTextWrapPos(0.0f);
    if (group_name) {
      ImGui::TextDisabled("%s – %s", group_name, room_label.c_str());
    } else {
      ImGui::TextDisabled("%s", room_label.c_str());
    }
    if (is_custom_name) {
      ImGui::SameLine();
      ImGui::TextColored(theme.text_info, "(custom)");
    }
    ImGui::PopTextWrapPos();
  } else {
    ImGui::TextUnformatted(tr("Room: None"));
  }
}

void DungeonWorkbenchContent::DrawRoomContextRow(DungeonCanvasViewer& viewer) {
  auto* rooms = viewer.rooms();
  const int room_id = viewer.current_room_id();
  if (!rooms || room_id < 0 || room_id >= static_cast<int>(rooms->size())) {
    return;
  }
  const bool wide =
      ImGui::GetContentRegionAvail().x >= ImGui::GetFontSize() * 55.0f;
  ImGui::PushID("WorkbenchRoomContext");
  if (ImGui::BeginTable("##RoomContextRow", wide ? 2 : 1,
                        ImGuiTableFlags_BordersOuterH |
                            ImGuiTableFlags_NoPadOuterX |
                            ImGuiTableFlags_SizingStretchProp)) {
    if (wide) {
      ImGui::TableSetupColumn("Room", ImGuiTableColumnFlags_WidthFixed,
                              ImGui::GetFontSize() * 19.0f);
      ImGui::TableSetupColumn("Properties", ImGuiTableColumnFlags_WidthStretch);
    }
    ImGui::TableNextColumn();
    DrawRoomContextHeader(viewer);
    ImGui::TableNextColumn();
    DrawDungeonRoomPropertyFields(viewer, (*rooms)[room_id], room_id,
                                  RoomPropertyLayout::kCoreGraphicsRow, [&]() {
                                    DrawPitDamageControls(room_id, true);
                                  });
    ImGui::EndTable();
  }
  ImGui::PopID();
}

void DungeonWorkbenchContent::DrawInspectorShelfRoom(
    DungeonCanvasViewer& viewer) {
  int room_id = viewer.current_room_id();
  if (room_id < 0 && current_room_id_) {
    room_id = *current_room_id_;
  }
  if (!layout_state_.show_room_context_row) {
    DrawRoomContextHeader(viewer);
  }

  // Room properties share the editor's validated metadata edit boundary.
  if (auto* rooms = viewer.rooms();
      rooms && room_id >= 0 && room_id < static_cast<int>(rooms->size())) {
    auto& room = (*rooms)[room_id];

    if (!layout_state_.show_room_context_row) {
      ImGui::Spacing();
      DrawDungeonRoomPropertyFields(
          viewer, room, room_id, RoomPropertyLayout::kCoreGraphicsRow,
          [&]() { DrawPitDamageControls(room_id, true); });
    }

    ImGui::Spacing();
    workbench::DrawInspectorSectionHeader("Room behavior");
    DrawDungeonRoomPropertyFields(viewer, room, room_id,
                                  RoomPropertyLayout::kRoomDetails);
    ImGui::Spacing();
    workbench::DrawInspectorSectionHeader("Warps & stairs");
    DrawDungeonDestinationEditor(viewer);
    if (viewer.current_room_id() != room_id)
      return;
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
