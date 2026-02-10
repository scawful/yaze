#ifndef YAZE_APP_EDITOR_DUNGEON_PANELS_WATER_FILL_PANEL_H
#define YAZE_APP_EDITOR_DUNGEON_PANELS_WATER_FILL_PANEL_H

#include <array>
#include <algorithm>
#include <cstdint>
#include <exception>
#include <fstream>
#include <string>
#include <unordered_map>
#include <vector>

#include "absl/strings/str_format.h"
#include "app/editor/agent/agent_ui_theme.h"
#include "app/editor/dungeon/dungeon_canvas_viewer.h"
#include "app/editor/dungeon/dungeon_object_interaction.h"
#include "app/editor/system/editor_panel.h"
#include "app/gui/core/icons.h"
#include "util/file_util.h"
#include "zelda3/dungeon/dungeon_rom_addresses.h"
#include "zelda3/dungeon/water_fill_zone.h"

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
        !viewer_->rom()->is_loaded() || !viewer_->rooms()) {
      ImGui::TextDisabled(ICON_MD_INFO " No dungeon rooms loaded.");
      return;
    }

    auto* rooms = viewer_->rooms();
    const int room_id = viewer_->current_room_id();
    const bool room_id_valid =
        (room_id >= 0 && room_id < static_cast<int>(rooms->size()));

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

    ImGui::Separator();
    ImGui::TextUnformatted("Authoring");

    util::FileDialogOptions json_options;
    json_options.filters.push_back({"Water Fill Zones", "json"});
    json_options.filters.push_back({"All Files", "*"});

    ImGui::BeginDisabled(!reserved_region_present);
    if (ImGui::Button(ICON_MD_UPLOAD " Import Zones...")) {
      std::string path =
          util::FileDialogWrapper::ShowOpenFileDialog(json_options);
      if (!path.empty()) {
        try {
          std::string contents = util::LoadFile(path);
          auto zones_or = zelda3::LoadWaterFillZonesFromJsonString(contents);
          if (!zones_or.ok()) {
            last_io_error_ = zones_or.status().message();
            last_io_status_.clear();
          } else {
            auto zones = std::move(zones_or.value());
            for (const auto& z : zones) {
              if (z.room_id < 0 || z.room_id >= static_cast<int>(rooms->size())) {
                continue;
              }
              ApplyZoneToRoom(z, &(*rooms)[z.room_id]);
            }
            viewer_->set_show_water_fill_overlay(true);
            last_io_status_ = absl::StrFormat("Imported %zu zone(s) from %s",
                                              zones.size(), path.c_str());
            last_io_error_.clear();
          }
        } catch (const std::exception& e) {
          last_io_error_ = e.what();
          last_io_status_.clear();
        }
      }
    }
    ImGui::SameLine();
    if (ImGui::Button(ICON_MD_TUNE " Normalize Masks Now")) {
      auto zones = CollectZones(*rooms);
      auto st = zelda3::NormalizeWaterFillZoneMasks(&zones);
      if (!st.ok()) {
        last_io_error_ = st.message();
        last_io_status_.clear();
      } else {
        int changed = 0;
        for (const auto& z : zones) {
          auto& r = (*rooms)[z.room_id];
          if (r.water_fill_sram_bit_mask() != z.sram_bit_mask) {
            r.set_water_fill_sram_bit_mask(z.sram_bit_mask);
            ++changed;
          }
        }
        if (changed > 0) {
          viewer_->set_show_water_fill_overlay(true);
        }
        last_io_status_ =
            absl::StrFormat("Normalized masks (%d room(s) updated)", changed);
        last_io_error_.clear();
      }
    }
    ImGui::EndDisabled();

    ImGui::SameLine();
    if (ImGui::Button(ICON_MD_DOWNLOAD " Export Zones...")) {
      auto zones = CollectZones(*rooms);
      auto json_or = zelda3::DumpWaterFillZonesToJsonString(zones);
      if (!json_or.ok()) {
        last_io_error_ = json_or.status().message();
        last_io_status_.clear();
      } else {
        std::string path = util::FileDialogWrapper::ShowSaveFileDialog(
            "water_fill_zones.json", "json");
        if (!path.empty()) {
          std::ofstream file(path);
          if (!file.is_open()) {
            last_io_error_ =
                absl::StrFormat("Cannot write file: %s", path.c_str());
            last_io_status_.clear();
          } else {
            file << *json_or;
            file.close();
            last_io_status_ = absl::StrFormat("Exported %zu zone(s) to %s",
                                              zones.size(), path.c_str());
            last_io_error_.clear();
          }
        }
      }
    }

    if (!last_io_error_.empty()) {
      ImGui::TextColored(theme.status_error, ICON_MD_ERROR " %s",
                         last_io_error_.c_str());
    } else if (!last_io_status_.empty()) {
      ImGui::TextColored(theme.status_success, ICON_MD_CHECK_CIRCLE " %s",
                         last_io_status_.c_str());
    }

    ImGui::TextWrapped(
        "Import/export uses a room-indexed JSON format. Normalize masks before "
        "saving to avoid duplicate SRAM bits.");

    ImGui::Separator();
    if (!room_id_valid) {
      ImGui::TextDisabled(ICON_MD_INFO " Invalid room ID.");
    } else {
      auto& room = (*rooms)[room_id];
      const bool room_loaded = room.IsLoaded();
      if (!room_loaded) {
        ImGui::TextDisabled(
            ICON_MD_INFO
            " Room not loaded yet (open it to paint and validate sprites).");
      }

      if (!interaction_) {
        ImGui::TextDisabled("Painting requires an active interaction context.");
      } else {
        // Brush controls are shared across paint modes.
        auto& state = interaction_->mode_manager().GetModeState();
        int brush_radius = std::clamp(state.paint_brush_radius, 0, 8);
        if (ImGui::SliderInt("Brush Radius", &brush_radius, 0, 8)) {
          state.paint_brush_radius = brush_radius;
        }
        ImGui::SameLine();
        ImGui::TextDisabled("%dx%d", (brush_radius * 2) + 1,
                            (brush_radius * 2) + 1);

        bool is_painting = (interaction_->mode_manager().GetMode() ==
                            InteractionMode::PaintWaterFill);
        const bool can_paint = reserved_region_present && room_loaded;
        ImGui::BeginDisabled(!can_paint);
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
      }

      const int tile_count = room.WaterFillTileCount();
      ImGui::Separator();
      ImGui::Text("Zone Tiles: %d", tile_count);
      if (tile_count > 255) {
        ImGui::TextColored(theme.status_error,
                           ICON_MD_ERROR " Too many tiles (max 255 per room)");
      }

      if (room_loaded) {
        bool has_switch_sprite = false;
        for (const auto& spr : room.GetSprites()) {
          if (spr.id() == 0x04 || spr.id() == 0x21) {
            has_switch_sprite = true;
            break;
          }
        }
        if (!has_switch_sprite) {
          ImGui::TextColored(
              theme.text_warning_yellow,
              ICON_MD_WARNING
              " No PullSwitch (0x04) / PushSwitch (0x21) sprite found");
        }
      } else {
        ImGui::TextDisabled("Sprite checks require the room to be loaded.");
      }

      ImGui::Separator();
      uint8_t mask = room.water_fill_sram_bit_mask();
      std::string preview =
          (mask == 0) ? "Auto (0x00)" : absl::StrFormat("0x%02X", mask);
      ImGui::BeginDisabled(!reserved_region_present);
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
      ImGui::EndDisabled();

      ImGui::TextWrapped(
          "Water fill zones are serialized as compact tile offset lists. "
          "Keep zones under 255 tiles per room.");
    }

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
  static std::vector<zelda3::WaterFillZoneEntry> CollectZones(
      const std::array<zelda3::Room, 0x128>& rooms) {
    std::vector<zelda3::WaterFillZoneEntry> zones;
    zones.reserve(8);
    for (int room_id = 0; room_id < static_cast<int>(rooms.size()); ++room_id) {
      const auto& room = rooms[room_id];
      const int tile_count = room.WaterFillTileCount();
      if (tile_count <= 0) {
        continue;
      }

      zelda3::WaterFillZoneEntry z;
      z.room_id = room_id;
      z.sram_bit_mask = room.water_fill_sram_bit_mask();
      z.fill_offsets.reserve(static_cast<size_t>(tile_count));

      const auto& map = room.water_fill_zone().tiles;
      for (size_t i = 0; i < map.size(); ++i) {
        if (map[i] != 0) {
          z.fill_offsets.push_back(static_cast<uint16_t>(i));
        }
      }
      zones.push_back(std::move(z));
    }
    return zones;
  }

  static void ApplyZoneToRoom(const zelda3::WaterFillZoneEntry& z,
                             zelda3::Room* room) {
    if (room == nullptr) {
      return;
    }
    room->ClearWaterFillZone();
    room->set_water_fill_sram_bit_mask(z.sram_bit_mask);
    for (uint16_t off : z.fill_offsets) {
      const int x = static_cast<int>(off % 64);
      const int y = static_cast<int>(off / 64);
      room->SetWaterFillTile(x, y, true);
    }
  }

  std::string last_io_status_;
  std::string last_io_error_;

  DungeonCanvasViewer* viewer_ = nullptr;
  DungeonObjectInteraction* interaction_ = nullptr;
};

}  // namespace yaze::editor

#endif  // YAZE_APP_EDITOR_DUNGEON_PANELS_WATER_FILL_PANEL_H
