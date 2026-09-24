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

// Accepts 135, "135", "0x87", or "$87".
absl::StatusOr<int> ParseRoomId(const json& value, const std::string& where,
                                int max_room_id) {
  int room = -1;
  if (value.is_number_integer()) {
    room = value.get<int>();
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
  ProposalTile tile{value[0].get<int>(), value[1].get<int>()};
  if (tile.x < 0 || tile.x >= kProposalRoomTiles || tile.y < 0 ||
      tile.y >= kProposalRoomTiles) {
    return Invalid(where, absl::StrFormat("tile (%d,%d) is outside the room",
                                          tile.x, tile.y));
  }
  return tile;
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
  } else if (type == "marker" || type == "remove") {
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
  } else {
    return Invalid(where,
                   absl::StrFormat("unknown shape type \"%s\" (use rect, path, "
                                   "marker, remove)",
                                   type));
  }
  return shape;
}

}  // namespace

absl::StatusOr<uint32_t> ParseProposalColor(std::string_view text) {
  if (text.size() != 7 && text.size() != 9) {
    return absl::InvalidArgumentError(
        absl::StrFormat("color \"%s\" must be #RRGGBB or #RRGGBBAA", text));
  }
  if (text[0] != '#') {
    return absl::InvalidArgumentError(
        absl::StrFormat("color \"%s\" must start with #", text));
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
          absl::StrFormat("color \"%s\" has a non-hex digit", text));
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
    return Invalid("root", absl::StrFormat("\"format\" must be \"%s\"",
                                           kProposalOverlayFormat));
  }
  auto version = root.find("version");
  if (version == root.end() || !version->is_number_integer() ||
      version->get<int>() != kProposalOverlayVersion) {
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
