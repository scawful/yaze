#include "app/editor/dungeon/inspectors/dungeon_chest_editor.h"

#include <algorithm>
#include <array>
#include <optional>
#include <string>

#include "absl/strings/ascii.h"
#include "absl/strings/match.h"
#include "absl/strings/str_format.h"
#include "app/editor/dungeon/dungeon_canvas_viewer.h"
#include "app/gui/automation/widget_auto_register.h"
#include "app/gui/core/input.h"
#include "imgui/imgui.h"
#include "zelda3/dungeon/chest_edit.h"
#include "zelda3/dungeon/dungeon_limits.h"
#include "zelda3/dungeon/object_dimensions.h"
#include "zelda3/dungeon/room.h"
#include "zelda3/resource_labels.h"

namespace yaze::editor {
namespace {

// Source-Origin: USDASM bank_09.asm, AncillaAdd_ItemReceipt.offset_y
// ($09836C-$0983B7). These are receipt IDs; the generic GetItemNames table is
// offset by an extra "None" entry and includes incompatible hack-only items.
constexpr std::array<const char*, 0x4C> kChestItemNames = {
    "Fighter Sword",
    "Master Sword",
    "Tempered Sword",
    "Golden Sword",
    "Fighter Shield",
    "Fire Shield",
    "Mirror Shield",
    "Fire Rod",
    "Ice Rod",
    "Hammer",
    "Hookshot",
    "Bow",
    "Boomerang",
    "Powder",
    "Bottle Refill (Bee)",
    "Bombos Medallion",
    "Ether Medallion",
    "Quake Medallion",
    "Lamp",
    "Shovel",
    "Flute",
    "Cane of Somaria",
    "Bottle",
    "Piece of Heart",
    "Cane of Byrna",
    "Cape",
    "Magic Mirror",
    "Power Glove",
    "Titan's Mitt",
    "Book of Mudora",
    "Flippers",
    "Moon Pearl",
    "Crystal",
    "Bug Net",
    "Blue Mail",
    "Red Mail",
    "Small Key",
    "Compass",
    "Heart Container (Four Pieces)",
    "Bomb",
    "3 Bombs",
    "Mushroom",
    "Red Boomerang",
    "Bottle (Red Potion)",
    "Bottle (Green Potion)",
    "Bottle (Blue Potion)",
    "Red Potion Refill",
    "Green Potion Refill",
    "Blue Potion Refill",
    "10 Bombs",
    "Big Key",
    "Map",
    "1 Rupee",
    "5 Rupees",
    "20 Rupees",
    "Green Pendant",
    "Blue Pendant",
    "Red Pendant",
    "Tossed Bow",
    "Silver Arrows",
    "Bottle (Bee)",
    "Bottle (Fairy)",
    "Heart Container (Boss)",
    "Heart Container (Sanctuary)",
    "100 Rupees",
    "50 Rupees",
    "Heart",
    "1 Arrow",
    "10 Arrows",
    "Small Magic",
    "300 Rupees",
    "20 Rupees (Green)",
    "Bottle (Good Bee)",
    "Tossed Fighter Sword",
    "Flute (Activated)",
    "Pegasus Boots"};

std::string ItemPreview(uint8_t item_id) {
  return absl::StrFormat("%02X  %s", item_id,
                         GetDungeonChestItemLabel(item_id));
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

void DrawItemChoice(int room_id, size_t index, const chest_data chest,
                    DungeonCanvasViewer& viewer,
                    DungeonChestEditorState& state) {
  ImGui::TextUnformatted("Reward");
  ImGui::SetNextItemWidth(-1);
  const bool open =
      ImGui::BeginCombo("##ChestReward", ItemPreview(chest.id).c_str(),
                        ImGuiComboFlags_HeightLarge);
  gui::AutoRegisterLastItem("combo", "ChestReward");
  if (!open) {
    return;
  }
  ImGui::SetNextItemWidth(-1);
  const bool search_changed =
      ImGui::InputTextWithHint("##ChestSearch", "Search name or hex ID...",
                               state.search.data(), state.search.size());
  gui::AutoRegisterLastItem("input", "ChestSearch");
  ImGui::Separator();
  const std::string search = absl::AsciiStrToLower(state.search.data());
  // Keep search visible when opening on a late item. The selected result may
  // scroll into view without moving the search control out of the popup.
  if (ImGui::BeginChild("##ChestRewardChoices",
                        ImVec2(0, ImGui::GetTextLineHeightWithSpacing() * 8))) {
    if (search_changed) {
      ImGui::SetScrollY(0);
    }
    auto& labels = zelda3::GetResourceLabels();
    bool has_matches = false;
    for (int id = 0; id <= 0xFF; ++id) {
      // Retain any existing unknown value and explicitly named hack items, but
      // do not suggest unverified high IDs as ordinary vanilla rewards.
      if (id >= static_cast<int>(kChestItemNames.size()) && id != chest.id &&
          !labels.HasProjectLabel(zelda3::ResourceType::kItem, id)) {
        continue;
      }
      const std::string label = ItemPreview(static_cast<uint8_t>(id));
      if (!absl::StrContains(absl::AsciiStrToLower(label), search)) {
        continue;
      }
      has_matches = true;
      if (ImGui::Selectable(label.c_str(), chest.id == id)) {
        ApplyEdit(viewer, state, room_id, index, static_cast<uint8_t>(id),
                  chest.size);
        // The selectable is in a child window, so close its containing combo
        // explicitly after a committed choice.
        ImGui::CloseCurrentPopup();
      }
      gui::AutoRegisterLastItem("selectable",
                                absl::StrFormat("ChestReward%02X", id));
      if (chest.id == id && search.empty()) {
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

}  // namespace

std::string GetDungeonChestItemLabel(uint8_t item_id) {
  auto& labels = zelda3::GetResourceLabels();
  if (labels.HasProjectLabel(zelda3::ResourceType::kItem, item_id)) {
    return labels.GetLabel(zelda3::ResourceType::kItem, item_id);
  }
  if (item_id < kChestItemNames.size()) {
    return kChestItemNames[item_id];
  }
  return absl::StrFormat("Unknown item %02X", item_id);
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
  const std::string current =
      absl::StrFormat("Chest %d · %s", state.selected_index + 1,
                      ItemPreview(chests[state.selected_index].id));
  const bool open = ImGui::BeginCombo("##ChestRecord", current.c_str());
  gui::AutoRegisterLastItem("combo", "ChestRecord");
  if (open) {
    for (size_t i = 0; i < chests.size(); ++i) {
      const std::string label =
          absl::StrFormat("Chest %zu · %s", i + 1, ItemPreview(chests[i].id));
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
  DrawItemChoice(room_id, index, chest, viewer, state);
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
