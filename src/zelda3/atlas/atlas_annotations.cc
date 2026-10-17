#include "zelda3/atlas/atlas_annotations.h"

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <initializer_list>
#include <sstream>
#include <utility>

#include "absl/strings/str_format.h"
#include "absl/strings/str_join.h"
#include "nlohmann/json.hpp"
#include "zelda3/atlas/atlas_defaults.h"

namespace yaze::zelda3::atlas {

namespace {

// ordered_json keeps the file's key order (region and legend order matter).
using Json = nlohmann::ordered_json;

constexpr int kItemIdLimit = 0x100;
constexpr int kTileTypeLimit = 0x100;
constexpr int kTile16Limit = 0x1000;

std::string EscapePointerToken(const std::string& token) {
  std::string out;
  for (char c : token) {
    if (c == '~') {
      out += "~0";
    } else if (c == '/') {
      out += "~1";
    } else {
      out += c;
    }
  }
  return out;
}

std::string Child(const std::string& ptr, const std::string& key) {
  return ptr + "/" + EscapePointerToken(key);
}

std::string Child(const std::string& ptr, size_t index) {
  return ptr + "/" + std::to_string(index);
}

std::string Describe(const Json& value) {
  std::string text = value.dump();
  if (text.size() > 40) {
    text = text.substr(0, 37) + "...";
  }
  return text;
}

bool IsHexDigit(char c) {
  return std::isxdigit(static_cast<unsigned char>(c)) != 0;
}

// Reads one annotation document. Every error names the file and the JSON
// pointer of the offending value.
class DocReader {
 public:
  explicit DocReader(std::string file) : file_(std::move(file)) {}

  const std::string& file() const { return file_; }
  Origin origin() const { return Origin::Annotation(file_); }

  absl::Status Error(const std::string& ptr, const std::string& message) const {
    return absl::InvalidArgumentError(
        absl::StrFormat("%s: %s: %s", file_, ptr.empty() ? "/" : ptr, message));
  }

  // `value` must be an object whose keys are all in `allowed`; keys starting
  // with '_' are comments.
  absl::Status CheckObject(const Json& value, const std::string& ptr,
                           std::initializer_list<const char*> allowed) const {
    if (!value.is_object()) {
      return Error(ptr, "expected an object, got " + Describe(value));
    }
    for (const auto& [key, unused] : value.items()) {
      if (!key.empty() && key.front() == '_') {
        continue;
      }
      const bool known =
          std::any_of(allowed.begin(), allowed.end(),
                      [&key = key](const char* name) { return key == name; });
      if (!known) {
        std::vector<std::string> names(allowed.begin(), allowed.end());
        return Error(Child(ptr, key),
                     absl::StrFormat("unknown key (allowed: %s; keys starting "
                                     "with '_' are comments)",
                                     absl::StrJoin(names, ", ")));
      }
    }
    return absl::OkStatus();
  }

  absl::Status Require(const Json& object, const std::string& ptr,
                       const char* key) const {
    if (!object.contains(key)) {
      return Error(ptr, absl::StrFormat("missing required key \"%s\"", key));
    }
    return absl::OkStatus();
  }

  absl::StatusOr<std::string> String(const Json& value,
                                     const std::string& ptr) const {
    if (!value.is_string()) {
      return Error(ptr, "expected a string, got " + Describe(value));
    }
    return value.get<std::string>();
  }

  absl::StatusOr<std::string> Enum(
      const Json& value, const std::string& ptr,
      std::initializer_list<const char*> choices) const {
    auto text = String(value, ptr);
    if (!text.ok()) {
      return text.status();
    }
    for (const char* choice : choices) {
      if (*text == choice) {
        return *text;
      }
    }
    std::vector<std::string> names(choices.begin(), choices.end());
    return Error(ptr, absl::StrFormat("\"%s\" is not one of: %s", *text,
                                      absl::StrJoin(names, ", ")));
  }

  absl::StatusOr<std::string> Code(const Json& value,
                                   const std::string& ptr) const {
    auto text = String(value, ptr);
    if (text.ok() && text->size() > 4) {
      return Error(ptr, absl::StrFormat("code \"%s\" is longer than 4 "
                                        "characters",
                                        *text));
    }
    return text;
  }

  absl::StatusOr<int> Integer(const Json& value, const std::string& ptr,
                              int min) const {
    if (!value.is_number_integer() || value.get<int64_t>() < min ||
        value.get<int64_t>() > 0x7FFFFFFF) {
      return Error(ptr, absl::StrFormat("expected an integer >= %d, got %s",
                                        min, Describe(value)));
    }
    return static_cast<int>(value.get<int64_t>());
  }

  absl::StatusOr<bool> Bool(const Json& value, const std::string& ptr) const {
    if (!value.is_boolean()) {
      return Error(ptr, "expected true or false, got " + Describe(value));
    }
    return value.get<bool>();
  }

  // A room, map or item ID: "0x81", "81" (hex) or an integer, below `limit`.
  absl::StatusOr<int> Id(const Json& value, const std::string& ptr, int limit,
                         const char* what) const {
    std::optional<int> id;
    if (value.is_number_integer() && value.get<int64_t>() >= 0 &&
        value.get<int64_t>() < 0x1000) {
      id = static_cast<int>(value.get<int64_t>());
    } else if (value.is_string()) {
      id = ParseAtlasId(value.get<std::string>());
    }
    if (!id.has_value()) {
      return Error(ptr, absl::StrFormat("%s is not a %s ID (hex like \"0x1F\" "
                                        "or \"1F\", or a number)",
                                        Describe(value), what));
    }
    if (*id >= limit) {
      return Error(ptr, absl::StrFormat("%s 0x%X is out of range (0x0-0x%X)",
                                        what, *id, limit - 1));
    }
    return *id;
  }

  absl::StatusOr<int> Id(const std::string& text, const std::string& ptr,
                         int limit, const char* what) const {
    return Id(Json(text), ptr, limit, what);
  }

  // IDs and ranges ("0x90-0x96").
  absl::StatusOr<std::vector<int>> IdList(const Json& value,
                                          const std::string& ptr, int limit,
                                          const char* what) const {
    if (!value.is_array()) {
      return Error(ptr, "expected a list of IDs, got " + Describe(value));
    }
    std::vector<int> out;
    for (size_t i = 0; i < value.size(); ++i) {
      const Json& item = value[i];
      const std::string item_ptr = Child(ptr, i);
      if (item.is_string()) {
        const std::string text = item.get<std::string>();
        const size_t dash = text.find('-');
        if (dash != std::string::npos) {
          auto first = Id(text.substr(0, dash), item_ptr, limit, what);
          if (!first.ok()) {
            return first.status();
          }
          auto last = Id(text.substr(dash + 1), item_ptr, limit, what);
          if (!last.ok()) {
            return last.status();
          }
          if (*last < *first) {
            return Error(
                item_ptr,
                absl::StrFormat("range \"%s\" ends before it starts", text));
          }
          for (int id = *first; id <= *last; ++id) {
            out.push_back(id);
          }
          continue;
        }
      }
      auto id = Id(item, item_ptr, limit, what);
      if (!id.ok()) {
        return id.status();
      }
      out.push_back(*id);
    }
    return out;
  }

  absl::StatusOr<AtlasColor> Color(const Json& value,
                                   const std::string& ptr) const {
    if (value.is_string()) {
      const std::string text = value.get<std::string>();
      if (text.size() == 7 && text[0] == '#' &&
          std::all_of(text.begin() + 1, text.end(), IsHexDigit)) {
        const int rgb = std::stoi(text.substr(1), nullptr, 16);
        return AtlasColor{static_cast<uint8_t>(rgb >> 16),
                          static_cast<uint8_t>((rgb >> 8) & 0xFF),
                          static_cast<uint8_t>(rgb & 0xFF)};
      }
    }
    return Error(ptr, absl::StrFormat("%s is not a color (\"#RRGGBB\")",
                                      Describe(value)));
  }

  absl::StatusOr<std::vector<std::string>> StringList(
      const Json& value, const std::string& ptr) const {
    if (!value.is_array()) {
      return Error(ptr, "expected a list of strings, got " + Describe(value));
    }
    std::vector<std::string> out;
    for (size_t i = 0; i < value.size(); ++i) {
      auto text = String(value[i], Child(ptr, i));
      if (!text.ok()) {
        return text.status();
      }
      out.push_back(*text);
    }
    return out;
  }

 private:
  std::string file_;
};

#define ATLAS_ASSIGN(lhs, expr)            \
  do {                                     \
    auto atlas_value_ = (expr);            \
    if (!atlas_value_.ok()) {              \
      return atlas_value_.status();        \
    }                                      \
    lhs = std::move(atlas_value_).value(); \
  } while (false)

#define ATLAS_CHECK(expr)                \
  do {                                   \
    absl::Status atlas_status_ = (expr); \
    if (!atlas_status_.ok()) {           \
      return atlas_status_;              \
    }                                    \
  } while (false)

// ---------------------------------------------------------------- merging

void MergeDungeon(AtlasAnnotations* out, DungeonAnnotation entry) {
  for (auto& existing : out->dungeons) {
    const bool same =
        !entry.owner.empty()
            ? existing.owner == entry.owner
            : existing.owner.empty() && existing.dungeon_id == entry.dungeon_id;
    if (!same) {
      continue;
    }
    if (!entry.code.empty()) {
      existing.code = entry.code;
    }
    if (!entry.name.empty()) {
      existing.name = entry.name;
    }
    if (entry.color.has_value()) {
      existing.color = entry.color;
      existing.color_origin = entry.color_origin;
    }
    if (entry.dungeon_id >= 0) {
      existing.dungeon_id = entry.dungeon_id;
    }
    existing.origin = entry.origin;
    return;
  }
  out->dungeons.push_back(std::move(entry));
}

void MergeArea(AtlasAnnotations* out, AreaAnnotation entry) {
  for (auto& existing : out->areas) {
    if (existing.key != entry.key) {
      continue;
    }
    if (!entry.code.empty()) {
      existing.code = entry.code;
    }
    if (!entry.name.empty()) {
      existing.name = entry.name;
    }
    if (entry.color.has_value()) {
      existing.color = entry.color;
    }
    existing.rooms = entry.rooms;
    if (!entry.note.empty()) {
      existing.note = entry.note;
    }
    existing.origin = entry.origin;
    return;
  }
  out->areas.push_back(std::move(entry));
}

void MergePlan(AtlasAnnotations* out, PlanAnnotation entry) {
  for (auto& existing : out->plans) {
    if (existing.key != entry.key) {
      continue;
    }
    if (!entry.label.empty()) {
      existing.label = entry.label;
    }
    if (!entry.name.empty()) {
      existing.name = entry.name;
    }
    if (!entry.status.empty()) {
      existing.status = entry.status;
    }
    if (entry.color.has_value()) {
      existing.color = entry.color;
    }
    existing.rooms = entry.rooms;
    if (!entry.note.empty()) {
      existing.note = entry.note;
    }
    existing.origin = entry.origin;
    return;
  }
  out->plans.push_back(std::move(entry));
}

void MergeLink(AtlasAnnotations* out, LinkAnnotation entry) {
  if (!entry.id.empty()) {
    for (auto& existing : out->links) {
      if (existing.id == entry.id) {
        existing = std::move(entry);
        return;
      }
    }
  }
  out->links.push_back(std::move(entry));
}

void MergeRoom(AtlasAnnotations* out, int room, RoomAnnotation entry) {
  auto& existing = out->rooms[room];
  if (!entry.name.empty()) {
    existing.name = entry.name;
  }
  existing.reserved = existing.reserved || entry.reserved;
  if (!entry.note.empty()) {
    existing.note = entry.note;
  }
  existing.origin = entry.origin;
}

void MergeOverworldMap(AtlasAnnotations* out, int map,
                       OverworldMapAnnotation entry) {
  auto& existing = out->overworld.maps[map];
  if (!entry.name.empty()) {
    existing.name = entry.name;
  }
  if (!entry.note.empty()) {
    existing.note = entry.note;
  }
  existing.origin = entry.origin;
}

void MergeGate(AtlasAnnotations* out, OverworldGateAnnotation entry) {
  for (auto& existing : out->overworld.gates) {
    if (existing.map == entry.map && existing.gate == entry.gate) {
      if (!entry.note.empty()) {
        existing.note = entry.note;
      }
      if (!entry.role.empty()) {
        existing.role = entry.role;
      }
      existing.suppress = entry.suppress;
      existing.origin = entry.origin;
      return;
    }
  }
  out->overworld.gates.push_back(std::move(entry));
}

void MergeRegion(AtlasAnnotations* out, OverworldRegionAnnotation entry) {
  for (auto& existing : out->overworld.regions) {
    if (existing.name == entry.name && existing.world == entry.world) {
      existing = std::move(entry);
      return;
    }
  }
  out->overworld.regions.push_back(std::move(entry));
}

void AppendUnique(std::vector<int>* list, const std::vector<int>& more) {
  for (int id : more) {
    if (std::find(list->begin(), list->end(), id) == list->end()) {
      list->push_back(id);
    }
  }
}

// ---------------------------------------------------------------- shared

absl::StatusOr<SymbolLinkAnnotation> ReadSymbolLink(const DocReader& r,
                                                    const Json& value,
                                                    const std::string& ptr) {
  ATLAS_CHECK(
      r.CheckObject(value, ptr, {"symbol", "count", "width", "kind", "label"}));
  ATLAS_CHECK(r.Require(value, ptr, "symbol"));
  ATLAS_CHECK(r.Require(value, ptr, "count"));
  ATLAS_CHECK(r.Require(value, ptr, "kind"));
  SymbolLinkAnnotation link;
  link.origin = r.origin();
  ATLAS_ASSIGN(link.symbol, r.String(value["symbol"], Child(ptr, "symbol")));
  ATLAS_ASSIGN(link.count, r.Integer(value["count"], Child(ptr, "count"), 1));
  if (link.count > kAtlasRoomCount) {
    return r.Error(Child(ptr, "count"),
                   absl::StrFormat("count %d is more than %d rooms", link.count,
                                   kAtlasRoomCount));
  }
  if (value.contains("width")) {
    ATLAS_ASSIGN(link.width, r.Integer(value["width"], Child(ptr, "width"), 1));
    if (link.width != 1 && link.width != 2) {
      return r.Error(Child(ptr, "width"), "width must be 1 or 2");
    }
  }
  ATLAS_ASSIGN(link.kind, r.Enum(value["kind"], Child(ptr, "kind"),
                                 {"pit", "warp", "stair", "pad", "whirlpool",
                                  "special", "dream", "warn"}));
  if (value.contains("label")) {
    ATLAS_ASSIGN(link.label, r.String(value["label"], Child(ptr, "label")));
  }
  return link;
}

absl::StatusOr<PlanAnnotation> ReadPlan(const DocReader& r, const Json& value,
                                        const std::string& ptr) {
  ATLAS_CHECK(r.CheckObject(
      value, ptr,
      {"key", "label", "name", "status", "color", "rooms", "source"}));
  ATLAS_CHECK(r.Require(value, ptr, "key"));
  ATLAS_CHECK(r.Require(value, ptr, "rooms"));
  PlanAnnotation plan;
  plan.origin = r.origin();
  ATLAS_ASSIGN(plan.key, r.String(value["key"], Child(ptr, "key")));
  if (value.contains("label")) {
    ATLAS_ASSIGN(plan.label, r.String(value["label"], Child(ptr, "label")));
  }
  if (value.contains("name")) {
    ATLAS_ASSIGN(plan.name, r.String(value["name"], Child(ptr, "name")));
  }
  if (value.contains("status")) {
    ATLAS_ASSIGN(plan.status, r.Enum(value["status"], Child(ptr, "status"),
                                     {"decided", "provisional"}));
  }
  if (value.contains("color")) {
    ATLAS_ASSIGN(plan.color, r.Color(value["color"], Child(ptr, "color")));
  }
  ATLAS_ASSIGN(plan.rooms, r.IdList(value["rooms"], Child(ptr, "rooms"),
                                    kAtlasRoomCount, "room"));
  if (value.contains("source")) {
    ATLAS_ASSIGN(plan.note, r.String(value["source"], Child(ptr, "source")));
  }
  return plan;
}

absl::StatusOr<AreaAnnotation> ReadArea(const DocReader& r, const Json& value,
                                        const std::string& ptr) {
  ATLAS_CHECK(r.CheckObject(
      value, ptr, {"key", "code", "name", "color", "rooms", "source"}));
  ATLAS_CHECK(r.Require(value, ptr, "key"));
  ATLAS_CHECK(r.Require(value, ptr, "rooms"));
  AreaAnnotation area;
  area.origin = r.origin();
  ATLAS_ASSIGN(area.key, r.String(value["key"], Child(ptr, "key")));
  if (area.key.empty()) {
    return r.Error(Child(ptr, "key"), "key must not be empty");
  }
  if (value.contains("code")) {
    ATLAS_ASSIGN(area.code, r.Code(value["code"], Child(ptr, "code")));
  }
  if (value.contains("name")) {
    ATLAS_ASSIGN(area.name, r.String(value["name"], Child(ptr, "name")));
  }
  if (value.contains("color")) {
    ATLAS_ASSIGN(area.color, r.Color(value["color"], Child(ptr, "color")));
  }
  ATLAS_ASSIGN(area.rooms, r.IdList(value["rooms"], Child(ptr, "rooms"),
                                    kAtlasRoomCount, "room"));
  if (value.contains("source")) {
    ATLAS_ASSIGN(area.note, r.String(value["source"], Child(ptr, "source")));
  }
  return area;
}

absl::Status ReadLegendNotes(const DocReader& r, const Json& value,
                             const std::string& ptr, AtlasAnnotations* out) {
  ATLAS_CHECK(r.CheckObject(value, ptr, {"dungeon", "overworld"}));
  if (value.contains("dungeon")) {
    std::vector<std::string> notes;
    ATLAS_ASSIGN(notes, r.StringList(value["dungeon"], Child(ptr, "dungeon")));
    out->legend_notes_dungeon.insert(out->legend_notes_dungeon.end(),
                                     notes.begin(), notes.end());
  }
  if (value.contains("overworld")) {
    std::vector<std::string> notes;
    ATLAS_ASSIGN(notes,
                 r.StringList(value["overworld"], Child(ptr, "overworld")));
    out->legend_notes_overworld.insert(out->legend_notes_overworld.end(),
                                       notes.begin(), notes.end());
  }
  return absl::OkStatus();
}

absl::StatusOr<OverworldSectionAnnotation> ReadSection(const DocReader& r,
                                                       const Json& value,
                                                       const std::string& ptr) {
  ATLAS_CHECK(
      r.CheckObject(value, ptr, {"title", "subtitle", "maps", "origin"}));
  ATLAS_CHECK(r.Require(value, ptr, "maps"));
  ATLAS_CHECK(r.Require(value, ptr, "origin"));
  OverworldSectionAnnotation section;
  section.origin = r.origin();
  if (value.contains("title")) {
    ATLAS_ASSIGN(section.title, r.String(value["title"], Child(ptr, "title")));
  }
  if (value.contains("subtitle")) {
    ATLAS_ASSIGN(section.subtitle,
                 r.String(value["subtitle"], Child(ptr, "subtitle")));
  }
  ATLAS_ASSIGN(section.maps, r.IdList(value["maps"], Child(ptr, "maps"),
                                      kAtlasMapCount, "map"));
  const Json& origin = value["origin"];
  if (!origin.is_array() || origin.size() != 2 || !origin[0].is_number() ||
      !origin[1].is_number()) {
    return r.Error(Child(ptr, "origin"),
                   "origin must be [column, row] (two numbers)");
  }
  section.origin_col = origin[0].get<double>();
  section.origin_row = origin[1].get<double>();
  return section;
}

// "0x0F:Hammer" -> {0x0F, "Hammer"}
absl::StatusOr<std::pair<int, std::string>> ReadGateKey(
    const DocReader& r, const std::string& key, const std::string& ptr) {
  const size_t colon = key.find(':');
  if (colon == std::string::npos) {
    return r.Error(ptr, absl::StrFormat("gate key \"%s\" must be "
                                        "\"<map>:<Glove|Mitt|Hammer|Boots>\"",
                                        key));
  }
  int map = 0;
  ATLAS_ASSIGN(map, r.Id(key.substr(0, colon), ptr, kAtlasMapCount, "map"));
  const std::string gate = key.substr(colon + 1);
  if (gate != "Glove" && gate != "Mitt" && gate != "Hammer" &&
      gate != "Boots") {
    return r.Error(ptr, absl::StrFormat("gate \"%s\" is not Glove, Mitt, "
                                        "Hammer or Boots",
                                        gate));
  }
  return std::make_pair(map, gate);
}

absl::Status ReadOverworldV1(const DocReader& r, const Json& value,
                             const std::string& ptr, AtlasAnnotations* out) {
  ATLAS_CHECK(
      r.CheckObject(value, ptr,
                    {"sections", "regions", "maps", "gates", "edges",
                     "closed_border_maps", "isolated_maps", "tile_classes",
                     "gate_role_thresholds", "warp_tile_rule"}));
  auto& ow = out->overworld;
  if (value.contains("sections")) {
    const Json& list = value["sections"];
    const std::string list_ptr = Child(ptr, "sections");
    if (!list.is_array()) {
      return r.Error(list_ptr, "expected a list");
    }
    for (size_t i = 0; i < list.size(); ++i) {
      OverworldSectionAnnotation section;
      ATLAS_ASSIGN(section, ReadSection(r, list[i], Child(list_ptr, i)));
      ow.sections.push_back(std::move(section));
    }
  }
  if (value.contains("regions")) {
    const Json& list = value["regions"];
    const std::string list_ptr = Child(ptr, "regions");
    if (!list.is_array()) {
      return r.Error(list_ptr, "expected a list");
    }
    for (size_t i = 0; i < list.size(); ++i) {
      const std::string item_ptr = Child(list_ptr, i);
      const Json& item = list[i];
      ATLAS_CHECK(r.CheckObject(item, item_ptr,
                                {"name", "code", "world", "color", "maps"}));
      ATLAS_CHECK(r.Require(item, item_ptr, "name"));
      ATLAS_CHECK(r.Require(item, item_ptr, "maps"));
      OverworldRegionAnnotation region;
      region.origin = r.origin();
      ATLAS_ASSIGN(region.name,
                   r.String(item["name"], Child(item_ptr, "name")));
      if (item.contains("code")) {
        ATLAS_ASSIGN(region.code,
                     r.String(item["code"], Child(item_ptr, "code")));
      }
      if (item.contains("world")) {
        ATLAS_ASSIGN(region.world,
                     r.Enum(item["world"], Child(item_ptr, "world"),
                            {"LW", "DW", "SW"}));
      }
      if (item.contains("color")) {
        ATLAS_ASSIGN(region.color,
                     r.Color(item["color"], Child(item_ptr, "color")));
      }
      ATLAS_ASSIGN(region.maps, r.IdList(item["maps"], Child(item_ptr, "maps"),
                                         kAtlasMapCount, "map"));
      MergeRegion(out, std::move(region));
    }
  }
  if (value.contains("maps")) {
    const Json& maps = value["maps"];
    const std::string maps_ptr = Child(ptr, "maps");
    if (!maps.is_object()) {
      return r.Error(maps_ptr, "expected an object keyed by map ID");
    }
    for (const auto& [key, item] : maps.items()) {
      const std::string item_ptr = Child(maps_ptr, key);
      int map = 0;
      ATLAS_ASSIGN(map, r.Id(key, item_ptr, kAtlasMapCount, "map"));
      ATLAS_CHECK(r.CheckObject(item, item_ptr, {"name", "note"}));
      OverworldMapAnnotation entry;
      entry.origin = r.origin();
      if (item.contains("name")) {
        ATLAS_ASSIGN(entry.name,
                     r.String(item["name"], Child(item_ptr, "name")));
      }
      if (item.contains("note")) {
        ATLAS_ASSIGN(entry.note,
                     r.String(item["note"], Child(item_ptr, "note")));
      }
      MergeOverworldMap(out, map, std::move(entry));
    }
  }
  if (value.contains("gates")) {
    const Json& gates = value["gates"];
    const std::string gates_ptr = Child(ptr, "gates");
    if (!gates.is_object()) {
      return r.Error(gates_ptr, "expected an object keyed by \"<map>:<Gate>\"");
    }
    for (const auto& [key, item] : gates.items()) {
      const std::string item_ptr = Child(gates_ptr, key);
      std::pair<int, std::string> gate_key;
      ATLAS_ASSIGN(gate_key, ReadGateKey(r, key, item_ptr));
      ATLAS_CHECK(r.CheckObject(item, item_ptr, {"note", "role", "suppress"}));
      OverworldGateAnnotation gate;
      gate.origin = r.origin();
      gate.map = gate_key.first;
      gate.gate = gate_key.second;
      if (item.contains("note")) {
        ATLAS_ASSIGN(gate.note,
                     r.String(item["note"], Child(item_ptr, "note")));
      }
      if (item.contains("role")) {
        ATLAS_ASSIGN(gate.role, r.Enum(item["role"], Child(item_ptr, "role"),
                                       {"ROUTE", "POCKET", "DECOR"}));
      }
      if (item.contains("suppress")) {
        ATLAS_ASSIGN(gate.suppress,
                     r.Bool(item["suppress"], Child(item_ptr, "suppress")));
      }
      MergeGate(out, std::move(gate));
    }
  }
  if (value.contains("edges")) {
    const Json& list = value["edges"];
    const std::string list_ptr = Child(ptr, "edges");
    if (!list.is_array()) {
      return r.Error(list_ptr, "expected a list");
    }
    for (size_t i = 0; i < list.size(); ++i) {
      const std::string item_ptr = Child(list_ptr, i);
      const Json& item = list[i];
      ATLAS_CHECK(r.CheckObject(item, item_ptr, {"a", "b", "status", "note"}));
      ATLAS_CHECK(r.Require(item, item_ptr, "a"));
      ATLAS_CHECK(r.Require(item, item_ptr, "b"));
      ATLAS_CHECK(r.Require(item, item_ptr, "status"));
      OverworldEdgeAnnotation edge;
      edge.origin = r.origin();
      ATLAS_ASSIGN(
          edge.a, r.Id(item["a"], Child(item_ptr, "a"), kAtlasMapCount, "map"));
      ATLAS_ASSIGN(
          edge.b, r.Id(item["b"], Child(item_ptr, "b"), kAtlasMapCount, "map"));
      ATLAS_ASSIGN(edge.status,
                   r.Enum(item["status"], Child(item_ptr, "status"),
                          {"open", "swim", "blocked", "special"}));
      if (item.contains("note")) {
        ATLAS_ASSIGN(edge.note,
                     r.String(item["note"], Child(item_ptr, "note")));
      }
      ow.edges.push_back(std::move(edge));
    }
  }
  for (const char* key : {"closed_border_maps", "isolated_maps"}) {
    if (!value.contains(key)) {
      continue;
    }
    std::vector<int> maps;
    ATLAS_ASSIGN(maps,
                 r.IdList(value[key], Child(ptr, key), kAtlasMapCount, "map"));
    AppendUnique(std::string(key) == "closed_border_maps"
                     ? &ow.closed_border_maps
                     : &ow.isolated_maps,
                 maps);
  }
  if (value.contains("tile_classes")) {
    const Json& classes = value["tile_classes"];
    const std::string classes_ptr = Child(ptr, "tile_classes");
    ATLAS_CHECK(r.CheckObject(classes, classes_ptr,
                              {"free", "water", "ledge", "glove", "mitt",
                               "boots", "pit", "hammer_tile16"}));
    for (const auto& [key, item] : classes.items()) {
      if (!key.empty() && key.front() == '_') {
        continue;
      }
      std::vector<int> ids;
      const bool tile16 = key == "hammer_tile16";
      ATLAS_ASSIGN(ids, r.IdList(item, Child(classes_ptr, key),
                                 tile16 ? kTile16Limit : kTileTypeLimit,
                                 tile16 ? "tile16" : "tile type"));
      ow.tile_classes[key] = std::move(ids);
    }
  }
  if (value.contains("gate_role_thresholds")) {
    const Json& thresholds = value["gate_role_thresholds"];
    const std::string t_ptr = Child(ptr, "gate_role_thresholds");
    ATLAS_CHECK(r.CheckObject(thresholds, t_ptr, {"big", "small"}));
    if (thresholds.contains("big")) {
      ATLAS_ASSIGN(ow.gate_role_big,
                   r.Integer(thresholds["big"], Child(t_ptr, "big"), 1));
    }
    if (thresholds.contains("small")) {
      ATLAS_ASSIGN(ow.gate_role_small,
                   r.Integer(thresholds["small"], Child(t_ptr, "small"), 1));
    }
  }
  if (value.contains("warp_tile_rule")) {
    ATLAS_ASSIGN(ow.warp_tile_rule,
                 r.Enum(value["warp_tile_rule"], Child(ptr, "warp_tile_rule"),
                        {"toggle_world", "none"}));
  }
  return absl::OkStatus();
}

// ---------------------------------------------------------------- v1

absl::Status ReadAtlasV1(const DocReader& r, const Json& root,
                         AtlasAnnotations* out) {
  ATLAS_CHECK(r.CheckObject(
      root, "",
      {"schema", "title", "about", "dungeons", "areas", "rooms", "links",
       "symbol_links", "plans", "legend_notes", "items", "overworld"}));
  if (root.contains("title")) {
    ATLAS_ASSIGN(out->title, r.String(root["title"], "/title"));
    out->title_origin = r.origin();
  }
  if (root.contains("about")) {
    ATLAS_CHECK(r.String(root["about"], "/about").status());
  }
  if (root.contains("dungeons")) {
    const Json& list = root["dungeons"];
    if (!list.is_array()) {
      return r.Error("/dungeons", "expected a list");
    }
    for (size_t i = 0; i < list.size(); ++i) {
      const std::string ptr = Child("/dungeons", i);
      const Json& item = list[i];
      ATLAS_CHECK(r.CheckObject(
          item, ptr, {"owner", "dungeon_id", "code", "name", "color"}));
      if (!item.contains("owner") && !item.contains("dungeon_id")) {
        return r.Error(ptr,
                       "needs \"owner\" (registry dungeon ID) or "
                       "\"dungeon_id\" (entrance dungeon ID)");
      }
      DungeonAnnotation dungeon;
      dungeon.origin = r.origin();
      dungeon.color_origin = r.origin();
      if (item.contains("owner")) {
        ATLAS_ASSIGN(dungeon.owner,
                     r.String(item["owner"], Child(ptr, "owner")));
      }
      if (item.contains("dungeon_id")) {
        ATLAS_ASSIGN(dungeon.dungeon_id,
                     r.Id(item["dungeon_id"], Child(ptr, "dungeon_id"), 0x100,
                          "dungeon"));
      }
      if (item.contains("code")) {
        ATLAS_ASSIGN(dungeon.code, r.Code(item["code"], Child(ptr, "code")));
      }
      if (item.contains("name")) {
        ATLAS_ASSIGN(dungeon.name, r.String(item["name"], Child(ptr, "name")));
      }
      if (item.contains("color")) {
        ATLAS_ASSIGN(dungeon.color,
                     r.Color(item["color"], Child(ptr, "color")));
      }
      MergeDungeon(out, std::move(dungeon));
    }
  }
  if (root.contains("areas")) {
    const Json& list = root["areas"];
    if (!list.is_array()) {
      return r.Error("/areas", "expected a list");
    }
    for (size_t i = 0; i < list.size(); ++i) {
      AreaAnnotation area;
      ATLAS_ASSIGN(area, ReadArea(r, list[i], Child("/areas", i)));
      MergeArea(out, std::move(area));
    }
  }
  if (root.contains("rooms")) {
    const Json& rooms = root["rooms"];
    if (!rooms.is_object()) {
      return r.Error("/rooms", "expected an object keyed by room ID");
    }
    for (const auto& [key, item] : rooms.items()) {
      const std::string ptr = Child("/rooms", key);
      if (!key.empty() && key.front() == '_') {
        continue;
      }
      int room = 0;
      ATLAS_ASSIGN(room, r.Id(key, ptr, kAtlasRoomCount, "room"));
      ATLAS_CHECK(r.CheckObject(item, ptr, {"name", "status", "note"}));
      RoomAnnotation entry;
      entry.origin = r.origin();
      if (item.contains("name")) {
        ATLAS_ASSIGN(entry.name, r.String(item["name"], Child(ptr, "name")));
      }
      if (item.contains("status")) {
        ATLAS_CHECK(r.Enum(item["status"], Child(ptr, "status"), {"reserved"})
                        .status());
        entry.reserved = true;
      }
      if (item.contains("note")) {
        ATLAS_ASSIGN(entry.note, r.String(item["note"], Child(ptr, "note")));
      }
      MergeRoom(out, room, std::move(entry));
    }
  }
  if (root.contains("links")) {
    const Json& list = root["links"];
    if (!list.is_array()) {
      return r.Error("/links", "expected a list");
    }
    for (size_t i = 0; i < list.size(); ++i) {
      const std::string ptr = Child("/links", i);
      const Json& item = list[i];
      ATLAS_CHECK(r.CheckObject(
          item, ptr, {"id", "view", "kind", "from", "to", "label", "source"}));
      ATLAS_CHECK(r.Require(item, ptr, "view"));
      ATLAS_CHECK(r.Require(item, ptr, "kind"));
      ATLAS_CHECK(r.Require(item, ptr, "from"));
      LinkAnnotation link;
      link.origin = r.origin();
      if (item.contains("id")) {
        ATLAS_ASSIGN(link.id, r.String(item["id"], Child(ptr, "id")));
      }
      std::string view;
      ATLAS_ASSIGN(view, r.Enum(item["view"], Child(ptr, "view"),
                                {"dungeon", "overworld"}));
      link.view =
          view == "dungeon" ? AtlasView::kDungeon : AtlasView::kOverworld;
      const int limit =
          link.view == AtlasView::kDungeon ? kAtlasRoomCount : kAtlasMapCount;
      const char* what = link.view == AtlasView::kDungeon ? "room" : "map";
      ATLAS_ASSIGN(link.kind,
                   r.Enum(item["kind"], Child(ptr, "kind"),
                          {"pit", "warp", "stair", "pad", "whirlpool",
                           "special", "dream", "warn"}));
      ATLAS_ASSIGN(link.from,
                   r.Id(item["from"], Child(ptr, "from"), limit, what));
      if (item.contains("to")) {
        ATLAS_ASSIGN(link.to, r.Id(item["to"], Child(ptr, "to"), limit, what));
      }
      if (item.contains("label")) {
        ATLAS_ASSIGN(link.label, r.String(item["label"], Child(ptr, "label")));
      }
      if (item.contains("source")) {
        ATLAS_ASSIGN(link.note, r.String(item["source"], Child(ptr, "source")));
      }
      MergeLink(out, std::move(link));
    }
  }
  if (root.contains("symbol_links")) {
    const Json& list = root["symbol_links"];
    if (!list.is_array()) {
      return r.Error("/symbol_links", "expected a list");
    }
    for (size_t i = 0; i < list.size(); ++i) {
      SymbolLinkAnnotation link;
      ATLAS_ASSIGN(link, ReadSymbolLink(r, list[i], Child("/symbol_links", i)));
      out->symbol_links.push_back(std::move(link));
    }
  }
  if (root.contains("plans")) {
    const Json& list = root["plans"];
    if (!list.is_array()) {
      return r.Error("/plans", "expected a list");
    }
    for (size_t i = 0; i < list.size(); ++i) {
      PlanAnnotation plan;
      ATLAS_ASSIGN(plan, ReadPlan(r, list[i], Child("/plans", i)));
      MergePlan(out, std::move(plan));
    }
  }
  if (root.contains("legend_notes")) {
    ATLAS_CHECK(ReadLegendNotes(r, root["legend_notes"], "/legend_notes", out));
  }
  if (root.contains("items")) {
    const Json& items = root["items"];
    ATLAS_CHECK(r.CheckObject(
        items, "/items",
        {"small_key", "compass", "big_key", "map", "pot_small_key"}));
    const std::pair<const char*, int*> fields[] = {
        {"small_key", &out->items.small_key},
        {"compass", &out->items.compass},
        {"big_key", &out->items.big_key},
        {"map", &out->items.map},
        {"pot_small_key", &out->items.pot_small_key}};
    for (const auto& [key, target] : fields) {
      if (items.contains(key)) {
        ATLAS_ASSIGN(*target, r.Id(items[key], Child("/items", key),
                                   kItemIdLimit, "item"));
      }
    }
  }
  if (root.contains("overworld")) {
    ATLAS_CHECK(ReadOverworldV1(r, root["overworld"], "/overworld", out));
  }
  return absl::OkStatus();
}

// ---------------------------------------------------------------- Oracle v0

// Scripts/Maps/room_annotations.json. Besides the keys the Oracle scripts
// read, the optional keys the design doc (section 6.5) lets Oracle add are
// accepted: title, dungeon_colors, areas[].code/color, plans[].color,
// symbol_links, legend_notes.
absl::Status ReadOracleRoomV0(const DocReader& r, const Json& root,
                              AtlasAnnotations* out) {
  ATLAS_CHECK(r.CheckObject(
      root, "",
      {"dungeon_codes", "areas", "room_names", "runtime_pits", "plans", "title",
       "dungeon_colors", "symbol_links", "legend_notes"}));
  if (root.contains("title")) {
    ATLAS_ASSIGN(out->title, r.String(root["title"], "/title"));
    out->title_origin = r.origin();
  }
  if (root.contains("dungeon_codes")) {
    const Json& codes = root["dungeon_codes"];
    if (!codes.is_object()) {
      return r.Error("/dungeon_codes",
                     "expected an object {registry dungeon ID: code}");
    }
    for (const auto& [owner, code] : codes.items()) {
      if (!owner.empty() && owner.front() == '_') {
        continue;
      }
      DungeonAnnotation dungeon;
      dungeon.origin = r.origin();
      dungeon.owner = owner;
      ATLAS_ASSIGN(dungeon.code, r.Code(code, Child("/dungeon_codes", owner)));
      MergeDungeon(out, std::move(dungeon));
    }
  }
  if (root.contains("dungeon_colors")) {
    const Json& colors = root["dungeon_colors"];
    if (!colors.is_object()) {
      return r.Error("/dungeon_colors",
                     "expected an object {registry dungeon ID: \"#RRGGBB\"}");
    }
    for (const auto& [owner, color] : colors.items()) {
      if (!owner.empty() && owner.front() == '_') {
        continue;
      }
      DungeonAnnotation dungeon;
      dungeon.origin = r.origin();
      dungeon.color_origin = r.origin();
      dungeon.owner = owner;
      ATLAS_ASSIGN(dungeon.color,
                   r.Color(color, Child("/dungeon_colors", owner)));
      MergeDungeon(out, std::move(dungeon));
    }
  }
  if (root.contains("areas")) {
    const Json& list = root["areas"];
    if (!list.is_array()) {
      return r.Error("/areas", "expected a list");
    }
    for (size_t i = 0; i < list.size(); ++i) {
      AreaAnnotation area;
      ATLAS_ASSIGN(area, ReadArea(r, list[i], Child("/areas", i)));
      MergeArea(out, std::move(area));
    }
  }
  if (root.contains("room_names")) {
    const Json& names = root["room_names"];
    if (!names.is_object()) {
      return r.Error("/room_names", "expected an object {room ID: name}");
    }
    for (const auto& [key, name] : names.items()) {
      if (!key.empty() && key.front() == '_') {
        continue;
      }
      const std::string ptr = Child("/room_names", key);
      int room = 0;
      ATLAS_ASSIGN(room, r.Id(key, ptr, kAtlasRoomCount, "room"));
      RoomAnnotation entry;
      entry.origin = r.origin();
      ATLAS_ASSIGN(entry.name, r.String(name, ptr));
      MergeRoom(out, room, std::move(entry));
    }
  }
  if (root.contains("runtime_pits")) {
    const Json& list = root["runtime_pits"];
    if (!list.is_array()) {
      return r.Error("/runtime_pits", "expected a list");
    }
    for (size_t i = 0; i < list.size(); ++i) {
      const std::string ptr = Child("/runtime_pits", i);
      const Json& item = list[i];
      ATLAS_CHECK(r.CheckObject(item, ptr, {"room", "to", "source"}));
      ATLAS_CHECK(r.Require(item, ptr, "room"));
      ATLAS_CHECK(r.Require(item, ptr, "to"));
      LinkAnnotation link;
      link.origin = r.origin();
      link.view = AtlasView::kDungeon;
      link.kind = "pit";
      ATLAS_ASSIGN(link.from, r.Id(item["room"], Child(ptr, "room"),
                                   kAtlasRoomCount, "room"));
      ATLAS_ASSIGN(link.to,
                   r.Id(item["to"], Child(ptr, "to"), kAtlasRoomCount, "room"));
      if (item.contains("source")) {
        ATLAS_ASSIGN(link.note, r.String(item["source"], Child(ptr, "source")));
      }
      MergeLink(out, std::move(link));
    }
  }
  if (root.contains("plans")) {
    const Json& list = root["plans"];
    if (!list.is_array()) {
      return r.Error("/plans", "expected a list");
    }
    for (size_t i = 0; i < list.size(); ++i) {
      PlanAnnotation plan;
      ATLAS_ASSIGN(plan, ReadPlan(r, list[i], Child("/plans", i)));
      MergePlan(out, std::move(plan));
    }
  }
  if (root.contains("symbol_links")) {
    const Json& list = root["symbol_links"];
    if (!list.is_array()) {
      return r.Error("/symbol_links", "expected a list");
    }
    for (size_t i = 0; i < list.size(); ++i) {
      SymbolLinkAnnotation link;
      ATLAS_ASSIGN(link, ReadSymbolLink(r, list[i], Child("/symbol_links", i)));
      out->symbol_links.push_back(std::move(link));
    }
  }
  if (root.contains("legend_notes")) {
    std::vector<std::string> notes;
    ATLAS_ASSIGN(notes, r.StringList(root["legend_notes"], "/legend_notes"));
    out->legend_notes_dungeon.insert(out->legend_notes_dungeon.end(),
                                     notes.begin(), notes.end());
  }
  return absl::OkStatus();
}

// Scripts/Maps/overworld_annotations.json, plus the optional sections,
// legend_notes and title keys (design doc section 6.5).
absl::Status ReadOracleOverworldV0(const DocReader& r, const Json& root,
                                   AtlasAnnotations* out) {
  ATLAS_CHECK(r.CheckObject(
      root, "",
      {"regions", "names", "gate_notes", "special_edge_block",
       "sw_isolated_maps", "links", "sections", "legend_notes", "title"}));
  auto& ow = out->overworld;
  if (root.contains("title")) {
    ATLAS_ASSIGN(out->title, r.String(root["title"], "/title"));
    out->title_origin = r.origin();
  }
  if (root.contains("regions")) {
    const Json& regions = root["regions"];
    if (!regions.is_object()) {
      return r.Error("/regions", "expected an object {\"LW Name\": [map IDs]}");
    }
    for (const auto& [key, maps] : regions.items()) {
      if (!key.empty() && key.front() == '_') {
        continue;
      }
      const std::string ptr = Child("/regions", key);
      if (key.size() < 4 || key[2] != ' ' ||
          (key.compare(0, 2, "LW") != 0 && key.compare(0, 2, "DW") != 0 &&
           key.compare(0, 2, "SW") != 0)) {
        return r.Error(ptr,
                       "region names start with the world: \"LW \", "
                       "\"DW \" or \"SW \"");
      }
      OverworldRegionAnnotation region;
      region.origin = r.origin();
      region.world = key.substr(0, 2);
      region.name = key.substr(3);
      ATLAS_ASSIGN(region.maps, r.IdList(maps, ptr, kAtlasMapCount, "map"));
      MergeRegion(out, std::move(region));
    }
  }
  if (root.contains("names")) {
    const Json& names = root["names"];
    if (!names.is_object()) {
      return r.Error("/names", "expected an object {map ID: name}");
    }
    for (const auto& [key, name] : names.items()) {
      if (!key.empty() && key.front() == '_') {
        continue;
      }
      const std::string ptr = Child("/names", key);
      int map = 0;
      ATLAS_ASSIGN(map, r.Id(key, ptr, kAtlasMapCount, "map"));
      OverworldMapAnnotation entry;
      entry.origin = r.origin();
      ATLAS_ASSIGN(entry.name, r.String(name, ptr));
      MergeOverworldMap(out, map, std::move(entry));
    }
  }
  if (root.contains("gate_notes")) {
    const Json& notes = root["gate_notes"];
    if (!notes.is_object()) {
      return r.Error("/gate_notes",
                     "expected an object {\"<map>:<Gate>\": note}");
    }
    for (const auto& [key, note] : notes.items()) {
      if (!key.empty() && key.front() == '_') {
        continue;
      }
      const std::string ptr = Child("/gate_notes", key);
      std::pair<int, std::string> gate_key;
      ATLAS_ASSIGN(gate_key, ReadGateKey(r, key, ptr));
      OverworldGateAnnotation gate;
      gate.origin = r.origin();
      gate.map = gate_key.first;
      gate.gate = gate_key.second;
      ATLAS_ASSIGN(gate.note, r.String(note, ptr));
      MergeGate(out, std::move(gate));
    }
  }
  if (root.contains("special_edge_block")) {
    std::vector<int> maps;
    ATLAS_ASSIGN(maps, r.IdList(root["special_edge_block"],
                                "/special_edge_block", kAtlasMapCount, "map"));
    AppendUnique(&ow.closed_border_maps, maps);
  }
  if (root.contains("sw_isolated_maps")) {
    std::vector<int> maps;
    ATLAS_ASSIGN(maps, r.IdList(root["sw_isolated_maps"], "/sw_isolated_maps",
                                kAtlasMapCount, "map"));
    AppendUnique(&ow.isolated_maps, maps);
  }
  if (root.contains("links")) {
    const Json& list = root["links"];
    if (!list.is_array()) {
      return r.Error("/links", "expected a list");
    }
    for (size_t i = 0; i < list.size(); ++i) {
      const std::string ptr = Child("/links", i);
      const Json& item = list[i];
      ATLAS_CHECK(r.CheckObject(item, ptr, {"map", "text", "kind"}));
      ATLAS_CHECK(r.Require(item, ptr, "map"));
      ATLAS_CHECK(r.Require(item, ptr, "text"));
      ATLAS_CHECK(r.Require(item, ptr, "kind"));
      LinkAnnotation link;
      link.origin = r.origin();
      link.view = AtlasView::kOverworld;
      ATLAS_ASSIGN(link.from,
                   r.Id(item["map"], Child(ptr, "map"), kAtlasMapCount, "map"));
      ATLAS_ASSIGN(link.label, r.String(item["text"], Child(ptr, "text")));
      ATLAS_ASSIGN(link.kind,
                   r.Enum(item["kind"], Child(ptr, "kind"),
                          {"pit", "warp", "stair", "pad", "whirlpool",
                           "special", "dream", "warn"}));
      MergeLink(out, std::move(link));
    }
  }
  if (root.contains("sections")) {
    const Json& list = root["sections"];
    if (!list.is_array()) {
      return r.Error("/sections", "expected a list");
    }
    for (size_t i = 0; i < list.size(); ++i) {
      OverworldSectionAnnotation section;
      ATLAS_ASSIGN(section, ReadSection(r, list[i], Child("/sections", i)));
      ow.sections.push_back(std::move(section));
    }
  }
  if (root.contains("legend_notes")) {
    std::vector<std::string> notes;
    ATLAS_ASSIGN(notes, r.StringList(root["legend_notes"], "/legend_notes"));
    out->legend_notes_overworld.insert(out->legend_notes_overworld.end(),
                                       notes.begin(), notes.end());
  }
  return absl::OkStatus();
}

absl::StatusOr<AnnotationFormat> DetectFormat(const DocReader& r,
                                              const Json& root) {
  if (!root.is_object()) {
    return r.Error("", "an annotation file is a JSON object");
  }
  if (root.contains("schema")) {
    if (root["schema"] != "yaze.atlas/1") {
      return r.Error("/schema",
                     absl::StrFormat("unknown schema %s (this yaze reads "
                                     "\"yaze.atlas/1\")",
                                     Describe(root["schema"])));
    }
    return AnnotationFormat::kAtlasV1;
  }
  static constexpr const char* kRoomKeys[] = {
      "dungeon_codes", "areas", "room_names", "runtime_pits", "plans"};
  static constexpr const char* kOverworldKeys[] = {
      "regions",          "names", "gate_notes", "special_edge_block",
      "sw_isolated_maps", "links"};
  bool room = false;
  bool overworld = false;
  for (const char* key : kRoomKeys) {
    room |= root.contains(key);
  }
  for (const char* key : kOverworldKeys) {
    overworld |= root.contains(key);
  }
  if (room && overworld) {
    return r.Error("",
                   "mixes Oracle room keys (dungeon_codes, areas, ...) "
                   "and overworld keys (regions, names, ...); split the "
                   "file or add \"schema\": \"yaze.atlas/1\"");
  }
  if (room) {
    return AnnotationFormat::kOracleRoomV0;
  }
  if (overworld) {
    return AnnotationFormat::kOracleOverworldV0;
  }
  return r.Error("",
                 "cannot tell the format; add \"schema\": "
                 "\"yaze.atlas/1\"");
}

#undef ATLAS_ASSIGN
#undef ATLAS_CHECK

}  // namespace

std::string AtlasColor::Hex() const {
  return absl::StrFormat("#%02X%02X%02X", r, g, b);
}

std::string Origin::ToString() const {
  switch (kind) {
    case Provenance::kRom:
      return "rom";
    case Provenance::kRegistry:
      return "registry";
    case Provenance::kAnnotation:
      return "annotation:" + ref;
    case Provenance::kSymbol:
      return "symbol:" + ref;
    case Provenance::kDefault:
      return "default";
  }
  return "rom";
}

const char* AnnotationFormatName(AnnotationFormat format) {
  switch (format) {
    case AnnotationFormat::kAtlasV1:
      return "yaze.atlas/1";
    case AnnotationFormat::kOracleRoomV0:
      return "oracle-room-v0";
    case AnnotationFormat::kOracleOverworldV0:
      return "oracle-overworld-v0";
  }
  return "?";
}

std::optional<int> ParseAtlasId(const std::string& text) {
  std::string digits = text;
  if (digits.size() > 2 && digits[0] == '0' &&
      (digits[1] == 'x' || digits[1] == 'X')) {
    digits = digits.substr(2);
  }
  if (digits.empty() || digits.size() > 3 ||
      !std::all_of(digits.begin(), digits.end(), IsHexDigit)) {
    return std::nullopt;
  }
  return std::stoi(digits, nullptr, 16);
}

absl::Status MergeAtlasAnnotationText(const std::string& text,
                                      const std::string& file_label,
                                      AtlasAnnotations* out,
                                      AnnotationFormat* format) {
  const DocReader reader(file_label);
  Json root;
  try {
    root = Json::parse(text);
  } catch (const Json::parse_error& error) {
    return reader.Error(
        "", absl::StrFormat("not valid JSON (byte %d): %s",
                            static_cast<int>(error.byte), error.what()));
  }
  auto detected = DetectFormat(reader, root);
  if (!detected.ok()) {
    return detected.status();
  }
  if (format != nullptr) {
    *format = *detected;
  }
  // Merge into a copy so a failing file leaves `out` unchanged.
  AtlasAnnotations merged = *out;
  absl::Status status;
  switch (*detected) {
    case AnnotationFormat::kAtlasV1:
      status = ReadAtlasV1(reader, root, &merged);
      break;
    case AnnotationFormat::kOracleRoomV0:
      status = ReadOracleRoomV0(reader, root, &merged);
      break;
    case AnnotationFormat::kOracleOverworldV0:
      status = ReadOracleOverworldV0(reader, root, &merged);
      break;
  }
  if (!status.ok()) {
    return status;
  }
  merged.files.push_back(file_label);
  *out = std::move(merged);
  return absl::OkStatus();
}

absl::Status MergeAtlasAnnotationFile(const std::string& path,
                                      AtlasAnnotations* out,
                                      AnnotationFormat* format) {
  std::ifstream file(path, std::ios::binary);
  if (!file) {
    return absl::NotFoundError(
        absl::StrFormat("Cannot read Atlas annotation file %s", path));
  }
  std::stringstream buffer;
  buffer << file.rdbuf();
  return MergeAtlasAnnotationText(
      buffer.str(), std::filesystem::path(path).filename().string(), out,
      format);
}

void ApplyVanillaAtlasDefaults(AtlasAnnotations* out) {
  for (const auto& entry : VanillaDungeonDefaults()) {
    DungeonAnnotation dungeon;
    dungeon.dungeon_id = entry.dungeon_id;
    dungeon.code = entry.code;
    dungeon.name = entry.name;
    dungeon.color = entry.color;
    dungeon.origin = Origin::Default();
    dungeon.color_origin = Origin::Default();
    MergeDungeon(out, std::move(dungeon));
  }
  out->vanilla_defaults = true;
}

absl::StatusOr<AtlasAnnotations> LoadAtlasAnnotations(
    const std::vector<std::string>& files, const std::string& defaults) {
  AtlasAnnotations annotations;
  if (defaults == "vanilla" || defaults.empty()) {
    ApplyVanillaAtlasDefaults(&annotations);
  } else if (defaults != "none") {
    return absl::InvalidArgumentError(absl::StrFormat(
        "[atlas] defaults must be \"vanilla\" or \"none\", got \"%s\"",
        defaults));
  }
  for (const auto& file : files) {
    auto status = MergeAtlasAnnotationFile(file, &annotations);
    if (!status.ok()) {
      return status;
    }
  }
  return annotations;
}

}  // namespace yaze::zelda3::atlas
