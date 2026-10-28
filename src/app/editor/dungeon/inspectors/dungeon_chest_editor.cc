#include "app/editor/dungeon/inspectors/dungeon_chest_editor.h"

#include <algorithm>
#include <functional>
#include <optional>
#include <string>
#include <string_view>

#include "absl/strings/ascii.h"
#include "absl/strings/match.h"
#include "absl/strings/str_format.h"
#include "app/editor/dungeon/dungeon_canvas_viewer.h"
#include "app/gui/automation/widget_auto_register.h"
#include "app/gui/core/input.h"
#include "core/project.h"
#include "imgui/imgui.h"
#include "imgui/imgui_internal.h"
#include "zelda3/dungeon/chest_edit.h"
#include "zelda3/dungeon/dungeon_limits.h"
#include "zelda3/dungeon/object_dimensions.h"
#include "zelda3/dungeon/room.h"
#include "zelda3/resource_labels.h"
#include "zelda3/zelda3_labels.h"

namespace yaze::editor {
namespace {

std::string ItemPreview(uint8_t item_id, const project::YazeProject* project) {
  return absl::StrFormat("%02X  %s", item_id,
                         GetDungeonChestItemLabel(item_id, project));
}

void ApplyEdit(DungeonCanvasViewer& viewer, DungeonChestEditorState& state,
               int room_id, size_t index, uint8_t item_id, bool big_chest) {
  const auto status = viewer.EditChest(room_id, index, item_id, big_chest);
  state.error = status.ok() ? "" : std::string(status.message());
}

std::optional<size_t> FindChestObject(const zelda3::Room& room,
                                      size_t ordinal) {
  const auto& objects = room.GetTileObjects();
  for (size_t i = 0; i < objects.size(); ++i) {
    if (zelda3::ChestIndexForObject(objects, i) == ordinal) {
      return i;
    }
  }
  return std::nullopt;
}

void DrawChestPlacement(int room_id, bool has_mapping,
                        DungeonCanvasViewer& viewer,
                        DungeonChestEditorState& state) {
  const bool active_room = viewer.current_room_id() == room_id;
  ImGui::BeginDisabled(viewer.header_read_only() || !active_room ||
                       !has_mapping);
  const bool inline_buttons =
      ImGui::GetContentRegionAvail().x >=
      ImGui::CalcTextSize("Add small chestAdd big chest").x +
          ImGui::GetStyle().FramePadding.x * 4 +
          ImGui::GetStyle().ItemSpacing.x;
  const float width = inline_buttons ? (ImGui::GetContentRegionAvail().x -
                                        ImGui::GetStyle().ItemSpacing.x) /
                                           2
                                     : -1;
  for (int big = 0; big <= 1; ++big) {
    if (big && inline_buttons) {
      ImGui::SameLine();
    }
    if (ImGui::Button(big ? "Add big chest" : "Add small chest",
                      ImVec2(width, 0))) {
      const int id = big ? 0xFB1 : 0xF99;
      viewer.SetPreviewObject(
          zelda3::RoomObject(id, 0, 0, zelda3::CanonicalRoomObjectSize(id, 0)));
      viewer.SetObjectInteractionEnabled(true);
      state.error.clear();
    }
    gui::AutoRegisterLastItem("button", big ? "ChestAddBig" : "ChestAddSmall");
  }
  ImGui::EndDisabled();
  const auto* preview = viewer.object_interaction().GetPlacementPreview();
  if (active_room && preview && zelda3::IsStatefulChestObjectId(preview->id_)) {
    ImGui::TextWrapped(
        "Click in the canvas to place a %s chest. Escape cancels.",
        preview->id_ == 0xFB1 ? "big" : "small");
  } else if (!active_room) {
    ImGui::TextWrapped(
        "Open this room in the canvas to place or select a chest.");
  }
  ImGui::TextWrapped(
      "New chests start with 1 Rupee. Choose a chest below to change its "
      "reward.");
}

bool DrawChestActions(int room_id, size_t index, bool has_mapping,
                      zelda3::Room& room, DungeonCanvasViewer& viewer,
                      DungeonChestEditorState& state) {
  const auto object_index =
      has_mapping ? FindChestObject(room, index) : std::nullopt;
  ImGui::BeginDisabled(!object_index || viewer.current_room_id() != room_id);
  if (ImGui::Button("Select in canvas", ImVec2(-1, 0))) {
    const auto& object = room.GetTileObjects()[*object_index];
    viewer.object_interaction().CancelPlacement();
    viewer.object_interaction().SetSelectedObjects({*object_index});
    viewer.ScrollToTile(object.x_, object.y_);
    viewer.TriggerCanvasPingRect(object.x_ * 8, object.y_ * 8, object.width_,
                                 object.height_);
  }
  gui::AutoRegisterLastItem("button", "ChestSelect");
  ImGui::EndDisabled();
  ImGui::BeginDisabled(viewer.header_read_only() || !object_index);
  bool deleted = false;
  if (ImGui::Button("Delete chest", ImVec2(-1, 0))) {
    const auto status = viewer.DeleteChest(room_id, index);
    state.error = status.ok() ? "" : std::string(status.message());
    deleted = status.ok();
  }
  gui::AutoRegisterLastItem("button", "ChestDelete");
  if (ImGui::IsItemHovered()) {
    ImGui::SetTooltip(
        "Remove the chest object and its reward. Undo restores both.");
  }
  ImGui::EndDisabled();
  return deleted;
}

void DrawItemChoice(uint8_t item_id, DungeonCanvasViewer& viewer,
                    std::array<char, 64>& search_buffer,
                    const std::function<void(uint8_t)>& select_item) {
  ImGui::TextUnformatted("Reward");
  ImGui::SetNextItemWidth(-1);
  const bool open = ImGui::BeginCombo(
      "##ChestReward", ItemPreview(item_id, viewer.project()).c_str(),
      ImGuiComboFlags_HeightLarge);
  gui::AutoRegisterLastItem("combo", "ChestReward");
  if (!open) {
    return;
  }
  ImGui::SetNextItemWidth(-1);
  const bool search_changed =
      ImGui::InputTextWithHint("##ChestSearch", "Search name or hex ID...",
                               search_buffer.data(), search_buffer.size());
  gui::AutoRegisterLastItem("input", "ChestSearch");
  ImGui::Separator();
  const std::string search = absl::AsciiStrToLower(search_buffer.data());
  // Keep search visible when opening on a late item. The selected result may
  // scroll into view without moving the search control out of the popup.
  if (ImGui::BeginChild("##ChestRewardChoices",
                        ImVec2(0, ImGui::GetTextLineHeightWithSpacing() * 8))) {
    if (search_changed) {
      ImGui::SetScrollY(0);
    }
    const auto* labels =
        viewer.project() ? &viewer.project()->resource_labels : nullptr;
    bool has_matches = false;
    for (int id = 0; id <= 0xFF; ++id) {
      // Retain any existing unknown value and explicitly named hack items, but
      // do not suggest unverified high IDs as ordinary vanilla rewards.
      if (id >= static_cast<int>(
                    zelda3::Zelda3Labels::GetItemReceiptNames().size()) &&
          id != item_id &&
          std::string_view(zelda3::GetItemReceiptLabelSource(
              static_cast<uint8_t>(id), labels)) != "project") {
        continue;
      }
      const std::string label =
          ItemPreview(static_cast<uint8_t>(id), viewer.project());
      if (!absl::StrContains(absl::AsciiStrToLower(label), search)) {
        continue;
      }
      has_matches = true;
      if (ImGui::Selectable(label.c_str(), item_id == id)) {
        select_item(static_cast<uint8_t>(id));
        // The selectable is in a child window, so close its containing combo
        // explicitly after a committed choice.
        ImGui::CloseCurrentPopup();
      }
      gui::AutoRegisterLastItem("selectable",
                                absl::StrFormat("ChestReward%02X", id));
      if (item_id == id && search.empty()) {
        ImGui::SetItemDefaultFocus();
      }
    }
    if (!has_matches) {
      ImGui::TextDisabled("No matching rewards");
    }
  }
  ImGui::EndChild();
  ImGui::EndCombo();
}

std::vector<uint64_t> ChestObjectSignature(const zelda3::Room& room) {
  std::vector<uint64_t> result;
  for (const auto& object : room.GetTileObjects()) {
    result.push_back(static_cast<uint16_t>(object.id_) |
                     (static_cast<uint64_t>(object.x_) << 16) |
                     (static_cast<uint64_t>(object.y_) << 24) |
                     (static_cast<uint64_t>(object.size_) << 32) |
                     (static_cast<uint64_t>(object.layer_) << 40));
  }
  return result;
}

std::vector<uint16_t> ChestRecordSignature(const zelda3::Room& room) {
  std::vector<uint16_t> result;
  for (const auto& chest : room.GetChests()) {
    result.push_back(chest.id | (static_cast<uint16_t>(chest.size) << 8));
  }
  return result;
}

absl::Status ValidateChestDialogTarget(
    const DungeonCanvasViewer& viewer,
    const DungeonChestEditDialogState& state) {
  const auto* room =
      viewer.rooms() ? viewer.rooms()->GetIfLoaded(state.room_id) : nullptr;
  if (!room || !room->AreChestsLoaded() ||
      reinterpret_cast<uintptr_t>(room) != state.room_identity ||
      reinterpret_cast<uintptr_t>(viewer.rom()) != state.rom_identity ||
      state.index >= room->GetChests().size() ||
      ChestObjectSignature(*room) != state.source_objects ||
      ChestRecordSignature(*room) != state.source_chests) {
    return absl::FailedPreconditionError(
        "This chest changed while the editor was open. Cancel and reopen it.");
  }
  if (viewer.header_read_only() || !viewer.IsObjectInteractionEnabled()) {
    return absl::FailedPreconditionError("Chest contents are read-only.");
  }
  return absl::OkStatus();
}

}  // namespace

absl::Status DungeonCanvasViewer::OpenChestEditor(int room_id, size_t index) {
  auto* room = rooms_ ? rooms_->GetIfLoaded(room_id) : nullptr;
  if (header_read_only_ || !IsObjectInteractionEnabled() ||
      !chest_edit_callback_ || !room || !room->AreChestsLoaded() ||
      room->rom() != rom_) {
    return absl::FailedPreconditionError("Chest contents are not editable");
  }
  if (index >= room->GetChests().size()) {
    return absl::InvalidArgumentError(
        "Select an existing chest contents record");
  }
  auto& state = chest_edit_dialog_state_;
  state.open = true;
  state.request_open = true;
  state.room_id = room_id;
  state.index = index;
  state.room_identity = reinterpret_cast<uintptr_t>(room);
  state.rom_identity = reinterpret_cast<uintptr_t>(rom_);
  state.source_objects = ChestObjectSignature(*room);
  state.source_chests = ChestRecordSignature(*room);
  state.item_id = room->GetChests()[index].id;
  state.big_chest = room->GetChests()[index].size;
  state.mapping_valid = zelda3::ValidateChestObjectMapping(
                            room->GetTileObjects(), room->GetChests())
                            .ok();
  state.search.fill(0);
  state.error.clear();
  return absl::OkStatus();
}

void DungeonCanvasViewer::DrawChestEditorPopup() {
  if (!ImGui::GetCurrentContext()) {
    return;
  }
  auto& state = chest_edit_dialog_state_;
  const int frame = ImGui::GetFrameCount();
  if (state.last_draw_frame == frame) {
    return;
  }
  state.last_draw_frame = frame;
  // Use a viewer-owned ID independent of the canvas/inspector's current child.
  // Workbench and standalone callers can safely render this once per frame.
  const auto owner = reinterpret_cast<uintptr_t>(this);
  ImGui::PushOverrideID(ImHashData(&owner, sizeof(owner)));
  const std::string popup_name =
      absl::StrFormat("Edit Chest###DungeonChestDialog%p", this);
  if (state.show_diagnostics) {
    ImGui::SetNextWindowSize(ImVec2(380, 580), ImGuiCond_FirstUseEver);
    const std::string title =
        absl::StrFormat("Chest Records###ChestRecords%p", this);
    if (ImGui::Begin(title.c_str(), &state.show_diagnostics,
                     ImGuiWindowFlags_NoSavedSettings)) {
      if (auto* room = rooms_ ? rooms_->GetIfLoaded(state.diagnostics_room_id)
                              : nullptr) {
        DrawDungeonChestEditor(state.diagnostics_room_id, *room, *this);
      } else {
        ImGui::TextUnformatted("Open this room to load its chest records.");
      }
    }
    ImGui::End();
  }
  if (state.request_open) {
    ImGui::OpenPopup(popup_name.c_str());
    state.request_open = false;
  }
  if (state.open) {
    ImGui::SetNextWindowSize(ImVec2(400, 0), ImGuiCond_Appearing);
  }
  if (ImGui::BeginPopupModal(popup_name.c_str(), &state.open,
                             ImGuiWindowFlags_AlwaysAutoResize |
                                 ImGuiWindowFlags_NoSavedSettings)) {
    {
      gui::AutoWidgetScope scope("Dungeon/ChestDialog");
      ImGui::Text("Room %03X · Chest %zu", state.room_id, state.index + 1);
      const auto target_status = ValidateChestDialogTarget(*this, state);
      ImGui::BeginDisabled(!target_status.ok());
      DrawItemChoice(static_cast<uint8_t>(state.item_id), *this, state.search,
                     [&](uint8_t id) { state.item_id = id; });
      ImGui::BeginDisabled(!state.mapping_valid);
      ImGui::Checkbox("Big chest", &state.big_chest);
      gui::AutoRegisterLastItem("checkbox", "ChestBig");
      ImGui::EndDisabled();
      if (!state.mapping_valid) {
        ImGui::TextWrapped(
            "Chest records do not match their objects. Only the reward can be "
            "changed.");
      }
      ImGui::SetNextItemWidth(ImGui::CalcTextSize("000").x +
                              ImGui::GetStyle().FramePadding.x * 2);
      ImGui::InputScalar("Item ID", ImGuiDataType_S32, &state.item_id, nullptr,
                         nullptr, "%02X", ImGuiInputTextFlags_CharsHexadecimal);
      gui::AutoRegisterLastItem("input_int", "ChestItemId");
      ImGui::EndDisabled();
      if (!target_status.ok()) {
        ImGui::TextWrapped("%s", std::string(target_status.message()).c_str());
      } else if (!state.error.empty()) {
        ImGui::TextWrapped("Edit not applied: %s", state.error.c_str());
      }
      ImGui::Separator();
      ImGui::BeginDisabled(!target_status.ok());
      if (ImGui::Button("OK")) {
        const auto status = ValidateChestDialogTarget(*this, state);
        if (!status.ok()) {
          state.error = std::string(status.message());
        } else if (state.item_id < 0 || state.item_id > 0xFF) {
          state.error = "Item ID must fit one byte (00-FF).";
        } else {
          const auto status =
              EditChest(state.room_id, state.index,
                        static_cast<uint8_t>(state.item_id), state.big_chest);
          state.error = status.ok() ? "" : std::string(status.message());
          if (status.ok()) {
            state.open = false;
            ImGui::CloseCurrentPopup();
          }
        }
      }
      gui::AutoRegisterLastItem("button", "ChestOK");
      ImGui::EndDisabled();
      ImGui::SameLine();
      const bool cancel = ImGui::Button("Cancel");
      gui::AutoRegisterLastItem("button", "ChestCancel");
      if (cancel || ImGui::IsKeyPressed(ImGuiKey_Escape)) {
        state.open = false;
        ImGui::CloseCurrentPopup();
      }
    }
    ImGui::EndPopup();
  }
  ImGui::PopID();
}

std::string GetDungeonChestItemLabel(uint8_t item_id) {
  return zelda3::GetItemReceiptLabel(item_id);
}

std::string GetDungeonChestItemLabel(uint8_t item_id,
                                     const project::YazeProject* project) {
  return zelda3::GetItemReceiptLabel(
      item_id, project ? &project->resource_labels : nullptr);
}

void DrawDungeonChestEditor(int room_id, zelda3::Room& room,
                            DungeonCanvasViewer& viewer) {
  ImGui::TextUnformatted("Chests");
  if (!viewer.rooms() || viewer.rooms()->GetIfLoaded(room_id) != &room ||
      !room.AreChestsLoaded()) {
    ImGui::TextWrapped(
        "Open this room in the canvas to load its chest contents.");
    return;
  }
  auto& state = viewer.chest_editor_state();
  if (state.room_id != room_id) {
    state = DungeonChestEditorState{};
    state.room_id = room_id;
  }
  gui::AutoWidgetScope scope("Dungeon/ChestEditor");
  ImGui::PushID("DungeonChestEditor");
  ImGui::PushID(room_id);
  const auto& chests = room.GetChests();
  size_t chest_objects = 0;
  for (const auto& object : room.GetTileObjects()) {
    chest_objects += zelda3::IsStatefulChestObjectId(object.id_);
  }
  const auto mapping_status =
      zelda3::ValidateChestObjectMapping(room.GetTileObjects(), chests);
  const bool has_mapping = mapping_status.ok();
  ImGui::TextWrapped("%zu contents records · %zu chest objects", chests.size(),
                     chest_objects);
  if (!has_mapping) {
    ImGui::TextWrapped(
        "Chest objects and contents do not match in room order. Rewards remain "
        "editable; adding, deleting, and changing chest type require a "
        "matching "
        "object for each record.");
    ImGui::TextWrapped("%s", std::string(mapping_status.message()).c_str());
  }
  if (chests.size() > zelda3::kMaxChests) {
    ImGui::TextWrapped("This room exceeds the standard six-chest limit.");
  }
  DrawChestPlacement(room_id, has_mapping, viewer, state);
  if (chests.empty()) {
    ImGui::TextWrapped("No chest contents records in this room.");
    state.selected_index = 0;
    state.last_canvas_selection.reset();
    state.error.clear();
    ImGui::PopID();
    ImGui::PopID();
    return;
  }
  if (viewer.current_room_id() == room_id) {
    const auto selection =
        viewer.object_interaction().GetSelectedObjectIndices();
    const auto canvas_selection = selection.size() == 1
                                      ? std::optional<size_t>(selection.front())
                                      : std::nullopt;
    if (canvas_selection != state.last_canvas_selection) {
      state.last_canvas_selection = canvas_selection;
      if (has_mapping && canvas_selection) {
        const auto ordinal = zelda3::ChestIndexForObject(room.GetTileObjects(),
                                                         *canvas_selection);
        if (ordinal) {
          state.selected_index = static_cast<int>(*ordinal);
          state.search.fill(0);
          state.error.clear();
        }
      }
    }
  }
  state.selected_index =
      std::clamp(state.selected_index, 0, static_cast<int>(chests.size()) - 1);
  ImGui::SetNextItemWidth(-1);
  const std::string current = absl::StrFormat(
      "Chest %d · %s", state.selected_index + 1,
      ItemPreview(chests[state.selected_index].id, viewer.project()));
  const bool open = ImGui::BeginCombo("##ChestRecord", current.c_str());
  gui::AutoRegisterLastItem("combo", "ChestRecord");
  if (open) {
    for (size_t i = 0; i < chests.size(); ++i) {
      const std::string label = absl::StrFormat(
          "Chest %zu · %s", i + 1, ItemPreview(chests[i].id, viewer.project()));
      if (ImGui::Selectable(label.c_str(), state.selected_index == i)) {
        state.selected_index = static_cast<int>(i);
        state.search.fill(0);
        state.error.clear();
      }
      gui::AutoRegisterLastItem("selectable",
                                absl::StrFormat("ChestRecord%zu", i));
    }
    ImGui::EndCombo();
  }
  const size_t index = static_cast<size_t>(state.selected_index);
  if (DrawChestActions(room_id, index, has_mapping, room, viewer, state)) {
    state.search.fill(0);
    ImGui::PopID();
    ImGui::PopID();
    return;
  }
  ImGui::PushID(state.selected_index);
  // The mutation callback may replace the vector. Do not keep a record
  // reference across calls into the editor's undo-backed mutation path.
  const auto chest = chests[index];
  ImGui::BeginDisabled(viewer.header_read_only());
  DrawItemChoice(chest.id, viewer, state.search, [&](uint8_t id) {
    ApplyEdit(viewer, state, room_id, index, id, chest.size);
  });
  bool big = room.GetChests()[index].size;
  ImGui::BeginDisabled(!has_mapping);
  if (ImGui::Checkbox("Big chest", &big)) {
    ApplyEdit(viewer, state, room_id, index, room.GetChests()[index].id, big);
  }
  gui::AutoRegisterLastItem("checkbox", "ChestBig");
  ImGui::EndDisabled();
  if (ImGui::TreeNode("Advanced##ChestAdvanced")) {
    int item_id = room.GetChests()[index].id;
    ImGui::SetNextItemWidth(-1);
    const bool changed =
        gui::InputScalarDeferred("##ChestItemId", ImGuiDataType_S32, &item_id,
                                 "%02X", ImGuiInputTextFlags_CharsHexadecimal,
                                 {reinterpret_cast<uintptr_t>(&viewer),
                                  static_cast<uint64_t>(room_id), index});
    gui::AutoRegisterLastItem("input_int", "ChestItemId");
    if (changed) {
      if (item_id < 0 || item_id > 0xFF) {
        state.error = "Item ID must fit one byte (00-FF).";
      } else {
        ApplyEdit(viewer, state, room_id, index, static_cast<uint8_t>(item_id),
                  room.GetChests()[index].size);
      }
    }
    ImGui::TextWrapped(
        "Raw item ID (hex). Unknown hack-specific values are kept.");
    ImGui::TreePop();
  }
  ImGui::EndDisabled();
  if (!state.error.empty()) {
    ImGui::TextWrapped("Edit not applied: %s", state.error.c_str());
  }
  ImGui::PopID();
  ImGui::PopID();
  ImGui::PopID();
}

}  // namespace yaze::editor
