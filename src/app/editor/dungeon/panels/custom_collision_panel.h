#ifndef YAZE_APP_EDITOR_DUNGEON_PANELS_CUSTOM_COLLISION_PANEL_H
#define YAZE_APP_EDITOR_DUNGEON_PANELS_CUSTOM_COLLISION_PANEL_H

#include <exception>
#include <fstream>
#include <string>
#include <vector>

#include "app/editor/agent/agent_ui_theme.h"
#include "app/editor/dungeon/dungeon_canvas_viewer.h"
#include "app/editor/dungeon/dungeon_object_interaction.h"
#include "app/editor/system/editor_panel.h"
#include "zelda3/zelda3_labels.h"
#include "app/gui/core/icons.h"
#include "absl/strings/str_format.h"
#include "util/file_util.h"
#include "zelda3/dungeon/custom_collision.h"
#include "zelda3/dungeon/dungeon_rom_addresses.h"

#include <algorithm>

namespace yaze::editor {

class CustomCollisionPanel : public EditorPanel {
 public:
  CustomCollisionPanel(DungeonCanvasViewer* viewer, DungeonObjectInteraction* interaction)
      : viewer_(viewer), interaction_(interaction) {}

  std::string GetId() const override { return "dungeon.custom_collision"; }
  std::string GetDisplayName() const override { return "Custom Collision"; }
  std::string GetIcon() const override { return ICON_MD_GRID_ON; }
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

    const size_t rom_size = viewer_->rom()->vector().size();
    const int ptr_table_end =
        zelda3::kCustomCollisionRoomPointers + (zelda3::kNumberOfRooms * 3);
    const bool collision_table_present =
        zelda3::HasCustomCollisionPointerTable(rom_size);
    const bool collision_data_region_present =
        zelda3::HasCustomCollisionDataRegion(rom_size);
    const bool collision_save_supported =
        zelda3::HasCustomCollisionWriteSupport(rom_size);
    if (!collision_table_present) {
      ImGui::TextColored(
          theme.status_error,
          ICON_MD_ERROR
          " Custom collision table missing (use an expanded-collision Oracle ROM)");
      ImGui::TextDisabled(
          "Expected ROM >= 0x%X bytes (custom collision pointer table end). Current ROM is %zu bytes.",
          ptr_table_end, rom_size);
      ImGui::Separator();
    } else if (!collision_data_region_present) {
      ImGui::TextColored(
          theme.status_error,
          ICON_MD_ERROR
          " Custom collision data region missing/truncated");
      ImGui::TextDisabled(
          "Expected ROM >= 0x%X bytes (custom collision data soft end). Current ROM is %zu bytes.",
          zelda3::kCustomCollisionDataSoftEnd, rom_size);
      ImGui::Separator();
    }

    auto* rooms = viewer_->rooms();
    int room_id = viewer_->current_room_id();
    if (room_id < 0 || room_id >= 296) {
      ImGui::TextDisabled(ICON_MD_INFO " Invalid room ID.");
      return;
    }

    auto& room = (*rooms)[room_id];
    bool has_collision = room.has_custom_collision();

    ImGui::BeginDisabled(!collision_save_supported);
    if (ImGui::Button(has_collision ? "Disable Custom Collision"
                                    : "Enable Custom Collision")) {
        room.set_has_custom_collision(!has_collision);
        viewer_->set_show_custom_collision_overlay(room.has_custom_collision());
    }
    ImGui::EndDisabled();

    ImGui::Separator();

    ImGui::TextUnformatted("Authoring");

    util::FileDialogOptions json_options;
    json_options.filters.push_back({"Custom Collision", "json"});
    json_options.filters.push_back({"All Files", "*"});

    ImGui::BeginDisabled(!collision_save_supported);
    if (ImGui::Button(ICON_MD_UPLOAD " Import Collision...")) {
      std::string path =
          util::FileDialogWrapper::ShowOpenFileDialog(json_options);
      if (!path.empty()) {
        try {
          std::string contents = util::LoadFile(path);
          auto rooms_or =
              zelda3::LoadCustomCollisionRoomsFromJsonString(contents);
          if (!rooms_or.ok()) {
            last_io_error_ = rooms_or.status().message();
            last_io_status_.clear();
          } else {
            const auto imported = std::move(rooms_or.value());
            for (const auto& entry : imported) {
              if (entry.room_id < 0 ||
                  entry.room_id >= static_cast<int>(rooms->size())) {
                continue;
              }
              ApplyRoomEntry(entry, &(*rooms)[entry.room_id]);
            }
            viewer_->set_show_custom_collision_overlay(true);
            last_io_status_ =
                absl::StrFormat("Imported %zu room(s) from %s", imported.size(),
                                path.c_str());
            last_io_error_.clear();
          }
        } catch (const std::exception& e) {
          last_io_error_ = e.what();
          last_io_status_.clear();
        }
      }
    }
    ImGui::SameLine();
    if (ImGui::Button(ICON_MD_DOWNLOAD " Export Collision...")) {
      auto exported = CollectRoomEntries(*rooms);
      auto json_or = zelda3::DumpCustomCollisionRoomsToJsonString(exported);
      if (!json_or.ok()) {
        last_io_error_ = json_or.status().message();
        last_io_status_.clear();
      } else {
        std::string path = util::FileDialogWrapper::ShowSaveFileDialog(
            "custom_collision.json", "json");
        if (!path.empty()) {
          std::ofstream file(path);
          if (!file.is_open()) {
            last_io_error_ =
                absl::StrFormat("Cannot write file: %s", path.c_str());
            last_io_status_.clear();
          } else {
            file << *json_or;
            file.close();
            last_io_status_ = absl::StrFormat("Exported %zu room(s) to %s",
                                              exported.size(), path.c_str());
            last_io_error_.clear();
          }
        }
      }
    }
    ImGui::EndDisabled();

    if (!last_io_error_.empty()) {
      ImGui::TextColored(theme.status_error, ICON_MD_ERROR " %s",
                         last_io_error_.c_str());
    } else if (!last_io_status_.empty()) {
      ImGui::TextColored(theme.status_success, ICON_MD_CHECK_CIRCLE " %s",
                         last_io_status_.c_str());
    }

    ImGui::Separator();

    if (has_collision) {
        if (!collision_save_supported) {
          ImGui::TextColored(
              theme.text_warning_yellow,
              ICON_MD_WARNING
              " This ROM cannot save custom collision edits (expanded collision region missing).");
        }

        bool show_overlay = viewer_->show_custom_collision_overlay();
        if (ImGui::Checkbox("Show Collision Overlay", &show_overlay)) {
            viewer_->set_show_custom_collision_overlay(show_overlay);
        }

        if (!interaction_) {
          ImGui::TextDisabled("Painting requires an active interaction context.");
          return;
        }

        bool is_painting = (interaction_->mode_manager().GetMode() ==
                            InteractionMode::PaintCollision);
        ImGui::BeginDisabled(!collision_save_supported);
        if (ImGui::Checkbox("Paint Mode", &is_painting)) {
            if (is_painting) {
                interaction_->mode_manager().SetMode(InteractionMode::PaintCollision);
            } else {
                interaction_->mode_manager().SetMode(InteractionMode::Select);
            }
        }
        ImGui::EndDisabled();

        if (is_painting) {
            ImGui::TextColored(theme.text_warning_yellow,
                               "Click/Drag on canvas to paint");
            
            auto& state = interaction_->mode_manager().GetModeState();
            int current_val = state.paint_collision_value;

            int brush_radius = std::clamp(state.paint_brush_radius, 0, 8);
            if (ImGui::SliderInt("Brush Radius", &brush_radius, 0, 8)) {
              state.paint_brush_radius = brush_radius;
            }
            ImGui::SameLine();
            ImGui::TextDisabled("%dx%d", (brush_radius * 2) + 1,
                                (brush_radius * 2) + 1);
            
            const auto& tile_types = zelda3::Zelda3Labels::GetTileTypeNames();
            
            if (ImGui::BeginCombo("Collision Type", absl::StrFormat("%02X: %s", current_val, (current_val < tile_types.size() ? tile_types[current_val] : "Unknown")).c_str())) {
                for (int i = 0; i < tile_types.size(); ++i) {
                    bool selected = (current_val == i);
                    if (ImGui::Selectable(absl::StrFormat("%02X: %s", i, tile_types[i]).c_str(), selected)) {
                        state.paint_collision_value = static_cast<uint8_t>(i);
                    }
                }
                ImGui::EndCombo();
            }

            ImGui::Separator();
            ImGui::Text("Quick Select:");
            auto quick_button = [&](const char* label, uint8_t val) {
              if (ImGui::Button(label)) {
                state.paint_collision_value = val;
              }
            };
            quick_button("Floor (00)", 0x00); ImGui::SameLine();
            quick_button("Solid (02)", 0x02); ImGui::SameLine();
            quick_button("D.Water (08)", 0x08);
            quick_button("S.Water (09)", 0x09); ImGui::SameLine();
            quick_button("Pit (1B)", 0x1B); ImGui::SameLine();
            quick_button("Spikes (0E)", 0x0E);
        }
        
        ImGui::Separator();
        ImGui::BeginDisabled(!collision_save_supported);
        if (ImGui::Button("Clear All Custom Collision")) {
            room.custom_collision().tiles.fill(0);
            // Clearing should remove the override (room falls back to vanilla).
            room.custom_collision().has_data = false;
            room.MarkCustomCollisionDirty();
            viewer_->set_show_custom_collision_overlay(false);
        }
        ImGui::EndDisabled();
    } else {
        ImGui::TextWrapped("Custom collision allows you to override the physics of individual 8x8 tiles in the room. This is useful for creating water, pits, or other effects that don't match the background tiles.");
    }
  }

 private:
  static std::vector<zelda3::CustomCollisionRoomEntry> CollectRoomEntries(
      const std::array<zelda3::Room, 0x128>& rooms) {
    std::vector<zelda3::CustomCollisionRoomEntry> out;
    out.reserve(16);
    for (int rid = 0; rid < static_cast<int>(rooms.size()); ++rid) {
      const auto& room = rooms[rid];

      // Export only rooms with any non-zero override tiles.
      bool any = false;
      zelda3::CustomCollisionRoomEntry entry;
      entry.room_id = rid;
      const auto& map = room.custom_collision().tiles;
      for (size_t off = 0; off < map.size(); ++off) {
        const uint8_t val = map[off];
        if (val == 0) {
          continue;
        }
        any = true;
        entry.tiles.push_back(zelda3::CustomCollisionTileEntry{
            static_cast<uint16_t>(off), val});
      }
      if (!any) {
        continue;
      }
      out.push_back(std::move(entry));
    }
    return out;
  }

  static void ApplyRoomEntry(const zelda3::CustomCollisionRoomEntry& entry,
                             zelda3::Room* room) {
    if (room == nullptr) {
      return;
    }
    room->custom_collision().tiles.fill(0);
    bool any = false;
    for (const auto& t : entry.tiles) {
      if (t.offset >= room->custom_collision().tiles.size()) {
        continue;
      }
      room->custom_collision().tiles[t.offset] = t.value;
      if (t.value != 0) {
        any = true;
      }
    }
    room->custom_collision().has_data = any;
    room->MarkCustomCollisionDirty();
  }

  DungeonCanvasViewer* viewer_;
  DungeonObjectInteraction* interaction_;

  std::string last_io_status_;
  std::string last_io_error_;
};

} // namespace yaze::editor

#endif
