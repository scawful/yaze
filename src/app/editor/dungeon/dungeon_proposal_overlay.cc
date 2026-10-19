#include "app/editor/dungeon/dungeon_proposal_overlay.h"

#include <algorithm>
#include <cmath>
#include <set>
#include <string>

#include "absl/status/status.h"
#include "absl/strings/str_format.h"
#include "nlohmann/json.hpp"

namespace yaze::editor {
namespace {

using json = nlohmann::json;

absl::Status Invalid(const std::string& where, const std::string& what) {
  return absl::InvalidArgumentError(
      absl::StrFormat("proposal overlay %s: %s", where, what));
}

absl::StatusOr<int> ParseInteger(const json& value, const std::string& where,
                                 const char* name, int min, int max) {
  if (!value.is_number_integer() || value < min || value > max) {
    return Invalid(where, absl::StrFormat("%s must be an integer in %d..%d",
                                          name, min, max));
  }
  return value.get<int>();
}

// Accepts 135, "135", "0x87", or "$87".
absl::StatusOr<int> ParseRoomId(const json& value, const std::string& where,
                                int max_room_id) {
  int room = -1;
  if (value.is_number_integer()) {
    auto number = ParseInteger(value, where, "room id", 0, max_room_id);
    if (!number.ok())
      return number.status();
    room = *number;
  } else if (value.is_string()) {
    std::string text = value.get<std::string>();
    int base = 10;
    if (text.rfind("0x", 0) == 0 || text.rfind("0X", 0) == 0) {
      text = text.substr(2);
      base = 16;
    } else if (!text.empty() && text[0] == '$') {
      text = text.substr(1);
      base = 16;
    }
    if (text.empty()) {
      return Invalid(where, "empty room id");
    }
    size_t used = 0;
    try {
      room = std::stoi(text, &used, base);
    } catch (...) {
      return Invalid(where, "room id is not a number");
    }
    if (used != text.size()) {
      return Invalid(where, "room id is not a number");
    }
  } else {
    return Invalid(where, "room id must be a number or string");
  }
  if (room < 0 || room > max_room_id) {
    return Invalid(where, absl::StrFormat("room 0x%X is out of range", room));
  }
  return room;
}

absl::StatusOr<ProposalTile> ParseTile(const json& value,
                                       const std::string& where) {
  if (!value.is_array() || value.size() != 2 || !value[0].is_number_integer() ||
      !value[1].is_number_integer()) {
    return Invalid(where, "tile must be [x, y] integers");
  }
  auto x = ParseInteger(value[0], where, "tile x", 0, kProposalRoomTiles - 1);
  auto y = ParseInteger(value[1], where, "tile y", 0, kProposalRoomTiles - 1);
  if (!x.ok())
    return x.status();
  if (!y.ok())
    return y.status();
  return ProposalTile{*x, *y};
}

std::string OptionalString(const json& object, const char* key) {
  auto it = object.find(key);
  if (it == object.end() || !it->is_string()) {
    return {};
  }
  return it->get<std::string>();
}

absl::StatusOr<ProposalShape> ParseShape(const json& value,
                                         const std::string& where,
                                         const std::set<int>& rooms,
                                         int max_room_id) {
  if (!value.is_object()) {
    return Invalid(where, "shape must be an object");
  }
  ProposalShape shape;
  auto room_it = value.find("room");
  if (room_it == value.end()) {
    return Invalid(where, "missing \"room\"");
  }
  auto room = ParseRoomId(*room_it, where, max_room_id);
  if (!room.ok()) {
    return room.status();
  }
  if (!rooms.contains(*room)) {
    return Invalid(
        where, absl::StrFormat("room 0x%X is not listed in \"rooms\"", *room));
  }
  shape.room_id = *room;
  shape.label = OptionalString(value, "label");
  shape.detail = OptionalString(value, "detail");

  const std::string type = OptionalString(value, "type");
  if (type == "rect") {
    shape.type = ProposalShapeType::kRect;
    auto it = value.find("tiles");
    if (it == value.end() || !it->is_array() || it->size() != 4) {
      return Invalid(where, "rect needs \"tiles\": [x0, y0, x1, y1]");
    }
    auto a = ParseTile(json::array({(*it)[0], (*it)[1]}), where);
    auto b = ParseTile(json::array({(*it)[2], (*it)[3]}), where);
    if (!a.ok())
      return a.status();
    if (!b.ok())
      return b.status();
    if (b->x < a->x || b->y < a->y) {
      return Invalid(where,
                     "rect corners must be [x0, y0, x1, y1] with "
                     "x0<=x1 and y0<=y1");
    }
    shape.tiles = {*a, *b};
  } else if (type == "path") {
    shape.type = ProposalShapeType::kPath;
    auto it = value.find("tiles");
    if (it == value.end() || !it->is_array() || it->size() < 2) {
      return Invalid(where, "path needs \"tiles\": [[x, y], [x, y], ...]");
    }
    for (const auto& point : *it) {
      auto tile = ParseTile(point, where);
      if (!tile.ok())
        return tile.status();
      shape.tiles.push_back(*tile);
    }
  } else if (type == "marker" || type == "remove" || type == "actor" ||
             type == "object") {
    shape.type = type == "marker" ? ProposalShapeType::kMarker
                                  : ProposalShapeType::kRemove;
    auto it = value.find("tile");
    if (it == value.end()) {
      return Invalid(where, "missing \"tile\": [x, y]");
    }
    auto tile = ParseTile(*it, where);
    if (!tile.ok())
      return tile.status();
    shape.tiles = {*tile};
    if (type == "actor") {
      shape.type = ProposalShapeType::kActor;
      const std::string facing = OptionalString(value, "facing");
      if (facing == "north")
        shape.facing = ProposalFacing::kNorth;
      else if (facing == "east")
        shape.facing = ProposalFacing::kEast;
      else if (facing == "south")
        shape.facing = ProposalFacing::kSouth;
      else if (facing == "west")
        shape.facing = ProposalFacing::kWest;
      else
        return Invalid(where,
                       "actor needs facing: north, east, south, or west");
      if (auto offset = value.find("offset"); offset != value.end()) {
        auto parsed = ParseTile(*offset, where + " offset");
        if (!parsed.ok())
          return parsed.status();
        if (parsed->x >= kProposalTilePixels ||
            parsed->y >= kProposalTilePixels) {
          return Invalid(where, "actor offset must be [x, y] pixels in 0..7");
        }
        shape.pixel_offset = *parsed;
      }
    } else if (type == "object") {
      shape.type = ProposalShapeType::kObject;
      auto id = value.find("object_id");
      if (id == value.end())
        return Invalid(where, "object needs object_id");
      // Object IDs use the same decimal/$hex/0xhex notation as room IDs.
      auto parsed_id = ParseRoomId(*id, where + " object_id", 0xFFF);
      if (!parsed_id.ok())
        return parsed_id.status();
      if (!(*parsed_id <= 0xF7 ||
            (*parsed_id >= 0x100 && *parsed_id <= 0x13F) ||
            *parsed_id >= 0xF80)) {
        return Invalid(where, "object_id is not an encodable dungeon object");
      }
      shape.object_id = *parsed_id;
      for (const char* field : {"size", "list"}) {
        if (auto entry = value.find(field); entry != value.end()) {
          const bool is_size = std::string_view(field) == "size";
          auto number =
              ParseInteger(*entry, where, field, 0, is_size ? 255 : 2);
          if (!number.ok())
            return number.status();
          (is_size ? shape.object_size : shape.object_list) = *number;
        }
      }
      auto footprint = value.find("footprint");
      if (footprint == value.end() || !footprint->is_array() ||
          footprint->size() != 2) {
        return Invalid(where,
                       "object needs footprint: [width, height] in tiles");
      }
      auto width = ParseInteger((*footprint)[0], where, "footprint width", 1,
                                kProposalRoomTiles - tile->x);
      auto height = ParseInteger((*footprint)[1], where, "footprint height", 1,
                                 kProposalRoomTiles - tile->y);
      if (!width.ok())
        return width.status();
      if (!height.ok())
        return height.status();
      shape.footprint = {*width, *height};
    }
  } else {
    return Invalid(where,
                   absl::StrFormat("unknown shape type \"%s\" (use rect, path, "
                                   "marker, remove, actor, object)",
                                   type));
  }
  return shape;
}

}  // namespace

absl::StatusOr<uint32_t> ParseProposalColor(std::string_view text) {
  // absl::StrFormat gets std::string, not std::string_view: Ubuntu 22.04's
  // Abseil (20210324, absl::string_view != std::string_view) cannot format a
  // std::string_view argument.
  const std::string shown(text);
  if (text.size() != 7 && text.size() != 9) {
    return absl::InvalidArgumentError(
        absl::StrFormat("color \"%s\" must be #RRGGBB or #RRGGBBAA", shown));
  }
  if (text[0] != '#') {
    return absl::InvalidArgumentError(
        absl::StrFormat("color \"%s\" must start with #", shown));
  }
  uint32_t value = 0;
  for (size_t i = 1; i < text.size(); ++i) {
    const char c = text[i];
    int digit = -1;
    if (c >= '0' && c <= '9')
      digit = c - '0';
    if (c >= 'a' && c <= 'f')
      digit = c - 'a' + 10;
    if (c >= 'A' && c <= 'F')
      digit = c - 'A' + 10;
    if (digit < 0) {
      return absl::InvalidArgumentError(
          absl::StrFormat("color \"%s\" has a non-hex digit", shown));
    }
    value = (value << 4) | static_cast<uint32_t>(digit);
  }
  if (text.size() == 7) {
    value = (value << 8) | 0xFF;
  }
  return value;
}

absl::StatusOr<DungeonProposalOverlay> ParseDungeonProposalOverlay(
    std::string_view json_text, int max_room_id) {
  json root = json::parse(json_text.begin(), json_text.end(), nullptr,
                          /*allow_exceptions=*/false);
  if (root.is_discarded()) {
    return absl::InvalidArgumentError("proposal overlay is not valid JSON");
  }
  if (!root.is_object()) {
    return Invalid("root", "must be an object");
  }
  if (OptionalString(root, "format") != kProposalOverlayFormat) {
    return Invalid("root",
                   absl::StrFormat("\"format\" must be \"%s\"",
                                   std::string(kProposalOverlayFormat)));
  }
  auto version = root.find("version");
  if (version == root.end() || !version->is_number_integer() ||
      *version != kProposalOverlayVersion) {
    return Invalid("root",
                   absl::StrFormat("unsupported \"version\" (expected %d)",
                                   kProposalOverlayVersion));
  }

  DungeonProposalOverlay overlay;
  overlay.title = OptionalString(root, "title");
  overlay.status = OptionalString(root, "status");
  if (auto notes = root.find("notes");
      notes != root.end() && notes->is_array()) {
    for (const auto& note : *notes) {
      if (note.is_string())
        overlay.notes.push_back(note.get<std::string>());
    }
  }

  auto rooms = root.find("rooms");
  if (rooms == root.end() || !rooms->is_array() || rooms->empty()) {
    return Invalid("root", "\"rooms\" must be a non-empty list");
  }
  std::set<int> room_ids;
  for (size_t i = 0; i < rooms->size(); ++i) {
    const std::string where = absl::StrFormat("rooms[%d]", i);
    const json& entry = (*rooms)[i];
    const json& id =
        entry.is_object() && entry.contains("room") ? entry["room"] : entry;
    auto room = ParseRoomId(id, where, max_room_id);
    if (!room.ok())
      return room.status();
    if (!room_ids.insert(*room).second) {
      return Invalid(where,
                     absl::StrFormat("room 0x%X is listed twice", *room));
    }
    overlay.rooms.push_back(
        {*room, entry.is_object() ? OptionalString(entry, "label") : ""});
  }

  auto layers = root.find("layers");
  if (layers == root.end() || !layers->is_array()) {
    return Invalid("root", "\"layers\" must be a list");
  }
  std::set<std::string> layer_ids;
  for (size_t li = 0; li < layers->size(); ++li) {
    const json& entry = (*layers)[li];
    std::string where = absl::StrFormat("layers[%d]", li);
    if (!entry.is_object()) {
      return Invalid(where, "must be an object");
    }
    ProposalLayer layer;
    layer.id = OptionalString(entry, "id");
    if (layer.id.empty()) {
      return Invalid(where, "missing \"id\"");
    }
    if (!layer_ids.insert(layer.id).second) {
      return Invalid(where,
                     absl::StrFormat("duplicate layer id \"%s\"", layer.id));
    }
    where = absl::StrFormat("layer \"%s\"", layer.id);
    layer.label = OptionalString(entry, "label");
    if (layer.label.empty())
      layer.label = layer.id;
    if (auto color = entry.find("color"); color != entry.end()) {
      if (!color->is_string()) {
        return Invalid(where, "\"color\" must be a string");
      }
      auto rgba = ParseProposalColor(color->get<std::string>());
      if (!rgba.ok()) {
        return Invalid(where, std::string(rgba.status().message()));
      }
      layer.color_rgba = *rgba;
    }
    if (auto visible = entry.find("visible");
        visible != entry.end() && visible->is_boolean()) {
      layer.visible = visible->get<bool>();
    }
    auto shapes = entry.find("shapes");
    if (shapes == entry.end() || !shapes->is_array()) {
      return Invalid(where, "\"shapes\" must be a list");
    }
    for (size_t si = 0; si < shapes->size(); ++si) {
      auto shape =
          ParseShape((*shapes)[si], absl::StrFormat("%s shapes[%d]", where, si),
                     room_ids, max_room_id);
      if (!shape.ok())
        return shape.status();
      layer.shapes.push_back(std::move(*shape));
    }
    overlay.layers.push_back(std::move(layer));
  }
  if (auto steps = root.find("steps"); steps != root.end()) {
    if (!steps->is_array())
      return Invalid("root", "steps must be a list");
    std::set<std::string> step_ids;
    for (size_t i = 0; i < steps->size(); ++i) {
      const auto& entry = (*steps)[i];
      const std::string where = absl::StrFormat("steps[%d]", i);
      if (!entry.is_object())
        return Invalid(where, "must be an object");
      ProposalStep step;
      step.id = OptionalString(entry, "id");
      if (step.id.empty() || !step_ids.insert(step.id).second)
        return Invalid(where, "step id must be non-empty and unique");
      step.label = OptionalString(entry, "label");
      if (step.label.empty())
        step.label = step.id;
      step.detail = OptionalString(entry, "detail");
      auto refs = entry.find("layers");
      if (refs == entry.end() || !refs->is_array() || refs->empty())
        return Invalid(where, "layers must be a non-empty list of layer ids");
      std::set<std::string> referenced;
      for (const auto& ref : *refs) {
        if (!ref.is_string())
          return Invalid(where, "layer reference must be a string");
        const auto id = ref.get<std::string>();
        if (!layer_ids.contains(id))
          return Invalid(where, absl::StrFormat("unknown layer id \"%s\"", id));
        if (!referenced.insert(id).second)
          return Invalid(where,
                         absl::StrFormat("duplicate layer id \"%s\"", id));
        step.layer_ids.push_back(id);
      }
      overlay.steps.push_back(std::move(step));
    }
  }
  return overlay;
}

ProposalPixelRect ProposalTileRectToPixels(ProposalTile a, ProposalTile b) {
  const int x0 = std::min(a.x, b.x);
  const int y0 = std::min(a.y, b.y);
  const int x1 = std::max(a.x, b.x) + 1;
  const int y1 = std::max(a.y, b.y) + 1;
  return {static_cast<float>(x0 * kProposalTilePixels),
          static_cast<float>(y0 * kProposalTilePixels),
          static_cast<float>(x1 * kProposalTilePixels),
          static_cast<float>(y1 * kProposalTilePixels)};
}

void ProposalTileCenter(ProposalTile tile, float* x, float* y) {
  *x = (tile.x + 0.5f) * kProposalTilePixels;
  *y = (tile.y + 0.5f) * kProposalTilePixels;
}

ProposalPixelRect ProposalFootprintToPixels(const ProposalShape& shape) {
  const bool actor = shape.type == ProposalShapeType::kActor;
  const float x = shape.tiles[0].x * kProposalTilePixels +
                  (actor ? shape.pixel_offset.x : 0);
  const float y = shape.tiles[0].y * kProposalTilePixels +
                  (actor ? shape.pixel_offset.y : 0);
  return {x, y, x + (actor ? 16 : shape.footprint.x * kProposalTilePixels),
          y + (actor ? 16 : shape.footprint.y * kProposalTilePixels)};
}

bool ProposalLayerInStep(const DungeonProposalOverlay& overlay,
                         std::string_view layer_id, int step) {
  if (step < 0)
    return true;
  if (static_cast<size_t>(step) >= overlay.steps.size())
    return false;
  const auto& ids = overlay.steps[step].layer_ids;
  return std::find(ids.begin(), ids.end(), layer_id) != ids.end();
}

float ProposalRoomOriginX(int index, float gap_pixels) {
  return index * (kProposalRoomTiles * kProposalTilePixels + gap_pixels);
}

float ProposalDistanceToSegment(float px, float py, float ax, float ay,
                                float bx, float by) {
  const float dx = bx - ax;
  const float dy = by - ay;
  const float len2 = dx * dx + dy * dy;
  float t = 0.0f;
  if (len2 > 0.0f) {
    t = std::clamp(((px - ax) * dx + (py - ay) * dy) / len2, 0.0f, 1.0f);
  }
  const float cx = ax + t * dx - px;
  const float cy = ay + t * dy - py;
  return std::sqrt(cx * cx + cy * cy);
}

bool ProposalShapeHitTest(const ProposalShape& shape, float px, float py,
                          float tolerance) {
  switch (shape.type) {
    case ProposalShapeType::kActor:
    case ProposalShapeType::kObject: {
      const auto r = ProposalFootprintToPixels(shape);
      return px >= r.x0 && px < r.x1 && py >= r.y0 && py < r.y1;
    }
    case ProposalShapeType::kRect: {
      const auto r = ProposalTileRectToPixels(shape.tiles[0], shape.tiles[1]);
      return px >= r.x0 && px < r.x1 && py >= r.y0 && py < r.y1;
    }
    case ProposalShapeType::kPath: {
      for (size_t i = 1; i < shape.tiles.size(); ++i) {
        float ax, ay, bx, by;
        ProposalTileCenter(shape.tiles[i - 1], &ax, &ay);
        ProposalTileCenter(shape.tiles[i], &bx, &by);
        if (ProposalDistanceToSegment(px, py, ax, ay, bx, by) <= tolerance) {
          return true;
        }
      }
      return false;
    }
    case ProposalShapeType::kMarker:
    case ProposalShapeType::kRemove: {
      float cx, cy;
      ProposalTileCenter(shape.tiles[0], &cx, &cy);
      const float dx = px - cx;
      const float dy = py - cy;
      return std::sqrt(dx * dx + dy * dy) <= tolerance;
    }
  }
  return false;
}

}  // namespace yaze::editor
