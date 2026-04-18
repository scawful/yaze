#include "app/editor/overworld/overworld_toolbar.h"

#include "app/editor/overworld/map_properties.h"
#include "app/editor/system/workspace_window_manager.h"
#include "app/gui/core/agent_theme.h"
#include "app/gui/core/layout_helpers.h"
#include "app/gui/core/style_guard.h"
#include "app/gui/widgets/themed_widgets.h"
#include "zelda3/overworld/overworld_version_helper.h"

namespace yaze::editor {

using ImGui::BeginTable;
using ImGui::TableNextColumn;

void OverworldToolbar::Draw(int& current_world, int& current_map,
                            bool& current_map_lock, EditingMode& current_mode,
                            EntityEditMode& entity_edit_mode,
                            WorkspaceWindowManager* window_manager, bool has_selection,
                            bool scratch_has_data, Rom* rom,
                            zelda3::Overworld* overworld) {
  if (!overworld || !overworld->is_loaded() || !window_manager)
    return;

  gui::StyleVarGuard toolbar_style_guard(
      {{ImGuiStyleVar_FramePadding, ImVec2(6.0f, 5.0f)},
       {ImGuiStyleVar_CellPadding, ImVec2(6.0f, 5.0f)}});

  // Simplified canvas toolbar - Navigation and Mode controls
  if (BeginTable("CanvasToolbar", 8,
                 ImGuiTableFlags_Borders | ImGuiTableFlags_SizingStretchProp,
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
    ImGui::TableSetupColumn("Context", ImGuiTableColumnFlags_WidthStretch);
    ImGui::TableSetupColumn("Panels", ImGuiTableColumnFlags_WidthStretch);
    ImGui::TableSetupColumn("Sidebar", ImGuiTableColumnFlags_WidthFixed, 44.0f);

    TableNextColumn();
    ImGui::SetNextItemWidth(kComboWorldWidth);
    int selected_world = current_world;
    if (ImGui::Combo("##world", &selected_world, kWorldNames, 3)) {
      current_world = selected_world;
      if (on_world_changed) {
        on_world_changed(selected_world);
      } else {
        current_map = selected_world * 0x40 + (current_map & 0x3F);
      }
    }

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
          if (on_refresh_graphics)
            on_refresh_graphics();
          if (on_refresh_map)
            on_refresh_map();
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
          if (on_refresh_graphics)
            on_refresh_graphics();
          if (on_refresh_map)
            on_refresh_map();
        }
      }

      if (rom_version == zelda3::OverworldVersion::kVanilla ||
          !zelda3::OverworldVersionHelper::SupportsAreaEnum(rom_version)) {
        HOVER_HINT("Small (1x1) and Large (2x2) maps. Wide/Tall require v3+");
      }
    }

    TableNextColumn();
    if (gui::ToolbarIconButton(
            current_map_lock ? ICON_MD_LOCK : ICON_MD_LOCK_OPEN,
            current_map_lock ? "Unlock Map" : "Lock Map")) {
      current_map_lock = !current_map_lock;
    }

    TableNextColumn();
    // Mode Controls
    {
      if (gui::ToolbarIconButton(
              ICON_MD_MOUSE,
              "Mouse Mode (1)\nNavigate, pan, and manage entities",
              current_mode == EditingMode::MOUSE)) {
        current_mode = EditingMode::MOUSE;
      }

      ImGui::SameLine(0, 2);
      if (gui::ToolbarIconButton(
              ICON_MD_DRAW,
              "Brush Mode (2/B)\nDraw tiles on the map\nRight-click or I to "
              "sample tile16 under cursor",
              current_mode == EditingMode::DRAW_TILE)) {
        current_mode = EditingMode::DRAW_TILE;
      }

      ImGui::SameLine(0, 2);
      if (gui::ToolbarIconButton(
              ICON_MD_FORMAT_COLOR_FILL,
              "Fill Screen Mode (F)\nFill the 32x32 screen under the cursor "
              "with the selected tile\nRight-click or I to sample tile16 under "
              "cursor",
              current_mode == EditingMode::FILL_TILE)) {
        current_mode = EditingMode::FILL_TILE;
      }
    }

    TableNextColumn();
    // Entity status / ROM version plus a small amount of high-value map info.
    const auto& theme = AgentUI::GetTheme();
    const zelda3::OverworldMap* map = overworld->overworld_map(current_map);
    const float context_width = ImGui::GetContentRegionAvail().x;
    const bool show_map_summary = map != nullptr && context_width >= 188.0f;
    const bool show_overlay_toggle = context_width >= 132.0f;

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
      if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip(
          "ROM version determines available overworld features.\n"
          "v2+: Custom BG colors, main palettes\n"
          "v3+: Wide/Tall maps, custom tile GFX, animated GFX");
      }

      if (show_upgrade && on_upgrade_rom_version) {
        ImGui::SameLine();
        if (gui::PrimaryButton(ICON_MD_UPGRADE " Upgrade")) {
          on_upgrade_rom_version(3);  // Upgrade to v3
        }
        HOVER_HINT(
            "Upgrade ROM to ZSCustomOverworld v3\n"
            "Enables all advanced features");
      }
    }

    if (show_map_summary) {
      ImGui::TextDisabled("Parent %02X  Pal %02X  Msg %04X", map->parent(),
                          map->area_palette(), map->message_id());
    } else if (show_overlay_toggle && on_toggle_overlay_preview &&
               is_overlay_preview_enabled) {
      const bool overlay_preview_enabled = is_overlay_preview_enabled();
      if (gui::ToolbarIconButton(
              ICON_MD_VISIBILITY,
              overlay_preview_enabled ? "Hide overlay preview"
                                      : "Show overlay preview",
              overlay_preview_enabled)) {
        on_toggle_overlay_preview();
      }
    }

    TableNextColumn();
    // Panel Toggle Controls - using WorkspaceWindowManager for visibility
    const size_t session_id = window_manager->GetActiveSessionId();
    const float panel_width = ImGui::GetContentRegionAvail().x;
    const bool compact_panel_controls = panel_width < 320.0f;
    const auto toggle_window = [&](const char* panel_id) {
      window_manager->ToggleWindow(session_id, panel_id);
    };
    const auto popup_toggle_item =
        [&](const char* label, const char* panel_id, const char* shortcut = nullptr) {
          const bool open = window_manager->IsWindowOpen(panel_id);
          if (ImGui::MenuItem(label, shortcut, open)) {
            toggle_window(panel_id);
          }
        };
    {
      gui::StyleVarGuard panel_spacing_guard(ImGuiStyleVar_ItemSpacing,
                                             ImVec2(4, 0));

      if (compact_panel_controls) {
        if (gui::ToolbarIconButton(ICON_MD_APPS,
                                   "Overworld Windows\nOpen panel toggle menu")) {
          ImGui::OpenPopup("OverworldWindowsPopup");
        }

        if (ImGui::BeginPopup("OverworldWindowsPopup")) {
          popup_toggle_item("Tile16 Editor", OverworldPanelIds::kTile16Editor,
                            "Ctrl+T");
          popup_toggle_item("Tile16 Selector", OverworldPanelIds::kTile16Selector);
          popup_toggle_item("Tile8 Selector", OverworldPanelIds::kTile8Selector);
          popup_toggle_item("Area Graphics", OverworldPanelIds::kAreaGraphics);
          popup_toggle_item("GFX Groups", OverworldPanelIds::kGfxGroups);
          popup_toggle_item("Usage Statistics", OverworldPanelIds::kUsageStats);
          popup_toggle_item("Item List", OverworldPanelIds::kItemList,
                            "Ctrl+Shift+I");
          popup_toggle_item("Scratch Workspace",
                            OverworldPanelIds::kScratchSpace);
          ImGui::EndPopup();
        }
      } else {
        // Tile16 Editor toggle (Ctrl+T)
        if (gui::ToolbarIconButton(ICON_MD_EDIT, "Tile16 Editor (Ctrl+T)",
                                   window_manager->IsWindowOpen(
                                        OverworldPanelIds::kTile16Editor))) {
          toggle_window(OverworldPanelIds::kTile16Editor);
        }

        ImGui::SameLine();
        if (gui::ToolbarIconButton(ICON_MD_GRID_ON, "Tile16 Selector",
                                   window_manager->IsWindowOpen(
                                        OverworldPanelIds::kTile16Selector))) {
          toggle_window(OverworldPanelIds::kTile16Selector);
        }

        ImGui::SameLine();
        if (gui::ToolbarIconButton(ICON_MD_GRID_VIEW, "Tile8 Selector",
                                   window_manager->IsWindowOpen(
                                        OverworldPanelIds::kTile8Selector))) {
          toggle_window(OverworldPanelIds::kTile8Selector);
        }

        ImGui::SameLine();
        if (gui::ToolbarIconButton(ICON_MD_IMAGE, "Area Graphics",
                                   window_manager->IsWindowOpen(
                                        OverworldPanelIds::kAreaGraphics))) {
          toggle_window(OverworldPanelIds::kAreaGraphics);
        }

        ImGui::SameLine();
        if (gui::ToolbarIconButton(
                ICON_MD_LAYERS, "GFX Groups",
                window_manager->IsWindowOpen(OverworldPanelIds::kGfxGroups))) {
          toggle_window(OverworldPanelIds::kGfxGroups);
        }

        ImGui::SameLine();
        if (gui::ToolbarIconButton(
                ICON_MD_ANALYTICS, "Usage Statistics",
                window_manager->IsWindowOpen(OverworldPanelIds::kUsageStats))) {
          toggle_window(OverworldPanelIds::kUsageStats);
        }

        ImGui::SameLine();
        if (gui::ToolbarIconButton(
                ICON_MD_LIST,
                "Overworld Item List (Ctrl+Shift+I)\nFilter/select items and use "
                "duplicate/nudge shortcuts",
                window_manager->IsWindowOpen(OverworldPanelIds::kItemList))) {
          toggle_window(OverworldPanelIds::kItemList);
        }

        ImGui::SameLine();
        if (gui::ToolbarIconButton(ICON_MD_AUTO_FIX_HIGH, "Scratch Workspace",
                                   window_manager->IsWindowOpen(
                                        OverworldPanelIds::kScratchSpace))) {
          toggle_window(OverworldPanelIds::kScratchSpace);
        }
      }
    }

    TableNextColumn();
    // Sidebar Toggle (Map Properties)
    if (gui::ToolbarIconButton(
            ICON_MD_TUNE, "Toggle Map Properties Sidebar",
            window_manager->IsWindowOpen(OverworldPanelIds::kMapProperties))) {
      window_manager->ToggleWindow(window_manager->GetActiveSessionId(),
                                   OverworldPanelIds::kMapProperties);
    }

    ImGui::EndTable();
  }
}

}  // namespace yaze::editor
