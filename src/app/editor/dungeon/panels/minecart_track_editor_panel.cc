#include "minecart_track_editor_panel.h"
#include <fstream>
#include <iomanip>
#include <sstream>
#include "absl/status/status.h"
#include "absl/strings/str_format.h"
#include "absl/strings/str_split.h"
#include "imgui/imgui.h"
#include "imgui/misc/cpp/imgui_stdlib.h"

#include <filesystem>
#include <iostream>
#include <regex>
#include "app/gui/core/icons.h"
#include "app/gui/core/input.h"
#include "util/log.h"
#include "zelda3/dungeon/custom_collision.h"
#include "zelda3/dungeon/track_collision_generator.h"

namespace yaze::editor {

namespace {
constexpr int kTrackSlotCount = 32;
constexpr int kDefaultTrackRoom = 0x89;
constexpr int kDefaultTrackX = 0x1300;
constexpr int kDefaultTrackY = 0x1100;

std::string FormatHexList(const std::vector<uint16_t>& values) {
  std::string out;
  for (size_t i = 0; i < values.size(); ++i) {
    if (i > 0) {
      out += ", ";
    }
    out += absl::StrFormat("0x%X", values[i]);
  }
  return out;
}

std::vector<uint16_t> ParseHexList(const std::string& input) {
  std::vector<uint16_t> out;
  for (absl::string_view token_view : absl::StrSplit(
           input, absl::ByAnyChar(", \n\t"), absl::SkipEmpty())) {
    if (token_view.empty()) {
      continue;
    }
    std::string token(token_view);
    if (token[0] == '$') {
      token = "0x" + token.substr(1);
    }
    int base = 10;
    if (token.rfind("0x", 0) == 0 || token.rfind("0X", 0) == 0) {
      token = token.substr(2);
      base = 16;
    }
    try {
      auto value = std::stoul(token, nullptr, base);
      if (value <= 0xFFFF) {
        out.push_back(static_cast<uint16_t>(value));
      }
    } catch (...) {
      continue;
    }
  }
  return out;
}
}  // namespace

void MinecartTrackEditorPanel::SetProjectRoot(const std::string& root) {
  if (project_root_ != root) {
    project_root_ = root;
    loaded_ = false;  // Trigger reload on next draw
    audit_dirty_ = true;
  }
}

void MinecartTrackEditorPanel::InitializeOverlayInputs() {
  if (overlay_inputs_initialized_ || !project_) {
    return;
  }
  overlay_track_tiles_input_ =
      FormatHexList(project_->dungeon_overlay.track_tiles);
  overlay_track_stop_tiles_input_ =
      FormatHexList(project_->dungeon_overlay.track_stop_tiles);
  overlay_track_switch_tiles_input_ =
      FormatHexList(project_->dungeon_overlay.track_switch_tiles);
  overlay_track_object_ids_input_ =
      FormatHexList(project_->dungeon_overlay.track_object_ids);
  overlay_minecart_sprite_ids_input_ =
      FormatHexList(project_->dungeon_overlay.minecart_sprite_ids);
  overlay_inputs_initialized_ = true;
}

bool MinecartTrackEditorPanel::UpdateOverlayList(
    const char* label, std::string& input, std::vector<uint16_t>& target) {
  bool changed = ImGui::InputText(label, &input);
  if (changed && ImGui::IsItemDeactivatedAfterEdit()) {
    target = ParseHexList(input);
    audit_dirty_ = true;
    return true;
  }
  return false;
}

void MinecartTrackEditorPanel::DrawOverlaySettings() {
  if (!project_) {
    return;
  }

  InitializeOverlayInputs();

  if (!ImGui::CollapsingHeader(ICON_MD_TUNE " Overlay Config",
                               ImGuiTreeNodeFlags_DefaultOpen)) {
    return;
  }

  ImGui::TextDisabled("Empty list = defaults. Use hex (0xB0) or decimal.");
  ImGui::TextDisabled(
      "Defaults: Track 0xB0-0xBE | Stop 0xB7-0xBA | Switch 0xD0-0xD3 | "
      "Track Obj 0x31 | Cart Sprite 0xA3");

  bool changed = false;
  changed |= UpdateOverlayList("Track Tiles",
                               overlay_track_tiles_input_,
                               project_->dungeon_overlay.track_tiles);
  changed |= UpdateOverlayList("Stop Tiles",
                               overlay_track_stop_tiles_input_,
                               project_->dungeon_overlay.track_stop_tiles);
  changed |= UpdateOverlayList("Switch Tiles",
                               overlay_track_switch_tiles_input_,
                               project_->dungeon_overlay.track_switch_tiles);
  changed |= UpdateOverlayList("Track Object IDs",
                               overlay_track_object_ids_input_,
                               project_->dungeon_overlay.track_object_ids);
  changed |= UpdateOverlayList("Minecart Sprite IDs",
                               overlay_minecart_sprite_ids_input_,
                               project_->dungeon_overlay.minecart_sprite_ids);

  if (ImGui::Button("Reset Overlay Defaults")) {
    project_->dungeon_overlay.track_tiles.clear();
    project_->dungeon_overlay.track_stop_tiles.clear();
    project_->dungeon_overlay.track_switch_tiles.clear();
    project_->dungeon_overlay.track_object_ids.clear();
    project_->dungeon_overlay.minecart_sprite_ids.clear();
    overlay_track_tiles_input_.clear();
    overlay_track_stop_tiles_input_.clear();
    overlay_track_switch_tiles_input_.clear();
    overlay_track_object_ids_input_.clear();
    overlay_minecart_sprite_ids_input_.clear();
    audit_dirty_ = true;
    changed = true;
  }

  if (changed) {
    ImGui::TextDisabled("Remember to save the project to persist changes.");
  }
}

const std::vector<MinecartTrack>& MinecartTrackEditorPanel::GetTracks() {
  if (!loaded_) {
    LoadTracks();
  }
  return tracks_;
}

void MinecartTrackEditorPanel::SetPickedCoordinates(int room_id,
                                                    uint16_t camera_x,
                                                    uint16_t camera_y) {
  if (picking_mode_ && picking_track_index_ >= 0 &&
      picking_track_index_ < static_cast<int>(tracks_.size())) {
    tracks_[picking_track_index_].room_id = room_id;
    tracks_[picking_track_index_].start_x = camera_x;
    tracks_[picking_track_index_].start_y = camera_y;

    last_picked_x_ = camera_x;
    last_picked_y_ = camera_y;
    has_picked_coords_ = true;
    audit_dirty_ = true;

    status_message_ =
        absl::StrFormat("Track %d: Set to Room $%04X, Pos ($%04X, $%04X)",
                        picking_track_index_, room_id, camera_x, camera_y);
    show_success_ = true;
  }

  // Exit picking mode
  picking_mode_ = false;
  picking_track_index_ = -1;
}

void MinecartTrackEditorPanel::StartCoordinatePicking(int track_index) {
  picking_mode_ = true;
  picking_track_index_ = track_index;
  status_message_ = absl::StrFormat(
      "Click on the dungeon canvas to set Track %d position", track_index);
  show_success_ = false;
}

void MinecartTrackEditorPanel::CancelCoordinatePicking() {
  picking_mode_ = false;
  picking_track_index_ = -1;
  status_message_ = "";
}

bool MinecartTrackEditorPanel::IsDefaultTrack(
    const MinecartTrack& track) const {
  return track.room_id == kDefaultTrackRoom &&
         track.start_x == kDefaultTrackX && track.start_y == kDefaultTrackY;
}

void MinecartTrackEditorPanel::RebuildAuditCache() {
  room_audit_.clear();
  track_usage_rooms_.clear();
  track_subtype_used_.assign(kTrackSlotCount, false);

  if (!rooms_) {
    audit_dirty_ = false;
    return;
  }

  std::array<bool, 256> track_tiles{};
  std::array<bool, 256> stop_tiles{};
  std::array<bool, 256> switch_tiles{};
  auto apply_list = [](std::array<bool, 256>& dest,
                       const std::vector<uint16_t>& values) {
    dest.fill(false);
    for (uint16_t value : values) {
      if (value < dest.size()) {
        dest[value] = true;
      }
    }
  };

  if (project_ && !project_->dungeon_overlay.track_tiles.empty()) {
    apply_list(track_tiles, project_->dungeon_overlay.track_tiles);
  } else {
    std::vector<uint16_t> default_track_tiles;
    for (uint16_t tile = 0xB0; tile <= 0xBE; ++tile) {
      default_track_tiles.push_back(tile);
    }
    apply_list(track_tiles, default_track_tiles);
  }

  if (project_ && !project_->dungeon_overlay.track_stop_tiles.empty()) {
    apply_list(stop_tiles, project_->dungeon_overlay.track_stop_tiles);
  } else {
    apply_list(stop_tiles, {0xB7, 0xB8, 0xB9, 0xBA});
  }

  if (project_ && !project_->dungeon_overlay.track_switch_tiles.empty()) {
    apply_list(switch_tiles, project_->dungeon_overlay.track_switch_tiles);
  } else {
    apply_list(switch_tiles, {0xD0, 0xD1, 0xD2, 0xD3});
  }

  std::vector<uint16_t> track_object_ids = {0x31};
  std::vector<uint16_t> minecart_sprite_ids = {0xA3};
  if (project_) {
    if (!project_->dungeon_overlay.track_object_ids.empty()) {
      track_object_ids = project_->dungeon_overlay.track_object_ids;
    }
    if (!project_->dungeon_overlay.minecart_sprite_ids.empty()) {
      minecart_sprite_ids = project_->dungeon_overlay.minecart_sprite_ids;
    }
  }

  std::unordered_map<int, bool> track_object_id_map;
  for (uint16_t id : track_object_ids) {
    track_object_id_map[static_cast<int>(id)] = true;
  }
  std::unordered_map<int, bool> minecart_sprite_id_map;
  for (uint16_t id : minecart_sprite_ids) {
    minecart_sprite_id_map[static_cast<int>(id)] = true;
  }

  for (int room_id = 0; room_id < static_cast<int>(rooms_->size()); ++room_id) {
    auto& room = (*rooms_)[room_id];
    RoomTrackAudit audit;

    if (room.GetTileObjects().empty()) {
      room.LoadObjects();
    }
    if (room.GetSprites().empty()) {
      room.LoadSprites();
    }

    std::array<bool, kTrackSlotCount> seen_subtype{};

    for (const auto& obj : room.GetTileObjects()) {
      if (!track_object_id_map[static_cast<int>(obj.id_)]) {
        continue;
      }
      int subtype = obj.size_ & 0x1F;
      if (subtype >= 0 && subtype < kTrackSlotCount) {
        if (!seen_subtype[static_cast<size_t>(subtype)]) {
          seen_subtype[static_cast<size_t>(subtype)] = true;
          track_subtype_used_[static_cast<size_t>(subtype)] = true;
          track_usage_rooms_[subtype].push_back(room_id);
          audit.track_subtypes.push_back(subtype);
        }
      }
    }

    std::unordered_map<int, bool> stop_positions;
    auto map_or = zelda3::LoadCustomCollisionMap(room.rom(), room_id);
    if (map_or.ok() && map_or.value().has_data) {
      const auto& map = map_or.value().tiles;
      for (int y = 0; y < 64; ++y) {
        for (int x = 0; x < 64; ++x) {
          uint8_t tile = map[static_cast<size_t>(y * 64 + x)];
          if (track_tiles[tile] || stop_tiles[tile] || switch_tiles[tile]) {
            audit.has_track_collision = true;
          }
          if (stop_tiles[tile]) {
            audit.has_stop_tiles = true;
            stop_positions[y * 64 + x] = true;
          }
        }
      }
    }

    if (audit.has_track_collision) {
      for (const auto& sprite : room.GetSprites()) {
        if (!minecart_sprite_id_map[static_cast<int>(sprite.id())]) {
          continue;
        }
        audit.has_minecart_sprite = true;
        int tile_x = sprite.x() * 2;
        int tile_y = sprite.y() * 2;
        if (tile_x >= 0 && tile_x < 64 && tile_y >= 0 && tile_y < 64) {
          int idx = tile_y * 64 + tile_x;
          if (stop_positions[idx]) {
            audit.has_minecart_on_stop = true;
          }
        }
      }
    }

    if (audit.has_track_collision || !audit.track_subtypes.empty() ||
        audit.has_minecart_sprite) {
      room_audit_[room_id] = audit;
    }
  }

  audit_dirty_ = false;
}

void MinecartTrackEditorPanel::Draw(bool* p_open) {
  if (project_root_.empty()) {
    ImGui::TextColored(ImVec4(1, 0, 0, 1), "Project root not set.");
    return;
  }

  if (!loaded_) {
    LoadTracks();
  }

  if (audit_dirty_) {
    RebuildAuditCache();
  }

  ImGui::Text("Minecart Track Editor");
  if (ImGui::Button(ICON_MD_SAVE " Save Tracks")) {
    SaveTracks();
  }
  ImGui::SameLine();
  const bool can_save_project = project_ && project_->project_opened();
  if (!can_save_project) {
    ImGui::BeginDisabled();
  }
  if (ImGui::Button(ICON_MD_SAVE " Save Project")) {
    auto status = project_->Save();
    if (status.ok()) {
      status_message_ = "Project saved.";
      show_success_ = true;
    } else {
      status_message_ =
          absl::StrFormat("Project save failed: %s", status.message());
      show_success_ = false;
    }
  }
  if (!can_save_project) {
    ImGui::EndDisabled();
  }

  // Show picking mode indicator
  if (picking_mode_) {
    ImGui::SameLine();
    if (ImGui::Button(ICON_MD_CANCEL " Cancel Pick")) {
      CancelCoordinatePicking();
    }
    ImGui::SameLine();
    ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.2f, 1.0f),
                       ICON_MD_MY_LOCATION " Picking for Track %d...",
                       picking_track_index_);
  }

  if (!status_message_.empty() && !picking_mode_) {
    ImGui::SameLine();
    ImGui::TextColored(show_success_ ? ImVec4(0, 1, 0, 1) : ImVec4(1, 0, 0, 1),
                       "%s", status_message_.c_str());
  }

  DrawOverlaySettings();
  ImGui::Separator();

  // Coordinate format help
  ImGui::TextDisabled(
      "Camera coordinates use $1XXX format (base $1000 + room offset + local "
      "position)");
  ImGui::TextDisabled(
      "Hover over dungeon canvas to see coordinates, or click 'Pick' button.");
  ImGui::Separator();

  if (ImGui::BeginTable("TracksTable", 7,
                        ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg |
                            ImGuiTableFlags_Resizable)) {
    ImGui::TableSetupColumn("ID", ImGuiTableColumnFlags_WidthFixed, 30.0f);
    ImGui::TableSetupColumn("Room ID", ImGuiTableColumnFlags_WidthFixed, 80.0f);
    ImGui::TableSetupColumn("Camera X", ImGuiTableColumnFlags_WidthFixed,
                            80.0f);
    ImGui::TableSetupColumn("Camera Y", ImGuiTableColumnFlags_WidthFixed,
                            80.0f);
    ImGui::TableSetupColumn("Pick", ImGuiTableColumnFlags_WidthFixed, 50.0f);
    ImGui::TableSetupColumn("Go", ImGuiTableColumnFlags_WidthFixed, 40.0f);
    ImGui::TableSetupColumn("Status", ImGuiTableColumnFlags_WidthFixed, 60.0f);
    ImGui::TableHeadersRow();

    for (auto& track : tracks_) {
      ImGui::TableNextRow();

      const bool is_default = IsDefaultTrack(track);
      const bool used_in_rooms =
          track.id >= 0 &&
          track.id < static_cast<int>(track_subtype_used_.size()) &&
          track_subtype_used_[track.id];
      const bool missing_start = used_in_rooms && is_default;

      if (missing_start) {
        ImGui::TableSetBgColor(ImGuiTableBgTarget_RowBg0,
                               IM_COL32(120, 40, 40, 120));
      } else if (is_default) {
        ImGui::TableSetBgColor(ImGuiTableBgTarget_RowBg0,
                               IM_COL32(60, 60, 60, 80));
      }

      // Highlight the row being picked
      if (picking_mode_ && track.id == picking_track_index_) {
        ImGui::TableSetBgColor(ImGuiTableBgTarget_RowBg0,
                               IM_COL32(80, 80, 0, 100));
      }

      ImGui::TableNextColumn();
      ImGui::Text("%d", track.id);

      ImGui::TableNextColumn();
      uint16_t room_id = static_cast<uint16_t>(track.room_id);
      if (yaze::gui::InputHexWordCustom(
              absl::StrFormat("##Room%d", track.id).c_str(), &room_id, 60.0f)) {
        track.room_id = room_id;
      }

      ImGui::TableNextColumn();
      uint16_t start_x = static_cast<uint16_t>(track.start_x);
      if (yaze::gui::InputHexWordCustom(
              absl::StrFormat("##StartX%d", track.id).c_str(), &start_x,
              60.0f)) {
        track.start_x = start_x;
      }

      ImGui::TableNextColumn();
      uint16_t start_y = static_cast<uint16_t>(track.start_y);
      if (yaze::gui::InputHexWordCustom(
              absl::StrFormat("##StartY%d", track.id).c_str(), &start_y,
              60.0f)) {
        track.start_y = start_y;
      }

      // Pick button to select coordinates from canvas
      ImGui::TableNextColumn();
      ImGui::PushID(track.id);
      bool is_picking_this = picking_mode_ && picking_track_index_ == track.id;
      if (is_picking_this) {
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.8f, 0.6f, 0.0f, 1.0f));
      }
      if (ImGui::SmallButton(ICON_MD_MY_LOCATION)) {
        if (is_picking_this) {
          CancelCoordinatePicking();
        } else {
          StartCoordinatePicking(track.id);
        }
      }
      if (is_picking_this) {
        ImGui::PopStyleColor();
      }
      if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip(is_picking_this ? "Cancel picking"
                                          : "Pick coordinates from canvas");
      }
      ImGui::PopID();

      // Go to room button
      ImGui::TableNextColumn();
      ImGui::PushID(track.id + 1000);
      if (ImGui::SmallButton(ICON_MD_ARROW_FORWARD)) {
        if (room_navigation_callback_) {
          room_navigation_callback_(track.room_id);
        }
      }
      if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("Navigate to room $%04X", track.room_id);
      }
      ImGui::PopID();

      // Status column
      ImGui::TableNextColumn();
      if (missing_start) {
        ImGui::TextColored(ImVec4(1.0f, 0.6f, 0.1f, 1.0f),
                           ICON_MD_WARNING_AMBER);
      } else if (is_default) {
        ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), ICON_MD_INFO);
      } else if (used_in_rooms) {
        ImGui::TextColored(ImVec4(0.4f, 0.9f, 0.4f, 1.0f),
                           ICON_MD_CHECK_CIRCLE);
      } else {
        ImGui::Text("-");
      }

      if (ImGui::IsItemHovered()) {
        ImGui::BeginTooltip();
        if (missing_start) {
          ImGui::TextColored(ImVec4(1.0f, 0.6f, 0.1f, 1.0f),
                             "Used in rooms but still default");
        } else if (is_default) {
          ImGui::Text("Default filler slot");
        } else if (used_in_rooms) {
          ImGui::Text("Used in rooms");
        } else {
          ImGui::Text("No usage detected");
        }

        auto rooms_it = track_usage_rooms_.find(track.id);
        if (rooms_it != track_usage_rooms_.end()) {
          ImGui::Separator();
          ImGui::Text("Rooms:");
          for (int room_id : rooms_it->second) {
            ImGui::BulletText("0x%03X", room_id);
          }
        }
        ImGui::EndTooltip();
      }
    }

    ImGui::EndTable();
  }

  // Summary + room audit
  int default_count = 0;
  int used_count = 0;
  int missing_start_count = 0;
  for (const auto& track : tracks_) {
    bool is_default = IsDefaultTrack(track);
    bool used_in_rooms =
        track.id >= 0 &&
        track.id < static_cast<int>(track_subtype_used_.size()) &&
        track_subtype_used_[track.id];
    if (is_default) {
      default_count++;
    }
    if (used_in_rooms) {
      used_count++;
    }
    if (used_in_rooms && is_default) {
      missing_start_count++;
    }
  }

  ImGui::Separator();
  ImGui::Text("Usage Summary: used %d/%d, default %d, missing starts %d",
              used_count, kTrackSlotCount, default_count, missing_start_count);

  if (!room_audit_.empty()) {
    ImGui::Separator();
    ImGui::Text("Rooms with track objects:");

    // "Generate All" button: batch-generate collision for all rooms that have
    // rail objects but no collision data yet.
    if (rom_ && rooms_) {
      // Count rooms that need generation
      int rooms_needing_collision = 0;
      for (const auto& [rid, audit] : room_audit_) {
        if (!audit.track_subtypes.empty() && !audit.has_track_collision) {
          rooms_needing_collision++;
        }
      }

      if (rooms_needing_collision > 0) {
        if (ImGui::Button(
                absl::StrFormat(ICON_MD_AUTO_FIX_HIGH
                                " Generate All (%d rooms)",
                                rooms_needing_collision).c_str())) {
          int generated_rooms = 0;
          int total_tiles = 0;
          bool had_error = false;

          for (auto& [rid, audit] : room_audit_) {
            if (audit.track_subtypes.empty() || audit.has_track_collision) {
              continue;
            }

            auto& target_room = (*rooms_)[rid];
            zelda3::GeneratorOptions opts;
            auto gen_result =
                zelda3::GenerateTrackCollision(&target_room, opts);
            if (!gen_result.ok()) {
              status_message_ = absl::StrFormat(
                  "Generate failed for room 0x%03X: %s", rid,
                  gen_result.status().message());
              show_success_ = false;
              had_error = true;
              break;
            }

            auto write_status = zelda3::WriteTrackCollision(
                rom_, rid, gen_result->collision_map);
            if (!write_status.ok()) {
              status_message_ = absl::StrFormat(
                  "Write failed for room 0x%03X: %s", rid,
                  write_status.message());
              show_success_ = false;
              had_error = true;
              break;
            }

            generated_rooms++;
            total_tiles += gen_result->tiles_generated;
          }

          if (!had_error) {
            status_message_ = absl::StrFormat(
                "Generated collision for %d rooms (%d tiles total)",
                generated_rooms, total_tiles);
            show_success_ = true;
          }
          audit_dirty_ = true;
        }
        if (ImGui::IsItemHovered()) {
          ImGui::SetTooltip(
              "Generate collision for all %d rooms with rail objects "
              "but no collision data",
              rooms_needing_collision);
        }
      }
    }

    ImGui::BeginChild("##TrackAuditRooms", ImVec2(0, 160), true);
    for (const auto& [room_id, audit] : room_audit_) {
      if (audit.track_subtypes.empty() && !audit.has_track_collision) {
        continue;
      }

      // Status icon
      if (!audit.has_track_collision) {
        ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.1f, 1.0f),
                           ICON_MD_ERROR " Room 0x%03X (no collision)",
                           room_id);
      } else if (!audit.has_minecart_on_stop) {
        ImGui::TextColored(ImVec4(1.0f, 0.6f, 0.1f, 1.0f),
                           ICON_MD_WARNING_AMBER " Room 0x%03X (no cart on stop)",
                           room_id);
      } else {
        ImGui::TextColored(ImVec4(0.4f, 0.9f, 0.4f, 1.0f),
                           ICON_MD_CHECK_CIRCLE " Room 0x%03X", room_id);
      }

      ImGui::SameLine();
      ImGui::PushID(room_id);
      if (ImGui::SmallButton(ICON_MD_ARROW_FORWARD)) {
        if (room_navigation_callback_) {
          room_navigation_callback_(room_id);
        }
      }
      if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("Navigate to room 0x%03X", room_id);
      }

      // Generate Collision button (only if rom available and no collision yet)
      if (rom_ && rooms_ && !audit.has_track_collision) {
        ImGui::SameLine();
        if (ImGui::SmallButton(
                absl::StrFormat(ICON_MD_AUTO_FIX_HIGH " Generate##%d",
                                room_id).c_str())) {
          auto& target_room = (*rooms_)[room_id];
          zelda3::GeneratorOptions opts;
          auto gen_result =
              zelda3::GenerateTrackCollision(&target_room, opts);
          if (gen_result.ok()) {
            auto write_status = zelda3::WriteTrackCollision(
                rom_, room_id, gen_result->collision_map);
            if (write_status.ok()) {
              status_message_ = absl::StrFormat(
                  "Room 0x%03X: Generated %d tiles (%d stops, %d corners)",
                  room_id, gen_result->tiles_generated,
                  gen_result->stop_count, gen_result->corner_count);
              show_success_ = true;
              audit_dirty_ = true;
            } else {
              status_message_ = absl::StrFormat(
                  "Write failed: %s", write_status.message());
              show_success_ = false;
            }
          } else {
            status_message_ = absl::StrFormat(
                "Generate failed: %s", gen_result.status().message());
            show_success_ = false;
          }
        }
        if (ImGui::IsItemHovered()) {
          ImGui::SetTooltip(
              "Auto-generate collision tiles from rail objects in this room");
        }
      }

      ImGui::PopID();
    }
    ImGui::EndChild();
  }
}

void MinecartTrackEditorPanel::LoadTracks() {
  std::filesystem::path path = std::filesystem::path(project_root_) /
                               "Sprites/Objects/data/minecart_tracks.asm";

  if (!std::filesystem::exists(path)) {
    status_message_ = "File not found: " + path.string();
    show_success_ = false;
    loaded_ = true;  // Prevent retry loop
    tracks_.clear();
    return;
  }

  std::ifstream file(path);
  std::stringstream buffer;
  buffer << file.rdbuf();
  std::string content = buffer.str();

  std::vector<int> rooms;
  std::vector<int> xs;
  std::vector<int> ys;

  if (!ParseSection(content, ".TrackStartingRooms", rooms) ||
      !ParseSection(content, ".TrackStartingX", xs) ||
      !ParseSection(content, ".TrackStartingY", ys)) {
    status_message_ = "Error parsing file format.";
    show_success_ = false;
  } else {
    tracks_.clear();
    size_t count = std::min({rooms.size(), xs.size(), ys.size()});
    for (size_t i = 0; i < count; ++i) {
      tracks_.push_back({(int)i, rooms[i], xs[i], ys[i]});
    }
    while (tracks_.size() < kTrackSlotCount) {
      tracks_.push_back({static_cast<int>(tracks_.size()), kDefaultTrackRoom,
                         kDefaultTrackX, kDefaultTrackY});
    }
    status_message_ = "";
    show_success_ = true;
  }
  audit_dirty_ = true;
  loaded_ = true;
}

bool MinecartTrackEditorPanel::ParseSection(const std::string& content,
                                            const std::string& label,
                                            std::vector<int>& out_values) {
  size_t pos = content.find(label);
  if (pos == std::string::npos)
    return false;

  // Start searching after the label
  size_t start = pos + label.length();

  // Find lines starting with 'dw'
  std::regex dw_regex(R"(dw\s+((?:\$[0-9A-Fa-f]{4}(?:,\s*)?)+))");

  // Create a substring from start to end or next label (simplified: just search until next dot label or end)
  // Actually, searching line by line is safer.
  std::stringstream ss(content.substr(start));
  std::string line;
  while (std::getline(ss, line)) {
    // Stop if we hit another label
    size_t trimmed_start = line.find_first_not_of(" \t");
    if (trimmed_start != std::string::npos && line[trimmed_start] == '.')
      break;

    std::smatch match;
    if (std::regex_search(line, match, dw_regex)) {
      std::string values_str = match[1];
      std::stringstream val_ss(values_str);
      std::string segment;
      while (std::getline(val_ss, segment, ',')) {
        // Trim
        segment.erase(0, segment.find_first_not_of(" \t$"));
        // Parse hex
        try {
          out_values.push_back(std::stoi(segment, nullptr, 16));
        } catch (...) {}
      }
    }
  }
  return true;
}

void MinecartTrackEditorPanel::SaveTracks() {
  std::filesystem::path path = std::filesystem::path(project_root_) /
                               "Sprites/Objects/data/minecart_tracks.asm";

  std::ofstream file(path);
  if (!file.is_open()) {
    status_message_ = "Failed to open file for writing.";
    show_success_ = false;
    return;
  }

  std::vector<int> rooms, xs, ys;
  for (const auto& t : tracks_) {
    rooms.push_back(t.room_id);
    xs.push_back(t.start_x);
    ys.push_back(t.start_y);
  }

  file << "  ; This is which room each track should start in if it hasn't "
          "already\n";
  file << "  ; been given a track.\n";
  file << FormatSection(".TrackStartingRooms", rooms);
  file << "\n";

  file << "  ; This is where within the room each track should start in if it "
          "hasn't\n";
  file << "  ; already been given a position. This is necessary to allow for "
          "more\n";
  file << "  ; than one stopping point to be in one room.\n";
  file << FormatSection(".TrackStartingX", xs);
  file << "\n";

  file << FormatSection(".TrackStartingY", ys);

  status_message_ = "Tracks saved successfully!";
  show_success_ = true;
}

std::string MinecartTrackEditorPanel::FormatSection(
    const std::string& label, const std::vector<int>& values) {
  std::stringstream ss;
  ss << "  " << label << "\n";

  for (size_t i = 0; i < values.size(); i += 8) {
    ss << "  dw ";
    for (size_t j = 0; j < 8 && i + j < values.size(); ++j) {
      if (j > 0)
        ss << ", ";
      ss << absl::StrFormat("$%04X", values[i + j]);
    }
    ss << "\n";
  }
  return ss.str();
}

}  // namespace yaze::editor
