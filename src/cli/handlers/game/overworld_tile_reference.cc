#include "cli/handlers/game/overworld_tile_reference.h"

#include <cctype>
#include <climits>
#include <fstream>

#include "absl/strings/ascii.h"
#include "absl/strings/match.h"
#include "absl/strings/str_format.h"
#include "absl/strings/string_view.h"
#include "app/gfx/types/snes_tile.h"
#include "nlohmann/json.hpp"
#include "rom/lyra_layout_descriptor.h"
#include "util/hex.h"
#include "util/macro.h"
#include "zelda3/game_data.h"
#include "zelda3/overworld/overworld_map.h"

namespace yaze {
namespace cli {
namespace overworld {
namespace {

absl::StatusOr<uint16_t> ParseHexU16(absl::string_view text) {
  absl::string_view cleaned = absl::StripAsciiWhitespace(text);
  if (absl::StartsWithIgnoreCase(cleaned, "0x"))
    cleaned.remove_prefix(2);
  uint32_t value = 0;
  if (cleaned.empty() || cleaned.front() == '$' ||
      absl::StartsWithIgnoreCase(cleaned, "0x") ||
      std::isspace(static_cast<unsigned char>(cleaned.front())) ||
      !yaze::util::ParseHexString(cleaned, &value) || value > 0xFFFF) {
    return absl::InvalidArgumentError(
        absl::StrFormat("Invalid hex u16: %s", text));
  }
  return static_cast<uint16_t>(value);
}

// Reject negatives and values above 65535 before any uint16_t cast. JSON
// integers and hex strings are accepted; floats/bools/null are not.
absl::StatusOr<uint16_t> ParseU16Field(const nlohmann::json& value,
                                       absl::string_view field) {
  if (value.is_number_integer()) {
    const auto n = value.get<std::int64_t>();
    if (n < 0 || n > 0xFFFF) {
      return absl::InvalidArgumentError(absl::StrFormat(
          "Reference policy %s must be an integer in 0..65535", field));
    }
    return static_cast<uint16_t>(n);
  }
  if (value.is_string())
    return ParseHexU16(value.get<std::string>());
  return absl::InvalidArgumentError(absl::StrFormat(
      "Reference policy %s must be an integer or hex string", field));
}

absl::StatusOr<int> ParseCoordField(const nlohmann::json& value,
                                    absl::string_view field) {
  if (!value.is_number_integer()) {
    return absl::InvalidArgumentError(
        absl::StrFormat("Reference policy %s must be an integer", field));
  }
  const auto n = value.get<std::int64_t>();
  if (n < INT_MIN || n > INT_MAX) {
    return absl::InvalidArgumentError(
        absl::StrFormat("Reference policy %s is out of int range", field));
  }
  return static_cast<int>(n);
}

absl::StatusOr<TileReferenceRuleCell> ParseCell(const nlohmann::json& cell) {
  TileReferenceRuleCell out;
  if (cell.is_array()) {
    if (cell.size() != 3)
      return absl::InvalidArgumentError(
          "Reference policy cell arrays must be [x,y,tile16]");
    ASSIGN_OR_RETURN(out.x, ParseCoordField(cell[0], "cell.x"));
    ASSIGN_OR_RETURN(out.y, ParseCoordField(cell[1], "cell.y"));
    ASSIGN_OR_RETURN(out.tile16, ParseU16Field(cell[2], "cell.tile16"));
    return out;
  }
  if (!cell.is_object())
    return absl::InvalidArgumentError(
        "Reference policy cell must be array/object");
  if (!cell.contains("x") || !cell.contains("y") || !cell.contains("tile16"))
    return absl::InvalidArgumentError("Reference policy cell needs x/y/tile16");
  ASSIGN_OR_RETURN(out.x, ParseCoordField(cell.at("x"), "cell.x"));
  ASSIGN_OR_RETURN(out.y, ParseCoordField(cell.at("y"), "cell.y"));
  ASSIGN_OR_RETURN(out.tile16, ParseU16Field(cell.at("tile16"), "cell.tile16"));
  return out;
}

absl::StatusOr<TileReferenceStatus> ParseStatus(absl::string_view text) {
  const std::string value = absl::AsciiStrToLower(text);
  if (value == "observed")
    return TileReferenceStatus::kObserved;
  if (value == "verified_valid" ||
      value == "verified_valid_selected_correction")
    return TileReferenceStatus::kVerifiedValid;
  if (value == "confirmed_error")
    return TileReferenceStatus::kConfirmedError;
  if (value == "deliberate_exception")
    return TileReferenceStatus::kDeliberateException;
  if (value == "unresolved" || value == "unresolved_preimage_changed")
    return TileReferenceStatus::kUnresolved;
  return absl::InvalidArgumentError(
      absl::StrFormat("Unknown reference status %s", text));
}

absl::StatusOr<std::array<uint16_t, 4>> ParseWords(
    const nlohmann::json& words) {
  if (!words.is_array() || words.size() != 4)
    return absl::InvalidArgumentError("definitions words must be length 4");
  std::array<uint16_t, 4> out{};
  for (size_t i = 0; i < 4; ++i) {
    ASSIGN_OR_RETURN(out[i],
                     ParseU16Field(words[i], absl::StrFormat("word[%zu]", i)));
  }
  return out;
}

bool RuleCoversCell(const TileReferenceRule& rule, int x, int y) {
  for (const auto& cell : rule.before) {
    if (cell.x == x && cell.y == y)
      return true;
  }
  for (const auto& cell : rule.after) {
    if (cell.x == x && cell.y == y)
      return true;
  }
  return false;
}

absl::StatusOr<bool> CellsMatch(
    const std::vector<TileReferenceRuleCell>& cells,
    const std::function<absl::StatusOr<uint16_t>(int, int)>& read_cell) {
  for (const auto& cell : cells) {
    ASSIGN_OR_RETURN(const uint16_t tile, read_cell(cell.x, cell.y));
    if (tile != cell.tile16)
      return false;
  }
  return true;
}

absl::StatusOr<bool> DefinitionsMatch(
    const std::vector<std::pair<uint16_t, std::array<uint16_t, 4>>>& defs,
    const std::function<absl::StatusOr<std::array<uint16_t, 4>>(uint16_t)>&
        read_definition) {
  for (const auto& [id, words] : defs) {
    ASSIGN_OR_RETURN(const auto actual, read_definition(id));
    if (actual != words)
      return false;
  }
  return true;
}

}  // namespace

const char* TileReferenceStatusName(TileReferenceStatus status) {
  switch (status) {
    case TileReferenceStatus::kObserved:
      return "observed";
    case TileReferenceStatus::kVerifiedValid:
      return "verified_valid";
    case TileReferenceStatus::kConfirmedError:
      return "confirmed_error";
    case TileReferenceStatus::kDeliberateException:
      return "deliberate_exception";
    case TileReferenceStatus::kUnresolved:
      return "unresolved";
  }
  return "unresolved";
}

absl::StatusOr<TileReferencePolicy> LoadTileReferencePolicy(
    const std::string& path) {
  std::ifstream in(path);
  if (!in)
    return absl::NotFoundError(
        absl::StrFormat("Cannot open reference policy %s", path));
  nlohmann::json root;
  try {
    in >> root;
  } catch (const nlohmann::json::exception& e) {
    return absl::InvalidArgumentError(
        absl::StrFormat("Invalid reference policy JSON: %s", e.what()));
  }
  const nlohmann::json* rules = nullptr;
  if (root.is_object() && root.contains("rules") && root["rules"].is_array())
    rules = &root["rules"];
  else if (root.is_array())
    rules = &root;
  else
    return absl::InvalidArgumentError(
        "Reference policy needs a rules array (or top-level array)");

  TileReferencePolicy policy;
  for (const auto& item : *rules) {
    if (!item.is_object())
      return absl::InvalidArgumentError(
          "Each reference rule must be an object");
    TileReferenceRule rule;
    rule.id = item.value("id", "");
    if (rule.id.empty())
      return absl::InvalidArgumentError("Reference rule requires id");
    if (!item.contains("classification") || !item["classification"].is_string())
      return absl::InvalidArgumentError(
          "Reference rule requires classification string");
    ASSIGN_OR_RETURN(rule.classification,
                     ParseStatus(item["classification"].get<std::string>()));
    if (!item.contains("map"))
      return absl::InvalidArgumentError("Reference rule requires map");
    ASSIGN_OR_RETURN(const auto map_id, ParseU16Field(item["map"], "map"));
    rule.map = static_cast<int>(map_id);
    for (const char* key : {"before", "after", "preserve"}) {
      if (!item.contains(key))
        continue;
      if (!item[key].is_array())
        return absl::InvalidArgumentError(
            absl::StrFormat("Reference rule %s must be an array", key));
      for (const auto& cell : item[key]) {
        ASSIGN_OR_RETURN(auto parsed, ParseCell(cell));
        if (std::string(key) == "before")
          rule.before.push_back(parsed);
        else if (std::string(key) == "after")
          rule.after.push_back(parsed);
        else
          rule.preserve.push_back(parsed);
      }
    }
    if (item.contains("definitions")) {
      if (!item["definitions"].is_object())
        return absl::InvalidArgumentError("definitions must be an object");
      for (auto it = item["definitions"].begin();
           it != item["definitions"].end(); ++it) {
        ASSIGN_OR_RETURN(const auto tile_id, ParseHexU16(it.key()));
        ASSIGN_OR_RETURN(const auto words, ParseWords(it.value()));
        rule.definitions.emplace_back(tile_id, words);
      }
    }
    rule.reason = item.value("reason", "");
    rule.user_ruling = item.value("user_ruling", "");
    policy.rules.push_back(std::move(rule));
  }
  return policy;
}

absl::StatusOr<TileReferenceMatch> ClassifyTileReferenceOccurrence(
    const TileReferencePolicy& policy, int map_id, int x, int y,
    const std::function<absl::StatusOr<uint16_t>(int, int)>& read_cell,
    const std::function<absl::StatusOr<std::array<uint16_t, 4>>(uint16_t)>&
        read_definition) {
  TileReferenceMatch out;
  out.status = TileReferenceStatus::kObserved;
  out.reason = "No matching source-reference rule for this occurrence";

  for (const auto& rule : policy.rules) {
    if (rule.map != map_id || !RuleCoversCell(rule, x, y))
      continue;
    out.rule_id = rule.id;
    out.user_ruling = rule.user_ruling;
    bool defs_ok = true;
    if (!rule.definitions.empty()) {
      ASSIGN_OR_RETURN(defs_ok,
                       DefinitionsMatch(rule.definitions, read_definition));
    }
    if (defs_ok && !rule.after.empty()) {
      ASSIGN_OR_RETURN(const bool after_ok, CellsMatch(rule.after, read_cell));
      if (after_ok) {
        out.status = TileReferenceStatus::kVerifiedValid;
        out.reason = rule.reason.empty()
                         ? "Selected correction preimage matches"
                         : rule.reason;
        return out;
      }
    }
    if (defs_ok && !rule.before.empty()) {
      ASSIGN_OR_RETURN(const bool before_ok,
                       CellsMatch(rule.before, read_cell));
      if (before_ok) {
        out.status = rule.classification;
        out.reason = rule.reason;
        return out;
      }
    }
    out.status = TileReferenceStatus::kUnresolved;
    out.reason = rule.reason.empty()
                     ? "Rule covers this cell but preimage/definitions drifted"
                     : rule.reason;
    return out;
  }
  return out;
}

absl::StatusOr<TileReferenceInspection> InspectTile16Reference(
    zelda3::Overworld& overworld, int map_id, int x, int y,
    const TileReferencePolicy* policy) {
  ASSIGN_OR_RETURN(const auto location,
                   ResolveAreaTileForMap(overworld, map_id, x, y));
  ASSIGN_OR_RETURN(const uint16_t tile, ReadAreaTile(overworld, location));

  // Quadrant words/collision never need source-group resolution. Passing an
  // empty screen list keeps this usable without EnsureMapBuilt.
  ASSIGN_OR_RETURN(const auto details, DescribeTile16(overworld, tile, {}));

  TileReferenceInspection out;
  out.map_id = map_id;
  out.x = x;
  out.y = y;
  out.tile16 = tile;
  out.location = location;
  out.quadrants = details.quadrants;
  out.status = TileReferenceStatus::kObserved;
  out.reason = "No matching source-reference rule for this occurrence";

  for (int dy = -1; dy <= 1; ++dy) {
    for (int dx = -1; dx <= 1; ++dx) {
      TileNeighborCell neighbor;
      neighbor.dx = dx;
      neighbor.dy = dy;
      const auto neighbor_location =
          ResolveAreaTileForMap(overworld, map_id, x + dx, y + dy);
      if (!neighbor_location.ok()) {
        neighbor.in_bounds = false;
        out.neighbors.push_back(neighbor);
        continue;
      }
      const auto neighbor_tile = ReadAreaTile(overworld, *neighbor_location);
      if (!neighbor_tile.ok()) {
        neighbor.in_bounds = false;
        out.neighbors.push_back(neighbor);
        continue;
      }
      neighbor.in_bounds = true;
      neighbor.tile16 = *neighbor_tile;
      out.neighbors.push_back(neighbor);
    }
  }

  if (policy == nullptr)
    return out;

  auto read_cell = [&](int cx, int cy) -> absl::StatusOr<uint16_t> {
    ASSIGN_OR_RETURN(const auto loc,
                     ResolveAreaTileForMap(overworld, map_id, cx, cy));
    return ReadAreaTile(overworld, loc);
  };
  auto read_definition =
      [&](uint16_t id) -> absl::StatusOr<std::array<uint16_t, 4>> {
    const auto& tiles = overworld.tiles16();
    if (id >= tiles.size()) {
      return absl::OutOfRangeError(
          absl::StrFormat("Tile16 0x%04X has no definition", id));
    }
    const auto& t = tiles[id];
    return std::array<uint16_t, 4>{
        gfx::TileInfoToWord(t.tile0_), gfx::TileInfoToWord(t.tile1_),
        gfx::TileInfoToWord(t.tile2_), gfx::TileInfoToWord(t.tile3_)};
  };
  ASSIGN_OR_RETURN(const auto match,
                   ClassifyTileReferenceOccurrence(*policy, map_id, x, y,
                                                   read_cell, read_definition));
  out.status = match.status;
  out.reason = match.reason;
  out.rule_id = match.rule_id;
  out.user_ruling = match.user_ruling;
  return out;
}

}  // namespace overworld

namespace handlers {
namespace {

using ::yaze::util::ParseHexString;

void AddQuadrantFields(resources::OutputFormatter& formatter,
                       const overworld::Tile16QuadrantDetail& q) {
  formatter.AddHexField("word", q.word, 4);
  formatter.AddHexField("tile8_id", q.tile8_id, 3);
  formatter.AddField("palette", q.palette);
  formatter.AddField("priority", q.priority);
  formatter.AddField("flip_x", q.flip_x);
  formatter.AddField("flip_y", q.flip_y);
  if (q.tile_type.has_value())
    formatter.AddHexField("tile_type", *q.tile_type, 2);
  else
    formatter.AddField("tile_type", "unresolved");
}

absl::StatusOr<int> ParseMapIdArg(const resources::ArgumentParser& parser,
                                  int map_count) {
  const auto value = parser.GetString("map");
  int map_id = 0;
  if (!value.has_value() || !ParseHexString(*value, &map_id)) {
    return absl::InvalidArgumentError(
        "--map must be a hex map id (e.g. 0x0D or 0D)");
  }
  if (map_id < 0 || map_id >= map_count) {
    return absl::InvalidArgumentError(
        absl::StrFormat("Map ID out of range: 0x%02X", map_id));
  }
  return map_id;
}

int TileCommandMapCount(const Rom* rom) {
  if (rom) {
    const auto layout = rom->InspectLyraLayout();
    if (rom::IsNativeSpecialOverworldLayout(layout))
      return layout.capacity;
  }
  return zelda3::kNumOverworldMaps;
}

absl::StatusOr<int> ParseCoordinateArg(const resources::ArgumentParser& parser,
                                       const std::string& name) {
  auto value = parser.GetInt(name);
  if (!value.ok()) {
    return absl::InvalidArgumentError(absl::StrFormat(
        "--%s must be a decimal tile coordinate (or 0x-prefixed hex): %s", name,
        value.status().message()));
  }
  return *value;
}

}  // namespace

absl::Status OverworldTileReferenceInspectCommandHandler::Execute(
    Rom* rom, const resources::ArgumentParser& parser,
    resources::OutputFormatter& formatter) {
  if (rom != nullptr && parser.HasLyraWriteAdmission())
    rom->EnableExperimentalPageTile16();
  if (rom == nullptr || !rom->is_loaded())
    return absl::FailedPreconditionError("ROM not loaded");

  ASSIGN_OR_RETURN(const int map_id,
                   ParseMapIdArg(parser, TileCommandMapCount(rom)));
  ASSIGN_OR_RETURN(const int x, ParseCoordinateArg(parser, "x"));
  ASSIGN_OR_RETURN(const int y, ParseCoordinateArg(parser, "y"));

  std::optional<overworld::TileReferencePolicy> policy;
  if (const auto path = parser.GetString("reference-policy");
      path.has_value()) {
    ASSIGN_OR_RETURN(policy, overworld::LoadTileReferencePolicy(*path));
  }

  zelda3::GameData game_data;
  zelda3::LoadOptions options;
  options.expand_rom = false;
  RETURN_IF_ERROR(zelda3::LoadGameData(*rom, game_data, options));
  zelda3::Overworld overworld(rom, &game_data);
  RETURN_IF_ERROR(overworld.Load(rom));
  ASSIGN_OR_RETURN(const auto inspection,
                   overworld::InspectTile16Reference(
                       overworld, map_id, x, y, policy ? &*policy : nullptr));

  formatter.BeginObject("Overworld Tile Reference");
  formatter.AddHexField("map", inspection.map_id, 2);
  formatter.AddField("x", inspection.x);
  formatter.AddField("y", inspection.y);
  formatter.AddHexField("tile16", inspection.tile16, 3);
  formatter.AddHexField("parent_map", inspection.location.parent_map, 2);
  formatter.AddField("area_width", inspection.location.area_width);
  formatter.AddField("area_height", inspection.location.area_height);
  formatter.AddField("world_x", inspection.location.world_x);
  formatter.AddField("world_y", inspection.location.world_y);

  formatter.BeginArray("quadrants");
  static constexpr const char* kNames[] = {"TL", "TR", "BL", "BR"};
  for (int i = 0; i < 4; ++i) {
    formatter.BeginObject();
    formatter.AddField("corner", kNames[i]);
    AddQuadrantFields(formatter, inspection.quadrants[i]);
    formatter.EndObject();
  }
  formatter.EndArray();

  formatter.BeginArray("neighbors");
  for (const auto& n : inspection.neighbors) {
    formatter.BeginObject();
    formatter.AddField("dx", n.dx);
    formatter.AddField("dy", n.dy);
    formatter.AddField("in_bounds", n.in_bounds);
    if (n.tile16.has_value())
      formatter.AddHexField("tile16", *n.tile16, 3);
    else
      formatter.AddField("tile16", "out_of_bounds");
    formatter.EndObject();
  }
  formatter.EndArray();

  formatter.BeginObject("reference");
  formatter.AddField("status",
                     overworld::TileReferenceStatusName(inspection.status));
  formatter.AddField("reason", inspection.reason);
  if (!inspection.rule_id.empty())
    formatter.AddField("rule_id", inspection.rule_id);
  if (!inspection.user_ruling.empty())
    formatter.AddField("user_ruling", inspection.user_ruling);
  formatter.EndObject();
  formatter.EndObject();
  return absl::OkStatus();
}

}  // namespace handlers
}  // namespace cli
}  // namespace yaze
