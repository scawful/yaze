#include "app/editor/ui/selection_properties_panel.h"

#include <cstdio>

#include "app/gui/core/icons.h"
#include "app/gui/core/style.h"
#include "app/gui/core/theme_manager.h"
#include "rom/rom.h"
#include "imgui/imgui.h"

namespace yaze {
namespace editor {

const char* GetSelectionTypeName(SelectionType type) {
  switch (type) {
    case SelectionType::kNone:
      return "None";
    case SelectionType::kDungeonRoom:
      return "Dungeon Room";
    case SelectionType::kDungeonObject:
      return "Dungeon Object";
    case SelectionType::kDungeonSprite:
      return "Dungeon Sprite";
    case SelectionType::kDungeonEntrance:
      return "Dungeon Entrance";
    case SelectionType::kOverworldMap:
      return "Overworld Map";
    case SelectionType::kOverworldTile:
      return "Overworld Tile";
    case SelectionType::kOverworldSprite:
      return "Overworld Sprite";
    case SelectionType::kOverworldEntrance:
      return "Overworld Entrance";
    case SelectionType::kOverworldExit:
      return "Overworld Exit";
    case SelectionType::kOverworldItem:
      return "Overworld Item";
    case SelectionType::kGraphicsSheet:
      return "Graphics Sheet";
    case SelectionType::kPalette:
      return "Palette";
    default:
      return "Unknown";
  }
}

void SelectionPropertiesPanel::SetSelection(const SelectionContext& context) {
  selection_ = context;
}

void SelectionPropertiesPanel::ClearSelection() {
  selection_ = SelectionContext{};
}

void SelectionPropertiesPanel::Draw() {
  if (selection_.type != SelectionType::kNone) {
    DrawSelectionSummary();
    ImGui::Spacing();
  }

  switch (selection_.type) {
    case SelectionType::kNone:
      DrawNoSelection();
      break;
    case SelectionType::kDungeonRoom:
      DrawDungeonRoomProperties();
      break;
    case SelectionType::kDungeonObject:
      DrawDungeonObjectProperties();
      break;
    case SelectionType::kDungeonSprite:
      DrawDungeonSpriteProperties();
      break;
    case SelectionType::kDungeonEntrance:
      DrawDungeonEntranceProperties();
      break;
    case SelectionType::kOverworldMap:
      DrawOverworldMapProperties();
      break;
    case SelectionType::kOverworldTile:
      DrawOverworldTileProperties();
      break;
    case SelectionType::kOverworldSprite:
      DrawOverworldSpriteProperties();
      break;
    case SelectionType::kOverworldEntrance:
      DrawOverworldEntranceProperties();
      break;
    case SelectionType::kOverworldExit:
      DrawOverworldExitProperties();
      break;
    case SelectionType::kOverworldItem:
      DrawOverworldItemProperties();
      break;
    case SelectionType::kGraphicsSheet:
      DrawGraphicsSheetProperties();
      break;
    case SelectionType::kPalette:
      DrawPaletteProperties();
      break;
  }

  // Advanced options toggle
  ImGui::Spacing();
  ImGui::Separator();
  ImGui::Spacing();
  ImGui::Checkbox("Show Advanced", &show_advanced_);
  ImGui::SameLine();
  ImGui::Checkbox("Raw Data", &show_raw_data_);
}

void SelectionPropertiesPanel::DrawNoSelection() {
  const auto& theme = gui::ThemeManager::Get().GetCurrentTheme();

  ImGui::PushStyleColor(ImGuiCol_Text, gui::GetTextDisabledVec4());
  ImGui::Text(ICON_MD_TOUCH_APP " Select an Item");
  ImGui::PopStyleColor();

  ImGui::Spacing();
  ImGui::TextWrapped(
      "Click on an object in the editor to view and edit its properties.");

  ImGui::Spacing();
  ImGui::Separator();
  ImGui::Spacing();

  // Show quick reference for what can be selected
  ImGui::TextDisabled("Selectable Items:");
  ImGui::BulletText("Dungeon: Rooms, Objects, Sprites");
  ImGui::BulletText("Overworld: Maps, Tiles, Entities");
  ImGui::BulletText("Graphics: Sheets, Palettes");
}

void SelectionPropertiesPanel::DrawPropertyHeader(const char* icon,
                                                   const char* title) {
  ImGui::PushStyleColor(ImGuiCol_Text, gui::GetPrimaryVec4());
  ImGui::Text("%s", icon);
  ImGui::PopStyleColor();
  ImGui::SameLine();
  ImGui::Text("%s", title);

  if (!selection_.display_name.empty()) {
    ImGui::SameLine();
    ImGui::TextDisabled("- %s", selection_.display_name.c_str());
  }

  if (selection_.read_only) {
    ImGui::SameLine();
    ImGui::TextDisabled("(Read Only)");
  }

  ImGui::Separator();
  ImGui::Spacing();
}

void SelectionPropertiesPanel::DrawSelectionSummary() {
  ImGui::PushStyleColor(ImGuiCol_Text, gui::GetPrimaryVec4());
  ImGui::Text("%s Selection", ICON_MD_INFO);
  ImGui::PopStyleColor();
  ImGui::Separator();

  ImGui::Text("Type: %s", GetSelectionTypeName(selection_.type));
  if (!selection_.display_name.empty()) {
    ImGui::Text("Name: %s", selection_.display_name.c_str());
  }
  if (selection_.id >= 0) {
    ImGui::Text("ID: %d (0x%X)", selection_.id, selection_.id);
  }
  if (selection_.secondary_id >= 0) {
    ImGui::Text("Secondary: %d (0x%X)", selection_.secondary_id,
                selection_.secondary_id);
  }
  if (selection_.read_only) {
    ImGui::TextDisabled("Read Only");
  }

  ImGui::Spacing();
  bool wrote_action = false;
  if (selection_.id >= 0) {
    if (ImGui::SmallButton("Copy ID")) {
      char buffer[32];
      std::snprintf(buffer, sizeof(buffer), "%d", selection_.id);
      ImGui::SetClipboardText(buffer);
    }
    wrote_action = true;
  }
  if (selection_.id >= 0) {
    if (wrote_action) ImGui::SameLine();
    if (ImGui::SmallButton("Copy Hex")) {
      char buffer[32];
      std::snprintf(buffer, sizeof(buffer), "0x%X", selection_.id);
      ImGui::SetClipboardText(buffer);
    }
    wrote_action = true;
  }
  if (!selection_.display_name.empty()) {
    if (wrote_action) ImGui::SameLine();
    if (ImGui::SmallButton("Copy Name")) {
      ImGui::SetClipboardText(selection_.display_name.c_str());
    }
    wrote_action = true;
  }

  if (show_raw_data_) {
    ImGui::Spacing();
    ImGui::TextDisabled("Data Ptr: %p", selection_.data);
  }
}

bool SelectionPropertiesPanel::DrawPositionEditor(const char* label, int* x,
                                                   int* y, int min_val,
                                                   int max_val) {
  bool changed = false;
  ImGui::PushID(label);

  ImGui::Text("%s", label);
  ImGui::PushItemWidth(80);

  ImGui::Text("X:");
  ImGui::SameLine();
  if (ImGui::InputInt("##X", x, 1, 8)) {
    *x = std::clamp(*x, min_val, max_val);
    changed = true;
  }

  ImGui::SameLine();
  ImGui::Text("Y:");
  ImGui::SameLine();
  if (ImGui::InputInt("##Y", y, 1, 8)) {
    *y = std::clamp(*y, min_val, max_val);
    changed = true;
  }

  ImGui::PopItemWidth();
  ImGui::PopID();

  return changed;
}

bool SelectionPropertiesPanel::DrawSizeEditor(const char* label, int* width,
                                               int* height) {
  bool changed = false;
  ImGui::PushID(label);

  ImGui::Text("%s", label);
  ImGui::PushItemWidth(80);

  ImGui::Text("W:");
  ImGui::SameLine();
  if (ImGui::InputInt("##W", width, 1, 8)) {
    *width = std::max(1, *width);
    changed = true;
  }

  ImGui::SameLine();
  ImGui::Text("H:");
  ImGui::SameLine();
  if (ImGui::InputInt("##H", height, 1, 8)) {
    *height = std::max(1, *height);
    changed = true;
  }

  ImGui::PopItemWidth();
  ImGui::PopID();

  return changed;
}

bool SelectionPropertiesPanel::DrawByteProperty(const char* label,
                                                 uint8_t* value,
                                                 const char* tooltip) {
  bool changed = false;
  int val = *value;

  ImGui::PushItemWidth(80);
  if (ImGui::InputInt(label, &val, 1, 16)) {
    *value = static_cast<uint8_t>(std::clamp(val, 0, 255));
    changed = true;
  }
  ImGui::PopItemWidth();

  if (tooltip && ImGui::IsItemHovered()) {
    ImGui::SetTooltip("%s", tooltip);
  }

  return changed;
}

bool SelectionPropertiesPanel::DrawWordProperty(const char* label,
                                                 uint16_t* value,
                                                 const char* tooltip) {
  bool changed = false;
  int val = *value;

  ImGui::PushItemWidth(100);
  if (ImGui::InputInt(label, &val, 1, 256)) {
    *value = static_cast<uint16_t>(std::clamp(val, 0, 65535));
    changed = true;
  }
  ImGui::PopItemWidth();

  if (tooltip && ImGui::IsItemHovered()) {
    ImGui::SetTooltip("%s", tooltip);
  }

  return changed;
}

bool SelectionPropertiesPanel::DrawComboProperty(const char* label,
                                                  int* current_item,
                                                  const char* const items[],
                                                  int items_count) {
  return ImGui::Combo(label, current_item, items, items_count);
}

bool SelectionPropertiesPanel::DrawFlagsProperty(const char* label,
                                                  uint8_t* flags,
                                                  const char* const flag_names[],
                                                  int flag_count) {
  bool changed = false;

  if (ImGui::TreeNode(label)) {
    for (int i = 0; i < flag_count && i < 8; ++i) {
      bool bit_set = (*flags >> i) & 1;
      if (ImGui::Checkbox(flag_names[i], &bit_set)) {
        if (bit_set) {
          *flags |= (1 << i);
        } else {
          *flags &= ~(1 << i);
        }
        changed = true;
      }
    }
    ImGui::TreePop();
  }

  return changed;
}

void SelectionPropertiesPanel::DrawReadOnlyText(const char* label,
                                                 const char* value) {
  ImGui::Text("%s:", label);
  ImGui::SameLine();
  ImGui::TextDisabled("%s", value);
}

void SelectionPropertiesPanel::DrawReadOnlyHex(const char* label,
                                                uint32_t value, int digits) {
  ImGui::Text("%s:", label);
  ImGui::SameLine();
  char fmt[16];
  snprintf(fmt, sizeof(fmt), "0x%%0%dX", digits);
  ImGui::TextDisabled(fmt, value);
}

void SelectionPropertiesPanel::NotifyChange() {
  if (on_change_) {
    on_change_(selection_);
  }
}

// ============================================================================
// Type-specific property editors
// ============================================================================

void SelectionPropertiesPanel::DrawDungeonRoomProperties() {
  DrawPropertyHeader(ICON_MD_GRID_VIEW, "Dungeon Room");

  if (ImGui::CollapsingHeader("Identity", ImGuiTreeNodeFlags_DefaultOpen)) {
    DrawReadOnlyHex("Room ID", selection_.id, 4);
    DrawReadOnlyText("Name", selection_.display_name.c_str());
  }

  if (ImGui::CollapsingHeader("Layout", ImGuiTreeNodeFlags_DefaultOpen)) {
    // Placeholder - actual implementation would use real room data
    ImGui::TextDisabled("Layout properties would appear here");
    ImGui::BulletText("Floor 1 tileset");
    ImGui::BulletText("Floor 2 tileset");
    ImGui::BulletText("Sprite graphics");
    ImGui::BulletText("Room palette");
  }

  if (show_advanced_ &&
      ImGui::CollapsingHeader("Advanced", ImGuiTreeNodeFlags_None)) {
    ImGui::TextDisabled("Advanced room settings");
    ImGui::BulletText("Room effects");
    ImGui::BulletText("Message ID");
    ImGui::BulletText("Tag 1 / Tag 2");
  }
}

void SelectionPropertiesPanel::DrawDungeonObjectProperties() {
  DrawPropertyHeader(ICON_MD_CATEGORY, "Dungeon Object");

  if (ImGui::CollapsingHeader("Transform", ImGuiTreeNodeFlags_DefaultOpen)) {
    // Placeholder for actual object data
    int x = 0, y = 0;
    if (DrawPositionEditor("Position", &x, &y, 0, 63)) {
      NotifyChange();
    }

    int w = 1, h = 1;
    if (DrawSizeEditor("Size", &w, &h)) {
      NotifyChange();
    }
  }

  if (ImGui::CollapsingHeader("Object Type", ImGuiTreeNodeFlags_DefaultOpen)) {
    ImGui::TextDisabled("Object ID: --");
    ImGui::TextDisabled("Subtype: --");
    ImGui::TextDisabled("Layer: --");
  }

  if (show_raw_data_ &&
      ImGui::CollapsingHeader("Raw Data", ImGuiTreeNodeFlags_None)) {
    ImGui::TextDisabled("Byte 1: 0x00");
    ImGui::TextDisabled("Byte 2: 0x00");
    ImGui::TextDisabled("Byte 3: 0x00");
  }
}

void SelectionPropertiesPanel::DrawDungeonSpriteProperties() {
  DrawPropertyHeader(ICON_MD_PEST_CONTROL, "Dungeon Sprite");

  if (ImGui::CollapsingHeader("Position", ImGuiTreeNodeFlags_DefaultOpen)) {
    int x = 0, y = 0;
    if (DrawPositionEditor("Position", &x, &y, 0, 255)) {
      NotifyChange();
    }
  }

  if (ImGui::CollapsingHeader("Sprite Data", ImGuiTreeNodeFlags_DefaultOpen)) {
    ImGui::TextDisabled("Sprite ID: --");
    ImGui::TextDisabled("Subtype: --");
    ImGui::TextDisabled("Overlord: No");
  }
}

void SelectionPropertiesPanel::DrawDungeonEntranceProperties() {
  DrawPropertyHeader(ICON_MD_DOOR_FRONT, "Dungeon Entrance");

  if (ImGui::CollapsingHeader("Target", ImGuiTreeNodeFlags_DefaultOpen)) {
    ImGui::TextDisabled("Target Room: --");
    ImGui::TextDisabled("Entry Position: --");
  }

  if (ImGui::CollapsingHeader("Properties", ImGuiTreeNodeFlags_DefaultOpen)) {
    ImGui::TextDisabled("Door Type: --");
    ImGui::TextDisabled("Direction: --");
  }
}

void SelectionPropertiesPanel::DrawOverworldMapProperties() {
  DrawPropertyHeader(ICON_MD_MAP, "Overworld Map");

  if (ImGui::CollapsingHeader("Identity", ImGuiTreeNodeFlags_DefaultOpen)) {
    DrawReadOnlyHex("Map ID", selection_.id, 2);
    DrawReadOnlyText("Area", selection_.display_name.c_str());
  }

  if (ImGui::CollapsingHeader("Graphics", ImGuiTreeNodeFlags_DefaultOpen)) {
    ImGui::TextDisabled("GFX Set: --");
    ImGui::TextDisabled("Palette: --");
    ImGui::TextDisabled("Sprite GFX: --");
    ImGui::TextDisabled("Sprite Palette: --");
  }

  if (ImGui::CollapsingHeader("Properties", ImGuiTreeNodeFlags_DefaultOpen)) {
    ImGui::TextDisabled("Large Map: No");
    ImGui::TextDisabled("Area Size: 1x1");
    ImGui::TextDisabled("Parent ID: --");
  }
}

void SelectionPropertiesPanel::DrawOverworldTileProperties() {
  DrawPropertyHeader(ICON_MD_GRID_ON, "Overworld Tile");

  if (ImGui::CollapsingHeader("Tile Info", ImGuiTreeNodeFlags_DefaultOpen)) {
    DrawReadOnlyHex("Tile16 ID", selection_.id, 4);
    ImGui::TextDisabled("Position: --");
  }

  if (show_advanced_ &&
      ImGui::CollapsingHeader("Tile16 Data", ImGuiTreeNodeFlags_None)) {
    ImGui::TextDisabled("TL: 0x0000");
    ImGui::TextDisabled("TR: 0x0000");
    ImGui::TextDisabled("BL: 0x0000");
    ImGui::TextDisabled("BR: 0x0000");
  }
}

void SelectionPropertiesPanel::DrawOverworldSpriteProperties() {
  DrawPropertyHeader(ICON_MD_PEST_CONTROL, "Overworld Sprite");

  if (ImGui::CollapsingHeader("Position", ImGuiTreeNodeFlags_DefaultOpen)) {
    int x = 0, y = 0;
    if (DrawPositionEditor("Position", &x, &y, 0, 8191)) {
      NotifyChange();
    }
  }

  if (ImGui::CollapsingHeader("Sprite", ImGuiTreeNodeFlags_DefaultOpen)) {
    ImGui::TextDisabled("Sprite ID: --");
    ImGui::TextDisabled("Map ID: --");
  }
}

void SelectionPropertiesPanel::DrawOverworldEntranceProperties() {
  DrawPropertyHeader(ICON_MD_DOOR_FRONT, "Overworld Entrance");

  if (ImGui::CollapsingHeader("Position", ImGuiTreeNodeFlags_DefaultOpen)) {
    int x = 0, y = 0;
    if (DrawPositionEditor("Position", &x, &y)) {
      NotifyChange();
    }
  }

  if (ImGui::CollapsingHeader("Target", ImGuiTreeNodeFlags_DefaultOpen)) {
    ImGui::TextDisabled("Entrance ID: --");
    ImGui::TextDisabled("Target Room: --");
  }
}

void SelectionPropertiesPanel::DrawOverworldExitProperties() {
  DrawPropertyHeader(ICON_MD_EXIT_TO_APP, "Overworld Exit");

  if (ImGui::CollapsingHeader("Exit Point", ImGuiTreeNodeFlags_DefaultOpen)) {
    ImGui::TextDisabled("Exit ID: --");
    int x = 0, y = 0;
    if (DrawPositionEditor("Position", &x, &y)) {
      NotifyChange();
    }
  }

  if (ImGui::CollapsingHeader("Target Map", ImGuiTreeNodeFlags_DefaultOpen)) {
    ImGui::TextDisabled("Room ID: --");
    ImGui::TextDisabled("Target Map: --");
  }
}

void SelectionPropertiesPanel::DrawOverworldItemProperties() {
  DrawPropertyHeader(ICON_MD_STAR, "Overworld Item");

  if (ImGui::CollapsingHeader("Position", ImGuiTreeNodeFlags_DefaultOpen)) {
    int x = 0, y = 0;
    if (DrawPositionEditor("Position", &x, &y)) {
      NotifyChange();
    }
  }

  if (ImGui::CollapsingHeader("Item Data", ImGuiTreeNodeFlags_DefaultOpen)) {
    ImGui::TextDisabled("Item ID: --");
    ImGui::TextDisabled("Map ID: --");
  }
}

void SelectionPropertiesPanel::DrawGraphicsSheetProperties() {
  DrawPropertyHeader(ICON_MD_IMAGE, "Graphics Sheet");

  if (ImGui::CollapsingHeader("Sheet Info", ImGuiTreeNodeFlags_DefaultOpen)) {
    DrawReadOnlyHex("Sheet ID", selection_.id, 2);
    ImGui::TextDisabled("Size: 128x32");
    ImGui::TextDisabled("BPP: 4");
  }

  if (show_advanced_ &&
      ImGui::CollapsingHeader("ROM Location", ImGuiTreeNodeFlags_None)) {
    ImGui::TextDisabled("Address: --");
    ImGui::TextDisabled("Compressed: Yes");
    ImGui::TextDisabled("Original Size: --");
  }
}

void SelectionPropertiesPanel::DrawPaletteProperties() {
  DrawPropertyHeader(ICON_MD_PALETTE, "Palette");

  if (ImGui::CollapsingHeader("Palette Info", ImGuiTreeNodeFlags_DefaultOpen)) {
    DrawReadOnlyHex("Palette ID", selection_.id, 2);
    ImGui::TextDisabled("Colors: 16");
  }

  if (ImGui::CollapsingHeader("Colors", ImGuiTreeNodeFlags_DefaultOpen)) {
    // Would show color swatches in actual implementation
    ImGui::TextDisabled("Color editing not yet implemented");
  }
}

}  // namespace editor
}  // namespace yaze
