#include "dungeon_toolset.h"

#include <algorithm>
#include <array>

#include "app/gui/core/icons.h"
#include "imgui/imgui.h"

namespace yaze::editor {

using ImGui::BeginTable;
using ImGui::Button;
using ImGui::EndTable;
using ImGui::RadioButton;
using ImGui::TableNextColumn;
using ImGui::TableSetupColumn;
using ImGui::Text;

void DungeonToolset::Draw() {
  if (BeginTable("DWToolset", 16, ImGuiTableFlags_SizingFixedFit, ImVec2(0, 0))) {
    static std::array<const char*, 16> tool_names = {
        "Undo", "Redo",      "Separator", "All",    "BG1",  "BG2",
        "BG3",  "Separator", "Object",    "Sprite", "Item", "Entrance",
        "Door", "Chest",     "Block",     "Palette"};
    std::ranges::for_each(tool_names,
                          [](const char* name) { TableSetupColumn(name); });

    // Undo button
    TableNextColumn();
    if (Button(ICON_MD_UNDO)) {
      if (undo_callback_) undo_callback_();
    }

    // Redo button
    TableNextColumn();
    if (Button(ICON_MD_REDO)) {
      if (redo_callback_) redo_callback_();
    }

    // Separator
    TableNextColumn();
    Text(ICON_MD_MORE_VERT);

    // Background layer selection
    TableNextColumn();
    if (RadioButton("All", background_type_ == kBackgroundAny)) {
      background_type_ = kBackgroundAny;
    }
    if (ImGui::IsItemHovered()) {
      ImGui::SetTooltip("Show all background layers");
    }

    TableNextColumn();
    if (RadioButton("BG1", background_type_ == kBackground1)) {
      background_type_ = kBackground1;
    }
    if (ImGui::IsItemHovered()) {
      ImGui::SetTooltip("Show background layer 1 only");
    }

    TableNextColumn();
    if (RadioButton("BG2", background_type_ == kBackground2)) {
      background_type_ = kBackground2;
    }
    if (ImGui::IsItemHovered()) {
      ImGui::SetTooltip("Show background layer 2 only");
    }

    TableNextColumn();
    if (RadioButton("BG3", background_type_ == kBackground3)) {
      background_type_ = kBackground3;
    }
    if (ImGui::IsItemHovered()) {
      ImGui::SetTooltip("Show background layer 3 only");
    }

    // Separator
    TableNextColumn();
    Text(ICON_MD_MORE_VERT);

    // Placement mode selection
    TableNextColumn();
    if (RadioButton(ICON_MD_SQUARE, placement_type_ == kObject)) {
      placement_type_ = kObject;
    }
    if (ImGui::IsItemHovered()) {
      ImGui::SetTooltip("Objects");
    }

    TableNextColumn();
    if (RadioButton(ICON_MD_PEST_CONTROL, placement_type_ == kSprite)) {
      placement_type_ = kSprite;
    }
    if (ImGui::IsItemHovered()) {
      ImGui::SetTooltip("Sprites");
    }

    TableNextColumn();
    if (RadioButton(ICON_MD_GRASS, placement_type_ == kItem)) {
      placement_type_ = kItem;
    }
    if (ImGui::IsItemHovered()) {
      ImGui::SetTooltip("Items");
    }

    TableNextColumn();
    if (RadioButton(ICON_MD_NAVIGATION, placement_type_ == kEntrance)) {
      placement_type_ = kEntrance;
    }
    if (ImGui::IsItemHovered()) {
      ImGui::SetTooltip("Entrances");
    }

    TableNextColumn();
    if (RadioButton(ICON_MD_SENSOR_DOOR, placement_type_ == kDoor)) {
      placement_type_ = kDoor;
    }
    if (ImGui::IsItemHovered()) {
      ImGui::SetTooltip("Doors");
    }

    TableNextColumn();
    if (RadioButton(ICON_MD_INVENTORY, placement_type_ == kChest)) {
      placement_type_ = kChest;
    }
    if (ImGui::IsItemHovered()) {
      ImGui::SetTooltip("Chests");
    }

    TableNextColumn();
    if (RadioButton(ICON_MD_VIEW_MODULE, placement_type_ == kBlock)) {
      placement_type_ = kBlock;
    }
    if (ImGui::IsItemHovered()) {
      ImGui::SetTooltip("Blocks");
    }

    // Palette button
    TableNextColumn();
    if (Button(ICON_MD_PALETTE)) {
      if (palette_toggle_callback_) palette_toggle_callback_();
    }

    ImGui::EndTable();
  }
  
  ImGui::Separator();
  ImGui::Text("Instructions: Click to place objects, Ctrl+Click to select, drag to move");
}

}  // namespace yaze::editor
