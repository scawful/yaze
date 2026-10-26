#include "app/editor/dungeon/ui/window/custom_collision_panel.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <exception>
#include <fstream>
#include <string>
#include <utility>
#include <vector>

#include "absl/strings/str_format.h"
#include "app/editor/agent/agent_ui_theme.h"
#include "app/gui/automation/widget_auto_register.h"
#include "imgui/imgui.h"
#include "util/file_util.h"
#include "util/i18n/tr.h"
#include "zelda3/dungeon/dungeon_rom_addresses.h"
#include "zelda3/zelda3_labels.h"

namespace yaze::editor {

void CustomCollisionPanel::Draw(bool* p_open) {
  (void)p_open;
  gui::AutoWidgetScope automation_scope("Dungeon/CustomCollision");
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
    ImGui::TextColored(theme.status_error, ICON_MD_ERROR
                       " Custom collision table missing (use an "
                       "expanded-collision Oracle ROM)");
    ImGui::TextDisabled(
        tr("Expected ROM >= 0x%X bytes (custom collision pointer table end). "
           "Current ROM is %zu bytes."),
        ptr_table_end, rom_size);
    ImGui::Separator();
  } else if (!collision_data_region_present) {
    ImGui::TextColored(theme.status_error, ICON_MD_ERROR
                       " Custom collision data region missing/truncated");
    ImGui::TextDisabled(
        tr("Expected ROM >= 0x%X bytes (custom collision data soft end). "
           "Current ROM is %zu bytes."),
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
    if (interaction_) {
      interaction_->BeginCustomCollisionMutation();
    }
    room.set_has_custom_collision(!has_collision);
    if (!room.has_custom_collision() && interaction_ &&
        interaction_->mode_manager().GetMode() ==
            InteractionMode::PaintCollision) {
      interaction_->mode_manager().SetMode(InteractionMode::Select);
    }
    if (interaction_) {
      interaction_->EndCustomCollisionMutation();
    }
    viewer_->set_show_custom_collision_overlay(room.has_custom_collision());
    has_collision = room.has_custom_collision();
  }
  gui::AutoRegisterLastItem("button", "toggle_custom_collision");
  ImGui::EndDisabled();

  ImGui::Separator();

  ImGui::TextUnformatted(tr("Authoring"));

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
        (void)ImportCollisionJson(contents, path);
      } catch (const std::exception& e) {
        last_io_error_ = e.what();
        last_io_status_.clear();
      }
    }
  }
  ImGui::SameLine();
  if (ImGui::Button(ICON_MD_DOWNLOAD " Export Collision...")) {
    auto exported_or = CollectRoomEntries(*rooms);
    if (!exported_or.ok()) {
      last_io_error_ = std::string(exported_or.status().message());
      last_io_status_.clear();
    } else {
      std::string path = util::FileDialogWrapper::ShowSaveFileDialog(
          "custom_collision.json", "json");
      if (!path.empty()) {
        (void)ExportCollisionToFile(*rooms, path);
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
      ImGui::TextColored(theme.text_warning_yellow, ICON_MD_WARNING
                         " This ROM cannot save custom collision edits "
                         "(expanded collision region missing).");
    }

    const auto& map = room.custom_collision();
    size_t override_count = 0;
    size_t floor_override_count = 0;
    for (size_t off = 0; off < map.tiles.size(); ++off) {
      if (!map.HasOverride(off)) {
        continue;
      }
      ++override_count;
      if (map.tiles[off] == 0x00) {
        ++floor_override_count;
      }
    }
    ImGui::Text(tr("Override tiles: %zu (%zu floor 00)"), override_count,
                floor_override_count);

    bool show_overlay = viewer_->show_custom_collision_overlay();
    if (ImGui::Checkbox(tr("Show Collision Overlay"), &show_overlay)) {
      viewer_->set_show_custom_collision_overlay(show_overlay);
    }
    gui::AutoRegisterLastItem("checkbox", "show_collision_overlay");

    if (!interaction_) {
      ImGui::TextDisabled(
          tr("Painting requires an active interaction context."));
      return;
    }

    bool is_painting = (interaction_->mode_manager().GetMode() ==
                        InteractionMode::PaintCollision);
    ImGui::BeginDisabled(!collision_save_supported);
    if (ImGui::Checkbox(tr("Paint Mode"), &is_painting)) {
      if (is_painting) {
        interaction_->mode_manager().SetMode(InteractionMode::PaintCollision);
        viewer_->set_show_custom_collision_overlay(true);
      } else {
        interaction_->mode_manager().SetMode(InteractionMode::Select);
      }
    }
    gui::AutoRegisterLastItem("checkbox", "collision_paint_mode");
    ImGui::EndDisabled();

    if (is_painting) {
      ImGui::TextColored(
          theme.text_warning_yellow,
          tr("Left-drag paints; Alt-drag or Erase mode clears override"));

      auto& state = interaction_->mode_manager().GetModeState();
      int current_val = state.paint_collision_value;

      int brush_radius = std::clamp(state.paint_brush_radius, 0, 8);
      if (ImGui::SliderInt(tr("Brush Radius"), &brush_radius, 0, 8)) {
        state.paint_brush_radius = brush_radius;
      }
      gui::AutoRegisterLastItem("slider_int", "collision_brush_radius");
      ImGui::SameLine();
      ImGui::TextDisabled(tr("%dx%d"), (brush_radius * 2) + 1,
                          (brush_radius * 2) + 1);

      bool erase_override = state.paint_collision_erase;
      if (ImGui::Checkbox(tr("Erase Override (Unset)"), &erase_override)) {
        state.paint_collision_erase = erase_override;
      }
      gui::AutoRegisterLastItem("checkbox", "collision_erase_override");
      if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip(
            tr("Removes custom collision overrides so tiles fall back to "
               "underlying object collision. Hold Alt while painting to erase "
               "momentarily."));
      }

      const auto& tile_types = zelda3::Zelda3Labels::GetTileTypeNames();

      ImGui::BeginDisabled(state.paint_collision_erase);
      if (ImGui::BeginCombo(tr("Collision Type"),
                            absl::StrFormat("%02X: %s", current_val,
                                            (current_val < tile_types.size()
                                                 ? tile_types[current_val]
                                                 : "Unknown"))
                                .c_str())) {
        for (int i = 0; i < static_cast<int>(tile_types.size()); ++i) {
          bool selected = (current_val == i);
          if (ImGui::Selectable(
                  absl::StrFormat("%02X: %s", i, tile_types[i]).c_str(),
                  selected)) {
            state.paint_collision_value = static_cast<uint8_t>(i);
            state.paint_collision_erase = false;
          }
        }
        ImGui::EndCombo();
      }
      gui::AutoRegisterLastItem("combo", "collision_tile_type");
      ImGui::EndDisabled();

      ImGui::Separator();
      ImGui::Text(tr("Quick Select:"));
      auto quick_button = [&](const char* label, const char* id, uint8_t val,
                              const char* tooltip = nullptr) {
        if (ImGui::Button(label)) {
          state.paint_collision_value = val;
          state.paint_collision_erase = false;
        }
        gui::AutoRegisterLastItem("button", id);
        if (tooltip && ImGui::IsItemHovered()) {
          ImGui::SetTooltip("%s", tooltip);
        }
      };
      quick_button(
          "Floor Override (00)", "quick_collision_floor_00", 0x00,
          "Forces walkable floor (0x00) over solid object tiles. Use Erase "
          "Override (or Alt-drag) to remove an override instead.");
      ImGui::SameLine();
      quick_button("Solid (02)", "quick_collision_solid_02", 0x02);
      ImGui::SameLine();
      quick_button("D.Water (08)", "quick_collision_dwater_08", 0x08);
      quick_button("S.Water (09)", "quick_collision_swater_09", 0x09);
      ImGui::SameLine();
      quick_button("Pit (1B)", "quick_collision_pit_1b", 0x1B);
      ImGui::SameLine();
      quick_button("Spikes (0E)", "quick_collision_spikes_0e", 0x0E);
    }

    ImGui::Separator();
    ImGui::BeginDisabled(!collision_save_supported);
    if (ImGui::Button(tr("Clear All Custom Collision"))) {
      if (interaction_) {
        interaction_->BeginCustomCollisionMutation();
      }
      // Clearing should remove the override (room falls back to vanilla).
      room.custom_collision() = {};
      room.MarkCustomCollisionDirty();
      if (interaction_ && interaction_->mode_manager().GetMode() ==
                              InteractionMode::PaintCollision) {
        interaction_->mode_manager().SetMode(InteractionMode::Select);
      }
      if (interaction_) {
        interaction_->EndCustomCollisionMutation();
      }
      viewer_->set_show_custom_collision_overlay(false);
    }
    gui::AutoRegisterLastItem("button", "clear_all_custom_collision");
    ImGui::EndDisabled();
  } else {
    ImGui::TextWrapped(
        tr("Custom collision allows you to override the physics of individual "
           "8x8 tiles in the room. This is useful for creating water, pits, or "
           "walkable floor passages over solid objects."));
  }
}

absl::Status CustomCollisionPanel::ImportCollisionRooms(
    const std::vector<zelda3::CustomCollisionRoomEntry>& entries,
    std::string_view source_label) {
  if (!viewer_ || !viewer_->rooms() || !viewer_->rom() ||
      !viewer_->rom()->is_loaded()) {
    last_io_error_ = "No dungeon rooms loaded.";
    last_io_status_.clear();
    return absl::FailedPreconditionError(last_io_error_);
  }
  auto* rooms = viewer_->rooms();
  if (interaction_ && interaction_->HasCollisionBatchImportCallback()) {
    auto status = interaction_->ApplyCustomCollisionImportBatch(entries);
    if (!status.ok()) {
      last_io_error_ = std::string(status.message());
      last_io_status_.clear();
      return status;
    }
  } else {
    const int active_room_id = viewer_->current_room_id();
    for (const auto& entry : entries) {
      if (entry.room_id < 0 ||
          entry.room_id >= static_cast<int>(rooms->size())) {
        last_io_error_ = absl::StrFormat(
            "Collision import room 0x%03X is out of range", entry.room_id);
        last_io_status_.clear();
        return absl::OutOfRangeError(last_io_error_);
      }
    }
    for (const auto& entry : entries) {
      const bool is_active = (entry.room_id == active_room_id);
      if (is_active && interaction_) {
        interaction_->BeginCustomCollisionMutation();
      }
      ApplyRoomEntry(entry, &(*rooms)[entry.room_id]);
      if (is_active && interaction_) {
        interaction_->EndCustomCollisionMutation();
      }
    }
  }
  viewer_->set_show_custom_collision_overlay(true);
  last_io_status_ =
      absl::StrFormat("Imported %zu room(s) from %s", entries.size(),
                      std::string(source_label).c_str());
  last_io_error_.clear();
  return absl::OkStatus();
}

absl::Status CustomCollisionPanel::ImportCollisionJson(
    std::string_view json_content, std::string_view source_label) {
  auto rooms_or =
      zelda3::LoadCustomCollisionRoomsFromJsonString(std::string(json_content));
  if (!rooms_or.ok()) {
    last_io_error_ = std::string(rooms_or.status().message());
    last_io_status_.clear();
    return rooms_or.status();
  }
  return ImportCollisionRooms(*rooms_or, source_label);
}

absl::Status CustomCollisionPanel::ExportCollisionToFile(
    const DungeonRoomStore& rooms, const std::string& path) {
  auto exported_or = CollectRoomEntries(rooms);
  if (!exported_or.ok()) {
    last_io_error_ = std::string(exported_or.status().message());
    last_io_status_.clear();
    return exported_or.status();
  }
  auto json_or = zelda3::DumpCustomCollisionRoomsToJsonString(*exported_or);
  if (!json_or.ok()) {
    last_io_error_ = std::string(json_or.status().message());
    last_io_status_.clear();
    return json_or.status();
  }
  std::ofstream file(path);
  if (!file.is_open()) {
    last_io_error_ = absl::StrFormat("Cannot write file: %s", path.c_str());
    last_io_status_.clear();
    return absl::PermissionDeniedError(last_io_error_);
  }
  file << *json_or;
  file.close();
  if (!file) {
    last_io_error_ = absl::StrFormat("Failed writing file: %s", path.c_str());
    last_io_status_.clear();
    return absl::DataLossError(last_io_error_);
  }
  last_io_status_ = absl::StrFormat("Exported %zu room(s) to %s",
                                    exported_or->size(), path.c_str());
  last_io_error_.clear();
  return absl::OkStatus();
}

absl::StatusOr<std::vector<zelda3::CustomCollisionRoomEntry>>
CustomCollisionPanel::CollectRoomEntries(const DungeonRoomStore& rooms) {
  std::vector<zelda3::CustomCollisionRoomEntry> out;
  out.reserve(16);
  Rom* rom = rooms.rom();
  const bool can_read_rom =
      rom != nullptr && rom->is_loaded() &&
      zelda3::HasCustomCollisionPointerTable(rom->vector().size());
  for (int rid = 0; rid < static_cast<int>(rooms.size()); ++rid) {
    const zelda3::Room* materialized = rooms.GetIfMaterialized(rid);
    const zelda3::CustomCollisionMap* map_ptr = nullptr;
    zelda3::CustomCollisionMap loaded_from_rom;
    if (materialized != nullptr &&
        (materialized->IsLoaded() || materialized->custom_collision_dirty() ||
         materialized->has_custom_collision())) {
      map_ptr = &materialized->custom_collision();
    } else if (can_read_rom) {
      auto map_or = zelda3::LoadCustomCollisionMap(rom, rid);
      if (!map_or.ok()) {
        return absl::Status(
            map_or.status().code(),
            absl::StrFormat("Room 0x%03X: %s", rid, map_or.status().message()));
      }
      if (map_or->has_data) {
        loaded_from_rom = std::move(*map_or);
        map_ptr = &loaded_from_rom;
      }
    }
    if (map_ptr == nullptr || !map_ptr->has_data) {
      continue;
    }

    // Export explicit overrides, including floor (00), without inherited cells.
    bool any = false;
    zelda3::CustomCollisionRoomEntry entry;
    entry.room_id = rid;
    for (size_t off = 0; off < map_ptr->tiles.size(); ++off) {
      if (!map_ptr->HasOverride(off)) {
        continue;
      }
      any = true;
      entry.tiles.push_back(zelda3::CustomCollisionTileEntry{
          static_cast<uint16_t>(off), map_ptr->tiles[off]});
    }
    if (!any) {
      continue;
    }
    out.push_back(std::move(entry));
  }
  return out;
}

void CustomCollisionPanel::ApplyRoomEntry(
    const zelda3::CustomCollisionRoomEntry& entry, zelda3::Room* room) {
  if (room == nullptr) {
    return;
  }
  room->custom_collision() = {};
  for (const auto& t : entry.tiles) {
    if (t.offset >= room->custom_collision().tiles.size()) {
      continue;
    }
    room->custom_collision().SetOverride(t.offset, t.value);
  }
  room->MarkCustomCollisionDirty();
}

}  // namespace yaze::editor
