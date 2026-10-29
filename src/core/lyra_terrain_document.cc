#include "core/lyra_terrain_document.h"

#include <algorithm>
#include <cstdlib>
#include <memory>
#include <set>
#include <unordered_set>
#include <utility>

#include "nlohmann/json.hpp"
#include "util/macro.h"

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

bool IntegerIn(const Json& value, int64_t low, int64_t high) {
  if (value.is_number_unsigned()) {
    const auto number = value.get<uint64_t>();
    return high >= 0 && number <= static_cast<uint64_t>(high) &&
           (low <= 0 || number >= static_cast<uint64_t>(low));
  }
  if (!value.is_number_integer())
    return false;
  const auto number = value.get<int64_t>();
  return number >= low && number <= high;
}

bool Fingerprint(const std::string& value) {
  return !value.empty() && value.size() <= 256 &&
         std::none_of(value.begin(), value.end(),
                      [](unsigned char c) { return c <= 0x20 || c == 0x7F; });
}

bool ValidFlightName(const std::string& value) {
  return !value.empty() && value.size() <= 80 &&
         std::none_of(value.begin(), value.end(),
                      [](unsigned char c) { return c < 0x20 || c == 0x7F; });
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

Json PropertiesJson(const LyraMapProperties& p) {
  Json result = {{"graphics_sheets", p.graphics_sheets},
                 {"main_palette", p.main_palette},
                 {"palette_set", p.palette_set},
                 {"animated_sheet", p.animated_sheet},
                 {"sprite_graphics", p.sprite_graphics},
                 {"sprite_palettes", p.sprite_palettes},
                 {"music", p.music},
                 {"message_id", p.message_id}};
  if (p.background_palette)
    result["background_palette"] = *p.background_palette;
  return result;
}

Json SpriteJson(const LyraSpritePlacement& s) {
  return {{"id", s.id},
          {"sprite_id", s.sprite_id},
          {"x", s.x},
          {"y", s.y},
          {"state", s.state}};
}

Json DoorJson(const LyraDoorPlacement& d) {
  Json j = {{"id", d.id}, {"x", d.x}, {"y", d.y}, {"entrance_id", nullptr}};
  if (d.entrance_id.has_value()) {
    j["entrance_id"] = *d.entrance_id;
  }
  return j;
}

Json ExitJson(const LyraExitPlacement& e) {
  Json j = {{"id", e.id},
            {"room_id", e.room_id},
            {"x", e.x},
            {"y", e.y},
            {"exit_row", nullptr}};
  if (e.exit_row.has_value()) {
    j["exit_row"] = *e.exit_row;
  }
  return j;
}

Json AuthoringJson(const LyraMapAuthoring& a) {
  Json sprites = Json::array();
  for (const auto& s : a.sprites)
    sprites.push_back(SpriteJson(s));
  Json doors = Json::array();
  for (const auto& d : a.doors)
    doors.push_back(DoorJson(d));
  Json exits = Json::array();
  for (const auto& e : a.exits)
    exits.push_back(ExitJson(e));
  Json bindings = Json::array();
  for (const auto& b : a.asset_bindings)
    bindings.push_back(b);

  return {{"properties", PropertiesJson(a.properties)},
          {"sprites", std::move(sprites)},
          {"doors", std::move(doors)},
          {"exits", std::move(exits)},
          {"asset_bindings", std::move(bindings)}};
}

Json FlightPointJson(const LyraFlightPoint& f) {
  Json j = {{"id", f.id}, {"map_id", f.map_id}, {"x", f.x},
            {"y", f.y},   {"name", f.name},     {"runtime_slot", nullptr}};
  if (f.runtime_slot.has_value()) {
    j["runtime_slot"] = *f.runtime_slot;
  }
  return j;
}

Json EdgeConnectionJson(const LyraEdgeConnection& c) {
  Json j = {{"id", c.id},
            {"map_a", c.map_a},
            {"map_b", c.map_b},
            {"passage", nullptr}};
  if (c.passage.has_value()) {
    j["passage"] = {{"first_tile", c.passage->first_tile},
                    {"last_tile", c.passage->last_tile}};
  }
  return j;
}

const char* GameplayAreaSizeName(LyraGameplayAreaSize size) {
  switch (size) {
    case LyraGameplayAreaSize::kSmall:
      return "small";
    case LyraGameplayAreaSize::kWide:
      return "wide";
    case LyraGameplayAreaSize::kTall:
      return "tall";
    case LyraGameplayAreaSize::kLarge:
      return "large";
  }
  return "";
}

std::optional<LyraGameplayAreaSize> ParseGameplayAreaSize(const Json& value) {
  if (!value.is_string())
    return std::nullopt;
  if (value == "small")
    return LyraGameplayAreaSize::kSmall;
  if (value == "wide")
    return LyraGameplayAreaSize::kWide;
  if (value == "tall")
    return LyraGameplayAreaSize::kTall;
  if (value == "large")
    return LyraGameplayAreaSize::kLarge;
  return std::nullopt;
}

bool EdgeEndpoint(int map_id) {
  return (map_id >= 0x98 && map_id <= 0x9B) ||
         (map_id >= LyraTerrainDocument::kFirstMap &&
          map_id <
              LyraTerrainDocument::kFirstMap + LyraTerrainDocument::kMapCount);
}

absl::Status ValidateAuthoringState(const LyraAuthoringState& state) {
  std::unordered_set<uint32_t> sprite_ids;
  std::unordered_set<uint32_t> door_ids;
  std::unordered_set<uint32_t> exit_ids;
  std::unordered_set<uint32_t> flight_ids;
  std::unordered_set<uint32_t> edge_ids;
  std::set<std::pair<int, int>> edge_pairs;

  std::unordered_set<int> owned_maps;
  for (const auto& area : state.gameplay_areas) {
    const auto footprint =
        LyraGameplayAreaFootprint(area.parent_map_id, area.size);
    if (!footprint.ok())
      return footprint.status();
    if (area.maps != *footprint) {
      return absl::InvalidArgumentError(
          "Gameplay area maps must match its exact ordered footprint");
    }
    for (const int map_id : area.maps) {
      if (!owned_maps.insert(map_id).second) {
        return absl::InvalidArgumentError(
            "Gameplay areas cannot share ownership of a map");
      }
    }
  }

  const LyraMapProperties default_props;

  for (size_t map_idx = 0; map_idx < state.maps.size(); ++map_idx) {
    const auto& map_auth = state.maps[map_idx];
    const auto& props = map_auth.properties;

    // Validate properties
    if (props.background_palette) {
      for (const auto color : *props.background_palette) {
        if (color > 0x7FFF)
          return absl::InvalidArgumentError(
              "Lyra background palette colors require BGR555 values 0..32767");
      }
    }
    for (uint8_t sheet : props.graphics_sheets) {
      (void)sheet;  // uint8_t is 0..255
    }
    for (int g : props.sprite_graphics) {
      if (g < -1 || g > 255) {
        return absl::InvalidArgumentError(
            "Sprite graphics out of range -1..255");
      }
    }
    for (int p : props.sprite_palettes) {
      if (p < -1 || p > 255) {
        return absl::InvalidArgumentError(
            "Sprite palettes out of range -1..255");
      }
    }

    // asset_bindings: either all nonempty valid fingerprints OR all empty AND properties exactly default
    bool all_empty = true;
    bool all_valid = true;
    for (const auto& b : map_auth.asset_bindings) {
      if (!b.empty())
        all_empty = false;
      if (!Fingerprint(b))
        all_valid = false;
    }

    if (all_empty) {
      if (props != default_props) {
        return absl::InvalidArgumentError(
            "Empty asset bindings require exactly default map properties");
      }
    } else if (!all_valid) {
      return absl::InvalidArgumentError(
          "Asset bindings must be all valid fingerprints or all empty");
    }

    // sprites
    if (map_auth.sprites.size() > 128) {
      return absl::InvalidArgumentError("Max 128 sprites per map exceeded");
    }
    for (const auto& s : map_auth.sprites) {
      if (s.id == 0) {
        return absl::InvalidArgumentError("Sprite ID must be nonzero");
      }
      if (!sprite_ids.insert(s.id).second) {
        return absl::InvalidArgumentError(
            "Duplicate sprite ID across document");
      }
      if (s.x < 0 || s.x > 31 || s.y < 0 || s.y > 31) {
        return absl::InvalidArgumentError(
            "Sprite coordinates out of range 0..31");
      }
      if (s.state < 0 || s.state > 2) {
        return absl::InvalidArgumentError("Sprite state out of range 0..2");
      }
    }

    // doors
    if (map_auth.doors.size() > 64) {
      return absl::InvalidArgumentError("Max 64 doors per map exceeded");
    }
    std::set<std::pair<int, int>> door_positions;
    for (const auto& d : map_auth.doors) {
      if (d.id == 0) {
        return absl::InvalidArgumentError("Door ID must be nonzero");
      }
      if (!door_ids.insert(d.id).second) {
        return absl::InvalidArgumentError("Duplicate door ID across document");
      }
      if (d.x < 0 || d.x > 31 || d.y < 0 || d.y > 31) {
        return absl::InvalidArgumentError(
            "Door coordinates out of range 0..31");
      }
      if (!door_positions.insert({d.x, d.y}).second) {
        return absl::InvalidArgumentError(
            "Duplicate door position at same (map, x, y)");
      }
      if (d.entrance_id.has_value()) {
        if (*d.entrance_id < 0 || *d.entrance_id > 0xFE) {
          return absl::InvalidArgumentError(
              "Door entrance_id out of range 0..0xFE");
        }
      }
    }

    // exits
    if (map_auth.exits.size() > 64) {
      return absl::InvalidArgumentError("Max 64 exits per map exceeded");
    }
    for (const auto& e : map_auth.exits) {
      if (e.id == 0) {
        return absl::InvalidArgumentError("Exit ID must be nonzero");
      }
      if (!exit_ids.insert(e.id).second) {
        return absl::InvalidArgumentError("Duplicate exit ID across document");
      }
      if (e.room_id < 0 || e.room_id > 0x127) {
        return absl::InvalidArgumentError(
            "Exit room_id out of range 0..0x127 (295)");
      }
      if (e.x < 0 || e.x > 31 || e.y < 0 || e.y > 31) {
        return absl::InvalidArgumentError(
            "Exit coordinates out of range 0..31");
      }
      if (e.exit_row.has_value()) {
        if (*e.exit_row < 0 || *e.exit_row > 78) {
          return absl::InvalidArgumentError("Exit exit_row out of range 0..78");
        }
      }
    }
  }

  // flight points
  if (state.flight_points.size() > 64) {
    return absl::InvalidArgumentError("Max 64 flight points exceeded");
  }
  for (const auto& f : state.flight_points) {
    if (f.id == 0) {
      return absl::InvalidArgumentError("Flight point ID must be nonzero");
    }
    if (!flight_ids.insert(f.id).second) {
      return absl::InvalidArgumentError(
          "Duplicate flight point ID across document");
    }
    if (f.map_id < 0 || f.map_id > 0xBF) {
      return absl::InvalidArgumentError(
          "Flight point map_id out of range 0..0xBF");
    }
    if (f.x < 0 || f.x > 31 || f.y < 0 || f.y > 31) {
      return absl::InvalidArgumentError(
          "Flight point coordinates out of range 0..31");
    }
    if (!ValidFlightName(f.name)) {
      return absl::InvalidArgumentError("Flight point name invalid");
    }
    if (f.runtime_slot.has_value()) {
      return absl::InvalidArgumentError(
          "Flight point runtime_slot must be null");
    }
  }

  // edge connections
  if (state.edge_connections.size() >
      LyraTerrainDocument::kMaxEdgeConnections) {
    return absl::InvalidArgumentError("Max 64 edge connections exceeded");
  }
  for (const auto& c : state.edge_connections) {
    if (c.id == 0) {
      return absl::InvalidArgumentError("Edge connection ID must be nonzero");
    }
    if (!edge_ids.insert(c.id).second) {
      return absl::InvalidArgumentError("Duplicate edge connection ID");
    }
    if (!LyraEdgeMapsAdjacent(c.map_a, c.map_b)) {
      return absl::InvalidArgumentError(
          "Edge connection maps are not adjacent Lyra edge maps");
    }
    if (c.map_a > c.map_b) {
      return absl::InvalidArgumentError(
          "Edge connection must store map_a < map_b");
    }
    if (!edge_pairs.insert({c.map_a, c.map_b}).second) {
      return absl::InvalidArgumentError("Duplicate edge connection map pair");
    }
    if (c.passage.has_value() &&
        (c.passage->first_tile < 0 ||
         c.passage->last_tile >= LyraTerrainDocument::kMapSize ||
         c.passage->first_tile > c.passage->last_tile)) {
      return absl::InvalidArgumentError(
          "Edge passage requires 0 <= first_tile <= last_tile <= 31");
    }
  }

  return absl::OkStatus();
}

}  // namespace

bool LyraEdgeMapsAdjacent(int map_a, int map_b) {
  if (!EdgeEndpoint(map_a) || !EdgeEndpoint(map_b) ||
      std::max(map_a, map_b) < LyraTerrainDocument::kFirstMap) {
    return false;
  }
  const int difference = std::abs(map_a - map_b);
  return difference == LyraTerrainDocument::kColumns ||
         (difference == 1 && map_a / LyraTerrainDocument::kColumns ==
                                 map_b / LyraTerrainDocument::kColumns);
}

absl::StatusOr<std::vector<int>> LyraGameplayAreaFootprint(
    int parent_map_id, LyraGameplayAreaSize size) {
  if (parent_map_id < LyraTerrainDocument::kFirstMap ||
      parent_map_id >=
          LyraTerrainDocument::kFirstMap + LyraTerrainDocument::kMapCount) {
    return absl::InvalidArgumentError("Gameplay area parent must be A0-BF");
  }
  int width = 1;
  int height = 1;
  switch (size) {
    case LyraGameplayAreaSize::kSmall:
      break;
    case LyraGameplayAreaSize::kWide:
      width = 2;
      break;
    case LyraGameplayAreaSize::kTall:
      height = 2;
      break;
    case LyraGameplayAreaSize::kLarge:
      width = 2;
      height = 2;
      break;
    default:
      return absl::InvalidArgumentError("Unknown gameplay area size");
  }
  const int offset = parent_map_id - LyraTerrainDocument::kFirstMap;
  if (offset % LyraTerrainDocument::kColumns + width >
          LyraTerrainDocument::kColumns ||
      offset / LyraTerrainDocument::kColumns + height >
          LyraTerrainDocument::kRows) {
    return absl::InvalidArgumentError(
        "Gameplay area footprint leaves A0-BF grid");
  }
  std::vector<int> maps;
  for (int row = 0; row < height; ++row) {
    for (int column = 0; column < width; ++column) {
      maps.push_back(parent_map_id + row * LyraTerrainDocument::kColumns +
                     column);
    }
  }
  return maps;
}

const LyraGameplayArea* LyraTerrainDocument::GameplayAreaForMap(
    int map_id) const {
  for (const auto& area : authoring_.gameplay_areas) {
    if (std::find(area.maps.begin(), area.maps.end(), map_id) !=
        area.maps.end())
      return &area;
  }
  return nullptr;
}

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

absl::Status LyraTerrainDocument::RebindTile16Definitions(
    const LyraTerrainProfile& before, const LyraTerrainProfile& after) {
  if (profile_ != before)
    return absl::FailedPreconditionError("Lyra Tile16 edit binding is stale");
  RETURN_IF_ERROR(after.Validate());
  auto definition_only = before;
  definition_only.tile16_definition_fingerprint =
      after.tile16_definition_fingerprint;
  if (definition_only != after)
    return absl::FailedPreconditionError(
        "Tile16 editing cannot rebind Lyra graphics, palettes or animation");
  profile_ = after;
  return absl::OkStatus();
}

absl::Status LyraTerrainDocument::SetAuthoring(
    const LyraAuthoringState& before, const LyraAuthoringState& after) {
  if (stroke_active_) {
    return absl::FailedPreconditionError(
        "Cannot update Lyra authoring while a terrain stroke is active");
  }
  if (authoring_ != before) {
    return absl::FailedPreconditionError(
        "Lyra authoring state is stale or does not match before");
  }
  if (before == after) {
    return absl::OkStatus();
  }
  auto status = ValidateAuthoringState(after);
  if (!status.ok()) {
    return status;
  }
  authoring_ = after;
  return absl::OkStatus();
}

absl::StatusOr<LyraTerrainDocument> LyraTerrainDocument::Parse(
    std::string_view text, const LyraTerrainProfile& expected_profile) {
  auto created = Create(expected_profile);
  if (!created.ok())
    return created.status();
  if (text.size() > 4 * 1024 * 1024) {
    return absl::InvalidArgumentError("Lyra terrain document exceeds 4 MiB");
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
    if (duplicate_key) {
      return absl::InvalidArgumentError("Duplicate JSON keys");
    }

    if (!json.is_object() || !json.contains("schema") ||
        json["schema"] != "yaze.lyra-terrain" ||
        !json.contains("schema_version") ||
        !IntegerIn(json["schema_version"], 1, kSchemaVersion)) {
      return absl::InvalidArgumentError("Invalid Lyra terrain schema header");
    }

    int version = json["schema_version"].get<int>();
    if (version == 1) {
      if (!HasKeys(json,
                   {"schema", "schema_version", "columns", "rows", "map_width",
                    "map_height", "tile_pixels", "profile", "maps"})) {
        return absl::InvalidArgumentError(
            "Malformed Lyra schema 1 header keys");
      }
    } else if (version == 2) {
      if (!HasKeys(json, {"schema", "schema_version", "columns", "rows",
                          "map_width", "map_height", "tile_pixels", "profile",
                          "maps", "flight_points"})) {
        return absl::InvalidArgumentError(
            "Malformed Lyra schema 2 header keys");
      }
    } else if (version == 3 || version == 4) {
      if (!HasKeys(json, {"schema", "schema_version", "columns", "rows",
                          "map_width", "map_height", "tile_pixels", "profile",
                          "maps", "flight_points", "edge_connections"})) {
        return absl::InvalidArgumentError(
            version == 3 ? "Malformed Lyra schema 3 header keys"
                         : "Malformed Lyra schema 4 header keys");
      }
    } else if (version == 5) {
      if (!HasKeys(json,
                   {"schema", "schema_version", "columns", "rows", "map_width",
                    "map_height", "tile_pixels", "profile", "maps",
                    "flight_points", "edge_connections", "gameplay_areas"})) {
        return absl::InvalidArgumentError(
            "Malformed Lyra schema 5 header keys");
      }
    } else {
      return absl::InvalidArgumentError("Unsupported Lyra schema version");
    }

    if (!IntegerIn(json["columns"], kColumns, kColumns) ||
        !IntegerIn(json["rows"], kRows, kRows) ||
        !IntegerIn(json["map_width"], kMapSize, kMapSize) ||
        !IntegerIn(json["map_height"], kMapSize, kMapSize) ||
        !IntegerIn(json["tile_pixels"], kTilePixels, kTilePixels)) {
      return absl::InvalidArgumentError(
          "Lyra terrain requires an 8x4 map grid and 32x32 Tile16 maps");
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
    bool has_background_palette = false;
    std::array<bool, kMapCount> seen{};
    for (const auto& map : json["maps"]) {
      if (version == 1) {
        if (!HasKeys(map, {"map_id", "tiles16"})) {
          return absl::InvalidArgumentError(
              "Lyra schema 1 map requires map_id and tiles16");
        }
      } else {
        if (!HasKeys(map, {"map_id", "tiles16", "authoring"})) {
          return absl::InvalidArgumentError(
              "Lyra schema 2+ map requires map_id, tiles16, and authoring");
        }
      }
      if (!IntegerIn(map["map_id"], kFirstMap, kFirstMap + kMapCount - 1) ||
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

      if (version >= 2) {
        const auto& auth = map["authoring"];
        if (!HasKeys(auth, {"properties", "sprites", "doors", "exits",
                            "asset_bindings"})) {
          return absl::InvalidArgumentError("Malformed map authoring keys");
        }
        // properties
        const auto& props = auth["properties"];
        const bool has_override = props.contains("background_palette");
        if ((has_override && version < 4) ||
            !(has_override
                  ? HasKeys(
                        props,
                        {"graphics_sheets", "main_palette", "palette_set",
                         "animated_sheet", "sprite_graphics", "sprite_palettes",
                         "music", "message_id", "background_palette"})
                  : HasKeys(props,
                            {"graphics_sheets", "main_palette", "palette_set",
                             "animated_sheet", "sprite_graphics",
                             "sprite_palettes", "music", "message_id"}))) {
          return absl::InvalidArgumentError("Malformed map properties keys");
        }
        if (!props["graphics_sheets"].is_array() ||
            props["graphics_sheets"].size() != 8 ||
            !IntegerIn(props["main_palette"], 0, 255) ||
            !IntegerIn(props["palette_set"], 0, 255) ||
            !IntegerIn(props["animated_sheet"], 0, 255) ||
            !props["sprite_graphics"].is_array() ||
            props["sprite_graphics"].size() != 3 ||
            !props["sprite_palettes"].is_array() ||
            props["sprite_palettes"].size() != 3 ||
            !props["music"].is_array() || props["music"].size() != 4 ||
            !IntegerIn(props["message_id"], 0, 65535)) {
          return absl::InvalidArgumentError("Malformed map properties values");
        }
        LyraMapProperties map_properties;
        if (has_override) {
          const auto& colors = props["background_palette"];
          if (!colors.is_array() || colors.size() != 128)
            return absl::InvalidArgumentError(
                "Lyra background palette requires exactly 128 BGR555 colors");
          std::array<uint16_t, 128> palette;
          for (size_t i = 0; i < palette.size(); ++i) {
            if (!IntegerIn(colors[i], 0, 0x7FFF))
              return absl::InvalidArgumentError(
                  "Lyra background palette colors require integers 0..32767");
            palette[i] = colors[i].get<uint16_t>();
          }
          map_properties.background_palette = palette;
          has_background_palette = true;
        }
        for (int i = 0; i < 8; ++i) {
          if (!IntegerIn(props["graphics_sheets"][i], 0, 255)) {
            return absl::InvalidArgumentError(
                "Invalid graphics sheet in properties");
          }
          map_properties.graphics_sheets[i] =
              props["graphics_sheets"][i].get<uint8_t>();
        }
        map_properties.main_palette = props["main_palette"].get<uint8_t>();
        map_properties.palette_set = props["palette_set"].get<uint8_t>();
        map_properties.animated_sheet = props["animated_sheet"].get<uint8_t>();
        for (int i = 0; i < 3; ++i) {
          if (!IntegerIn(props["sprite_graphics"][i], -1, 255)) {
            return absl::InvalidArgumentError(
                "Invalid sprite_graphics in properties");
          }
          map_properties.sprite_graphics[i] =
              props["sprite_graphics"][i].get<int>();
        }
        for (int i = 0; i < 3; ++i) {
          if (!IntegerIn(props["sprite_palettes"][i], -1, 255)) {
            return absl::InvalidArgumentError(
                "Invalid sprite_palettes in properties");
          }
          map_properties.sprite_palettes[i] =
              props["sprite_palettes"][i].get<int>();
        }
        for (int i = 0; i < 4; ++i) {
          if (!IntegerIn(props["music"][i], 0, 255)) {
            return absl::InvalidArgumentError("Invalid music in properties");
          }
          map_properties.music[i] = props["music"][i].get<uint8_t>();
        }
        map_properties.message_id = props["message_id"].get<uint16_t>();
        document.authoring_.maps[index].properties = map_properties;

        // sprites
        if (!auth["sprites"].is_array()) {
          return absl::InvalidArgumentError("sprites must be an array");
        }
        for (const auto& s : auth["sprites"]) {
          if (!HasKeys(s, {"id", "sprite_id", "x", "y", "state"}) ||
              !IntegerIn(s["id"], 1, 0xFFFFFFFFULL) ||
              !IntegerIn(s["sprite_id"], 0, 255) || !IntegerIn(s["x"], 0, 31) ||
              !IntegerIn(s["y"], 0, 31) || !IntegerIn(s["state"], 0, 2)) {
            return absl::InvalidArgumentError("Malformed sprite placement");
          }
          document.authoring_.maps[index].sprites.push_back(
              {s["id"].get<uint32_t>(), s["sprite_id"].get<uint8_t>(),
               s["x"].get<int>(), s["y"].get<int>(), s["state"].get<int>()});
        }

        // doors
        if (!auth["doors"].is_array()) {
          return absl::InvalidArgumentError("doors must be an array");
        }
        for (const auto& d : auth["doors"]) {
          if (!HasKeys(d, {"id", "x", "y", "entrance_id"}) ||
              !IntegerIn(d["id"], 1, 0xFFFFFFFFULL) ||
              !IntegerIn(d["x"], 0, 31) || !IntegerIn(d["y"], 0, 31)) {
            return absl::InvalidArgumentError("Malformed door placement");
          }
          std::optional<int> entrance_id;
          if (!d["entrance_id"].is_null()) {
            if (!IntegerIn(d["entrance_id"], 0, 0xFE)) {
              return absl::InvalidArgumentError("Invalid entrance_id");
            }
            entrance_id = d["entrance_id"].get<int>();
          }
          document.authoring_.maps[index].doors.push_back(
              {d["id"].get<uint32_t>(), d["x"].get<int>(), d["y"].get<int>(),
               entrance_id});
        }

        // exits
        if (!auth["exits"].is_array()) {
          return absl::InvalidArgumentError("exits must be an array");
        }
        for (const auto& e : auth["exits"]) {
          if (!HasKeys(e, {"id", "room_id", "x", "y", "exit_row"}) ||
              !IntegerIn(e["id"], 1, 0xFFFFFFFFULL) ||
              !IntegerIn(e["room_id"], 0, 0x127) || !IntegerIn(e["x"], 0, 31) ||
              !IntegerIn(e["y"], 0, 31)) {
            return absl::InvalidArgumentError("Malformed exit placement");
          }
          std::optional<int> exit_row;
          if (!e["exit_row"].is_null()) {
            if (!IntegerIn(e["exit_row"], 0, 78)) {
              return absl::InvalidArgumentError("Invalid exit_row");
            }
            exit_row = e["exit_row"].get<int>();
          }
          document.authoring_.maps[index].exits.push_back(
              {e["id"].get<uint32_t>(), e["room_id"].get<int>(),
               e["x"].get<int>(), e["y"].get<int>(), exit_row});
        }

        // asset_bindings
        if (!auth["asset_bindings"].is_array() ||
            auth["asset_bindings"].size() != 3) {
          return absl::InvalidArgumentError(
              "asset_bindings must be an array of 3 strings");
        }
        for (int i = 0; i < 3; ++i) {
          if (!auth["asset_bindings"][i].is_string()) {
            return absl::InvalidArgumentError("asset_bindings must be strings");
          }
          document.authoring_.maps[index].asset_bindings[i] =
              auth["asset_bindings"][i].get<std::string>();
        }
      }
    }

    if (version >= 2) {
      if (!json["flight_points"].is_array()) {
        return absl::InvalidArgumentError("flight_points must be an array");
      }
      for (const auto& f : json["flight_points"]) {
        if (!HasKeys(f, {"id", "map_id", "x", "y", "name", "runtime_slot"}) ||
            !IntegerIn(f["id"], 1, 0xFFFFFFFFULL) ||
            !IntegerIn(f["map_id"], 0, 0xBF) || !IntegerIn(f["x"], 0, 31) ||
            !IntegerIn(f["y"], 0, 31) || !f["name"].is_string() ||
            !f["runtime_slot"].is_null()) {
          return absl::InvalidArgumentError("Malformed flight point");
        }
        document.authoring_.flight_points.push_back(
            {f["id"].get<uint32_t>(), f["map_id"].get<int>(), f["x"].get<int>(),
             f["y"].get<int>(), f["name"].get<std::string>(), std::nullopt});
      }
    }

    if (version >= 3) {
      const auto& edges = json["edge_connections"];
      if (!edges.is_array()) {
        return absl::InvalidArgumentError("edge_connections must be an array");
      }
      if (edges.size() > kMaxEdgeConnections) {
        return absl::InvalidArgumentError("Max 64 edge connections exceeded");
      }
      for (const auto& c : edges) {
        // Adjacency and order are checked by ValidateAuthoringState below.
        if (!HasKeys(c, {"id", "map_a", "map_b", "passage"}) ||
            !IntegerIn(c["id"], 1, 0xFFFFFFFFULL) ||
            !IntegerIn(c["map_a"], 0, 0xFF) ||
            !IntegerIn(c["map_b"], 0, 0xFF)) {
          return absl::InvalidArgumentError("Malformed edge connection");
        }
        std::optional<LyraEdgePassage> passage;
        if (!c["passage"].is_null()) {
          const auto& p = c["passage"];
          if (!HasKeys(p, {"first_tile", "last_tile"}) ||
              !IntegerIn(p["first_tile"], 0, kMapSize - 1) ||
              !IntegerIn(p["last_tile"], 0, kMapSize - 1)) {
            return absl::InvalidArgumentError("Malformed edge passage");
          }
          passage = LyraEdgePassage{p["first_tile"].get<int>(),
                                    p["last_tile"].get<int>()};
        }
        document.authoring_.edge_connections.push_back(
            {c["id"].get<uint32_t>(), c["map_a"].get<int>(),
             c["map_b"].get<int>(), passage});
      }
    }

    if (version == 5) {
      const auto& areas = json["gameplay_areas"];
      if (!areas.is_array() || areas.empty() || areas.size() > kMapCount) {
        return absl::InvalidArgumentError(
            "gameplay_areas requires 1..32 explicit areas");
      }
      for (const auto& area : areas) {
        if (!HasKeys(area, {"parent_map_id", "size", "maps"}) ||
            !IntegerIn(area["parent_map_id"], kFirstMap,
                       kFirstMap + kMapCount - 1) ||
            !area["maps"].is_array() || area["maps"].empty() ||
            area["maps"].size() > 4) {
          return absl::InvalidArgumentError("Malformed gameplay area");
        }
        const auto size = ParseGameplayAreaSize(area["size"]);
        if (!size.has_value())
          return absl::InvalidArgumentError("Unknown gameplay area size");
        LyraGameplayArea parsed{area["parent_map_id"].get<int>(), *size, {}};
        for (const auto& map_id : area["maps"]) {
          if (!IntegerIn(map_id, kFirstMap, kFirstMap + kMapCount - 1))
            return absl::InvalidArgumentError(
                "Gameplay area map must be A0-BF");
          parsed.maps.push_back(map_id.get<int>());
        }
        document.authoring_.gameplay_areas.push_back(std::move(parsed));
      }
    }

    if (version == 4 && !has_background_palette)
      return absl::InvalidArgumentError(
          "Lyra schema 4 requires a map background palette override");
    auto auth_status = ValidateAuthoringState(document.authoring_);
    if (!auth_status.ok()) {
      return auth_status;
    }

    document.MarkSaved();
    return std::move(document);
  } catch (const Json::exception& error) {
    return absl::InvalidArgumentError(std::string("Invalid Lyra terrain: ") +
                                      error.what());
  }
}

std::string LyraTerrainDocument::Serialize() const {
  bool has_background_palette = false;
  for (const auto& map : authoring_.maps)
    has_background_palette |= map.properties.background_palette.has_value();
  Json json = {{"schema", "yaze.lyra-terrain"},
               {"schema_version", !authoring_.gameplay_areas.empty()
                                      ? 5
                                      : (has_background_palette ? 4 : 3)},
               {"columns", kColumns},
               {"rows", kRows},
               {"map_width", kMapSize},
               {"map_height", kMapSize},
               {"tile_pixels", kTilePixels},
               {"profile", ProfileJson(profile_)},
               {"maps", Json::array()},
               {"flight_points", Json::array()},
               {"edge_connections", Json::array()}};
  for (int index = 0; index < kMapCount; ++index) {
    Json rows = Json::array();
    for (int y = 0; y < kMapSize; ++y) {
      const auto first = maps_[index].begin() + y * kMapSize;
      rows.push_back(std::vector<uint16_t>(first, first + kMapSize));
    }
    json["maps"].push_back(
        {{"map_id", kFirstMap + index},
         {"tiles16", std::move(rows)},
         {"authoring", AuthoringJson(authoring_.maps[index])}});
  }
  for (const auto& f : authoring_.flight_points) {
    json["flight_points"].push_back(FlightPointJson(f));
  }
  for (const auto& c : authoring_.edge_connections) {
    json["edge_connections"].push_back(EdgeConnectionJson(c));
  }
  if (!authoring_.gameplay_areas.empty()) {
    json["gameplay_areas"] = Json::array();
    for (const auto& area : authoring_.gameplay_areas) {
      json["gameplay_areas"].push_back(
          {{"parent_map_id", area.parent_map_id},
           {"size", GameplayAreaSizeName(area.size)},
           {"maps", area.maps}});
    }
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
