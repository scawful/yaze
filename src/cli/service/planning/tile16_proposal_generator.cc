#include "cli/service/planning/tile16_proposal_generator.h"

#include <fstream>
#include <sstream>

#include "absl/strings/match.h"
#include "absl/strings/numbers.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/str_split.h"
#include "nlohmann/json.hpp"
#include "util/macro.h"
#include "zelda3/overworld/overworld.h"

namespace yaze {
namespace cli {

std::string Tile16Change::ToString() const {
  std::ostringstream oss;
  oss << "Map " << map_id << " @ (" << x << "," << y << "): "
      << "0x" << std::hex << old_tile << " → 0x" << new_tile;
  return oss.str();
}

std::string Tile16Proposal::ToJson() const {
  std::ostringstream json;
  json << "{\n";
  json << "  \"id\": \"" << id << "\",\n";
  json << "  \"prompt\": \"" << prompt << "\",\n";
  json << "  \"ai_service\": \"" << ai_service << "\",\n";
  json << "  \"reasoning\": \"" << reasoning << "\",\n";
  json << "  \"status\": ";

  switch (status) {
    case Status::PENDING:
      json << "\"pending\"";
      break;
    case Status::ACCEPTED:
      json << "\"accepted\"";
      break;
    case Status::REJECTED:
      json << "\"rejected\"";
      break;
    case Status::APPLIED:
      json << "\"applied\"";
      break;
  }
  json << ",\n";

  json << "  \"changes\": [\n";
  for (size_t i = 0; i < changes.size(); ++i) {
    const auto& change = changes[i];
    json << "    {\n";
    json << "      \"map_id\": " << change.map_id << ",\n";
    json << "      \"x\": " << change.x << ",\n";
    json << "      \"y\": " << change.y << ",\n";
    json << "      \"old_tile\": \"0x" << std::hex << change.old_tile
         << "\",\n";
    json << "      \"new_tile\": \"0x" << std::hex << change.new_tile << "\"\n";
    json << "    }";
    if (i < changes.size() - 1)
      json << ",";
    json << "\n";
  }
  json << "  ]\n";
  json << "}\n";

  return json.str();
}

namespace {

absl::StatusOr<uint16_t> ParseTileValue(const nlohmann::json& json,
                                        const char* field) {
  if (!json.contains(field)) {
    return absl::InvalidArgumentError(
        absl::StrCat("Missing field '", field, "' in proposal change"));
  }

  if (json[field].is_number_integer()) {
    int value = json[field].get<int>();
    if (value < 0 || value > 0xFFFF) {
      return absl::InvalidArgumentError(
          absl::StrCat("Tile value for '", field, "' out of range: ", value));
    }
    return static_cast<uint16_t>(value);
  }

  if (json[field].is_string()) {
    std::string value = json[field].get<std::string>();
    if (value.empty()) {
      return absl::InvalidArgumentError(
          absl::StrCat("Tile value for '", field, "' is empty"));
    }

    // Support hex strings in 0xFFFF format or plain decimal strings
    if (absl::StartsWith(value, "0x") || absl::StartsWith(value, "0X")) {
      if (value.size() <= 2) {
        return absl::InvalidArgumentError(
            absl::StrCat("Invalid hex tile value for '", field,
                         "': ", json[field].get<std::string>()));
      }
      value = value.substr(2);
      unsigned int parsed = 0;
      if (!absl::SimpleHexAtoi(value, &parsed) || parsed > 0xFFFF) {
        return absl::InvalidArgumentError(
            absl::StrCat("Invalid hex tile value for '", field,
                         "': ", json[field].get<std::string>()));
      }
      return static_cast<uint16_t>(parsed);
    }

    unsigned int parsed = 0;
    if (!absl::SimpleAtoi(value, &parsed) || parsed > 0xFFFF) {
      return absl::InvalidArgumentError(
          absl::StrCat("Invalid tile value for '", field,
                       "': ", json[field].get<std::string>()));
    }
    return static_cast<uint16_t>(parsed);
  }

  return absl::InvalidArgumentError(
      absl::StrCat("Unsupported JSON type for tile field '", field, "'"));
}

Tile16Proposal::Status ParseStatus(absl::string_view status_text) {
  if (absl::StartsWith(status_text, "accept")) {
    return Tile16Proposal::Status::ACCEPTED;
  }
  if (absl::StartsWith(status_text, "reject")) {
    return Tile16Proposal::Status::REJECTED;
  }
  if (absl::StartsWith(status_text, "apply")) {
    return Tile16Proposal::Status::APPLIED;
  }
  return Tile16Proposal::Status::PENDING;
}

}  // namespace

absl::StatusOr<Tile16Proposal> Tile16Proposal::FromJson(
    const std::string& json_text) {
  nlohmann::json json;
  try {
    json = nlohmann::json::parse(json_text);
  } catch (const nlohmann::json::parse_error& error) {
    return absl::InvalidArgumentError(
        absl::StrCat("Failed to parse proposal JSON: ", error.what()));
  }

  Tile16Proposal proposal;

  if (!json.contains("id") || !json["id"].is_string()) {
    return absl::InvalidArgumentError(
        "Proposal JSON must include string field 'id'");
  }
  proposal.id = json["id"].get<std::string>();

  if (!json.contains("prompt") || !json["prompt"].is_string()) {
    return absl::InvalidArgumentError(
        "Proposal JSON must include string field 'prompt'");
  }
  proposal.prompt = json["prompt"].get<std::string>();

  if (json.contains("ai_service") && json["ai_service"].is_string()) {
    proposal.ai_service = json["ai_service"].get<std::string>();
  }

  if (json.contains("reasoning") && json["reasoning"].is_string()) {
    proposal.reasoning = json["reasoning"].get<std::string>();
  }

  if (json.contains("status")) {
    if (!json["status"].is_string()) {
      return absl::InvalidArgumentError(
          "Proposal 'status' must be a string value");
    }
    proposal.status = ParseStatus(json["status"].get<std::string>());
  } else {
    proposal.status = Status::PENDING;
  }

  if (json.contains("changes")) {
    if (!json["changes"].is_array()) {
      return absl::InvalidArgumentError(
          "Proposal 'changes' field must be an array");
    }

    for (const auto& change_json : json["changes"]) {
      if (!change_json.is_object()) {
        return absl::InvalidArgumentError(
            "Each change entry must be a JSON object");
      }

      Tile16Change change;
      if (!change_json.contains("map_id") ||
          !change_json["map_id"].is_number_integer()) {
        return absl::InvalidArgumentError(
            "Tile change missing integer field 'map_id'");
      }
      change.map_id = change_json["map_id"].get<int>();

      if (!change_json.contains("x") || !change_json["x"].is_number_integer()) {
        return absl::InvalidArgumentError(
            "Tile change missing integer field 'x'");
      }
      change.x = change_json["x"].get<int>();

      if (!change_json.contains("y") || !change_json["y"].is_number_integer()) {
        return absl::InvalidArgumentError(
            "Tile change missing integer field 'y'");
      }
      change.y = change_json["y"].get<int>();

      ASSIGN_OR_RETURN(change.old_tile,
                       ParseTileValue(change_json, "old_tile"));
      ASSIGN_OR_RETURN(change.new_tile,
                       ParseTileValue(change_json, "new_tile"));

      proposal.changes.push_back(change);
    }
  }

  if (proposal.changes.empty()) {
    return absl::InvalidArgumentError(
        "Proposal JSON did not include any tile16 changes");
  }

  proposal.created_at = std::chrono::system_clock::now();
  if (json.contains("created_at_ms") && json["created_at_ms"].is_number()) {
    auto millis = json["created_at_ms"].get<int64_t>();
    proposal.created_at = std::chrono::system_clock::time_point(
        std::chrono::milliseconds(millis));
  }

  return proposal;
}

std::string Tile16ProposalGenerator::GenerateProposalId() const {
  // Generate a simple timestamp-based ID
  auto now = std::chrono::system_clock::now();
  auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                now.time_since_epoch())
                .count();

  std::ostringstream oss;
  oss << "proposal_" << ms;
  return oss.str();
}

absl::StatusOr<Tile16Change> Tile16ProposalGenerator::ParseSetTileCommand(
    const std::string& command, Rom* rom) {
  // Expected format: "overworld set-tile --map 0 --x 10 --y 20 --tile 0x02E"
  std::vector<std::string> parts = absl::StrSplit(command, ' ');

  if (parts.size() < 10) {
    return absl::InvalidArgumentError(
        absl::StrCat("Invalid command format: ", command));
  }

  if (parts[0] != "overworld" || parts[1] != "set-tile") {
    return absl::InvalidArgumentError(
        absl::StrCat("Not a set-tile command: ", command));
  }

  Tile16Change change;

  // Parse arguments
  for (size_t i = 2; i < parts.size(); i += 2) {
    if (i + 1 >= parts.size())
      break;

    const std::string& flag = parts[i];
    const std::string& value = parts[i + 1];

    if (flag == "--map") {
      change.map_id = std::stoi(value);
    } else if (flag == "--x") {
      change.x = std::stoi(value);
    } else if (flag == "--y") {
      change.y = std::stoi(value);
    } else if (flag == "--tile") {
      // Parse as hex (both 0x prefix and plain hex)
      change.new_tile = static_cast<uint16_t>(std::stoi(value, nullptr, 16));
    }
  }

  // Load the ROM to get the old tile value
  if (rom && rom->is_loaded()) {
    zelda3::Overworld overworld(rom);
    auto status = overworld.Load(rom);
    if (!status.ok()) {
      return status;
    }

    // Set the correct world based on map_id
    if (change.map_id < 0x40) {
      overworld.set_current_world(0);  // Light World
    } else if (change.map_id < 0x80) {
      overworld.set_current_world(1);  // Dark World
    } else {
      overworld.set_current_world(2);  // Special World
    }

    change.old_tile = overworld.GetTile(change.x, change.y);
  } else {
    change.old_tile = 0x0000;  // Unknown
  }

  return change;
}

absl::StatusOr<std::vector<Tile16Change>>
Tile16ProposalGenerator::ParseSetAreaCommand(const std::string& command,
                                             Rom* rom) {
  // Expected format: "overworld set-area --map 0 --x 10 --y 20 --width 5
  // --height 3 --tile 0x02E"
  std::vector<std::string> parts = absl::StrSplit(command, ' ');

  if (parts.size() < 12) {
    return absl::InvalidArgumentError(
        absl::StrCat("Invalid set-area command format: ", command));
  }

  if (parts[0] != "overworld" || parts[1] != "set-area") {
    return absl::InvalidArgumentError(
        absl::StrCat("Not a set-area command: ", command));
  }

  int map_id = 0, x = 0, y = 0, width = 1, height = 1;
  uint16_t new_tile = 0;

  // Parse arguments
  for (size_t i = 2; i < parts.size(); i += 2) {
    if (i + 1 >= parts.size())
      break;

    const std::string& flag = parts[i];
    const std::string& value = parts[i + 1];

    if (flag == "--map") {
      map_id = std::stoi(value);
    } else if (flag == "--x") {
      x = std::stoi(value);
    } else if (flag == "--y") {
      y = std::stoi(value);
    } else if (flag == "--width") {
      width = std::stoi(value);
    } else if (flag == "--height") {
      height = std::stoi(value);
    } else if (flag == "--tile") {
      new_tile = static_cast<uint16_t>(std::stoi(value, nullptr, 16));
    }
  }

  // Load the ROM to get the old tile values
  std::vector<Tile16Change> changes;
  if (rom && rom->is_loaded()) {
    zelda3::Overworld overworld(rom);
    auto status = overworld.Load(rom);
    if (!status.ok()) {
      return status;
    }

    // Set the correct world based on map_id
    if (map_id < 0x40) {
      overworld.set_current_world(0);  // Light World
    } else if (map_id < 0x80) {
      overworld.set_current_world(1);  // Dark World
    } else {
      overworld.set_current_world(2);  // Special World
    }

    // Generate changes for each tile in the area
    for (int dy = 0; dy < height; ++dy) {
      for (int dx = 0; dx < width; ++dx) {
        Tile16Change change;
        change.map_id = map_id;
        change.x = x + dx;
        change.y = y + dy;
        change.new_tile = new_tile;
        change.old_tile = overworld.GetTile(change.x, change.y);
        changes.push_back(change);
      }
    }
  } else {
    // If ROM not loaded, just create changes with unknown old values
    for (int dy = 0; dy < height; ++dy) {
      for (int dx = 0; dx < width; ++dx) {
        Tile16Change change;
        change.map_id = map_id;
        change.x = x + dx;
        change.y = y + dy;
        change.new_tile = new_tile;
        change.old_tile = 0x0000;  // Unknown
        changes.push_back(change);
      }
    }
  }

  return changes;
}

absl::StatusOr<std::vector<Tile16Change>>
Tile16ProposalGenerator::ParseReplaceTileCommand(const std::string& command,
                                                 Rom* rom) {
  // Expected format: "overworld replace-tile --map 0 --old-tile 0x02E
  // --new-tile 0x030" Optional bounds: --x-min 0 --y-min 0 --x-max 31 --y-max
  // 31
  std::vector<std::string> parts = absl::StrSplit(command, ' ');

  if (parts.size() < 8) {
    return absl::InvalidArgumentError(
        absl::StrCat("Invalid replace-tile command format: ", command));
  }

  if (parts[0] != "overworld" || parts[1] != "replace-tile") {
    return absl::InvalidArgumentError(
        absl::StrCat("Not a replace-tile command: ", command));
  }

  int map_id = 0;
  uint16_t old_tile = 0, new_tile = 0;
  int x_min = 0, y_min = 0, x_max = 31, y_max = 31;

  // Parse arguments
  for (size_t i = 2; i < parts.size(); i += 2) {
    if (i + 1 >= parts.size())
      break;

    const std::string& flag = parts[i];
    const std::string& value = parts[i + 1];

    if (flag == "--map") {
      map_id = std::stoi(value);
    } else if (flag == "--old-tile") {
      old_tile = static_cast<uint16_t>(std::stoi(value, nullptr, 16));
    } else if (flag == "--new-tile") {
      new_tile = static_cast<uint16_t>(std::stoi(value, nullptr, 16));
    } else if (flag == "--x-min") {
      x_min = std::stoi(value);
    } else if (flag == "--y-min") {
      y_min = std::stoi(value);
    } else if (flag == "--x-max") {
      x_max = std::stoi(value);
    } else if (flag == "--y-max") {
      y_max = std::stoi(value);
    }
  }

  if (!rom || !rom->is_loaded()) {
    return absl::FailedPreconditionError(
        "ROM must be loaded to scan for tiles to replace");
  }

  zelda3::Overworld overworld(rom);
  auto status = overworld.Load(rom);
  if (!status.ok()) {
    return status;
  }

  // Set the correct world based on map_id
  if (map_id < 0x40) {
    overworld.set_current_world(0);  // Light World
  } else if (map_id < 0x80) {
    overworld.set_current_world(1);  // Dark World
  } else {
    overworld.set_current_world(2);  // Special World
  }

  // Scan the specified area for tiles to replace
  std::vector<Tile16Change> changes;
  for (int y = y_min; y <= y_max; ++y) {
    for (int x = x_min; x <= x_max; ++x) {
      uint16_t current_tile = overworld.GetTile(x, y);
      if (current_tile == old_tile) {
        Tile16Change change;
        change.map_id = map_id;
        change.x = x;
        change.y = y;
        change.old_tile = old_tile;
        change.new_tile = new_tile;
        changes.push_back(change);
      }
    }
  }

  if (changes.empty()) {
    std::ostringstream oss;
    oss << "0x" << std::hex << old_tile;
    return absl::NotFoundError(absl::StrCat("No tiles matching ", oss.str(),
                                            " found in specified area"));
  }

  return changes;
}

absl::StatusOr<Tile16Proposal> Tile16ProposalGenerator::GenerateFromCommands(
    const std::string& prompt, const std::vector<std::string>& commands,
    const std::string& ai_service, Rom* rom) {
  Tile16Proposal proposal;
  proposal.id = GenerateProposalId();
  proposal.prompt = prompt;
  proposal.ai_service = ai_service;
  proposal.created_at = std::chrono::system_clock::now();
  proposal.status = Tile16Proposal::Status::PENDING;

  // Parse each command
  for (const auto& command : commands) {
    // Skip empty commands or comments
    if (command.empty() || command[0] == '#') {
      continue;
    }

    // Check for different command types
    if (absl::StrContains(command, "overworld set-tile")) {
      auto change_or = ParseSetTileCommand(command, rom);
      if (change_or.ok()) {
        proposal.changes.push_back(change_or.value());
      } else {
        return change_or.status();
      }
    } else if (absl::StrContains(command, "overworld set-area")) {
      auto changes_or = ParseSetAreaCommand(command, rom);
      if (changes_or.ok()) {
        proposal.changes.insert(proposal.changes.end(),
                                changes_or.value().begin(),
                                changes_or.value().end());
      } else {
        return changes_or.status();
      }
    } else if (absl::StrContains(command, "overworld replace-tile")) {
      auto changes_or = ParseReplaceTileCommand(command, rom);
      if (changes_or.ok()) {
        proposal.changes.insert(proposal.changes.end(),
                                changes_or.value().begin(),
                                changes_or.value().end());
      } else {
        return changes_or.status();
      }
    }
  }

  if (proposal.changes.empty()) {
    return absl::InvalidArgumentError(
        "No valid tile16 changes found in commands");
  }

  proposal.reasoning = absl::StrCat("Generated ", proposal.changes.size(),
                                    " tile16 changes from prompt");

  return proposal;
}

absl::Status Tile16ProposalGenerator::ApplyProposal(
    const Tile16Proposal& proposal, Rom* rom) {
  if (!rom || !rom->is_loaded()) {
    return absl::FailedPreconditionError("ROM not loaded");
  }

  zelda3::Overworld overworld(rom);
  auto status = overworld.Load(rom);
  if (!status.ok()) {
    return status;
  }

  // Apply each change
  for (const auto& change : proposal.changes) {
    // Set the correct world
    if (change.map_id < 0x40) {
      overworld.set_current_world(0);  // Light World
    } else if (change.map_id < 0x80) {
      overworld.set_current_world(1);  // Dark World
    } else {
      overworld.set_current_world(2);  // Special World
    }

    // Apply the tile change
    overworld.SetTile(change.x, change.y, change.new_tile);
  }

  // Note: We don't save to disk here - that's the caller's responsibility
  // This allows for sandbox testing before committing

  return absl::OkStatus();
}

absl::StatusOr<gfx::Bitmap> Tile16ProposalGenerator::GenerateDiff(
    const Tile16Proposal& proposal, Rom* before_rom, Rom* after_rom) {
  if (!before_rom || !before_rom->is_loaded()) {
    return absl::FailedPreconditionError("Before ROM not loaded");
  }

  if (!after_rom || !after_rom->is_loaded()) {
    return absl::FailedPreconditionError("After ROM not loaded");
  }

  if (proposal.changes.empty()) {
    return absl::InvalidArgumentError("No changes to visualize");
  }

  // Find the bounding box of all changes
  int min_x = INT_MAX, min_y = INT_MAX;
  int max_x = INT_MIN, max_y = INT_MIN;
  int map_id = proposal.changes[0].map_id;

  for (const auto& change : proposal.changes) {
    if (change.x < min_x)
      min_x = change.x;
    if (change.y < min_y)
      min_y = change.y;
    if (change.x > max_x)
      max_x = change.x;
    if (change.y > max_y)
      max_y = change.y;
  }

  // Add some padding around the changes
  int padding = 2;
  min_x = std::max(0, min_x - padding);
  min_y = std::max(0, min_y - padding);
  max_x = std::min(31, max_x + padding);
  max_y = std::min(31, max_y + padding);

  int width = (max_x - min_x + 1) * 16;
  int height = (max_y - min_y + 1) * 16;

  // Create a side-by-side diff bitmap (before on left, after on right)
  int diff_width = width * 2 + 8;  // 8 pixels separator
  int diff_height = height;

  std::vector<uint8_t> diff_data(diff_width * diff_height, 0x00);
  gfx::Bitmap diff_bitmap(diff_width, diff_height, 8, diff_data);

  // Load overworld data from both ROMs
  zelda3::Overworld before_overworld(before_rom);
  zelda3::Overworld after_overworld(after_rom);

  auto before_status = before_overworld.Load(before_rom);
  if (!before_status.ok()) {
    return before_status;
  }

  auto after_status = after_overworld.Load(after_rom);
  if (!after_status.ok()) {
    return after_status;
  }

  // Set the correct world for both overworlds
  int world = 0;
  if (map_id < 0x40) {
    world = 0;  // Light World
  } else if (map_id < 0x80) {
    world = 1;  // Dark World
  } else {
    world = 2;  // Special World
  }

  before_overworld.set_current_world(world);
  after_overworld.set_current_world(world);

  // For now, create a simple colored diff representation
  // Red = changed tiles, Green = unchanged tiles
  // This is a placeholder until full tile rendering is implemented

  gfx::SnesColor red_color(31, 0, 0);          // Red for changed
  gfx::SnesColor green_color(0, 31, 0);        // Green for unchanged
  gfx::SnesColor separator_color(15, 15, 15);  // Gray separator

  for (int y = min_y; y <= max_y; ++y) {
    for (int x = min_x; x <= max_x; ++x) {
      uint16_t before_tile = before_overworld.GetTile(x, y);
      uint16_t after_tile = after_overworld.GetTile(x, y);

      bool is_changed = (before_tile != after_tile);
      gfx::SnesColor color = is_changed ? red_color : green_color;

      // Draw "before" tile on left side
      int pixel_x = (x - min_x) * 16;
      int pixel_y = (y - min_y) * 16;
      for (int py = 0; py < 16; ++py) {
        for (int px = 0; px < 16; ++px) {
          diff_bitmap.SetPixel(pixel_x + px, pixel_y + py, color);
        }
      }

      // Draw "after" tile on right side
      int right_offset = width + 8;
      for (int py = 0; py < 16; ++py) {
        for (int px = 0; px < 16; ++px) {
          diff_bitmap.SetPixel(right_offset + pixel_x + px, pixel_y + py,
                               color);
        }
      }
    }
  }

  // Draw separator line
  for (int y = 0; y < diff_height; ++y) {
    for (int x = 0; x < 8; ++x) {
      diff_bitmap.SetPixel(width + x, y, separator_color);
    }
  }

  return diff_bitmap;
}

absl::Status Tile16ProposalGenerator::SaveProposal(
    const Tile16Proposal& proposal, const std::string& path) {
  std::ofstream file(path);
  if (!file.is_open()) {
    return absl::InvalidArgumentError(
        absl::StrCat("Failed to open file for writing: ", path));
  }

  file << proposal.ToJson();
  file.close();

  return absl::OkStatus();
}

absl::StatusOr<Tile16Proposal> Tile16ProposalGenerator::LoadProposal(
    const std::string& path) {
  std::ifstream file(path);
  if (!file.is_open()) {
    return absl::InvalidArgumentError(
        absl::StrCat("Failed to open file for reading: ", path));
  }

  std::stringstream buffer;
  buffer << file.rdbuf();
  file.close();

  return Tile16Proposal::FromJson(buffer.str());
}

}  // namespace cli
}  // namespace yaze
