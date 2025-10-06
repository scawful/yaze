#include "cli/handlers/agent/commands.h"

#include <iostream>

#include "absl/status/status.h"
#include "absl/strings/str_format.h"
#include "absl/strings/match.h"
#include "absl/strings/numbers.h"
#include "app/rom.h"

namespace yaze {
namespace cli {
namespace agent {

absl::Status HandleMusicListCommand(
    const std::vector<std::string>& arg_vec, Rom* rom_context) {
  if (!rom_context || !rom_context->is_loaded()) {
    return absl::FailedPreconditionError("ROM not loaded");
  }

  std::string format = "json";
  
  for (size_t i = 0; i < arg_vec.size(); ++i) {
    const std::string& token = arg_vec[i];
    if (token == "--format") {
      if (i + 1 < arg_vec.size()) {
        format = arg_vec[++i];
      }
    } else if (absl::StartsWith(token, "--format=")) {
      format = token.substr(9);
    }
  }

  // ALTTP music tracks (simplified list)
  struct MusicTrack {
    int id;
    std::string name;
    std::string category;
  };

  std::vector<MusicTrack> tracks = {
    {0x02, "Opening Theme", "Title"},
    {0x03, "Light World", "Overworld"},
    {0x05, "Dark World", "Overworld"},
    {0x07, "Hyrule Castle", "Dungeon"},
    {0x09, "Cave", "Indoor"},
    {0x0A, "Boss Battle", "Combat"},
    {0x0D, "Sanctuary", "Indoor"},
    {0x10, "Village", "Town"},
    {0x11, "Kakariko Village", "Town"},
    {0x12, "Death Mountain", "Outdoor"},
    {0x13, "Lost Woods", "Outdoor"},
    {0x16, "Ganon's Theme", "Boss"},
    {0x17, "Triforce Room", "Special"},
    {0x18, "Zelda's Rescue", "Special"},
  };

  if (format == "json") {
    std::cout << "{\n";
    std::cout << "  \"music_tracks\": [\n";
    for (size_t i = 0; i < tracks.size(); ++i) {
      const auto& track = tracks[i];
      std::cout << "    {\n";
      std::cout << "      \"id\": \"0x" << std::hex << std::uppercase 
                << track.id << std::dec << "\",\n";
      std::cout << "      \"decimal_id\": " << track.id << ",\n";
      std::cout << "      \"name\": \"" << track.name << "\",\n";
      std::cout << "      \"category\": \"" << track.category << "\"\n";
      std::cout << "    }";
      if (i < tracks.size() - 1) {
        std::cout << ",";
      }
      std::cout << "\n";
    }
    std::cout << "  ],\n";
    std::cout << "  \"total\": " << tracks.size() << ",\n";
    std::cout << "  \"rom\": \"" << rom_context->filename() << "\"\n";
    std::cout << "}\n";
  } else {
    std::cout << "Music Tracks:\n";
    std::cout << "----------------------------------------\n";
    for (const auto& track : tracks) {
      std::cout << absl::StrFormat("0x%02X (%2d) | %-20s [%s]\n", 
                                   track.id, track.id, track.name, track.category);
    }
    std::cout << "----------------------------------------\n";
    std::cout << "Total: " << tracks.size() << " tracks\n";
  }

  return absl::OkStatus();
}

absl::Status HandleMusicInfoCommand(
    const std::vector<std::string>& arg_vec, Rom* rom_context) {
  if (!rom_context || !rom_context->is_loaded()) {
    return absl::FailedPreconditionError("ROM not loaded");
  }

  int track_id = -1;
  std::string format = "json";
  
  for (size_t i = 0; i < arg_vec.size(); ++i) {
    const std::string& token = arg_vec[i];
    if (token == "--id" || token == "--track") {
      if (i + 1 < arg_vec.size()) {
        std::string id_str = arg_vec[++i];
        if (absl::StartsWith(id_str, "0x") || absl::StartsWith(id_str, "0X")) {
          track_id = std::stoi(id_str, nullptr, 16);
        } else {
          absl::SimpleAtoi(id_str, &track_id);
        }
      }
    } else if (absl::StartsWith(token, "--id=") || absl::StartsWith(token, "--track=")) {
      std::string id_str = token.substr(token.find('=') + 1);
      if (absl::StartsWith(id_str, "0x") || absl::StartsWith(id_str, "0X")) {
        track_id = std::stoi(id_str, nullptr, 16);
      } else {
        absl::SimpleAtoi(id_str, &track_id);
      }
    } else if (token == "--format") {
      if (i + 1 < arg_vec.size()) {
        format = arg_vec[++i];
      }
    } else if (absl::StartsWith(token, "--format=")) {
      format = token.substr(9);
    }
  }

  if (track_id < 0) {
    return absl::InvalidArgumentError(
        "Usage: music-info --id <track_id> [--format json|text]");
  }

  // Simplified track info
  std::string track_name = absl::StrFormat("Music Track %d", track_id);
  std::string category = "Unknown";
  int num_channels = 4;
  std::string tempo = "Moderate";

  if (track_id == 0x03) {
    track_name = "Light World";
    category = "Overworld";
    tempo = "Upbeat";
  } else if (track_id == 0x05) {
    track_name = "Dark World";
    category = "Overworld";
    tempo = "Dark/Foreboding";
  }

  if (format == "json") {
    std::cout << "{\n";
    std::cout << "  \"track_id\": \"0x" << std::hex << std::uppercase 
              << track_id << std::dec << "\",\n";
    std::cout << "  \"decimal_id\": " << track_id << ",\n";
    std::cout << "  \"name\": \"" << track_name << "\",\n";
    std::cout << "  \"category\": \"" << category << "\",\n";
    std::cout << "  \"channels\": " << num_channels << ",\n";
    std::cout << "  \"tempo\": \"" << tempo << "\",\n";
    std::cout << "  \"rom\": \"" << rom_context->filename() << "\"\n";
    std::cout << "}\n";
  } else {
    std::cout << "Track ID: 0x" << std::hex << std::uppercase 
              << track_id << std::dec << " (" << track_id << ")\n";
    std::cout << "Name: " << track_name << "\n";
    std::cout << "Category: " << category << "\n";
    std::cout << "Channels: " << num_channels << "\n";
    std::cout << "Tempo: " << tempo << "\n";
  }

  return absl::OkStatus();
}

absl::Status HandleMusicTracksCommand(
    const std::vector<std::string>& arg_vec, Rom* rom_context) {
  if (!rom_context || !rom_context->is_loaded()) {
    return absl::FailedPreconditionError("ROM not loaded");
  }

  std::string category;
  std::string format = "json";
  
  for (size_t i = 0; i < arg_vec.size(); ++i) {
    const std::string& token = arg_vec[i];
    if (token == "--category") {
      if (i + 1 < arg_vec.size()) {
        category = arg_vec[++i];
      }
    } else if (absl::StartsWith(token, "--category=")) {
      category = token.substr(11);
    } else if (token == "--format") {
      if (i + 1 < arg_vec.size()) {
        format = arg_vec[++i];
      }
    } else if (absl::StartsWith(token, "--format=")) {
      format = token.substr(9);
    }
  }

  std::cout << "{\n";
  std::cout << "  \"category\": \"" << (category.empty() ? "all" : category) << "\",\n";
  std::cout << "  \"message\": \"Track channel data would be returned here\",\n";
  std::cout << "  \"note\": \"Full SPC700 data parsing not yet implemented\"\n";
  std::cout << "}\n";

  return absl::OkStatus();
}

}  // namespace agent
}  // namespace cli
}  // namespace yaze

