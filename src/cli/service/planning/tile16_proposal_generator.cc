#include "cli/service/planning/tile16_proposal_generator.h"

#include <fstream>
#include <sstream>

#include "absl/strings/match.h"
#include "absl/strings/str_split.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/numbers.h"
#include "app/zelda3/overworld/overworld.h"
#include "nlohmann/json.hpp"
#include "util/macro.h"

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
    case Status::PENDING: json << "\"pending\""; break;
    case Status::ACCEPTED: json << "\"accepted\""; break;
    case Status::REJECTED: json << "\"rejected\""; break;
    case Status::APPLIED: json << "\"applied\""; break;
  }
  json << ",\n";
  
  json << "  \"changes\": [\n";
  for (size_t i = 0; i < changes.size(); ++i) {
    const auto& change = changes[i];
    json << "    {\n";
    json << "      \"map_id\": " << change.map_id << ",\n";
    json << "      \"x\": " << change.x << ",\n";
    json << "      \"y\": " << change.y << ",\n";
    json << "      \"old_tile\": \"0x" << std::hex << change.old_tile << "\",\n";
    json << "      \"new_tile\": \"0x" << std::hex << change.new_tile << "\"\n";
    json << "    }";
    if (i < changes.size() - 1) json << ",";
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
          absl::StrCat("Tile value for '", field,
                       "' out of range: ", value));
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

      if (!change_json.contains("x") ||
          !change_json["x"].is_number_integer()) {
        return absl::InvalidArgumentError(
            "Tile change missing integer field 'x'");
      }
      change.x = change_json["x"].get<int>();

      if (!change_json.contains("y") ||
          !change_json["y"].is_number_integer()) {
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
      now.time_since_epoch()).count();
  
  std::ostringstream oss;
  oss << "proposal_" << ms;
  return oss.str();
}

absl::StatusOr<Tile16Change> Tile16ProposalGenerator::ParseSetTileCommand(
    const std::string& command,
    Rom* rom) {
  
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
    if (i + 1 >= parts.size()) break;
    
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

absl::StatusOr<Tile16Proposal> Tile16ProposalGenerator::GenerateFromCommands(
    const std::string& prompt,
    const std::vector<std::string>& commands,
    const std::string& ai_service,
    Rom* rom) {
  
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
    
    // Check if it's a set-tile command
    if (absl::StrContains(command, "overworld set-tile")) {
      auto change_or = ParseSetTileCommand(command, rom);
      if (change_or.ok()) {
        proposal.changes.push_back(change_or.value());
      } else {
        return change_or.status();
      }
    }
    // TODO: Add support for other command types (set-area, replace-tile, etc.)
  }
  
  if (proposal.changes.empty()) {
    return absl::InvalidArgumentError(
        "No valid tile16 changes found in commands");
  }
  
  proposal.reasoning = absl::StrCat(
      "Generated ", proposal.changes.size(), " tile16 changes from prompt");
  
  return proposal;
}

absl::Status Tile16ProposalGenerator::ApplyProposal(
    const Tile16Proposal& proposal,
    Rom* rom) {
  
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
    const Tile16Proposal& /* proposal */,
    Rom* /* before_rom */,
    Rom* /* after_rom */) {
  
  // TODO: Implement visual diff generation
  // This would:
  // 1. Load overworld from both ROMs
  // 2. Render the affected regions
  // 3. Create side-by-side or overlay comparison
  // 4. Highlight changed tiles
  
  return absl::UnimplementedError("Visual diff generation not yet implemented");
}

absl::Status Tile16ProposalGenerator::SaveProposal(
    const Tile16Proposal& proposal,
    const std::string& path) {
  
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

