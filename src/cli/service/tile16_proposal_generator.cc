#include "cli/service/tile16_proposal_generator.h"

#include <sstream>
#include <fstream>

#include "absl/strings/match.h"
#include "absl/strings/str_split.h"
#include "absl/strings/str_cat.h"
#include "app/zelda3/overworld/overworld.h"

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

absl::StatusOr<Tile16Proposal> Tile16Proposal::FromJson(const std::string& /* json */) {
  // TODO: Implement JSON parsing using nlohmann/json when available
  return absl::UnimplementedError("JSON parsing not yet implemented");
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

