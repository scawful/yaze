#include "core/lyra_terrain_document.h"

#include <algorithm>
#include <memory>
#include <set>
#include <utility>

#include "nlohmann/json.hpp"

namespace yaze::core {
namespace {
using Json = nlohmann::json;

bool HasKeys(const Json& value, std::initializer_list<const char*> keys) {
  if (!value.is_object() || value.size() != keys.size())
    return false;
  for (const auto* key : keys) {
    if (!value.contains(key))
      return false;
  }
  return true;
}

bool IntegerIn(const Json& value, int low, int high) {
  return value.is_number_integer() && value >= low && value <= high;
}

bool Fingerprint(const std::string& value) {
  return !value.empty() && value.size() <= 256 &&
         std::none_of(value.begin(), value.end(),
                      [](unsigned char c) { return c <= 0x20 || c == 0x7F; });
}

bool Coordinates(int map_id, int x, int y) {
  return map_id >= LyraTerrainDocument::kFirstMap &&
         map_id <
             LyraTerrainDocument::kFirstMap + LyraTerrainDocument::kMapCount &&
         x >= 0 && x < LyraTerrainDocument::kMapSize && y >= 0 &&
         y < LyraTerrainDocument::kMapSize;
}

size_t CellKey(int map_id, int x, int y) {
  return (map_id - LyraTerrainDocument::kFirstMap) *
             LyraTerrainDocument::kMapSize * LyraTerrainDocument::kMapSize +
         y * LyraTerrainDocument::kMapSize + x;
}

Json ProfileJson(const LyraTerrainProfile& profile) {
  return {
      {"graphics_sheets", profile.graphics_sheets},
      {"main_palette", profile.main_palette},
      {"palette_set", profile.palette_set},
      {"animated_sheet", profile.animated_sheet},
      {"tile_count", profile.tile_count},
      {"tile16_definition_fingerprint", profile.tile16_definition_fingerprint},
      {"graphics_fingerprint", profile.graphics_fingerprint},
      {"palette_fingerprint", profile.palette_fingerprint},
      {"animation_fingerprint", profile.animation_fingerprint}};
}
}  // namespace

absl::Status LyraTerrainProfile::Validate() const {
  if (tile_count == 0 || tile_count > 65536 ||
      !Fingerprint(tile16_definition_fingerprint) ||
      !Fingerprint(graphics_fingerprint) || !Fingerprint(palette_fingerprint) ||
      !Fingerprint(animation_fingerprint) || animated_sheet == 0xFF ||
      std::find(graphics_sheets.begin(), graphics_sheets.end(), 0xFF) !=
          graphics_sheets.end()) {
    return absl::InvalidArgumentError(
        "Lyra terrain requires bound Tile16, graphics, palette and animation "
        "identities, a valid tile count and explicit graphics sheets");
  }
  return absl::OkStatus();
}

absl::StatusOr<LyraTerrainDocument> LyraTerrainDocument::Create(
    const LyraTerrainProfile& profile, uint16_t fill_tile) {
  auto status = profile.Validate();
  if (!status.ok())
    return status;
  if (fill_tile >= profile.tile_count) {
    return absl::InvalidArgumentError(
        "Lyra fill tile is outside the bound atlas");
  }
  auto document = std::unique_ptr<LyraTerrainDocument>(new LyraTerrainDocument);
  document->profile_ = profile;
  for (auto& map : document->maps_)
    map.fill(fill_tile);
  document->MarkSaved();
  return std::move(*document);
}

absl::StatusOr<LyraTerrainDocument> LyraTerrainDocument::Parse(
    std::string_view text, const LyraTerrainProfile& expected_profile) {
  auto created = Create(expected_profile);
  if (!created.ok())
    return created.status();
  if (text.size() > 2 * 1024 * 1024) {
    return absl::InvalidArgumentError("Lyra terrain document exceeds 2 MiB");
  }
  try {
    // Reject repeated object keys instead of silently accepting the last value.
    bool duplicate_key = false;
    std::vector<std::set<std::string>> object_keys;
    auto json =
        Json::parse(text, [&](int, Json::parse_event_t event, Json& value) {
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
    if (duplicate_key ||
        !HasKeys(json,
                 {"schema", "schema_version", "columns", "rows", "map_width",
                  "map_height", "tile_pixels", "profile", "maps"}) ||
        json["schema"] != "yaze.lyra-terrain" ||
        !IntegerIn(json["schema_version"], kSchemaVersion, kSchemaVersion) ||
        !IntegerIn(json["columns"], kColumns, kColumns) ||
        !IntegerIn(json["rows"], kRows, kRows) ||
        !IntegerIn(json["map_width"], kMapSize, kMapSize) ||
        !IntegerIn(json["map_height"], kMapSize, kMapSize) ||
        !IntegerIn(json["tile_pixels"], kTilePixels, kTilePixels)) {
      return absl::InvalidArgumentError(
          "Lyra terrain requires schema 1, an 8x4 map grid and 32x32 Tile16 "
          "maps");
    }
    // Exact JSON equality alone treats 1.0 and 1 as equivalent, so validate
    // numeric types before comparing the complete profile binding.
    const auto& profile = json["profile"];
    if (!HasKeys(profile,
                 {"graphics_sheets", "main_palette", "palette_set",
                  "animated_sheet", "tile_count",
                  "tile16_definition_fingerprint", "graphics_fingerprint",
                  "palette_fingerprint", "animation_fingerprint"}) ||
        !profile["graphics_sheets"].is_array() ||
        profile["graphics_sheets"].size() != 8 ||
        !IntegerIn(profile["main_palette"], 0, 255) ||
        !IntegerIn(profile["palette_set"], 0, 255) ||
        !IntegerIn(profile["animated_sheet"], 0, 254) ||
        !IntegerIn(profile["tile_count"], 1, 65536)) {
      return absl::InvalidArgumentError("Malformed Lyra terrain profile");
    }
    for (const auto& sheet : profile["graphics_sheets"]) {
      if (!IntegerIn(sheet, 0, 254))
        return absl::InvalidArgumentError("Malformed Lyra graphics sheet ID");
    }
    if (profile != ProfileJson(expected_profile)) {
      return absl::FailedPreconditionError(
          "Lyra terrain profile does not match the current Tile16 atlas");
    }
    if (!json["maps"].is_array() || json["maps"].size() != kMapCount) {
      return absl::InvalidArgumentError("Lyra terrain requires all 32 maps");
    }
    auto& document = *created;
    std::array<bool, kMapCount> seen{};
    for (const auto& map : json["maps"]) {
      if (!HasKeys(map, {"map_id", "tiles16"}) ||
          !IntegerIn(map["map_id"], kFirstMap, kFirstMap + kMapCount - 1) ||
          !map["tiles16"].is_array() || map["tiles16"].size() != kMapSize) {
        return absl::InvalidArgumentError(
            "Lyra map requires an A0-BF ID and 32 Tile16 rows");
      }
      const int index = map["map_id"].get<int>() - kFirstMap;
      if (seen[index])
        return absl::InvalidArgumentError("Duplicate Lyra map ID");
      seen[index] = true;
      for (int y = 0; y < kMapSize; ++y) {
        const auto& row = map["tiles16"][y];
        if (!row.is_array() || row.size() != kMapSize) {
          return absl::InvalidArgumentError("Lyra map rows require 32 tiles");
        }
        for (int x = 0; x < kMapSize; ++x) {
          if (!IntegerIn(row[x], 0, expected_profile.tile_count - 1)) {
            return absl::InvalidArgumentError(
                "Lyra tile ID is outside the bound Tile16 atlas");
          }
          document.maps_[index][y * kMapSize + x] = row[x].get<uint16_t>();
        }
      }
    }
    document.MarkSaved();
    return std::move(document);
  } catch (const Json::exception& error) {
    return absl::InvalidArgumentError(std::string("Invalid Lyra terrain: ") +
                                      error.what());
  }
}

std::string LyraTerrainDocument::Serialize() const {
  Json json = {
      {"schema", "yaze.lyra-terrain"}, {"schema_version", kSchemaVersion},
      {"columns", kColumns},           {"rows", kRows},
      {"map_width", kMapSize},         {"map_height", kMapSize},
      {"tile_pixels", kTilePixels},    {"profile", ProfileJson(profile_)},
      {"maps", Json::array()}};
  for (int index = 0; index < kMapCount; ++index) {
    Json rows = Json::array();
    for (int y = 0; y < kMapSize; ++y) {
      const auto first = maps_[index].begin() + y * kMapSize;
      rows.push_back(std::vector<uint16_t>(first, first + kMapSize));
    }
    json["maps"].push_back(
        {{"map_id", kFirstMap + index}, {"tiles16", std::move(rows)}});
  }
  return json.dump(2) + "\n";
}

absl::StatusOr<uint16_t> LyraTerrainDocument::tile(int global_x,
                                                   int global_y) const {
  if (global_x < 0 || global_x >= kWidth || global_y < 0 ||
      global_y >= kHeight) {
    return absl::OutOfRangeError("Lyra canvas coordinate is outside 256x128");
  }
  return MapTile(
      kFirstMap + (global_y / kMapSize) * kColumns + global_x / kMapSize,
      global_x % kMapSize, global_y % kMapSize);
}

absl::StatusOr<uint16_t> LyraTerrainDocument::MapTile(int map_id, int x,
                                                      int y) const {
  if (!Coordinates(map_id, x, y))
    return absl::OutOfRangeError("Lyra map coordinate is outside A0-BF/32x32");
  return maps_[map_id - kFirstMap][y * kMapSize + x];
}

absl::Status LyraTerrainDocument::BeginStroke() {
  if (stroke_active_)
    return absl::FailedPreconditionError("A Lyra stroke is already active");
  stroke_active_ = true;
  active_stroke_.changes.clear();
  active_cells_.clear();
  return absl::OkStatus();
}

absl::Status LyraTerrainDocument::Paint(int global_x, int global_y,
                                        uint16_t tile_id) {
  if (!stroke_active_)
    return absl::FailedPreconditionError("Begin a Lyra stroke before painting");
  auto current = tile(global_x, global_y);
  if (!current.ok())
    return current.status();
  if (tile_id >= profile_.tile_count)
    return absl::OutOfRangeError("Lyra tile ID is outside the bound atlas");
  if (*current == tile_id)
    return absl::OkStatus();
  const int map_id =
      kFirstMap + (global_y / kMapSize) * kColumns + global_x / kMapSize;
  const int x = global_x % kMapSize;
  const int y = global_y % kMapSize;
  const auto [entry, inserted] = active_cells_.emplace(
      CellKey(map_id, x, y), active_stroke_.changes.size());
  if (inserted) {
    active_stroke_.changes.push_back({map_id, x, y, *current, tile_id});
  } else {
    active_stroke_.changes[entry->second].after = tile_id;
  }
  maps_[map_id - kFirstMap][y * kMapSize + x] = tile_id;
  return absl::OkStatus();
}

absl::StatusOr<LyraTerrainStroke> LyraTerrainDocument::EndStroke() {
  if (!stroke_active_)
    return absl::FailedPreconditionError("No Lyra stroke is active");
  stroke_active_ = false;
  active_cells_.clear();
  auto result = std::move(active_stroke_);
  std::erase_if(result.changes,
                [](const auto& cell) { return cell.before == cell.after; });
  return result;
}

void LyraTerrainDocument::CancelStroke() {
  if (!stroke_active_)
    return;
  for (const auto& cell : active_stroke_.changes) {
    maps_[cell.map_id - kFirstMap][cell.y * kMapSize + cell.x] = cell.before;
  }
  active_stroke_.changes.clear();
  active_cells_.clear();
  stroke_active_ = false;
}

absl::Status LyraTerrainDocument::ApplyStroke(const LyraTerrainStroke& stroke,
                                              bool undo) {
  if (stroke_active_)
    return absl::FailedPreconditionError("Finish the active Lyra stroke first");
  std::set<size_t> seen;
  for (const auto& cell : stroke.changes) {
    if (!Coordinates(cell.map_id, cell.x, cell.y) ||
        cell.before >= profile_.tile_count ||
        cell.after >= profile_.tile_count ||
        !seen.insert(CellKey(cell.map_id, cell.x, cell.y)).second) {
      return absl::InvalidArgumentError("Malformed Lyra stroke history");
    }
    const auto current =
        maps_[cell.map_id - kFirstMap][cell.y * kMapSize + cell.x];
    if (current != (undo ? cell.after : cell.before)) {
      return absl::FailedPreconditionError("Lyra stroke history is stale");
    }
  }
  for (const auto& cell : stroke.changes) {
    maps_[cell.map_id - kFirstMap][cell.y * kMapSize + cell.x] =
        undo ? cell.before : cell.after;
  }
  return absl::OkStatus();
}

}  // namespace yaze::core
