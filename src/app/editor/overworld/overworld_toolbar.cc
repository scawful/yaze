#include "app/editor/overworld/overworld_toolbar.h"

#include "app/editor/overworld/map_properties.h"
#include "app/gui/core/layout_helpers.h"
#include "zelda3/overworld/overworld_version_helper.h"

namespace yaze::editor {

using ImGui::BeginTable;
using ImGui::TableNextColumn;

void OverworldToolbar::Draw(int& current_world, int& current_map,
                            bool& current_map_lock, EditingMode& current_mode,
                            EntityEditMode& entity_edit_mode,
                            bool& show_map_properties_panel,
                            bool& show_scratch_space, int& current_scratch_slot,
                            bool has_selection, bool scratch_has_data, Rom* rom,
                            zelda3::Overworld* overworld) {
  if (!overworld || !overworld->is_loaded()) return;

  // Simplified canvas toolbar - Navigation and Mode controls
  if (BeginTable("CanvasToolbar", 8,
                 ImGuiTableFlags_Borders | ImGuiTableFlags_SizingFixedFit,
                 ImVec2(0, 0), -1)) {
    ImGui::TableSetupColumn("World", ImGuiTableColumnFlags_WidthFixed,
                            kTableColumnWorld);
    ImGui::TableSetupColumn("Map", ImGuiTableColumnFlags_WidthFixed,
                            kTableColumnMap);
    ImGui::TableSetupColumn("Area Size", ImGuiTableColumnFlags_WidthFixed,
                            kTableColumnAreaSize);
    ImGui::TableSetupColumn("Lock", ImGuiTableColumnFlags_WidthFixed,
                            kTableColumnLock);
    ImGui::TableSetupColumn("Mode", ImGuiTableColumnFlags_WidthFixed,
                            80.0f);  // Mouse/Paint
    ImGui::TableSetupColumn("Entity",
                            ImGuiTableColumnFlags_WidthStretch);  // Entity status
    ImGui::TableSetupColumn("Scratch", ImGuiTableColumnFlags_WidthFixed, 120.0f);
    ImGui::TableSetupColumn("Sidebar", ImGuiTableColumnFlags_WidthFixed, 40.0f);

    TableNextColumn();
    ImGui::SetNextItemWidth(kComboWorldWidth);
    ImGui::Combo("##world", &current_world, kWorldNames, 3);

    TableNextColumn();
    ImGui::Text("%d (0x%02X)", current_map, current_map);

    TableNextColumn();
    // Use centralized version detection
    auto rom_version = zelda3::OverworldVersionHelper::GetVersion(*rom);

    // ALL ROMs support Small/Large. Only v3+ supports Wide/Tall.
    int current_area_size =
        static_cast<int>(overworld->overworld_map(current_map)->area_size());
    ImGui::SetNextItemWidth(kComboAreaSizeWidth);

    if (zelda3::OverworldVersionHelper::SupportsAreaEnum(rom_version)) {
      // v3+ ROM: Show all 4 area size options
      if (ImGui::Combo("##AreaSize", &current_area_size, kAreaSizeNames, 4)) {
        auto status = overworld->ConfigureMultiAreaMap(
            current_map, static_cast<zelda3::AreaSizeEnum>(current_area_size));
        if (status.ok()) {
          if (on_refresh_graphics) on_refresh_graphics();
          if (on_refresh_map) on_refresh_map();
        }
      }
    } else {
      // Vanilla/v1/v2 ROM: Show only Small/Large (first 2 options)
      const char* limited_names[] = {"Small (1x1)", "Large (2x2)"};
      int limited_size = (current_area_size == 0 || current_area_size == 1)
                             ? current_area_size
                             : 0;

      if (ImGui::Combo("##AreaSize", &limited_size, limited_names, 2)) {
        // limited_size is 0 (Small) or 1 (Large)
        auto size = (limited_size == 1) ? zelda3::AreaSizeEnum::LargeArea
                                        : zelda3::AreaSizeEnum::SmallArea;
        auto status = overworld->ConfigureMultiAreaMap(current_map, size);
        if (status.ok()) {
          if (on_refresh_graphics) on_refresh_graphics();
          if (on_refresh_map) on_refresh_map();
        }
      }

      if (rom_version == zelda3::OverworldVersion::kVanilla ||
          !zelda3::OverworldVersionHelper::SupportsAreaEnum(rom_version)) {
        HOVER_HINT("Small (1x1) and Large (2x2) maps. Wide/Tall require v3+");
      }
    }

    TableNextColumn();
    if (ImGui::Button(current_map_lock ? ICON_MD_LOCK : ICON_MD_LOCK_OPEN,
                      ImVec2(40, 0))) {
      current_map_lock = !current_map_lock;
    }
    HOVER_HINT(current_map_lock ? "Unlock Map" : "Lock Map");

    TableNextColumn();
    // Mode Controls
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0, 0));
    if (gui::ToggleButton(ICON_MD_MOUSE, current_mode == EditingMode::MOUSE,
                          ImVec2(30, 0))) {
      current_mode = EditingMode::MOUSE;
      // Note: Canvas mode update handled by editor
    }
    HOVER_HINT("Mouse Mode (1)\nNavigate, pan, and manage entities");

    ImGui::SameLine();
    if (gui::ToggleButton(ICON_MD_DRAW, current_mode == EditingMode::DRAW_TILE,
                          ImVec2(30, 0))) {
      current_mode = EditingMode::DRAW_TILE;
      // Note: Canvas mode update handled by editor
    }
    HOVER_HINT("Tile Paint Mode (2)\nDraw tiles on the map");
    ImGui::PopStyleVar();

    TableNextColumn();
    // Entity Status
    if (entity_edit_mode != EntityEditMode::NONE) {
      const char* entity_icon = "";
      const char* entity_label = "";
      switch (entity_edit_mode) {
        case EntityEditMode::ENTRANCES:
          entity_icon = ICON_MD_DOOR_FRONT;
          entity_label = "Entrances";
          break;
        case EntityEditMode::EXITS:
          entity_icon = ICON_MD_DOOR_BACK;
          entity_label = "Exits";
          break;
        case EntityEditMode::ITEMS:
          entity_icon = ICON_MD_GRASS;
          entity_label = "Items";
          break;
        case EntityEditMode::SPRITES:
          entity_icon = ICON_MD_PEST_CONTROL_RODENT;
          entity_label = "Sprites";
          break;
        case EntityEditMode::TRANSPORTS:
          entity_icon = ICON_MD_ADD_LOCATION;
          entity_label = "Transports";
          break;
        case EntityEditMode::MUSIC:
          entity_icon = ICON_MD_MUSIC_NOTE;
          entity_label = "Music";
          break;
        default:
          break;
      }
      ImGui::TextColored(ImVec4(0.4f, 0.8f, 1.0f, 1.0f), "%s %s", entity_icon,
                         entity_label);
    }

    TableNextColumn();
    // Scratch Space Controls
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(2, 0));
    
    // Toggle visibility
    if (gui::ToggleButton(ICON_MD_BRUSH, show_scratch_space, ImVec2(25, 0))) {
      show_scratch_space = !show_scratch_space;
    }
    HOVER_HINT("Toggle Scratch Space Window");

    ImGui::SameLine();
    
    // Slot selector (cycle)
    std::string slot_label = absl::StrFormat("%d", current_scratch_slot + 1);
    if (ImGui::Button(slot_label.c_str(), ImVec2(20, 0))) {
      current_scratch_slot = (current_scratch_slot + 1) % 4;
    }
    HOVER_HINT("Current Scratch Slot (Click to cycle 1-4)");

    ImGui::SameLine();

    // Quick Save (only if selection active)
    if (has_selection) {
      if (ImGui::Button(ICON_MD_DOWNLOAD, ImVec2(25, 0))) {
        if (on_save_to_scratch) on_save_to_scratch();
      }
      HOVER_HINT("Save Selection to Scratch Slot");
    } else if (scratch_has_data) {
      // Quick Load (only if slot has data and no selection active)
      if (ImGui::Button(ICON_MD_UPLOAD, ImVec2(25, 0))) {
        if (on_load_from_scratch) on_load_from_scratch();
      }
      HOVER_HINT("Load from Scratch Slot");
    } else {
      // Placeholder spacing
      ImGui::Dummy(ImVec2(25, 0));
    }
    
    ImGui::PopStyleVar();

    TableNextColumn();
    // Sidebar Toggle
    if (gui::ToggleButton(ICON_MD_TUNE, show_map_properties_panel,
                          ImVec2(40, 0))) {
      show_map_properties_panel = !show_map_properties_panel;
    }
    HOVER_HINT("Toggle Map Properties Sidebar");

    ImGui::EndTable();
  }
}

}  // namespace yaze::editor
