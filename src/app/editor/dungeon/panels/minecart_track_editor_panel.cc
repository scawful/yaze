#include "minecart_track_editor_panel.h"
#include <fstream>
#include <iomanip>
#include <sstream>
#include "absl/strings/str_format.h"
#include "absl/strings/str_split.h"
#include "imgui/imgui.h"

#include <filesystem>
#include <iostream>
#include <regex>
#include "app/gui/core/icons.h"
#include "app/gui/core/input.h"
#include "util/log.h"

namespace yaze::editor {

void MinecartTrackEditorPanel::SetProjectRoot(const std::string& root) {
  if (project_root_ != root) {
    project_root_ = root;
    loaded_ = false;  // Trigger reload on next draw
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

void MinecartTrackEditorPanel::Draw(bool* p_open) {
  if (project_root_.empty()) {
    ImGui::TextColored(ImVec4(1, 0, 0, 1), "Project root not set.");
    return;
  }

  if (!loaded_) {
    LoadTracks();
  }

  ImGui::Text("Minecart Track Editor");
  if (ImGui::Button(ICON_MD_SAVE " Save Tracks")) {
    SaveTracks();
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

  ImGui::Separator();

  // Coordinate format help
  ImGui::TextDisabled(
      "Camera coordinates use $1XXX format (base $1000 + room offset + local "
      "position)");
  ImGui::TextDisabled(
      "Hover over dungeon canvas to see coordinates, or click 'Pick' button.");
  ImGui::Separator();

  if (ImGui::BeginTable("TracksTable", 6,
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
    ImGui::TableHeadersRow();

    for (auto& track : tracks_) {
      ImGui::TableNextRow();

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
    }

    ImGui::EndTable();
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
    status_message_ = "";
    show_success_ = true;
  }
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
