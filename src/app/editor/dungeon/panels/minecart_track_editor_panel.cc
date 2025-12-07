#include "minecart_track_editor_panel.h"
#include "imgui/imgui.h"
#include "absl/strings/str_split.h"
#include "absl/strings/str_format.h"
#include <fstream>
#include <sstream>
#include <filesystem>
#include <iostream>
#include <regex>

namespace yaze::editor {

void MinecartTrackEditorPanel::SetProjectRoot(const std::string& root) {
  if (project_root_ != root) {
    project_root_ = root;
    loaded_ = false; // Trigger reload on next draw
  }
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
  if (ImGui::Button("Save Tracks")) {
    SaveTracks();
  }
  
  if (!status_message_.empty()) {
    ImGui::SameLine();
    ImGui::TextColored(show_success_ ? ImVec4(0, 1, 0, 1) : ImVec4(1, 0, 0, 1), "%s", status_message_.c_str());
    if (show_success_) {
       // Simple timer logic handled by UI redraw or just fade out manually
       // For now, just show it.
    }
  }

  ImGui::Separator();

  if (ImGui::BeginTable("TracksTable", 5, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_Resizable)) {
    ImGui::TableSetupColumn("ID", ImGuiTableColumnFlags_WidthFixed, 30.0f);
    ImGui::TableSetupColumn("Room ID (Hex)", ImGuiTableColumnFlags_WidthFixed, 80.0f);
    ImGui::TableSetupColumn("Start X (Hex)", ImGuiTableColumnFlags_WidthFixed, 80.0f);
    ImGui::TableSetupColumn("Start Y (Hex)", ImGuiTableColumnFlags_WidthFixed, 80.0f);
    ImGui::TableSetupColumn("Description", ImGuiTableColumnFlags_WidthStretch);
    ImGui::TableHeadersRow();

    for (auto& track : tracks_) {
      ImGui::PushID(track.id);
      ImGui::TableNextRow();
      
      ImGui::TableSetColumnIndex(0);
      ImGui::Text("%02X", track.id);

      ImGui::TableSetColumnIndex(1);
      int room = track.room_id;
      if (ImGui::InputInt("##Room", &room, 0, 0, ImGuiInputTextFlags_CharsHexadecimal)) {
        track.room_id = room & 0xFFFF;
      }

      ImGui::TableSetColumnIndex(2);
      int sx = track.start_x;
      if (ImGui::InputInt("##X", &sx, 0, 0, ImGuiInputTextFlags_CharsHexadecimal)) {
        track.start_x = sx & 0xFFFF;
      }

      ImGui::TableSetColumnIndex(3);
      int sy = track.start_y;
      if (ImGui::InputInt("##Y", &sy, 0, 0, ImGuiInputTextFlags_CharsHexadecimal)) {
        track.start_y = sy & 0xFFFF;
      }
      
      ImGui::TableSetColumnIndex(4);
      // Optional description or notes
      ImGui::TextDisabled("Track %d", track.id);

      ImGui::PopID();
    }
    ImGui::EndTable();
  }
}

void MinecartTrackEditorPanel::LoadTracks() {
  std::filesystem::path path = std::filesystem::path(project_root_) / "Sprites/Objects/data/minecart_tracks.asm";
  
  if (!std::filesystem::exists(path)) {
    status_message_ = "File not found: " + path.string();
    show_success_ = false;
    loaded_ = true; // Prevent retry loop
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

bool MinecartTrackEditorPanel::ParseSection(const std::string& content, const std::string& label, std::vector<int>& out_values) {
    size_t pos = content.find(label);
    if (pos == std::string::npos) return false;

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
        if (trimmed_start != std::string::npos && line[trimmed_start] == '.') break;
        
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
  std::filesystem::path path = std::filesystem::path(project_root_) / "Sprites/Objects/data/minecart_tracks.asm";
  
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

  file << "  ; This is which room each track should start in if it hasn't already\n";
  file << "  ; been given a track.\n";
  file << FormatSection(".TrackStartingRooms", rooms);
  file << "\n";
  
  file << "  ; This is where within the room each track should start in if it hasn't\n";
  file << "  ; already been given a position. This is necessary to allow for more\n";
  file << "  ; than one stopping point to be in one room.\n";
  file << FormatSection(".TrackStartingX", xs);
  file << "\n";
  
  file << FormatSection(".TrackStartingY", ys);
  
  status_message_ = "Tracks saved successfully!";
  show_success_ = true;
}

std::string MinecartTrackEditorPanel::FormatSection(const std::string& label, const std::vector<int>& values) {
    std::stringstream ss;
    ss << "  " << label << "\n";
    
    for (size_t i = 0; i < values.size(); i += 8) {
        ss << "  dw ";
        for (size_t j = 0; j < 8 && i + j < values.size(); ++j) {
            if (j > 0) ss << ", ";
            ss << absl::StrFormat("$%04X", values[i + j]);
        }
        ss << "\n";
    }
    return ss.str();
}

} // namespace yaze::editor
