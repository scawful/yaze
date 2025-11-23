#include "cli/service/agent/tools/memory_inspector_tool.h"

#include <algorithm>
#include <cctype>
#include <iomanip>
#include <sstream>

#include "absl/strings/str_cat.h"
#include "absl/strings/str_format.h"
#include "absl/strings/str_split.h"

namespace yaze {
namespace cli {
namespace agent {
namespace tools {

namespace {

// Game mode descriptions
const std::map<uint8_t, std::string> kGameModes = {
    {0x00, "Reset/Logo"},
    {0x01, "Title Screen"},
    {0x02, "File Select"},
    {0x03, "Copy/Erase"},
    {0x04, "Name Entry"},
    {0x05, "Loading Area"},
    {0x06, "Pre-Dungeon"},
    {0x07, "Dungeon"},
    {0x08, "Pre-Overworld"},
    {0x09, "Overworld"},
    {0x0A, "Pre-Overworld (Special)"},
    {0x0B, "Overworld (Special)"},
    {0x0C, "Blank Screen"},
    {0x0D, "Text (Dialogue)"},
    {0x0E, "Closing Spotlight"},
    {0x0F, "Opening Spotlight"},
    {0x10, "Spotlight (Other)"},
    {0x11, "Dungeon Spotlight"},
    {0x12, "Dungeon Endgame"},
    {0x13, "Ganon Emerging"},
    {0x14, "Ganon Phase 1"},
    {0x15, "Ganon Phase 2"},
    {0x16, "Triforce Room"},
    {0x17, "Ending Sequence"},
    {0x18, "Map Screen"},
    {0x19, "Inventory"},
    {0x1A, "Red Screen"},
    {0x1B, "Attract Mode"},
};

// Sprite type names (common ones)
const std::map<uint8_t, std::string> kSpriteTypes = {
    {0x00, "Raven"},        {0x01, "Vulture"},
    {0x02, "Flying Tile"},  {0x03, "Empty"},
    {0x04, "Pull Switch"},  {0x05, "Octorock"},
    {0x06, "Wall Master"},  {0x07, "Moldorm (tail)"},
    {0x08, "Octorock (4)"},  {0x09, "Chicken"},
    {0x0A, "Octorok (Stone)"}, {0x0B, "Buzzblob"},
    {0x0C, "Snap Dragon"},  {0x0D, "Octoballoon"},
    {0x0E, "Octoballoon Hatchling"}, {0x0F, "Hinox"},
    {0x10, "Moblin"},       {0x11, "Mini Helmasaur"},
    {0x12, "Gargoyle's Domain (Fireball)"}, {0x13, "Antifairy"},
    {0x14, "Sahasrahla/Elder"}, {0x15, "Bush Hoarder"},
};

// Link direction names
const std::map<uint8_t, std::string> kLinkDirections = {
    {0x00, "North"},
    {0x02, "South"},
    {0x04, "West"},
    {0x06, "East"},
};

}  // namespace

// ============================================================================
// MemoryInspectorBase Implementation
// ============================================================================

std::string MemoryInspectorBase::DescribeAddress(uint32_t address) const {
  // Check known regions
  if (address == ALTTPMemoryMap::kGameMode)
    return "Game Mode";
  if (address == ALTTPMemoryMap::kSubmodule)
    return "Submodule";
  if (address == ALTTPMemoryMap::kFrameCounter)
    return "Frame Counter";
  if (address == ALTTPMemoryMap::kLinkXLow ||
      address == ALTTPMemoryMap::kLinkXHigh)
    return "Link X Position";
  if (address == ALTTPMemoryMap::kLinkYLow ||
      address == ALTTPMemoryMap::kLinkYHigh)
    return "Link Y Position";
  if (address == ALTTPMemoryMap::kLinkState)
    return "Link State";
  if (address == ALTTPMemoryMap::kLinkDirection)
    return "Link Direction";
  if (address == ALTTPMemoryMap::kOverworldArea)
    return "Overworld Area";
  if (address == ALTTPMemoryMap::kDungeonRoom)
    return "Dungeon Room";
  if (address == ALTTPMemoryMap::kPlayerHealth)
    return "Player Health";
  if (address == ALTTPMemoryMap::kPlayerMaxHealth)
    return "Player Max Health";
  if (address == ALTTPMemoryMap::kPlayerRupees)
    return "Player Rupees";

  // Check ranges
  if (ALTTPMemoryMap::IsSpriteTable(address)) {
    int offset = address - 0x7E0D00;
    int sprite_index = offset % 16;
    return absl::StrFormat("Sprite Table (Sprite %d)", sprite_index);
  }
  if (ALTTPMemoryMap::IsSaveData(address)) {
    return "Save Data / Inventory";
  }
  if (address >= ALTTPMemoryMap::kOAMBuffer &&
      address <= ALTTPMemoryMap::kOAMBufferEnd) {
    return "OAM Buffer";
  }
  if (ALTTPMemoryMap::IsWRAM(address)) {
    return "WRAM";
  }

  return "Unknown";
}

std::string MemoryInspectorBase::IdentifyDataType(uint32_t address) const {
  if (ALTTPMemoryMap::IsSpriteTable(address))
    return "sprite_table";
  if (ALTTPMemoryMap::IsSaveData(address))
    return "save_data";
  if (address >= ALTTPMemoryMap::kOAMBuffer &&
      address <= ALTTPMemoryMap::kOAMBufferEnd)
    return "oam_buffer";
  if (address == ALTTPMemoryMap::kGameMode ||
      address == ALTTPMemoryMap::kSubmodule)
    return "game_state";
  if (address >= ALTTPMemoryMap::kLinkXLow &&
      address <= ALTTPMemoryMap::kLinkDirection)
    return "player_state";
  return "generic";
}

std::vector<MemoryRegionInfo> MemoryInspectorBase::GetKnownRegions() const {
  return {
      {"game_mode", "Current game mode/state", ALTTPMemoryMap::kGameMode,
       ALTTPMemoryMap::kGameMode, "byte"},
      {"submodule", "Game submodule state", ALTTPMemoryMap::kSubmodule,
       ALTTPMemoryMap::kSubmodule, "byte"},
      {"frame_counter", "Frame counter", ALTTPMemoryMap::kFrameCounter,
       ALTTPMemoryMap::kFrameCounter + 1, "word"},
      {"link_position", "Link's X/Y position", ALTTPMemoryMap::kLinkYLow,
       ALTTPMemoryMap::kLinkXHigh, "struct"},
      {"link_state", "Link's animation state", ALTTPMemoryMap::kLinkState,
       ALTTPMemoryMap::kLinkState, "byte"},
      {"link_direction", "Link's facing direction",
       ALTTPMemoryMap::kLinkDirection, ALTTPMemoryMap::kLinkDirection, "byte"},
      {"sprite_y_low", "Sprite Y positions (low byte)",
       ALTTPMemoryMap::kSpriteYLow, ALTTPMemoryMap::kSpriteYLow + 15, "array"},
      {"sprite_x_low", "Sprite X positions (low byte)",
       ALTTPMemoryMap::kSpriteXLow, ALTTPMemoryMap::kSpriteXLow + 15, "array"},
      {"sprite_state", "Sprite states", ALTTPMemoryMap::kSpriteState,
       ALTTPMemoryMap::kSpriteState + 15, "array"},
      {"sprite_type", "Sprite types", ALTTPMemoryMap::kSpriteType,
       ALTTPMemoryMap::kSpriteType + 15, "array"},
      {"oam_buffer", "OAM sprite buffer", ALTTPMemoryMap::kOAMBuffer,
       ALTTPMemoryMap::kOAMBufferEnd, "array"},
      {"overworld_area", "Current overworld area", ALTTPMemoryMap::kOverworldArea,
       ALTTPMemoryMap::kOverworldArea, "byte"},
      {"dungeon_room", "Current dungeon room", ALTTPMemoryMap::kDungeonRoom,
       ALTTPMemoryMap::kDungeonRoom + 1, "word"},
      {"player_health", "Player current health", ALTTPMemoryMap::kPlayerHealth,
       ALTTPMemoryMap::kPlayerHealth, "byte"},
      {"player_max_health", "Player max health",
       ALTTPMemoryMap::kPlayerMaxHealth, ALTTPMemoryMap::kPlayerMaxHealth,
       "byte"},
      {"player_rupees", "Player rupees", ALTTPMemoryMap::kPlayerRupees,
       ALTTPMemoryMap::kPlayerRupees + 1, "word"},
      {"inventory", "Player inventory", ALTTPMemoryMap::kInventoryStart,
       ALTTPMemoryMap::kInventoryStart + 0x2F, "struct"},
  };
}

std::string MemoryInspectorBase::FormatHex(const std::vector<uint8_t>& data,
                                           int bytes_per_line) const {
  std::ostringstream oss;
  for (size_t i = 0; i < data.size(); ++i) {
    if (i > 0 && i % bytes_per_line == 0)
      oss << "\n";
    else if (i > 0)
      oss << " ";
    oss << absl::StrFormat("%02X", data[i]);
  }
  return oss.str();
}

std::string MemoryInspectorBase::FormatAscii(
    const std::vector<uint8_t>& data) const {
  std::string result;
  result.reserve(data.size());
  for (uint8_t byte : data) {
    result += (std::isprint(byte) ? static_cast<char>(byte) : '.');
  }
  return result;
}

absl::StatusOr<uint32_t> MemoryInspectorBase::ParseAddress(
    const std::string& addr_str) const {
  std::string s = addr_str;

  // Remove $ or 0x prefix
  if (!s.empty() && s[0] == '$') {
    s = s.substr(1);
  } else if (s.size() > 2 && s[0] == '0' && (s[1] == 'x' || s[1] == 'X')) {
    s = s.substr(2);
  }

  // Parse as hex
  try {
    return static_cast<uint32_t>(std::stoul(s, nullptr, 16));
  } catch (const std::exception& e) {
    return absl::InvalidArgumentError(
        absl::StrCat("Invalid address: ", addr_str));
  }
}

// ============================================================================
// MemoryAnalyzeTool Implementation
// ============================================================================

absl::Status MemoryAnalyzeTool::ValidateArgs(
    const resources::ArgumentParser& parser) {
  return parser.RequireArgs({"address", "length"});
}

absl::Status MemoryAnalyzeTool::Execute(
    Rom* rom, const resources::ArgumentParser& parser,
    resources::OutputFormatter& formatter) {
  auto addr_str = parser.GetString("address");
  if (!addr_str.has_value()) {
    return absl::InvalidArgumentError("Missing required argument: address");
  }
  auto addr_result = ParseAddress(*addr_str);
  if (!addr_result.ok())
    return addr_result.status();
  uint32_t address = *addr_result;

  int length = std::stoi(parser.GetString("length").value_or("16"));
  if (length <= 0 || length > 0x10000) {
    return absl::InvalidArgumentError("Length must be between 1 and 65536");
  }

  // Build analysis result
  formatter.BeginObject("MemoryAnalysis");
  formatter.AddField("address", absl::StrFormat("$%06X", address));
  formatter.AddField("length", length);
  formatter.AddField("region", DescribeAddress(address));
  formatter.AddField("data_type", IdentifyDataType(address));
  formatter.AddField("note", "Connect to emulator via gRPC to read actual memory data");

  // Provide context-specific analysis hints
  if (ALTTPMemoryMap::IsSpriteTable(address)) {
    formatter.AddField("analysis_hint",
        "Sprite table: Check sprite_state ($DD0), sprite_type ($E20), "
        "sprite_health ($E50) for each sprite (0-15)");
  } else if (address >= ALTTPMemoryMap::kLinkYLow &&
             address <= ALTTPMemoryMap::kLinkDirection) {
    formatter.AddField("analysis_hint",
        "Player state: Position at $20-$23, state at $5D, direction at $2F");
  } else if (address == ALTTPMemoryMap::kGameMode) {
    formatter.AddField("analysis_hint",
        "Game mode: 0x07=Dungeon, 0x09=Overworld, 0x19=Inventory, "
        "0x0D=Dialogue");
  }

  formatter.EndObject();
  return absl::OkStatus();
}

std::map<std::string, std::string> MemoryAnalyzeTool::AnalyzeSpriteEntry(
    int sprite_index, const std::vector<uint8_t>& wram) const {
  std::map<std::string, std::string> result;

  if (wram.size() < 0x1000)
    return result;

  uint8_t state = wram[0x0DD0 + sprite_index];
  uint8_t type = wram[0x0E20 + sprite_index];
  uint8_t health = wram[0x0E50 + sprite_index];
  uint8_t y_low = wram[0x0D00 + sprite_index];
  uint8_t x_low = wram[0x0D10 + sprite_index];
  uint8_t y_high = wram[0x0D20 + sprite_index];
  uint8_t x_high = wram[0x0D30 + sprite_index];

  uint16_t x = (x_high << 8) | x_low;
  uint16_t y = (y_high << 8) | y_low;

  result["sprite_index"] = std::to_string(sprite_index);
  result["state"] = absl::StrFormat("$%02X", state);
  result["active"] = (state != 0x00) ? "yes" : "no";
  result["type"] = absl::StrFormat("$%02X", type);

  auto type_it = kSpriteTypes.find(type);
  if (type_it != kSpriteTypes.end()) {
    result["type_name"] = type_it->second;
  }

  result["health"] = std::to_string(health);
  result["position"] = absl::StrFormat("(%d, %d)", x, y);

  return result;
}

std::map<std::string, std::string> MemoryAnalyzeTool::AnalyzePlayerState(
    const std::vector<uint8_t>& wram) const {
  std::map<std::string, std::string> result;

  if (wram.size() < 0x100)
    return result;

  uint16_t x = (wram[0x23] << 8) | wram[0x22];
  uint16_t y = (wram[0x21] << 8) | wram[0x20];
  uint8_t state = wram[0x5D];
  uint8_t direction = wram[0x2F];

  result["position"] = absl::StrFormat("(%d, %d)", x, y);
  result["state"] = absl::StrFormat("$%02X", state);

  auto dir_it = kLinkDirections.find(direction);
  if (dir_it != kLinkDirections.end()) {
    result["direction"] = dir_it->second;
  } else {
    result["direction"] = absl::StrFormat("$%02X", direction);
  }

  return result;
}

std::map<std::string, std::string> MemoryAnalyzeTool::AnalyzeGameMode(
    const std::vector<uint8_t>& wram) const {
  std::map<std::string, std::string> result;

  if (wram.size() < 0x20)
    return result;

  uint8_t mode = wram[0x10];
  uint8_t submodule = wram[0x11];

  result["mode"] = absl::StrFormat("$%02X", mode);
  result["submodule"] = absl::StrFormat("$%02X", submodule);

  auto mode_it = kGameModes.find(mode);
  if (mode_it != kGameModes.end()) {
    result["mode_name"] = mode_it->second;
  }

  return result;
}

// ============================================================================
// MemorySearchTool Implementation
// ============================================================================

absl::Status MemorySearchTool::ValidateArgs(
    const resources::ArgumentParser& parser) {
  return parser.RequireArgs({"pattern"});
}

absl::Status MemorySearchTool::Execute(
    Rom* rom, const resources::ArgumentParser& parser,
    resources::OutputFormatter& formatter) {
  auto pattern_opt = parser.GetString("pattern");
  if (!pattern_opt.has_value()) {
    return absl::InvalidArgumentError("Missing required argument: pattern");
  }
  std::string pattern_str = *pattern_opt;
  int max_results = std::stoi(parser.GetString("max-results").value_or("10"));

  auto pattern_result = ParsePattern(pattern_str);
  if (!pattern_result.ok())
    return pattern_result.status();

  auto [pattern, mask] = *pattern_result;

  formatter.BeginObject("MemorySearch");
  formatter.AddField("pattern", pattern_str);
  formatter.AddField("pattern_length", static_cast<int>(pattern.size()));
  formatter.AddField("max_results", max_results);
  formatter.AddField("note", "Connect to emulator via gRPC to search actual memory");

  // Show parsed pattern
  std::ostringstream parsed;
  for (size_t i = 0; i < pattern.size(); ++i) {
    if (i > 0)
      parsed << " ";
    if (mask[i]) {
      parsed << absl::StrFormat("%02X", pattern[i]);
    } else {
      parsed << "??";
    }
  }
  formatter.AddField("parsed_pattern", parsed.str());

  formatter.EndObject();
  return absl::OkStatus();
}

absl::StatusOr<std::pair<std::vector<uint8_t>, std::vector<bool>>>
MemorySearchTool::ParsePattern(const std::string& pattern_str) const {
  std::vector<uint8_t> pattern;
  std::vector<bool> mask;  // true = must match, false = wildcard

  // Remove spaces and split into byte pairs
  std::string clean;
  for (char c : pattern_str) {
    if (!std::isspace(c))
      clean += c;
  }

  if (clean.length() % 2 != 0) {
    return absl::InvalidArgumentError(
        "Pattern must have even number of hex characters");
  }

  for (size_t i = 0; i < clean.length(); i += 2) {
    std::string byte_str = clean.substr(i, 2);

    if (byte_str == "??" || byte_str == "**") {
      pattern.push_back(0x00);
      mask.push_back(false);  // Wildcard
    } else {
      try {
        uint8_t byte = static_cast<uint8_t>(std::stoul(byte_str, nullptr, 16));
        pattern.push_back(byte);
        mask.push_back(true);  // Must match
      } catch (const std::exception&) {
        return absl::InvalidArgumentError(
            absl::StrCat("Invalid hex byte: ", byte_str));
      }
    }
  }

  return std::make_pair(pattern, mask);
}

std::vector<PatternMatch> MemorySearchTool::FindMatches(
    const std::vector<uint8_t>& memory, uint32_t base_address,
    const std::vector<uint8_t>& pattern, const std::vector<bool>& mask,
    int max_results) const {
  std::vector<PatternMatch> matches;

  if (pattern.empty() || memory.size() < pattern.size())
    return matches;

  for (size_t i = 0; i <= memory.size() - pattern.size(); ++i) {
    bool match = true;
    for (size_t j = 0; j < pattern.size() && match; ++j) {
      if (mask[j] && memory[i + j] != pattern[j]) {
        match = false;
      }
    }

    if (match) {
      PatternMatch m;
      m.address = base_address + static_cast<uint32_t>(i);
      m.matched_bytes =
          std::vector<uint8_t>(memory.begin() + i,
                               memory.begin() + i + pattern.size());
      m.context = DescribeAddress(m.address);
      matches.push_back(m);

      if (static_cast<int>(matches.size()) >= max_results)
        break;
    }
  }

  return matches;
}

// ============================================================================
// MemoryCompareTool Implementation
// ============================================================================

absl::Status MemoryCompareTool::ValidateArgs(
    const resources::ArgumentParser& parser) {
  return parser.RequireArgs({"address"});
}

absl::Status MemoryCompareTool::Execute(
    Rom* rom, const resources::ArgumentParser& parser,
    resources::OutputFormatter& formatter) {
  auto addr_result = ParseAddress(parser.GetString("address").value_or(""));
  if (!addr_result.ok())
    return addr_result.status();
  uint32_t address = *addr_result;

  std::string expected_str = parser.GetString("expected").value_or("");

  formatter.BeginObject();
  formatter.AddField("address", absl::StrFormat("$%06X", address));
  formatter.AddField("region", DescribeAddress(address));

  if (!expected_str.empty()) {
    formatter.AddField("expected", expected_str);
    formatter.AddField("note", "Connect to emulator via gRPC to compare actual memory");
  } else {
    formatter.AddField("note", "Provide --expected <hex> to compare against expected values");
  }

  formatter.EndObject();
  return absl::OkStatus();
}

// ============================================================================
// MemoryCheckTool Implementation
// ============================================================================

absl::Status MemoryCheckTool::ValidateArgs(
    const resources::ArgumentParser& parser) {
  return absl::OkStatus();  // No required args
}

absl::Status MemoryCheckTool::Execute(
    Rom* rom, const resources::ArgumentParser& parser,
    resources::OutputFormatter& formatter) {
  std::string region = parser.GetString("region").value_or("all");

  // Provide check descriptions
  std::vector<std::string> checks;
  if (region == "all" || region == "sprites") {
    checks.push_back("Sprite table: Check for invalid states, out-of-bounds positions");
  }
  if (region == "all" || region == "player") {
    checks.push_back("Player state: Check for invalid positions, corrupted state");
  }
  if (region == "all" || region == "game") {
    checks.push_back("Game mode: Check for invalid mode/submodule combinations");
  }

  std::ostringstream checks_str;
  for (const auto& check : checks) {
    checks_str << "- " << check << "\n";
  }

  formatter.BeginObject();
  formatter.AddField("region", region);
  formatter.AddField("note", "Connect to emulator via gRPC to check actual memory");
  formatter.AddField("available_checks", checks_str.str());
  formatter.EndObject();
  return absl::OkStatus();
}

std::vector<MemoryAnomaly> MemoryCheckTool::CheckSpriteTable(
    const std::vector<uint8_t>& wram) const {
  std::vector<MemoryAnomaly> anomalies;

  if (wram.size() < 0x1000)
    return anomalies;

  for (int i = 0; i < ALTTPMemoryMap::kMaxSprites; ++i) {
    uint8_t state = wram[0x0DD0 + i];
    uint8_t type = wram[0x0E20 + i];

    // Check for invalid state (non-zero but unusually high)
    if (state > 0 && state > 0x10) {
      anomalies.push_back({
          static_cast<uint32_t>(ALTTPMemoryMap::kSpriteState + i),
          "suspicious_state",
          absl::StrFormat("Sprite %d has unusual state $%02X", i, state),
          2,
      });
    }

    // Check for active sprite with type 0 (usually invalid)
    if (state != 0 && type == 0) {
      anomalies.push_back({
          static_cast<uint32_t>(ALTTPMemoryMap::kSpriteType + i),
          "type_mismatch",
          absl::StrFormat("Sprite %d is active but has type 0", i),
          3,
      });
    }
  }

  return anomalies;
}

std::vector<MemoryAnomaly> MemoryCheckTool::CheckPlayerState(
    const std::vector<uint8_t>& wram) const {
  std::vector<MemoryAnomaly> anomalies;

  if (wram.size() < 0x100)
    return anomalies;

  uint16_t x = (wram[0x23] << 8) | wram[0x22];
  uint16_t y = (wram[0x21] << 8) | wram[0x20];

  // Check for out-of-bounds position
  if (x > 0x2000 || y > 0x2000) {
    anomalies.push_back({
        ALTTPMemoryMap::kLinkXLow,
        "out_of_bounds",
        absl::StrFormat("Link position (%d, %d) seems out of bounds", x, y),
        4,
    });
  }

  return anomalies;
}

std::vector<MemoryAnomaly> MemoryCheckTool::CheckGameMode(
    const std::vector<uint8_t>& wram) const {
  std::vector<MemoryAnomaly> anomalies;

  if (wram.size() < 0x20)
    return anomalies;

  uint8_t mode = wram[0x10];

  // Check for invalid game mode
  if (mode > 0x1B) {
    anomalies.push_back({
        ALTTPMemoryMap::kGameMode,
        "invalid_mode",
        absl::StrFormat("Invalid game mode $%02X", mode),
        5,
    });
  }

  return anomalies;
}

// ============================================================================
// MemoryRegionsTool Implementation
// ============================================================================

absl::Status MemoryRegionsTool::ValidateArgs(
    const resources::ArgumentParser& parser) {
  return absl::OkStatus();  // No required args
}

absl::Status MemoryRegionsTool::Execute(
    Rom* rom, const resources::ArgumentParser& parser,
    resources::OutputFormatter& formatter) {
  std::string filter = parser.GetString("filter").value_or("");

  auto regions = GetKnownRegions();

  // Filter regions if requested
  if (!filter.empty()) {
    std::vector<MemoryRegionInfo> filtered;
    for (const auto& region : regions) {
      if (region.name.find(filter) != std::string::npos ||
          region.description.find(filter) != std::string::npos) {
        filtered.push_back(region);
      }
    }
    regions = filtered;
  }

  // Build output as object with regions array
  formatter.BeginObject();
  formatter.BeginArray("regions");
  for (const auto& region : regions) {
    formatter.BeginObject();
    formatter.AddField("name", region.name);
    formatter.AddField("start", absl::StrFormat("$%06X", region.start_address));
    formatter.AddField("end", absl::StrFormat("$%06X", region.end_address));
    formatter.AddField("type", region.data_type);
    formatter.AddField("description", region.description);
    formatter.EndObject();
  }
  formatter.EndArray();
  formatter.EndObject();
  return absl::OkStatus();
}

}  // namespace tools
}  // namespace agent
}  // namespace cli
}  // namespace yaze
