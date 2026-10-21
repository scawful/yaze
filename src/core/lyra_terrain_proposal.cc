#include "core/lyra_terrain_proposal.h"

#include <algorithm>
#include <array>
#include <initializer_list>
#include <limits>
#include <map>
#include <optional>
#include <set>
#include <utility>
#include <vector>

#include "absl/status/status.h"
#include "absl/strings/str_cat.h"
#include "core/source_artifact_publisher.h"
#include "nlohmann/json.hpp"

namespace yaze::core {
namespace {
using Json = nlohmann::json;
using Tile32 = std::array<uint16_t, 4>;

constexpr int kMapSize = LyraTerrainDocument::kMapSize;
constexpr char kBundleKind[] = "lyra_tile16_proposal_bundle";
constexpr char kAcceptance[] =
    "artifact proposal only; editor transaction and runtime unqualified";
constexpr int kMaxDepth = 16;
// Larger coordinates are refused by the planner as outside the 32x32 target.
constexpr int64_t kMaxCoordinate = 4096;
constexpr size_t kMaxPlacements = 64;
constexpr size_t kMaxListEntries = 1024;
constexpr int64_t kMaxInteger = std::numeric_limits<int64_t>::max();

absl::Status Malformed(std::string_view message) {
  return absl::InvalidArgumentError(
      absl::StrCat("Lyra proposal refused: ", std::string(message)));
}

// Booleans and floats are never integers, matching the planner's type checks.
std::optional<int64_t> Integer(const Json& value, int64_t low, int64_t high) {
  int64_t number = 0;
  if (value.is_number_unsigned()) {
    const auto unsigned_number = value.get<uint64_t>();
    if (unsigned_number > static_cast<uint64_t>(kMaxInteger))
      return std::nullopt;
    number = static_cast<int64_t>(unsigned_number);
  } else if (value.is_number_integer()) {
    number = value.get<int64_t>();
  } else {
    return std::nullopt;
  }
  if (number < low || number > high)
    return std::nullopt;
  return number;
}

bool Text(const Json& value, std::string_view expected) {
  return value.is_string() && value.get_ref<const std::string&>() == expected;
}

bool NonEmptyText(const Json& value) {
  return value.is_string() && !value.get_ref<const std::string&>().empty();
}

bool Listed(std::initializer_list<const char*> keys, const std::string& key) {
  return std::any_of(keys.begin(), keys.end(),
                     [&](const char* listed) { return key == listed; });
}

// Unknown fields fail closed; only provenance-style notes are optional.
bool Keys(const Json& value, std::initializer_list<const char*> required,
          std::initializer_list<const char*> optional = {}) {
  if (!value.is_object())
    return false;
  for (const char* key : required) {
    if (!value.contains(key))
      return false;
  }
  for (const auto& item : value.items()) {
    if (!Listed(required, item.key()) && !Listed(optional, item.key()))
      return false;
  }
  return true;
}

std::optional<std::pair<int64_t, int64_t>> Pair(const Json& value) {
  if (!value.is_array() || value.size() != 2)
    return std::nullopt;
  const auto x = Integer(value[0], 0, kMaxCoordinate);
  const auto y = Integer(value[1], 0, kMaxCoordinate);
  if (!x || !y)
    return std::nullopt;
  return std::make_pair(*x, *y);
}

bool InMap(int64_t x, int64_t y) {
  return x >= 0 && x < kMapSize && y >= 0 && y < kMapSize;
}

// Python json.dumps(sort_keys=True, separators=(",", ":"), ensure_ascii=True).
// nlohmann objects are byte-ordered maps, which equals code point order.
std::string Digest(const Json& value) {
  return ComputeSourceArtifactSha256(
      value.dump(-1, ' ', true, Json::error_handler_t::strict));
}

bool SameCanonical(const Json& a, const Json& b) {
  return a.dump(-1, ' ', true, Json::error_handler_t::strict) ==
         b.dump(-1, ' ', true, Json::error_handler_t::strict);
}

// Float spellings differ between Python and nlohmann, so no digest can bind
// them; refuse floats anywhere, including free-form provenance.
bool HasFloat(const Json& value) {
  if (value.is_number_float())
    return true;
  if (value.is_structured()) {
    for (const auto& child : value) {
      if (HasFloat(child))
        return true;
    }
  }
  return false;
}

bool LowerSha256(const std::string& value) {
  return value.size() == 64 &&
         std::all_of(value.begin(), value.end(), [](char c) {
           return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f');
         });
}

bool SameBinding(const LyraTerrainProposalBinding& a,
                 const LyraTerrainProposalBinding& b) {
  return a.game_id == b.game_id && a.adapter_id == b.adapter_id &&
         a.rom_sha256 == b.rom_sha256 &&
         a.definition_fingerprint == b.definition_fingerprint &&
         a.profile_fingerprint == b.profile_fingerprint &&
         a.tile_count == b.tile_count;
}

absl::StatusOr<LyraTerrainProposalBinding> ParseBinding(const Json& value) {
  if (!Keys(value,
            {"game_id", "adapter_id", "rom_sha256", "definition_fingerprint",
             "profile_fingerprint", "tile_count"})) {
    return Malformed(
        "binding must contain exactly the declared identity fields");
  }
  const auto tile_count =
      Integer(value.at("tile_count"), 1, std::numeric_limits<uint32_t>::max());
  if (!tile_count)
    return Malformed("binding.tile_count must be an integer >= 1");
  for (const char* key : {"game_id", "adapter_id", "rom_sha256",
                          "definition_fingerprint", "profile_fingerprint"}) {
    if (!NonEmptyText(value.at(key)))
      return Malformed(absl::StrCat("binding.", key, " must be nonempty"));
  }
  LyraTerrainProposalBinding binding{
      value.at("game_id").get<std::string>(),
      value.at("adapter_id").get<std::string>(),
      value.at("rom_sha256").get<std::string>(),
      value.at("definition_fingerprint").get<std::string>(),
      value.at("profile_fingerprint").get<std::string>(),
      static_cast<uint32_t>(*tile_count)};
  if (!LowerSha256(binding.rom_sha256) ||
      !LowerSha256(binding.definition_fingerprint) ||
      !LowerSha256(binding.profile_fingerprint)) {
    return Malformed("binding hashes must be lowercase SHA-256");
  }
  return binding;
}

absl::Status ParseBundle(std::string_view text, Json* json) {
  if (text.size() > kMaxLyraTerrainProposalBytes)
    return Malformed("bundle exceeds 1 MiB");
  bool duplicate_key = false;
  bool too_deep = false;
  std::vector<std::set<std::string>> object_keys;
  *json =
      Json::parse(text, [&](int depth, Json::parse_event_t event, Json& value) {
        if (depth > kMaxDepth)
          too_deep = true;
        if (event == Json::parse_event_t::object_start)
          object_keys.emplace_back();
        if (event == Json::parse_event_t::key &&
            !object_keys.back().insert(value.get<std::string>()).second) {
          duplicate_key = true;
        }
        if (event == Json::parse_event_t::object_end)
          object_keys.pop_back();
        return true;
      });
  if (duplicate_key)
    return Malformed("duplicate JSON keys");
  if (too_deep)
    return Malformed("JSON nesting is too deep");
  if (HasFloat(*json))
    return Malformed("floating-point numbers have no canonical digest");
  return absl::OkStatus();
}

struct Constraint {
  const char* kind;
  int64_t x;
  int64_t y;
  std::vector<int64_t> allowed;
};

absl::StatusOr<LyraTerrainStroke> Admit(
    std::string_view text, const LyraTerrainDocument& document,
    const LyraTerrainProposalBinding& actual) {
  Json bundle;
  if (auto status = ParseBundle(text, &bundle); !status.ok())
    return status;
  if (!Keys(bundle, {"schema_version", "kind", "map_id", "target", "footprint",
                     "report"}) ||
      !Integer(bundle.at("schema_version"), 1, 1) ||
      !Text(bundle.at("kind"), kBundleKind)) {
    return Malformed("unsupported proposal bundle schema");
  }
  const auto map_id = Integer(
      bundle.at("map_id"), LyraTerrainDocument::kFirstMap,
      LyraTerrainDocument::kFirstMap + LyraTerrainDocument::kMapCount - 1);
  if (!map_id)
    return Malformed("map_id must be an integer 0xA0..0xBF");

  const Json& target = bundle.at("target");
  const Json& footprint = bundle.at("footprint");
  const Json& report = bundle.at("report");
  if (!Keys(target, {"schema_version", "units", "binding", "rows"},
            {"protected", "tile32_inventory", "provenance"}) ||
      !Keys(footprint,
            {"schema_version", "units", "binding", "semantic_id",
             "review_state", "content_id", "rows", "anchor", "overlap_policy"},
            {"clearance", "ports", "unresolved_behavior", "provenance"})) {
    return Malformed("target or footprint has missing or unknown fields");
  }
  for (const Json* part : {&target, &footprint}) {
    if (!Integer(part->at("schema_version"), 1, 1) ||
        !Text(part->at("units"), "tile16")) {
      return Malformed("target and footprint require schema 1 tile16 units");
    }
  }

  auto bound = ParseBinding(target.at("binding"));
  if (!bound.ok())
    return bound.status();
  auto footprint_bound = ParseBinding(footprint.at("binding"));
  if (!footprint_bound.ok())
    return footprint_bound.status();
  if (!SameBinding(*bound, *footprint_bound))
    return Malformed("prefab and destination bindings differ");
  if (!SameBinding(*bound, actual)) {
    return absl::FailedPreconditionError(
        "Lyra proposal binding does not match the loaded source");
  }
  const auto& profile = document.profile();
  if (actual.tile_count != profile.tile_count ||
      actual.definition_fingerprint != profile.tile16_definition_fingerprint) {
    return absl::FailedPreconditionError(
        "Loaded source binding does not match the Lyra document profile");
  }
  const int64_t max_tile = static_cast<int64_t>(profile.tile_count) - 1;

  if (!Text(footprint.at("review_state"), "reviewed"))
    return Malformed("footprint must be explicitly reviewed");
  if (!NonEmptyText(footprint.at("semantic_id")))
    return Malformed("semantic_id is required");
  Json identity = footprint;
  identity.erase("content_id");
  if (!Text(footprint.at("content_id"), Digest(identity)))
    return Malformed("footprint content_id mismatch");

  // Every target cell is a guard, including cells outside the changes.
  const Json& rows = target.at("rows");
  if (!rows.is_array() || rows.size() != kMapSize)
    return Malformed("target must contain 32 Tile16 rows");
  std::array<uint16_t, kMapSize * kMapSize> before{};
  for (int y = 0; y < kMapSize; ++y) {
    const Json& row = rows[y];
    if (!row.is_array() || row.size() != kMapSize)
      return Malformed("target rows must contain 32 Tile16 cells");
    for (int x = 0; x < kMapSize; ++x) {
      const auto tile = Integer(row[x], 0, max_tile);
      if (!tile)
        return Malformed("target Tile16 is outside the bound definitions");
      before[y * kMapSize + x] = static_cast<uint16_t>(*tile);
      const auto current = document.MapTile(static_cast<int>(*map_id), x, y);
      if (!current.ok() || *current != *tile) {
        return absl::FailedPreconditionError(
            "Lyra proposal target is stale for the selected map");
      }
    }
  }

  const Json& cells = footprint.at("rows");
  if (!cells.is_array() || cells.empty() || cells.size() > kMapSize ||
      !cells[0].is_array() || cells[0].empty() || cells[0].size() > kMapSize) {
    return Malformed("footprint rows must be a nonempty rectangle up to 32x32");
  }
  const int width = static_cast<int>(cells[0].size());
  const int height = static_cast<int>(cells.size());
  std::vector<std::optional<uint16_t>> shape;
  for (const auto& row : cells) {
    if (!row.is_array() || row.size() != static_cast<size_t>(width))
      return Malformed("footprint rows must be rectangular");
    for (const auto& cell : row) {
      if (cell.is_null()) {
        shape.emplace_back();
        continue;
      }
      const auto tile = Integer(cell, 0, max_tile);
      if (!tile)
        return Malformed("footprint Tile16 is outside the bound definitions");
      shape.emplace_back(static_cast<uint16_t>(*tile));
    }
  }
  const auto anchor = Pair(footprint.at("anchor"));
  if (!anchor || anchor->first >= width || anchor->second >= height)
    return Malformed("anchor must be [x,y] inside the footprint");
  if (std::none_of(shape.begin(), shape.end(),
                   [](const auto& tile) { return tile.has_value(); })) {
    return Malformed("footprint has no occupied cells");
  }
  const Json& overlap = footprint.at("overlap_policy");
  if (!Text(overlap, "reject") && !Text(overlap, "same_only"))
    return Malformed("overlap_policy must be reject or same_only");
  const bool same_only = Text(overlap, "same_only");

  std::set<std::pair<int64_t, int64_t>> protected_cells;
  if (target.contains("protected")) {
    const Json& list = target.at("protected");
    if (!list.is_array() || list.size() > kMaxListEntries)
      return Malformed("protected must be a bounded list of cells");
    for (const auto& item : list) {
      const auto cell = Pair(item);
      if (!cell || !InMap(cell->first, cell->second))
        return Malformed("protected cell outside destination");
      protected_cells.insert(*cell);
    }
  }

  std::vector<Constraint> constraints;
  for (const char* kind : {"clearance", "ports"}) {
    if (!footprint.contains(kind))
      continue;
    const Json& list = footprint.at(kind);
    if (!list.is_array() || list.size() > kMaxListEntries)
      return Malformed(absl::StrCat(kind, " must be a bounded list"));
    for (const auto& guard : list) {
      if (!Keys(guard, {"at", "allowed_tile16"}, {"name", "provenance"}) ||
          (guard.contains("name") && !guard.at("name").is_string())) {
        return Malformed(
            absl::StrCat(kind, " must be a list of constraint objects"));
      }
      const auto at = Pair(guard.at("at"));
      const Json& allowed = guard.at("allowed_tile16");
      if (!at || !allowed.is_array() || allowed.empty() ||
          allowed.size() > kMaxListEntries) {
        return Malformed(
            absl::StrCat(kind, " needs [x,y] and explicit allowed_tile16"));
      }
      Constraint constraint{kind, at->first, at->second, {}};
      for (const auto& tile : allowed) {
        const auto value = Integer(tile, 0, max_tile);
        if (!value) {
          return Malformed(
              absl::StrCat(kind, " Tile16 is outside the bound definitions"));
        }
        constraint.allowed.push_back(*value);
      }
      constraints.push_back(std::move(constraint));
    }
  }

  const Json notes = footprint.contains("unresolved_behavior")
                         ? footprint.at("unresolved_behavior")
                         : Json::array();
  if (!notes.is_array() || notes.size() > kMaxListEntries ||
      !std::all_of(notes.begin(), notes.end(),
                   [](const Json& note) { return note.is_string(); })) {
    return Malformed("unresolved_behavior must be a list of strings");
  }

  if (!report.is_object() || !report.contains("placements"))
    return Malformed("report must be the complete planner report");
  const Json& placements = report.at("placements");
  if (!placements.is_array() || placements.empty() ||
      placements.size() > kMaxPlacements) {
    return Malformed("report needs 1 to 64 placements");
  }

  // Keyed (y, x) so iteration matches the planner's row-major change order.
  std::map<std::pair<int64_t, int64_t>, uint16_t> writes;
  Json occupied = Json::array();
  std::vector<std::pair<const Constraint*, std::pair<int64_t, int64_t>>> checks;
  for (size_t index = 0; index < placements.size(); ++index) {
    const auto at = Pair(placements[index]);
    if (!at)
      return Malformed("placement must be [x,y]");
    const int64_t ox = at->first - anchor->first;
    const int64_t oy = at->second - anchor->second;
    for (int y = 0; y < height; ++y) {
      for (int x = 0; x < width; ++x) {
        const auto& tile = shape[y * width + x];
        if (!tile)
          continue;
        const int64_t dx = ox + x;
        const int64_t dy = oy + y;
        if (!InMap(dx, dy))
          return Malformed("occupied cell outside destination");
        if (protected_cells.contains({dx, dy}) &&
            before[dy * kMapSize + dx] != *tile) {
          return Malformed("placement changes a protected cell");
        }
        const auto [entry, inserted] = writes.emplace(std::pair{dy, dx}, *tile);
        if (!inserted && (!same_only || entry->second != *tile))
          return Malformed("conflicting or forbidden footprint overlap");
        occupied.push_back(Json{
            {"placement", index}, {"x", dx}, {"y", dy}, {"tile16", *tile}});
      }
    }
    for (const auto& constraint : constraints) {
      const int64_t dx = ox + constraint.x;
      const int64_t dy = oy + constraint.y;
      if (!InMap(dx, dy)) {
        return Malformed(
            absl::StrCat(constraint.kind, " cell outside destination"));
      }
      checks.push_back({&constraint, {dx, dy}});
    }
  }
  // Constraints see the final combined proposal from every placement.
  for (const auto& [constraint, cell] : checks) {
    const auto [x, y] = cell;
    const auto write = writes.find({y, x});
    const int64_t value =
        write != writes.end() ? write->second : before[y * kMapSize + x];
    if (std::find(constraint->allowed.begin(), constraint->allowed.end(),
                  value) == constraint->allowed.end()) {
      return Malformed(absl::StrCat(constraint->kind, " constraint fails"));
    }
  }

  LyraTerrainStroke stroke;
  Json changes = Json::array();
  std::set<std::pair<int64_t, int64_t>> touched;  // Tile32 blocks as (y, x).
  for (const auto& [cell, tile] : writes) {
    const auto [y, x] = cell;
    const uint16_t old = before[y * kMapSize + x];
    if (old == tile)
      continue;
    changes.push_back(
        Json{{"x", x}, {"y", y}, {"before", old}, {"after", tile}});
    stroke.changes.push_back({static_cast<int>(*map_id), static_cast<int>(x),
                              static_cast<int>(y), old, tile});
    touched.insert({y / 2, x / 2});
  }

  std::map<Tile32, std::vector<int64_t>> known;
  bool inventory_complete = false;
  if (target.contains("tile32_inventory")) {
    const Json& inventory = target.at("tile32_inventory");
    if (!Keys(inventory, {"complete"}, {"entries", "capacity"}) ||
        !inventory.at("complete").is_boolean()) {
      return Malformed("tile32_inventory requires a boolean complete field");
    }
    inventory_complete = inventory.at("complete").get<bool>();
    if (inventory_complete) {
      return absl::UnimplementedError(
          "Lyra proposal refused: complete Tile32 inventory capacity checks "
          "are unsupported");
    }
    if (inventory.contains("capacity") &&
        !Integer(inventory.at("capacity"), 1, kMaxInteger)) {
      return Malformed("Tile32 capacity must be an integer >= 1");
    }
    const Json entries =
        inventory.contains("entries") ? inventory.at("entries") : Json::array();
    if (!entries.is_array() || entries.size() > kMaxListEntries)
      return Malformed("Tile32 entries must be a bounded list");
    std::set<int64_t> ids;
    for (const auto& entry : entries) {
      if (!Keys(entry, {"id", "tiles16"}))
        return Malformed("Tile32 entry requires exactly id and tiles16");
      const auto id = Integer(entry.at("id"), 0, kMaxInteger);
      if (!id || !ids.insert(*id).second)
        return Malformed("Tile32 inventory IDs must be unique integers");
      const Json& values = entry.at("tiles16");
      if (!values.is_array() || values.size() != 4)
        return Malformed("Tile32 entry needs four Tile16 values");
      Tile32 tuple{};
      for (size_t i = 0; i < tuple.size(); ++i) {
        const auto tile = Integer(values[i], 0, max_tile);
        if (!tile)
          return Malformed("Tile32 entry Tile16 is outside the definitions");
        tuple[i] = static_cast<uint16_t>(*tile);
      }
      known[tuple].push_back(*id);
    }
  }

  // Tile32 tuples are ordered top-left, top-right, bottom-left, bottom-right.
  Json blocks = Json::array();
  std::set<Tile32> missing;
  for (const auto& [by, bx] : touched) {
    Tile32 old{};
    Tile32 now{};
    for (int i = 0; i < 4; ++i) {
      const int64_t x = bx * 2 + i % 2;
      const int64_t y = by * 2 + i / 2;
      old[i] = before[y * kMapSize + x];
      const auto write = writes.find({y, x});
      now[i] = write != writes.end() ? write->second : old[i];
    }
    std::vector<int64_t> reuse;
    if (const auto found = known.find(now); found != known.end())
      reuse = found->second;
    std::sort(reuse.begin(), reuse.end());
    if (reuse.empty())
      missing.insert(now);
    blocks.push_back(Json{{"x", bx},
                          {"y", by},
                          {"before", old},
                          {"after", now},
                          {"existing_ids", reuse}});
  }

  const Json request = {{"prefab", footprint}, {"placements", placements}};
  const Json expected = {{"schema_version", 1},
                         {"kind", "read_only_tile16_proposal"},
                         {"writes_enabled", false},
                         {"units", "tile16"},
                         {"binding", target.at("binding")},
                         {"semantic_id", footprint.at("semantic_id")},
                         {"content_id", footprint.at("content_id")},
                         {"target_digest", Digest(target)},
                         {"request_digest", Digest(request)},
                         {"placements", placements},
                         {"occupied", occupied},
                         {"changes", changes},
                         {"touched_tile32", blocks},
                         {"capacity",
                          {{"status", "unknown"},
                           {"unseen_tuples", missing.size()},
                           {"inventory_complete", inventory_complete}}},
                         {"unresolved_behavior", notes},
                         {"acceptance", kAcceptance}};
  if (report.size() != expected.size())
    return Malformed("report fields differ from the planner output");
  for (const auto& item : expected.items()) {
    if (!report.contains(item.key()) ||
        !SameCanonical(report.at(item.key()), item.value())) {
      return Malformed(absl::StrCat("report.", item.key(),
                                    " differs from the recomputed plan"));
    }
  }
  if (stroke.empty()) {
    return absl::FailedPreconditionError(
        "Lyra proposal changes no cells on the selected map");
  }
  return stroke;
}

}  // namespace

absl::StatusOr<LyraTerrainStroke> AdmitLyraTerrainProposal(
    std::string_view bundle_json, const LyraTerrainDocument& document,
    const LyraTerrainProposalBinding& actual_binding) {
  if (document.stroke_active()) {
    return absl::FailedPreconditionError(
        "Finish the active Lyra stroke before admitting a proposal");
  }
  try {
    return Admit(bundle_json, document, actual_binding);
  } catch (const Json::exception& error) {
    return Malformed(error.what());
  }
}

}  // namespace yaze::core
