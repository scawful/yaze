#include "zelda3/atlas/atlas_json.h"

#include <cmath>

#include "absl/strings/str_format.h"
#include "nlohmann/json.hpp"

namespace yaze::zelda3::atlas {

namespace {

using Json = nlohmann::ordered_json;

std::string Hex(int id) {
  return absl::StrFormat("0x%02X", id);
}

Json HexOrNull(int id) {
  return id >= 0 ? Json(Hex(id)) : Json(nullptr);
}

Json RoomList(const std::vector<int>& rooms) {
  Json list = Json::array();
  for (int room : rooms) {
    list.push_back(Hex(room));
  }
  return list;
}

Json LinkJson(const AtlasLink& link, bool outgoing) {
  Json out;
  out["kind"] = LinkKindName(link.kind);
  if (!link.style.empty()) {
    out["style"] = link.style;
  }
  if (outgoing) {
    out["to"] = HexOrNull(link.to);
    out["cross_area"] = link.cross_area;
  }
  if (link.entrance >= 0) {
    out["entrance"] = Hex(link.entrance);
  }
  if (link.ow_map >= 0) {
    out["ow"] = Hex(link.ow_map);
  }
  switch (link.kind) {
    case LinkKind::kSpawn:
      out["spawn"] = link.index;
      break;
    case LinkKind::kStair:
    case LinkKind::kWarpTag:
      out["slot"] = link.index;
      break;
    case LinkKind::kSymbol:
      out["n"] = link.index;
      break;
    default:
      break;
  }
  if (!link.label.empty()) {
    out["label"] = link.label;
  }
  if (!link.note.empty()) {
    out["note"] = link.note;
  }
  out["source"] = link.origin.ToString();
  return out;
}

}  // namespace

std::string DungeonAtlasToJson(const DungeonAtlas& atlas,
                               const DungeonAtlasJsonContext& context,
                               int indent) {
  Json root;
  root["schema"] = kDungeonAtlasJsonSchema;
  root["rom"] = {{"name", atlas.rom.name},
                 {"sha1", atlas.rom.sha1},
                 {"size", atlas.rom.size}};
  root["project"] =
      context.project.empty() ? Json(nullptr) : Json(context.project);
  if (!context.generator.empty()) {
    root["generator"] = context.generator;
  }
  root["title"] = atlas.title;
  root["annotations"] = atlas.annotation_files;
  root["symbols"] = atlas.symbols.empty() ? Json(nullptr) : Json(atlas.symbols);
  root["owners_source"] =
      atlas.owners_from_project ? "project" : "derived_from_entrances";
  root["expanded_entrance_tables"] = atlas.expanded_entrance_tables;
  root["counts"] = {{"free", atlas.counts.free},
                    {"reclaimable", atlas.counts.reclaimable},
                    {"in_use", atlas.counts.in_use},
                    {"orphan", atlas.counts.orphan},
                    {"reserved", atlas.counts.reserved},
                    {"interior", atlas.counts.interior},
                    {"door_edges", atlas.edges.size()}};

  Json areas = Json::array();
  for (const auto& area : atlas.areas) {
    Json out;
    out["key"] = area.key;
    out["code"] = area.code;
    out["name"] = area.name;
    out["kind"] = area.kind;
    if (!area.owner.empty()) {
      out["owner"] = area.owner;
    }
    out["color"] = area.color.Hex();
    out["rooms"] = area.rooms;
    out["source"] = area.origin.ToString();
    out["color_source"] = area.color_origin.ToString();
    areas.push_back(std::move(out));
  }
  root["areas"] = std::move(areas);

  Json plans = Json::array();
  for (const auto& plan : atlas.plans) {
    Json out;
    out["key"] = plan.key;
    out["label"] = plan.label;
    out["name"] = plan.name;
    out["status"] = plan.status;
    out["color"] = plan.has_color ? Json(plan.color.Hex()) : Json(nullptr);
    out["rooms"] = RoomList(plan.rooms);
    if (!plan.note.empty()) {
      out["note"] = plan.note;
    }
    out["source"] = plan.origin.ToString();
    plans.push_back(std::move(out));
  }
  root["plans"] = std::move(plans);

  Json rooms = Json::array();
  for (const auto& room : atlas.rooms) {
    Json out;
    out["room"] = Hex(room.room_id);
    out["area"] = room.area;
    out["area_source"] = room.area_origin.ToString();
    out["owner"] = room.owner.empty() ? Json(nullptr) : Json(room.owner);
    out["owner_name"] = room.owner_name;
    out["name"] = room.name;
    out["name_source"] =
        room.name.empty() ? Json(nullptr) : Json(room.name_origin.ToString());
    if (!room.note.empty()) {
      out["note"] = room.note;
    }
    out["status"] = RoomStatusName(room.status);
    out["status_source"] = room.status_origin.ToString();
    out["reached"] = room.reached;
    out["empty"] = room.empty;
    out["interior"] = room.interior;
    out["objects"] = room.objects;
    out["sprites"] = room.sprites;
    out["vanilla_similarity"] =
        room.vanilla_similarity < 0.0f
            ? Json(nullptr)
            : Json(std::round(room.vanilla_similarity * 100.0) / 100.0);
    out["tags"] = {Hex(room.tag1), Hex(room.tag2)};
    out["blockset"] = room.blockset;
    out["pit_damage"] = room.pit_damage;
    out["plans"] = room.plans;
    out["reasons"] = room.reasons;
    Json doors = Json::array();
    for (const auto& door : room.doors) {
      doors.push_back({{"wall", std::string(1, door.wall)},
                       {"pos", door.pos},
                       {"type", Hex(door.type)},
                       {"type_name", door.type_name},
                       {"category", DoorCategoryName(door.category)}});
    }
    out["doors"] = std::move(doors);
    Json links_out = Json::array();
    for (const auto& link : room.links_out) {
      links_out.push_back(LinkJson(link, /*outgoing=*/true));
    }
    out["links_out"] = std::move(links_out);
    Json links_in = Json::array();
    for (const auto& link : room.links_in) {
      links_in.push_back(LinkJson(link, /*outgoing=*/false));
    }
    out["links_in"] = std::move(links_in);
    out["badges"] = {{"k_chest", room.badges.k_chest},
                     {"c", room.badges.c},
                     {"m", room.badges.m},
                     {"bk_chest", room.badges.bk_chest},
                     {"big_chest", room.badges.big_chest},
                     {"other", room.badges.other},
                     {"k_pot", room.badges.k_pot},
                     {"k_drop", room.badges.k_drop},
                     {"bk_drop", room.badges.bk_drop}};
    rooms.push_back(std::move(out));
  }
  root["rooms"] = std::move(rooms);

  Json edges = Json::array();
  for (const auto& edge : atlas.edges) {
    edges.push_back({{"a", Hex(edge.a)},
                     {"b", Hex(edge.b)},
                     {"dir", std::string(1, edge.dir)},
                     {"pos", edge.pos},
                     {"category", DoorCategoryName(edge.category)},
                     {"sides", edge.both_sides ? "both" : "a"},
                     {"one_sided_type", edge.one_sided_type},
                     {"types", edge.types}});
  }
  root["edges"] = std::move(edges);

  Json key_table = Json::array();
  for (const auto& row : atlas.key_table) {
    key_table.push_back({{"area", row.area},
                         {"k_chest", row.k_chest},
                         {"k_pot", row.k_pot},
                         {"k_drop", row.k_drop},
                         {"bk", row.bk},
                         {"bc", row.bc},
                         {"c", row.c},
                         {"m", row.m}});
  }
  root["key_table"] = std::move(key_table);
  root["legend_notes"] = atlas.legend_notes;
  root["warnings"] = atlas.warnings;
  return root.dump(indent);
}

}  // namespace yaze::zelda3::atlas
