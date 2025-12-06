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
                            ToolbarPanelState& panel_state, bool has_selection,
                            bool scratch_has_data, Rom* rom,
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
    ImGui::TableSetupColumn("Panels", ImGuiTableColumnFlags_WidthFixed, 200.0f);
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
    }
    HOVER_HINT("Mouse Mode (1)\nNavigate, pan, and manage entities");

    ImGui::SameLine();
    if (gui::ToggleButton(ICON_MD_DRAW, current_mode == EditingMode::DRAW_TILE,
                          ImVec2(30, 0))) {
      current_mode = EditingMode::DRAW_TILE;
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
    // Panel Toggle Controls
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(2, 0));

    // Tile16 Editor toggle (Ctrl+T)
    if (gui::ToggleButton(ICON_MD_EDIT, panel_state.show_tile16_editor,
                          ImVec2(25, 0))) {
      panel_state.show_tile16_editor = !panel_state.show_tile16_editor;
    }
    HOVER_HINT("Tile16 Editor (Ctrl+T)");

    ImGui::SameLine();

    // Tile16 Selector toggle
    if (gui::ToggleButton(ICON_MD_GRID_ON, panel_state.show_tile16_selector,
                          ImVec2(25, 0))) {
      panel_state.show_tile16_selector = !panel_state.show_tile16_selector;
    }
    HOVER_HINT("Tile16 Selector");

    ImGui::SameLine();

    // Tile8 Selector toggle
    if (gui::ToggleButton(ICON_MD_GRID_VIEW, panel_state.show_tile8_selector,
                          ImVec2(25, 0))) {
      panel_state.show_tile8_selector = !panel_state.show_tile8_selector;
    }
    HOVER_HINT("Tile8 Selector");

    ImGui::SameLine();

    // Area Graphics toggle
    if (gui::ToggleButton(ICON_MD_IMAGE, panel_state.show_area_graphics,
                          ImVec2(25, 0))) {
      panel_state.show_area_graphics = !panel_state.show_area_graphics;
    }
    HOVER_HINT("Area Graphics");

    ImGui::SameLine();

    // GFX Groups toggle
    if (gui::ToggleButton(ICON_MD_LAYERS, panel_state.show_gfx_groups,
                          ImVec2(25, 0))) {
      panel_state.show_gfx_groups = !panel_state.show_gfx_groups;
    }
    HOVER_HINT("GFX Groups");

    ImGui::SameLine();

    // Usage Stats toggle
    if (gui::ToggleButton(ICON_MD_ANALYTICS, panel_state.show_usage_stats,
                          ImVec2(25, 0))) {
      panel_state.show_usage_stats = !panel_state.show_usage_stats;
    }
    HOVER_HINT("Usage Statistics");

    ImGui::SameLine();

    // Scratch Space toggle
    if (gui::ToggleButton(ICON_MD_BRUSH, panel_state.show_scratch_space,
                          ImVec2(25, 0))) {
      panel_state.show_scratch_space = !panel_state.show_scratch_space;
    }
    HOVER_HINT("Scratch Workspace");

    ImGui::PopStyleVar();

    TableNextColumn();
    // Sidebar Toggle (Map Properties)
    if (gui::ToggleButton(ICON_MD_TUNE, panel_state.show_map_properties,
                          ImVec2(40, 0))) {
      panel_state.show_map_properties = !panel_state.show_map_properties;
    }
    HOVER_HINT("Toggle Map Properties Sidebar");

    ImGui::EndTable();
  }
}

}  // namespace yaze::editor
