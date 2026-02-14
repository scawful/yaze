#include "app/editor/overworld/overworld_toolbar.h"

#include "app/editor/overworld/map_properties.h"
#include "app/editor/system/panel_manager.h"
#include "app/gui/core/layout_helpers.h"
#include "app/gui/core/style_guard.h"
#include "app/gui/core/agent_theme.h"
#include "app/gui/widgets/themed_widgets.h"
#include "zelda3/overworld/overworld_version_helper.h"

namespace yaze::editor {

using ImGui::BeginTable;
using ImGui::TableNextColumn;

void OverworldToolbar::Draw(int& current_world, int& current_map,
                            bool& current_map_lock, EditingMode& current_mode,
                            EntityEditMode& entity_edit_mode,
                            PanelManager* panel_manager, bool has_selection,
                            bool scratch_has_data, Rom* rom,
                            zelda3::Overworld* overworld) {
  if (!overworld || !overworld->is_loaded() || !panel_manager) return;

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
                            124.0f);  // Mouse/Brush/Fill
    ImGui::TableSetupColumn("Entity",
                            ImGuiTableColumnFlags_WidthStretch);  // Entity status
    ImGui::TableSetupColumn("Panels", ImGuiTableColumnFlags_WidthFixed, 320.0f);
    ImGui::TableSetupColumn("Sidebar", ImGuiTableColumnFlags_WidthFixed, 50.0f);

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
    if (gui::ToolbarIconButton(current_map_lock ? ICON_MD_LOCK : ICON_MD_LOCK_OPEN,
                               current_map_lock ? "Unlock Map" : "Lock Map")) {
      current_map_lock = !current_map_lock;
    }

    TableNextColumn();
    // Mode Controls
    {
      if (gui::ToolbarIconButton(ICON_MD_MOUSE,
                                 "Mouse Mode (1)\nNavigate, pan, and manage entities",
                                 current_mode == EditingMode::MOUSE)) {
        current_mode = EditingMode::MOUSE;
      }

      ImGui::SameLine(0, 2);
      if (gui::ToolbarIconButton(ICON_MD_DRAW,
                                 "Brush Mode (2/B)\nDraw tiles on the map\nRight-click or I to sample tile16 under cursor",
                                 current_mode == EditingMode::DRAW_TILE)) {
        current_mode = EditingMode::DRAW_TILE;
      }

      ImGui::SameLine(0, 2);
      if (gui::ToolbarIconButton(ICON_MD_FORMAT_COLOR_FILL,
                                 "Fill Screen Mode (F)\nFill the 32x32 screen under the cursor with the selected tile\nRight-click or I to sample tile16 under cursor",
                                 current_mode == EditingMode::FILL_TILE)) {
        current_mode = EditingMode::FILL_TILE;
      }
    }

    TableNextColumn();
    // Entity Status or Version Badge
    const auto& theme = AgentUI::GetTheme();
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
      ImGui::TextColored(theme.selection_secondary, "%s %s", entity_icon,
                         entity_label);
    } else {
      // Show ROM version badge when no entity mode is active
      const char* version_label = "Vanilla";
      ImVec4 version_color = theme.status_inactive;
      bool show_upgrade = false;

      switch (rom_version) {
        case zelda3::OverworldVersion::kVanilla:
          version_label = "Vanilla";
          version_color = theme.text_warning_yellow;
          show_upgrade = true;
          break;
        case zelda3::OverworldVersion::kZSCustomV1:
          version_label = "ZSC v1";
          version_color = theme.status_active;
          break;
        case zelda3::OverworldVersion::kZSCustomV2:
          version_label = "ZSC v2";
          version_color = theme.status_active;
          break;
        case zelda3::OverworldVersion::kZSCustomV3:
          version_label = "ZSC v3";
          version_color = theme.status_success;
          break;
        default:
          break;
      }

      ImGui::TextColored(version_color, ICON_MD_INFO " %s", version_label);
      HOVER_HINT("ROM version determines available overworld features.\n"
                 "v2+: Custom BG colors, main palettes\n"
                 "v3+: Wide/Tall maps, custom tile GFX, animated GFX");

      if (show_upgrade && on_upgrade_rom_version) {
        ImGui::SameLine();
        if (gui::PrimaryButton(ICON_MD_UPGRADE " Upgrade")) {
          on_upgrade_rom_version(3);  // Upgrade to v3
        }
        HOVER_HINT("Upgrade ROM to ZSCustomOverworld v3\n"
                   "Enables all advanced features");
      }
    }

    TableNextColumn();
    // Panel Toggle Controls - using PanelManager for visibility
    {
      gui::StyleVarGuard panel_spacing_guard(ImGuiStyleVar_ItemSpacing,
                                             ImVec2(4, 0));

      // Tile16 Editor toggle (Ctrl+T)
      if (gui::ToolbarIconButton(ICON_MD_EDIT, "Tile16 Editor (Ctrl+T)",
                                 panel_manager->IsPanelVisible(OverworldPanelIds::kTile16Editor))) {
        panel_manager->TogglePanel(0, OverworldPanelIds::kTile16Editor);
      }

      ImGui::SameLine();
      // Tile16 Selector toggle
      if (gui::ToolbarIconButton(ICON_MD_GRID_ON, "Tile16 Selector",
                                 panel_manager->IsPanelVisible(OverworldPanelIds::kTile16Selector))) {
        panel_manager->TogglePanel(0, OverworldPanelIds::kTile16Selector);
      }

      ImGui::SameLine();
      // Tile8 Selector toggle
      if (gui::ToolbarIconButton(ICON_MD_GRID_VIEW, "Tile8 Selector",
                                 panel_manager->IsPanelVisible(OverworldPanelIds::kTile8Selector))) {
        panel_manager->TogglePanel(0, OverworldPanelIds::kTile8Selector);
      }

      ImGui::SameLine();
      // Area Graphics toggle
      if (gui::ToolbarIconButton(ICON_MD_IMAGE, "Area Graphics",
                                 panel_manager->IsPanelVisible(OverworldPanelIds::kAreaGraphics))) {
        panel_manager->TogglePanel(0, OverworldPanelIds::kAreaGraphics);
      }

      ImGui::SameLine();
      // GFX Groups toggle
      if (gui::ToolbarIconButton(ICON_MD_LAYERS, "GFX Groups",
                                 panel_manager->IsPanelVisible(OverworldPanelIds::kGfxGroups))) {
        panel_manager->TogglePanel(0, OverworldPanelIds::kGfxGroups);
      }

      ImGui::SameLine();
      // Usage Stats toggle
      if (gui::ToolbarIconButton(ICON_MD_ANALYTICS, "Usage Statistics",
                                 panel_manager->IsPanelVisible(OverworldPanelIds::kUsageStats))) {
        panel_manager->TogglePanel(0, OverworldPanelIds::kUsageStats);
      }

      ImGui::SameLine();
      // Scratch Space toggle
      if (gui::ToolbarIconButton(ICON_MD_AUTO_FIX_HIGH, "Scratch Workspace",
                                 panel_manager->IsPanelVisible(OverworldPanelIds::kScratchSpace))) {
        panel_manager->TogglePanel(0, OverworldPanelIds::kScratchSpace);
      }
    }

    TableNextColumn();
    // Sidebar Toggle (Map Properties)
    if (gui::ToolbarIconButton(ICON_MD_TUNE, "Toggle Map Properties Sidebar",
                               panel_manager->IsPanelVisible(OverworldPanelIds::kMapProperties))) {
      panel_manager->TogglePanel(0, OverworldPanelIds::kMapProperties);
    }

    ImGui::EndTable();
  }
}

}  // namespace yaze::editor
