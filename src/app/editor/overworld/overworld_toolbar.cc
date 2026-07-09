#include "app/editor/overworld/overworld_toolbar.h"
#include "util/i18n/tr.h"

#include <algorithm>
#include <array>
#include <cstring>
#include <string>

#include "absl/strings/str_format.h"
#include "app/editor/editor.h"
#include "app/editor/overworld/map_properties.h"
#include "app/editor/overworld/overworld_map_metadata.h"
#include "app/editor/system/workspace/workspace_window_manager.h"
#include "app/gui/core/agent_theme.h"
#include "app/gui/core/input.h"
#include "app/gui/core/layout_helpers.h"
#include "app/gui/core/style_guard.h"
#include "app/gui/widgets/themed_widgets.h"
#include "core/project.h"
#include "zelda3/common.h"
#include "zelda3/overworld/overworld_version_helper.h"

namespace yaze::editor {

using ImGui::BeginTable;
using ImGui::TableNextColumn;

namespace {

struct ToolbarResourceLabelTarget {
  std::string title;
  std::string type;
  int id = 0;
  int hex_width = 2;
};

std::string FormatResourceId(int id, int width) {
  return width <= 2 ? absl::StrFormat("0x%02X", id)
                    : absl::StrFormat("0x%04X", id);
}

}  // namespace

void OverworldToolbar::Draw(int& current_world, int& current_map,
                            bool& current_map_lock, EditingMode& current_mode,
                            EntityEditMode& entity_edit_mode,
                            WorkspaceWindowManager* window_manager,
                            bool has_selection, bool scratch_has_data, Rom* rom,
                            zelda3::Overworld* overworld,
                            project::YazeProject* project, int game_state,
                            SharedClipboard* shared_clipboard) {
  if (!overworld || !overworld->is_loaded() || !window_manager || !rom)
    return;

  const zelda3::OverworldMap* map = overworld->overworld_map(current_map);
  if (!map) {
    return;
  }
  const auto metadata = BuildOverworldMapMetadata(*overworld, rom, project,
                                                  current_map, game_state);
  const auto apply_property_edit =
      [this](const OverworldPropertyEdit& edit) -> absl::Status {
    if (!on_apply_property_edit) {
      return absl::FailedPreconditionError(
          "Overworld property edit callback is not configured");
    }
    return on_apply_property_edit(edit);
  };

  gui::StyleVarGuard toolbar_style_guard(
      {{ImGuiStyleVar_FramePadding, ImVec2(6.0f, 5.0f)},
       {ImGuiStyleVar_CellPadding, ImVec2(6.0f, 5.0f)}});
  ImGui::PushID("OverworldToolbar");

  // Simplified canvas toolbar - Navigation and Mode controls
  if (BeginTable("OverworldCanvasToolbar", 8,
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
    const char* world_buttons[] = {"LW", "DW", "SW"};
    ImGui::PushID("WorldButtons");
    for (int world_index = 0; world_index < 3; ++world_index) {
      if (world_index > 0) {
        ImGui::SameLine(0, 2);
      }
      if (gui::ToggleButton(world_buttons[world_index],
                            current_world == world_index,
                            ImVec2(30.0f, 0.0f))) {
        if (on_world_changed) {
          on_world_changed(world_index);
        } else {
          current_world = world_index;
          int local_map = current_map & 0x3F;
          const int world_start = world_index * 0x40;
          const int maps_remaining = zelda3::kNumOverworldMaps - world_start;
          if (maps_remaining > 0) {
            if (local_map >= maps_remaining) {
              local_map = maps_remaining - 1;
            }
            current_map = world_start + local_map;
          }
        }
      }
      if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("%s", kWorldNames[world_index]);
      }
    }
    ImGui::PopID();

    TableNextColumn();
    ImGui::TextUnformatted(metadata.map_id_label.c_str());
    if (ImGui::IsItemHovered()) {
      ImGui::SetTooltip("%s", metadata.map_title.c_str());
    }

    TableNextColumn();
    // Use centralized version detection
    auto rom_version = zelda3::OverworldVersionHelper::GetVersion(*rom);

    // ALL ROMs support Small/Large. Only v3+ supports Wide/Tall.
    int current_area_size = static_cast<int>(map->area_size());
    ImGui::SetNextItemWidth(kComboAreaSizeWidth);

    if (zelda3::OverworldVersionHelper::SupportsAreaEnum(rom_version)) {
      // v3+ ROM: Show all 4 area size options
      if (ImGui::Combo("##ToolbarAreaSize", &current_area_size, kAreaSizeNames,
                       4)) {
        auto status =
            on_apply_property_edit
                ? on_apply_property_edit({current_map,
                                          OverworldPropertyField::kAreaSize, 0,
                                          current_area_size})
                : overworld->ConfigureMultiAreaMap(
                      current_map,
                      static_cast<zelda3::AreaSizeEnum>(current_area_size));
        if (status.ok() && !on_apply_property_edit) {
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

      if (ImGui::Combo("##ToolbarAreaSize", &limited_size, limited_names, 2)) {
        // limited_size is 0 (Small) or 1 (Large)
        auto size = (limited_size == 1) ? zelda3::AreaSizeEnum::LargeArea
                                        : zelda3::AreaSizeEnum::SmallArea;
        auto status = on_apply_property_edit
                          ? on_apply_property_edit(
                                {current_map, OverworldPropertyField::kAreaSize,
                                 0, static_cast<int>(size)})
                          : overworld->ConfigureMultiAreaMap(current_map, size);
        if (status.ok() && !on_apply_property_edit) {
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
            current_map_lock ? "Unpin Map" : "Pin Map")) {
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
    // Entity status / ROM version plus high-value selected-map metadata.
    const auto& theme = AgentUI::GetTheme();
    const float context_width = ImGui::GetContentRegionAvail().x;
    const bool show_entity_context = entity_edit_mode != EntityEditMode::NONE;
    const bool show_overlay_toggle = context_width >= 132.0f;
    const bool has_metadata_clipboard =
        shared_clipboard && shared_clipboard->has_overworld_map_metadata &&
        shared_clipboard->overworld_map_metadata.valid;

    ImVec4 version_color = theme.status_inactive;
    bool show_upgrade = false;

    switch (rom_version) {
      case zelda3::OverworldVersion::kVanilla:
        version_color = theme.text_warning_yellow;
        show_upgrade = true;
        break;
      case zelda3::OverworldVersion::kZSCustomV1:
        version_color = theme.status_active;
        break;
      case zelda3::OverworldVersion::kZSCustomV2:
        version_color = theme.status_active;
        break;
      case zelda3::OverworldVersion::kZSCustomV3:
        version_color = theme.status_success;
        break;
      default:
        break;
    }
    const float map_summary_min_width =
        show_upgrade ? 400.0f : (show_entity_context ? 340.0f : 280.0f);
    const bool show_map_summary = context_width >= map_summary_min_width;

    bool wrote_metadata_item = false;
    if (show_entity_context) {
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
      wrote_metadata_item = true;
    }

    if (wrote_metadata_item) {
      ImGui::SameLine();
    }
    ImGui::TextColored(version_color, ICON_MD_INFO " %s",
                       metadata.version_label.c_str());
    wrote_metadata_item = true;
    if (ImGui::IsItemHovered()) {
      ImGui::SetTooltip(
          tr("ROM version determines available overworld features.\n"
             "v2+: Custom BG colors, main palettes\n"
             "v3+: Wide/Tall maps, custom tile GFX, animated GFX"));
    }

    if (has_metadata_clipboard && context_width >= 440.0f) {
      ImGui::SameLine();
      ImGui::TextColored(theme.text_secondary_gray, ICON_MD_CONTENT_PASTE " %s",
                         OverworldMapMetadataClipboardScopeName(
                             shared_clipboard->overworld_map_metadata.scope));
      if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip(
            tr("%s\nPaste from the canvas context menu onto the hovered map."),
            DescribeOverworldMapMetadataClipboard(
                shared_clipboard->overworld_map_metadata)
                .c_str());
      }
      wrote_metadata_item = true;
    }

    if (show_map_summary) {
      ImGui::SameLine();
      ImGui::TextDisabled("%s | %s | %s | %s", metadata.map_name.c_str(),
                          metadata.area_gfx_label.c_str(),
                          metadata.area_palette_label.c_str(),
                          metadata.message_label.c_str());
      if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("%s\n%s\n%s\n%s\n%s\n%s", metadata.map_title.c_str(),
                          metadata.area_size_label.c_str(),
                          metadata.parent_label.c_str(),
                          metadata.sprite_gfx_label.c_str(),
                          metadata.sprite_palette_label.c_str(),
                          metadata.music_label.c_str());
      }
      wrote_metadata_item = true;

      ImGui::SameLine();
      static std::array<char, 128> rename_label_buffer{};
      static int rename_map_id = -1;
      static std::string rename_error;
      ImGui::BeginDisabled(project == nullptr);
      if (gui::ToolbarIconButton(
              ICON_MD_EDIT, project ? "Rename selected map label in project"
                                    : "Open a project to rename map labels")) {
        rename_map_id = current_map;
        rename_error.clear();
        std::strncpy(rename_label_buffer.data(), metadata.map_name.c_str(),
                     rename_label_buffer.size() - 1);
        rename_label_buffer[rename_label_buffer.size() - 1] = '\0';
        ImGui::OpenPopup("RenameOverworldMapLabel");
      }
      ImGui::EndDisabled();

      if (ImGui::BeginPopup("RenameOverworldMapLabel")) {
        ImGui::Text(tr("%s Map Label"), ICON_MD_LABEL);
        ImGui::TextDisabled(tr("Map 0x%02X"), rename_map_id);
        ImGui::SetNextItemWidth(260.0f);
        ImGui::InputText("##OverworldMapLabel", rename_label_buffer.data(),
                         rename_label_buffer.size());
        if (ImGui::Button(ICON_MD_CHECK " Apply")) {
          auto status =
              on_rename_resource_label
                  ? on_rename_resource_label("overworld_map", rename_map_id,
                                             rename_label_buffer.data())
                  : RenameProjectResourceLabel(project, "overworld_map",
                                               rename_map_id,
                                               rename_label_buffer.data());
          if (status.ok()) {
            ImGui::CloseCurrentPopup();
          } else {
            rename_error = std::string(status.message());
          }
        }
        ImGui::SameLine();
        if (ImGui::Button(ICON_MD_CLEAR " Clear")) {
          auto status =
              on_rename_resource_label
                  ? on_rename_resource_label("overworld_map", rename_map_id, "")
                  : RenameProjectResourceLabel(project, "overworld_map",
                                               rename_map_id, "");
          if (status.ok()) {
            rename_label_buffer[0] = '\0';
            ImGui::CloseCurrentPopup();
          } else {
            rename_error = std::string(status.message());
          }
        }
        if (!rename_error.empty()) {
          ImGui::TextWrapped("%s", rename_error.c_str());
        }
        ImGui::EndPopup();
      }

      ImGui::SameLine();
      if (gui::ToolbarIconButton(
              ICON_MD_TUNE,
              "Edit selected map metadata\nGraphics, palettes, message, and "
              "music")) {
        ImGui::OpenPopup("OverworldToolbarMetadataEditor");
      }
      if (ImGui::BeginPopup("OverworldToolbarMetadataEditor")) {
        static std::string metadata_error;
        static std::array<char, 128> resource_label_buffer{};
        static std::string resource_label_type;
        static std::string resource_label_title;
        static std::string resource_label_error;
        static int resource_label_id = -1;
        static int resource_label_hex_width = 2;
        const auto apply_toolbar_edit = [&](const OverworldPropertyEdit& edit) {
          const auto status = apply_property_edit(edit);
          if (status.ok()) {
            metadata_error.clear();
          } else {
            metadata_error = std::string(status.message());
          }
        };
        const auto begin_resource_label_edit =
            [&](const ToolbarResourceLabelTarget& target) {
              resource_label_title = target.title;
              resource_label_type = target.type;
              resource_label_id = target.id;
              resource_label_hex_width = target.hex_width;
              resource_label_error.clear();
              const std::string current_label =
                  GetProjectResourceLabel(project, target.type, target.id);
              std::strncpy(resource_label_buffer.data(), current_label.c_str(),
                           resource_label_buffer.size() - 1);
              resource_label_buffer[resource_label_buffer.size() - 1] = '\0';
              ImGui::OpenPopup("OverworldMetadataResourceLabel");
            };
        auto apply_resource_label = [&](const std::string& label) {
          auto status =
              on_rename_resource_label
                  ? on_rename_resource_label(resource_label_type,
                                             resource_label_id, label)
                  : RenameProjectResourceLabel(project, resource_label_type,
                                               resource_label_id, label);
          if (status.ok()) {
            resource_label_error.clear();
            ImGui::CloseCurrentPopup();
          } else {
            resource_label_error = std::string(status.message());
          }
        };

        ImGui::Text(tr("%s Map 0x%02X Metadata"), ICON_MD_TUNE, current_map);
        ImGui::Separator();

        if (ImGui::BeginTable("ToolbarMetadataEditor", 2,
                              ImGuiTableFlags_SizingFixedFit)) {
          ImGui::TableSetupColumn("Property", ImGuiTableColumnFlags_WidthFixed,
                                  142.0f);
          ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthFixed,
                                  108.0f);

          TableNextColumn();
          ImGui::TextUnformatted(tr("Area GFX"));
          TableNextColumn();
          uint8_t area_gfx = map->area_graphics();
          if (gui::InputHexByte("##ToolbarMetaAreaGfx", &area_gfx, 96.0f)) {
            apply_toolbar_edit({current_map,
                                OverworldPropertyField::kAreaGraphics, 0,
                                area_gfx});
          }

          TableNextColumn();
          ImGui::TextUnformatted(tr("Area Palette"));
          TableNextColumn();
          uint8_t area_palette = map->area_palette();
          if (gui::InputHexByte("##ToolbarMetaAreaPalette", &area_palette,
                                96.0f)) {
            apply_toolbar_edit({current_map,
                                OverworldPropertyField::kAreaPalette, 0,
                                area_palette});
          }

          if (zelda3::OverworldVersionHelper::SupportsCustomBGColors(
                  rom_version)) {
            TableNextColumn();
            ImGui::TextUnformatted(tr("Main Palette"));
            TableNextColumn();
            uint8_t main_palette = map->main_palette();
            if (gui::InputHexByte("##ToolbarMetaMainPalette", &main_palette,
                                  96.0f)) {
              apply_toolbar_edit({current_map,
                                  OverworldPropertyField::kMainPalette, 0,
                                  main_palette});
            }
          }

          TableNextColumn();
          ImGui::TextUnformatted(tr("Sprite GFX"));
          TableNextColumn();
          uint8_t sprite_gfx = map->sprite_graphics(game_state);
          if (gui::InputHexByte("##ToolbarMetaSpriteGfx", &sprite_gfx, 96.0f)) {
            apply_toolbar_edit({current_map,
                                OverworldPropertyField::kSpriteGraphics,
                                game_state, sprite_gfx});
          }

          TableNextColumn();
          ImGui::TextUnformatted(tr("Sprite Palette"));
          TableNextColumn();
          uint8_t sprite_palette = map->sprite_palette(game_state);
          if (gui::InputHexByte("##ToolbarMetaSpritePalette", &sprite_palette,
                                96.0f)) {
            apply_toolbar_edit({current_map,
                                OverworldPropertyField::kSpritePalette,
                                game_state, sprite_palette});
          }

          if (zelda3::OverworldVersionHelper::SupportsAnimatedGFX(
                  rom_version)) {
            TableNextColumn();
            ImGui::TextUnformatted(tr("Animated GFX"));
            TableNextColumn();
            uint8_t animated_gfx = map->animated_gfx();
            if (gui::InputHexByte("##ToolbarMetaAnimatedGfx", &animated_gfx,
                                  96.0f)) {
              apply_toolbar_edit({current_map,
                                  OverworldPropertyField::kAnimatedGraphics, 0,
                                  animated_gfx});
            }
          }

          TableNextColumn();
          ImGui::TextUnformatted(tr("Message"));
          TableNextColumn();
          uint16_t message_id = map->message_id();
          if (gui::InputHexWord("##ToolbarMetaMessage", &message_id, 96.0f)) {
            apply_toolbar_edit({current_map, OverworldPropertyField::kMessageId,
                                0, message_id});
          }

          for (int i = 0; i < 4; ++i) {
            TableNextColumn();
            ImGui::Text(tr("Music %d"), i);
            TableNextColumn();
            uint8_t music = map->area_music(i);
            const std::string id = absl::StrFormat("##ToolbarMetaMusic%d", i);
            if (gui::InputHexByte(id.c_str(), &music, 96.0f)) {
              apply_toolbar_edit(
                  {current_map, OverworldPropertyField::kMusic, i, music});
            }
          }

          ImGui::EndTable();
        }

        ImGui::Separator();
        ImGui::Text(tr("%s Project Labels"), ICON_MD_LABEL);
        ImGui::BeginDisabled(project == nullptr);
        if (ImGui::BeginTable(
                "ToolbarMetadataProjectLabels", 4,
                ImGuiTableFlags_SizingFixedFit | ImGuiTableFlags_RowBg)) {
          ImGui::TableSetupColumn("Resource", ImGuiTableColumnFlags_WidthFixed,
                                  112.0f);
          ImGui::TableSetupColumn("ID", ImGuiTableColumnFlags_WidthFixed,
                                  62.0f);
          ImGui::TableSetupColumn("Label", ImGuiTableColumnFlags_WidthFixed,
                                  160.0f);
          ImGui::TableSetupColumn("Edit", ImGuiTableColumnFlags_WidthFixed,
                                  36.0f);

          auto draw_label_row = [&](const ToolbarResourceLabelTarget& target) {
            TableNextColumn();
            ImGui::TextUnformatted(target.title.c_str());
            TableNextColumn();
            ImGui::TextDisabled(
                "%s", FormatResourceId(target.id, target.hex_width).c_str());
            TableNextColumn();
            const std::string project_label =
                GetProjectResourceLabel(project, target.type, target.id);
            if (project_label.empty()) {
              ImGui::TextDisabled(tr("default"));
            } else {
              ImGui::TextUnformatted(project_label.c_str());
            }
            TableNextColumn();
            const std::string row_id =
                absl::StrFormat("Label%s%d", target.type, target.id);
            ImGui::PushID(row_id.c_str());
            if (ImGui::SmallButton(ICON_MD_EDIT)) {
              begin_resource_label_edit(target);
            }
            if (ImGui::IsItemHovered()) {
              ImGui::SetTooltip(
                  tr("Rename %s %s in the open project"), target.title.c_str(),
                  FormatResourceId(target.id, target.hex_width).c_str());
            }
            ImGui::PopID();
          };

          draw_label_row({"Area GFX", "graphics", map->area_graphics()});
          draw_label_row({absl::StrFormat("Sprite GFX %d", game_state),
                          "graphics", map->sprite_graphics(game_state)});
          if (zelda3::OverworldVersionHelper::SupportsAnimatedGFX(
                  rom_version)) {
            draw_label_row({"Animated GFX", "graphics", map->animated_gfx()});
          }
          draw_label_row(
              {"Area Palette", "overworld_area_palette", map->area_palette()});
          if (zelda3::OverworldVersionHelper::SupportsCustomBGColors(
                  rom_version)) {
            draw_label_row({"Main Palette", "overworld_main_palette",
                            map->main_palette()});
          }
          draw_label_row({"Sprite Palette", "overworld_sprite_palette",
                          map->sprite_palette(game_state)});
          draw_label_row(
              {"Message", "message", map->message_id(), /*hex_width=*/4});
          for (int i = 0; i < 4; ++i) {
            draw_label_row({absl::StrFormat("Music %d", i), "overworld_music",
                            map->area_music(i)});
          }

          ImGui::EndTable();
        }
        ImGui::EndDisabled();
        if (!project) {
          ImGui::TextDisabled(tr("Open a project to store metadata labels."));
        }

        if (ImGui::BeginPopup("OverworldMetadataResourceLabel")) {
          ImGui::Text("%s %s", ICON_MD_LABEL, resource_label_title.c_str());
          ImGui::TextDisabled("%s", FormatResourceId(resource_label_id,
                                                     resource_label_hex_width)
                                        .c_str());
          ImGui::SetNextItemWidth(260.0f);
          ImGui::InputText("##OverworldMetadataResourceLabelInput",
                           resource_label_buffer.data(),
                           resource_label_buffer.size());
          if (ImGui::Button(ICON_MD_CHECK " Apply")) {
            apply_resource_label(resource_label_buffer.data());
          }
          ImGui::SameLine();
          if (ImGui::Button(ICON_MD_CLEAR " Clear")) {
            apply_resource_label("");
          }
          if (!resource_label_error.empty()) {
            ImGui::TextWrapped("%s", resource_label_error.c_str());
          }
          ImGui::EndPopup();
        }

        if (!metadata_error.empty()) {
          ImGui::Separator();
          ImGui::TextWrapped("%s", metadata_error.c_str());
        }
        ImGui::EndPopup();
      }
    }

    if (show_upgrade && on_upgrade_rom_version) {
      if (wrote_metadata_item) {
        ImGui::SameLine();
      }
      if (gui::PrimaryButton(ICON_MD_UPGRADE " Upgrade")) {
        on_upgrade_rom_version(3);  // Upgrade to v3
      }
      HOVER_HINT(
          "Upgrade ROM to ZSCustomOverworld v3\n"
          "Enables all advanced features");
    } else if (!show_map_summary && show_overlay_toggle &&
               on_toggle_overlay_preview && is_overlay_preview_enabled) {
      const bool overlay_preview_enabled = is_overlay_preview_enabled();
      if (wrote_metadata_item) {
        ImGui::SameLine();
      }
      if (gui::ToolbarIconButton(ICON_MD_VISIBILITY,
                                 overlay_preview_enabled
                                     ? "Hide overlay preview"
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
    const auto popup_toggle_item = [&](const char* label, const char* panel_id,
                                       const char* shortcut = nullptr) {
      const bool open = window_manager->IsWindowOpen(panel_id);
      if (ImGui::MenuItem(label, shortcut, open)) {
        toggle_window(panel_id);
      }
    };
    {
      gui::StyleVarGuard panel_spacing_guard(ImGuiStyleVar_ItemSpacing,
                                             ImVec2(4, 0));

      if (compact_panel_controls) {
        if (gui::ToolbarIconButton(
                ICON_MD_APPS, "Overworld Windows\nOpen panel toggle menu")) {
          ImGui::OpenPopup("OverworldWindowsPopup");
        }

        if (ImGui::BeginPopup("OverworldWindowsPopup")) {
          popup_toggle_item("Tile16 Editor", OverworldPanelIds::kTile16Editor,
                            "Ctrl+T");
          popup_toggle_item("Tile16 Selector",
                            OverworldPanelIds::kTile16Selector);
          popup_toggle_item("Tile8 Selector",
                            OverworldPanelIds::kTile8Selector);
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
                "Overworld Item List (Ctrl+Shift+I)\nFilter/select items and "
                "use "
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
  ImGui::PopID();
}

}  // namespace yaze::editor
