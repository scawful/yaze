#ifndef YAZE_APP_EDITOR_DUNGEON_PANELS_WATER_FILL_PANEL_H
#define YAZE_APP_EDITOR_DUNGEON_PANELS_WATER_FILL_PANEL_H

#include <algorithm>
#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

#include "absl/strings/str_format.h"
#include "app/editor/agent/agent_ui_theme.h"
#include "app/editor/dungeon/dungeon_canvas_viewer.h"
#include "app/editor/dungeon/dungeon_object_interaction.h"
#include "app/editor/system/editor_panel.h"
#include "app/gui/core/icons.h"
#include "zelda3/dungeon/dungeon_rom_addresses.h"

namespace yaze::editor {

class WaterFillPanel : public EditorPanel {
 public:
  WaterFillPanel(DungeonCanvasViewer* viewer, DungeonObjectInteraction* interaction)
      : viewer_(viewer), interaction_(interaction) {}

  std::string GetId() const override { return "dungeon.water_fill"; }
  std::string GetDisplayName() const override { return "Water Fill"; }
  std::string GetIcon() const override { return ICON_MD_WATER_DROP; }
  std::string GetEditorCategory() const override { return "Dungeon"; }

  void SetCanvasViewer(DungeonCanvasViewer* viewer) { viewer_ = viewer; }
  void SetInteraction(DungeonObjectInteraction* interaction) {
    interaction_ = interaction;
  }

  void Draw(bool* p_open) override {
    (void)p_open;
    const auto& theme = AgentUI::GetTheme();

    if (!viewer_ || !viewer_->HasRooms() || !viewer_->rom() ||
        !viewer_->rom()->is_loaded()) {
      ImGui::TextDisabled(ICON_MD_INFO " No dungeon rooms loaded.");
      return;
    }

    auto* rooms = viewer_->rooms();
    int room_id = viewer_->current_room_id();
    if (room_id < 0 || room_id >= 296) {
      ImGui::TextDisabled(ICON_MD_INFO " Invalid room ID.");
      return;
    }

    auto& room = (*rooms)[room_id];
    if (!room.IsLoaded()) {
      ImGui::TextDisabled(ICON_MD_INFO " Room not loaded yet.");
      return;
    }

    const bool reserved_region_present =
        (zelda3::kWaterFillTableEnd <=
         static_cast<int>(viewer_->rom()->vector().size()));
    if (!reserved_region_present) {
      ImGui::TextColored(
          theme.status_error,
          ICON_MD_ERROR
          " WaterFill reserved region missing (use an expanded-collision Oracle ROM)");
      ImGui::TextDisabled(
          "Expected ROM >= 0x%X bytes (WaterFill end). Current ROM is %zu bytes.",
          zelda3::kWaterFillTableEnd, viewer_->rom()->vector().size());
      ImGui::Separator();
    }

    bool show_overlay = viewer_->show_water_fill_overlay();
    if (ImGui::Checkbox("Show Water Fill Overlay", &show_overlay)) {
      viewer_->set_show_water_fill_overlay(show_overlay);
    }

    if (!interaction_) {
      ImGui::TextDisabled("Painting requires an active interaction context.");
      return;
    }

    // Brush controls are shared across paint modes.
    {
      auto& state = interaction_->mode_manager().GetModeState();
      int brush_radius = std::clamp(state.paint_brush_radius, 0, 8);
      if (ImGui::SliderInt("Brush Radius", &brush_radius, 0, 8)) {
        state.paint_brush_radius = brush_radius;
      }
      ImGui::SameLine();
      ImGui::TextDisabled("%dx%d", (brush_radius * 2) + 1,
                          (brush_radius * 2) + 1);
    }

    bool is_painting = (interaction_->mode_manager().GetMode() ==
                        InteractionMode::PaintWaterFill);
    ImGui::BeginDisabled(!reserved_region_present);
    if (ImGui::Checkbox("Paint Mode", &is_painting)) {
        if (is_painting) {
          interaction_->mode_manager().SetMode(InteractionMode::PaintWaterFill);
          viewer_->set_show_water_fill_overlay(true);
        } else {
          interaction_->mode_manager().SetMode(InteractionMode::Select);
        }
    }
    ImGui::EndDisabled();

    if (is_painting) {
      ImGui::TextColored(theme.text_warning_yellow,
                         "Left-drag paints; Alt-drag erases");
    }

    const int tile_count = room.WaterFillTileCount();
    ImGui::Separator();
    ImGui::Text("Zone Tiles: %d", tile_count);
    if (tile_count > 255) {
      ImGui::TextColored(theme.status_error,
                         ICON_MD_ERROR " Too many tiles (max 255 per room)");
    }

    bool has_switch_sprite = false;
    for (const auto& spr : room.GetSprites()) {
      if (spr.id() == 0x04 || spr.id() == 0x21) {
        has_switch_sprite = true;
        break;
      }
    }
    if (!has_switch_sprite) {
      ImGui::TextColored(theme.text_warning_yellow,
                         ICON_MD_WARNING
                         " No PullSwitch (0x04) / PushSwitch (0x21) sprite found");
    }

    ImGui::Separator();
    uint8_t mask = room.water_fill_sram_bit_mask();
    std::string preview =
        (mask == 0) ? "Auto (0x00)" : absl::StrFormat("0x%02X", mask);
    if (ImGui::BeginCombo("SRAM Bit Mask ($7EF411)", preview.c_str())) {
      auto option = [&](const char* label, uint8_t val) {
        const bool selected = (mask == val);
        if (ImGui::Selectable(label, selected)) {
          room.set_water_fill_sram_bit_mask(val);
          mask = val;
        }
      };

      option("Auto (0x00)", 0x00);
      option("Bit 0 (0x01)", 0x01);
      option("Bit 1 (0x02)", 0x02);
      option("Bit 2 (0x04)", 0x04);
      option("Bit 3 (0x08)", 0x08);
      option("Bit 4 (0x10)", 0x10);
      option("Bit 5 (0x20)", 0x20);
      option("Bit 6 (0x40)", 0x40);
      option("Bit 7 (0x80)", 0x80);

      ImGui::EndCombo();
    }

    ImGui::Separator();
    if (ImGui::Button("Clear Water Fill Zone")) {
      room.ClearWaterFillZone();
    }

    ImGui::TextWrapped(
        "Water fill zones are serialized as compact tile offset lists. "
        "Keep zones under 255 tiles per room.");

    // Overview: show all rooms that currently have zone data, plus global
    // constraints (max 8 rooms / unique SRAM masks).
    ImGui::Separator();
    if (ImGui::CollapsingHeader("Zone Overview",
                                ImGuiTreeNodeFlags_DefaultOpen)) {
      struct ZoneRow {
        int room_id = 0;
        int tiles = 0;
        uint8_t mask = 0;
        bool dirty = false;
      };

      std::vector<ZoneRow> rows;
      rows.reserve(8);
      std::unordered_map<uint8_t, int> mask_counts;
      int rooms_over_tile_limit = 0;
      int rooms_unassigned_mask = 0;

      for (int rid = 0; rid < static_cast<int>(rooms->size()); ++rid) {
        auto& r = (*rooms)[rid];
        const int tiles = r.WaterFillTileCount();
        if (tiles <= 0) continue;
        rows.push_back(ZoneRow{rid, tiles, r.water_fill_sram_bit_mask(),
                               r.water_fill_dirty()});
        if (tiles > 255) {
          rooms_over_tile_limit++;
        }
        if (r.water_fill_sram_bit_mask() == 0) {
          rooms_unassigned_mask++;
        } else {
          mask_counts[r.water_fill_sram_bit_mask()]++;
        }
      }

      std::sort(rows.begin(), rows.end(),
                [](const ZoneRow& a, const ZoneRow& b) {
                  return a.room_id < b.room_id;
                });

      int duplicate_masks = 0;
      for (const auto& [mask, count] : mask_counts) {
        if (mask != 0 && count > 1) {
          duplicate_masks++;
        }
      }

      ImGui::Text("Rooms with zones: %zu / 8", rows.size());
      if (rows.size() > 8) {
        ImGui::TextColored(theme.status_error,
                           ICON_MD_ERROR " Too many rooms with zones (max 8)");
      }
      if (rooms_over_tile_limit > 0) {
        ImGui::TextColored(theme.status_error,
                           ICON_MD_ERROR
                           " %d room(s) exceed 255 tiles",
                           rooms_over_tile_limit);
      }
      if (duplicate_masks > 0) {
        ImGui::TextColored(theme.status_error,
                           ICON_MD_ERROR
                           " Duplicate SRAM bit masks detected (%d mask(s))",
                           duplicate_masks);
      }
      if (rooms_unassigned_mask > 0) {
        ImGui::TextColored(theme.text_warning_yellow,
                           ICON_MD_WARNING
                           " %d room(s) use Auto mask (assigned on save)",
                           rooms_unassigned_mask);
      }

      if (ImGui::BeginTable("##WaterFillZoneOverview", 6,
                            ImGuiTableFlags_Borders |
                                ImGuiTableFlags_RowBg |
                                ImGuiTableFlags_SizingFixedFit)) {
        ImGui::TableSetupColumn("Room");
        ImGui::TableSetupColumn("Tiles");
        ImGui::TableSetupColumn("Mask");
        ImGui::TableSetupColumn("Dirty");
        ImGui::TableSetupColumn("Dup?");
        ImGui::TableSetupColumn("Action");
        ImGui::TableHeadersRow();

        for (const auto& row : rows) {
          const bool is_current = (row.room_id == room_id);
          const bool is_dup =
              (row.mask != 0 && mask_counts.contains(row.mask) &&
               mask_counts[row.mask] > 1);

          ImGui::TableNextRow();
          ImGui::TableNextColumn();
          if (is_current) {
            ImGui::TextColored(theme.text_info, "0x%02X", row.room_id);
          } else {
            ImGui::Text("0x%02X", row.room_id);
          }

          ImGui::TableNextColumn();
          ImGui::Text("%d", row.tiles);

          ImGui::TableNextColumn();
          if (row.mask == 0) {
            ImGui::TextDisabled("Auto");
          } else {
            ImGui::Text("0x%02X", row.mask);
          }

          ImGui::TableNextColumn();
          ImGui::TextUnformatted(row.dirty ? "Yes" : "No");

          ImGui::TableNextColumn();
          if (is_dup) {
            ImGui::TextColored(theme.status_error, ICON_MD_ERROR);
          } else {
            ImGui::TextDisabled("-");
          }

          ImGui::TableNextColumn();
          if (viewer_->CanNavigateRooms()) {
            ImGui::PushID(row.room_id);
            if (ImGui::SmallButton("Open")) {
              viewer_->NavigateToRoom(row.room_id);
            }
            ImGui::PopID();
          } else {
            ImGui::TextDisabled("-");
          }
        }

        ImGui::EndTable();
      }
    }
  }

 private:
  DungeonCanvasViewer* viewer_ = nullptr;
  DungeonObjectInteraction* interaction_ = nullptr;
};

}  // namespace yaze::editor

#endif  // YAZE_APP_EDITOR_DUNGEON_PANELS_WATER_FILL_PANEL_H
