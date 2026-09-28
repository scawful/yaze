#include "cli/handlers/game/dungeon_graph_commands.h"

#include <algorithm>
#include <array>
#include <cstdint>
#include <optional>
#include <queue>
#include <set>
#include <string>
#include <vector>

#include "absl/status/statusor.h"
#include "absl/strings/numbers.h"
#include "absl/strings/str_format.h"
#include "cli/handlers/game/dungeon_census_commands.h"
#include "cli/handlers/game/dungeon_commands.h"
#include "cli/util/hex_util.h"
#include "core/hack_manifest.h"
#include "rom/rom.h"
#include "zelda3/dungeon/door_types.h"
#include "zelda3/dungeon/room.h"
#include "zelda3/dungeon/room_census.h"
#include "zelda3/dungeon/room_entrance.h"
#include "zelda3/dungeon/room_header_destination.h"
#include "zelda3/dungeon/room_links.h"

namespace yaze {
namespace cli {
namespace handlers {

using util::ParseHexString;

namespace {

// All three graph commands take their edges from zelda3::CollectRoomLinks,
// the same links the room census uses (room_links.h): entrance tables,
// header destination bytes (byte 0, out-of-range results), grid neighbors,
// teleport doors and live holes are decided in one place.

std::string RoomHex(int room_id) {
  return absl::StrFormat("0x%03X", room_id);
}

std::string RoomName(int room_id) {
  if (zelda3::IsDungeonRoomId(room_id)) {
    return std::string(zelda3::kRoomNames[room_id]);
  }
  return absl::StrFormat("Room 0x%03X", room_id);
}

const char* EntranceTableName(bool expanded) {
  return expanded ? "zscream_bank_0f" : "vanilla_bank_02";
}

// Links that come from header bytes: stair slots, warp tags, the holewarp,
// header-only bytes and teleport doors (stair slot 3 or 4).
bool UsesHeaderByte(const zelda3::RoomLink& link) {
  return link.kind != zelda3::RoomReferenceKind::kDoor;
}

// stair1-stair4 by slot, holewarp, or teleport_door.
std::string HeaderLinkType(const zelda3::RoomLink& link) {
  if (link.kind == zelda3::RoomReferenceKind::kTeleportDoor) {
    return "teleport_door";
  }
  if (link.slot >= 0) {
    return absl::StrFormat("stair%d", link.slot + 1);
  }
  return "holewarp";
}

void AddLinkObject(resources::OutputFormatter& formatter,
                   const zelda3::RoomLink& link) {
  formatter.BeginObject();
  formatter.AddField("from", RoomHex(link.from));
  formatter.AddField("to", RoomHex(link.to >= 0 ? link.to : link.raw_to));
  formatter.AddField("type", HeaderLinkType(link));
  formatter.AddField("kind", zelda3::RoomReferenceKindName(link.kind));
  formatter.AddField("strong", link.strong);
  formatter.AddField("detail", link.detail);
  formatter.EndObject();
}

// Header destinations past 0x127 are not rooms; list them instead of
// walking into them.
void AddOutOfRange(resources::OutputFormatter& formatter,
                   const std::vector<zelda3::RoomLink>& links) {
  formatter.BeginArray("out_of_range");
  for (const auto& link : links) {
    AddLinkObject(formatter, link);
  }
  formatter.EndArray();
}

// Warp tags from --project (the manifest's "WarpTag" room tags), the same
// input dungeon-room-census uses.
absl::StatusOr<std::set<uint8_t>> WarpTagsFromProject(
    const resources::ArgumentParser& parser, core::HackManifest* manifest,
    bool* loaded) {
  *loaded = false;
  auto project = parser.GetString("project");
  if (!project.has_value()) {
    return std::set<uint8_t>{};
  }
  if (auto status = LoadRoomCensusProject(*project, manifest); !status.ok()) {
    return status;
  }
  *loaded = true;
  return zelda3::RoomCensusWarpTagsFromManifest(*manifest);
}

absl::StatusOr<zelda3::DungeonEntranceTarget> EntranceStart(Rom* rom,
                                                            int entrance_id) {
  auto target = zelda3::ReadDungeonEntranceTarget(*rom, entrance_id);
  if (!target.has_value()) {
    return absl::OutOfRangeError(absl::StrFormat(
        "Entrance 0x%02X table entry is outside the ROM.", entrance_id));
  }
  if (!zelda3::IsDungeonRoomId(target->room_id)) {
    return absl::FailedPreconditionError(absl::StrFormat(
        "Entrance 0x%02X points at room 0x%04X, outside 0x000-0x127.",
        entrance_id, target->room_id));
  }
  return *target;
}

std::string DoorEdgeTypeName(int direction) {
  switch (direction) {
    case 0:
      return "door_north";
    case 1:
      return "door_south";
    case 2:
      return "door_west";
    case 3:
      return "door_east";
    default:
      return "door_unknown";
  }
}

}  // namespace

absl::Status DungeonGraphCommandHandler::Execute(
    Rom* rom, const resources::ArgumentParser& parser,
    resources::OutputFormatter& formatter) {
  auto room_id_opt = parser.GetString("room");
  auto dungeon_id_opt = parser.GetString("dungeon");

  int room_filter = -1;
  int dungeon_filter = -1;
  if (room_id_opt.has_value()) {
    if (!ParseHexString(room_id_opt.value(), &room_filter)) {
      return absl::InvalidArgumentError(
          "Invalid room ID format. Must be hex (e.g., 0x07).");
    }
    if (!zelda3::IsDungeonRoomId(room_filter)) {
      return absl::InvalidArgumentError(absl::StrFormat(
          "Room ID 0x%X out of range (0x000-0x127).", room_filter));
    }
  }
  if (dungeon_id_opt.has_value()) {
    if (!ParseHexString(dungeon_id_opt.value(), &dungeon_filter)) {
      return absl::InvalidArgumentError(
          "Invalid dungeon ID format. Must be hex (e.g., 0x02).");
    }
  }

  core::HackManifest manifest;
  bool has_project = false;
  auto warp_tags = WarpTagsFromProject(parser, &manifest, &has_project);
  if (!warp_tags.ok()) {
    return warp_tags.status();
  }

  // --dungeon: rooms the census assigns to that dungeon ID (entrances from
  // the table the game reads, flooded over the same links), not only rooms an
  // entrance leads into directly.
  std::vector<int> room_dungeon(zelda3::kDungeonRoomCount, -1);
  if (dungeon_filter >= 0) {
    zelda3::RoomCensusOptions options;
    options.manifest = has_project ? &manifest : nullptr;
    auto census = zelda3::ComputeRoomCensus(rom, options);
    if (!census.ok()) {
      return census.status();
    }
    for (const auto& entry : census->rooms) {
      if (entry.owner_index >= 0) {
        room_dungeon[entry.room_id] =
            census->owners[entry.owner_index].dungeon_id;
      }
    }
  }

  zelda3::RoomLinkFactsCache cache(rom);
  const auto lookup = cache.AsLookup();

  struct RoomNode {
    int room_id = 0;
    std::array<int, 4> stairs{};  // Destination ids, before any range check.
    int holewarp = 0;
    bool has_connections = false;
  };
  std::vector<RoomNode> nodes;
  std::vector<zelda3::RoomLink> edges;
  std::vector<zelda3::RoomLink> out_of_range;
  std::set<int> rooms_with_edges;

  const int start_room = (room_filter >= 0) ? room_filter : 0;
  const int end_room =
      (room_filter >= 0) ? room_filter : zelda3::kDungeonRoomCount - 1;
  for (int room_id = start_room; room_id <= end_room; ++room_id) {
    if (dungeon_filter >= 0 && room_dungeon[room_id] != dungeon_filter) {
      continue;
    }
    const zelda3::RoomLinkFacts* facts = cache.Get(room_id);
    if (facts == nullptr) {
      continue;
    }
    RoomNode node;
    node.room_id = room_id;
    for (int i = 0; i < 4; ++i) {
      node.stairs[i] =
          zelda3::ResolveHeaderDestinationRoom(room_id, facts->stair_bytes[i]);
    }
    node.holewarp =
        zelda3::ResolveHeaderDestinationRoom(room_id, facts->holewarp_byte);
    for (auto& link : zelda3::CollectRoomLinks(room_id, lookup, *warp_tags)) {
      if (!UsesHeaderByte(link)) {
        continue;
      }
      node.has_connections = true;
      rooms_with_edges.insert(room_id);
      if (link.to < 0) {
        out_of_range.push_back(std::move(link));
        continue;
      }
      rooms_with_edges.insert(link.to);
      edges.push_back(std::move(link));
    }
    nodes.push_back(node);
  }

  formatter.BeginObject("dungeon_graph");

  // Nodes: all rooms when filtering by room, otherwise connected ones.
  formatter.BeginArray("nodes");
  for (const auto& node : nodes) {
    if (room_filter >= 0 || node.has_connections ||
        rooms_with_edges.count(node.room_id)) {
      formatter.BeginObject();
      formatter.AddField("room_id", RoomHex(node.room_id));
      formatter.AddField("name", RoomName(node.room_id));
      formatter.BeginArray("stairs");
      for (int stair : node.stairs) {
        formatter.AddArrayItem(RoomHex(stair));
      }
      formatter.EndArray();
      formatter.AddField("holewarp", RoomHex(node.holewarp));
      formatter.EndObject();
    }
  }
  formatter.EndArray();

  formatter.BeginArray("edges");
  for (const auto& edge : edges) {
    AddLinkObject(formatter, edge);
  }
  formatter.EndArray();
  AddOutOfRange(formatter, out_of_range);

  int stair_edges = 0;
  int hole_edges = 0;
  int strong_edges = 0;
  for (const auto& edge : edges) {
    (HeaderLinkType(edge) == "holewarp" ? hole_edges : stair_edges)++;
    strong_edges += edge.strong ? 1 : 0;
  }
  formatter.BeginObject("stats");
  formatter.AddField("total_rooms_scanned",
                     static_cast<int>(end_room - start_room + 1));
  formatter.AddField("total_nodes", static_cast<int>(rooms_with_edges.size()));
  formatter.AddField("total_edges", static_cast<int>(edges.size()));
  formatter.AddField("staircase_connections", stair_edges);
  formatter.AddField("holewarp_connections", hole_edges);
  formatter.AddField("strong_edges", strong_edges);
  formatter.AddField("out_of_range_edges",
                     static_cast<int>(out_of_range.size()));
  formatter.EndObject();

  formatter.EndObject();
  return absl::OkStatus();
}

absl::Status EntranceInfoCommandHandler::Execute(
    Rom* rom, const resources::ArgumentParser& parser,
    resources::OutputFormatter& formatter) {
  auto entrance_id_str = parser.GetString("entrance").value();
  bool is_spawn_point = parser.HasFlag("spawn");

  int entrance_id;
  if (!ParseHexString(entrance_id_str, &entrance_id)) {
    return absl::InvalidArgumentError(
        "Invalid entrance ID format. Must be hex (e.g., 0x08).");
  }

  if (is_spawn_point) {
    return WriteDungeonSpawnPointReport(rom, entrance_id, formatter,
                                        "entrance");
  }

  // Validate entrance ID range
  if (entrance_id < 0 || entrance_id > 0x84) {
    return absl::InvalidArgumentError(absl::StrFormat(
        "Entrance ID 0x%02X out of range (0x00-0x84).", entrance_id));
  }

  zelda3::RoomEntrance entrance(rom, static_cast<uint8_t>(entrance_id), false);
  // Room and dungeon ID from the table the game reads (ZScream's bank $0F
  // copy when present), like dungeon-room-census.
  const auto target = zelda3::ReadDungeonEntranceTarget(*rom, entrance_id);
  if (!target.has_value()) {
    return absl::OutOfRangeError(absl::StrFormat(
        "Entrance 0x%02X table entry is outside the ROM.", entrance_id));
  }

  formatter.BeginObject("entrance");
  formatter.AddField("entrance_id", absl::StrFormat("0x%02X", entrance_id));
  formatter.AddField("is_spawn_point", is_spawn_point);
  formatter.AddField("entrance_table", EntranceTableName(target->expanded));
  formatter.AddField("room_id", absl::StrFormat("0x%03X", target->room_id));
  formatter.AddField("room_id_full",
                     absl::StrFormat("0x%04X", target->room_id));
  formatter.AddField("dungeon_id",
                     absl::StrFormat("0x%02X", target->dungeon_id));
  if (target->expanded) {
    // RoomEntrance still reads the fields below from the vanilla bank $02
    // tables; ZScream moves them to bank $0F too.
    formatter.AddField("vanilla_table_room_id",
                       absl::StrFormat("0x%03X", entrance.room_));
    formatter.AddField("other_fields_table", "vanilla_bank_02");
  }
  formatter.AddField("exit_id", absl::StrFormat("0x%04X", entrance.exit_));

  formatter.BeginObject("position");
  formatter.AddField("x", entrance.x_position_);
  formatter.AddField("y", entrance.y_position_);
  formatter.EndObject();

  formatter.BeginObject("camera");
  formatter.AddField("x", entrance.camera_x_);
  formatter.AddField("y", entrance.camera_y_);
  formatter.AddField("trigger_x", entrance.camera_trigger_x_);
  formatter.AddField("trigger_y", entrance.camera_trigger_y_);
  formatter.EndObject();

  formatter.BeginObject("properties");
  formatter.AddField("blockset", absl::StrFormat("0x%02X", entrance.blockset_));
  formatter.AddField("floor", absl::StrFormat("0x%02X", entrance.floor_));
  formatter.AddField("door", absl::StrFormat("0x%02X", entrance.door_));
  formatter.AddField("ladder_bg",
                     absl::StrFormat("0x%02X", entrance.ladder_bg_));
  formatter.AddField("scrolling",
                     absl::StrFormat("0x%02X", entrance.scrolling_));
  formatter.AddField("scroll_quadrant",
                     absl::StrFormat("0x%02X", entrance.scroll_quadrant_));
  formatter.AddField("music", absl::StrFormat("0x%02X", entrance.music_));
  formatter.EndObject();

  formatter.BeginObject("camera_boundaries");
  formatter.AddField("qn",
                     absl::StrFormat("0x%02X", entrance.camera_boundary_qn_));
  formatter.AddField("fn",
                     absl::StrFormat("0x%02X", entrance.camera_boundary_fn_));
  formatter.AddField("qs",
                     absl::StrFormat("0x%02X", entrance.camera_boundary_qs_));
  formatter.AddField("fs",
                     absl::StrFormat("0x%02X", entrance.camera_boundary_fs_));
  formatter.AddField("qw",
                     absl::StrFormat("0x%02X", entrance.camera_boundary_qw_));
  formatter.AddField("fw",
                     absl::StrFormat("0x%02X", entrance.camera_boundary_fw_));
  formatter.AddField("qe",
                     absl::StrFormat("0x%02X", entrance.camera_boundary_qe_));
  formatter.AddField("fe",
                     absl::StrFormat("0x%02X", entrance.camera_boundary_fe_));
  formatter.EndObject();

  formatter.EndObject();

  return absl::OkStatus();
}

absl::Status DungeonDiscoverCommandHandler::Execute(
    Rom* rom, const resources::ArgumentParser& parser,
    resources::OutputFormatter& formatter) {
  auto entrance_id_str = parser.GetString("entrance").value();
  auto depth_opt = parser.GetString("depth");

  int entrance_id;
  if (!ParseHexString(entrance_id_str, &entrance_id)) {
    return absl::InvalidArgumentError(
        "Invalid entrance ID format. Must be hex (e.g., 0x08).");
  }
  if (entrance_id < 0 || entrance_id >= zelda3::kDungeonEntranceCount) {
    return absl::InvalidArgumentError(absl::StrFormat(
        "Entrance ID 0x%02X out of range (0x00-0x84).", entrance_id));
  }

  int max_depth = 20;
  if (depth_opt.has_value()) {
    if (!absl::SimpleAtoi(depth_opt.value(), &max_depth)) {
      return absl::InvalidArgumentError(
          "Invalid depth format. Must be an integer between 1 and 100.");
    }
    if (max_depth < 1 || max_depth > 100) {
      return absl::InvalidArgumentError("Depth must be between 1 and 100.");
    }
  }

  core::HackManifest manifest;
  bool has_project = false;
  auto warp_tags = WarpTagsFromProject(parser, &manifest, &has_project);
  if (!warp_tags.ok()) {
    return warp_tags.status();
  }
  auto start = EntranceStart(rom, entrance_id);
  if (!start.ok()) {
    return start.status();
  }
  const int start_room = start->room_id;

  // BFS over stairs, warp tags, holes and teleport doors. Only links the
  // game can take are followed; header bytes nothing uses are listed.
  zelda3::RoomLinkFactsCache cache(rom);
  const auto lookup = cache.AsLookup();
  std::set<int> discovered_rooms{start_room};
  std::vector<zelda3::RoomLink> edges;
  std::vector<zelda3::RoomLink> out_of_range;
  std::queue<std::pair<int, int>> to_visit;  // (room_id, depth)
  to_visit.push({start_room, 0});
  while (!to_visit.empty()) {
    auto [current_room, current_depth] = to_visit.front();
    to_visit.pop();
    if (current_depth >= max_depth) {
      continue;
    }
    for (auto& link :
         zelda3::CollectRoomLinks(current_room, lookup, *warp_tags)) {
      if (!UsesHeaderByte(link)) {
        continue;
      }
      if (link.to < 0) {
        out_of_range.push_back(std::move(link));
        continue;
      }
      if (link.strong && discovered_rooms.insert(link.to).second) {
        to_visit.push({link.to, current_depth + 1});
      }
      edges.push_back(std::move(link));
    }
  }

  formatter.BeginObject("discovery");
  formatter.AddField("entrance_id", absl::StrFormat("0x%02X", entrance_id));
  formatter.AddField("entrance_table", EntranceTableName(start->expanded));
  formatter.AddField("start_room", RoomHex(start_room));
  formatter.AddField("dungeon_id",
                     absl::StrFormat("0x%02X", start->dungeon_id));
  formatter.AddField("max_depth", max_depth);
  formatter.AddField("rooms_discovered",
                     static_cast<int>(discovered_rooms.size()));

  formatter.BeginArray("discovered_rooms");
  for (int room_id : discovered_rooms) {
    formatter.BeginObject();
    formatter.AddField("room_id", RoomHex(room_id));
    formatter.AddField("name", RoomName(room_id));
    formatter.EndObject();
  }
  formatter.EndArray();

  formatter.BeginArray("connections");
  for (const auto& edge : edges) {
    AddLinkObject(formatter, edge);
  }
  formatter.EndArray();
  AddOutOfRange(formatter, out_of_range);

  formatter.EndObject();
  return absl::OkStatus();
}

absl::Status DungeonRoomGraphCommandHandler::Execute(
    Rom* rom, const resources::ArgumentParser& parser,
    resources::OutputFormatter& formatter) {
  auto entrance_id_str = parser.GetString("entrance").value();
  auto depth_opt = parser.GetString("depth");

  int entrance_id;
  if (!ParseHexString(entrance_id_str, &entrance_id)) {
    return absl::InvalidArgumentError(
        "Invalid entrance ID format. Must be hex (e.g., 0x27).");
  }
  if (entrance_id < 0 || entrance_id >= zelda3::kDungeonEntranceCount) {
    return absl::InvalidArgumentError(absl::StrFormat(
        "Entrance ID 0x%02X out of range (0x00-0x84).", entrance_id));
  }

  int max_depth = 50;
  if (depth_opt.has_value()) {
    if (!absl::SimpleAtoi(depth_opt.value(), &max_depth)) {
      return absl::InvalidArgumentError(
          "Invalid depth format. Must be an integer between 1 and 200.");
    }
    if (max_depth < 1 || max_depth > 200) {
      return absl::InvalidArgumentError("Depth must be between 1 and 200.");
    }
  }
  const bool same_blockset_filter = parser.HasFlag("same-blockset");

  core::HackManifest manifest;
  bool has_project = false;
  auto warp_tags = WarpTagsFromProject(parser, &manifest, &has_project);
  if (!warp_tags.ok()) {
    return warp_tags.status();
  }
  auto start = EntranceStart(rom, entrance_id);
  if (!start.ok()) {
    return start.status();
  }
  const int start_room = start->room_id;

  zelda3::RoomLinkFactsCache cache(rom);
  const auto lookup = cache.AsLookup();
  const zelda3::RoomLinkFacts* start_facts = cache.Get(start_room);
  const int start_blockset =
      start_facts != nullptr ? start_facts->blockset : -1;

  struct DoorEdge {
    int from_room = -1;
    std::string to;  // Room id, or exit / stairs / same_room / none.
    std::string type;
    std::string door_type_name;
    std::string role;
    std::string kind;
    std::string detail;
    int tile_x = 0;
    int tile_y = 0;
    bool is_exit = false;
    bool strong = false;
  };

  std::set<int> visited{start_room};
  std::vector<DoorEdge> door_edges;
  std::vector<zelda3::RoomLink> stair_edges;
  std::vector<zelda3::RoomLink> out_of_range;
  std::queue<std::pair<int, int>> to_visit;  // (room_id, depth)
  to_visit.push({start_room, 0});

  while (!to_visit.empty()) {
    auto [room_id, depth] = to_visit.front();
    to_visit.pop();
    if (depth >= max_depth) {
      continue;
    }
    const zelda3::RoomLinkFacts* facts = cache.Get(room_id);
    if (facts == nullptr) {
      continue;
    }
    const auto links = zelda3::CollectRoomLinks(room_id, lookup, *warp_tags);
    auto visit = [&](int target) {
      if (visited.insert(target).second) {
        to_visit.push({target, depth + 1});
      }
    };

    // Door edges: every door except layer/dungeon swap markers. Exits and
    // key-stair doors are listed but not followed (a key-stair door locks a
    // stair object; the stair is in stair_edges).
    for (size_t index = 0; index < facts->doors.size(); ++index) {
      const auto& door = facts->doors[index];
      const zelda3::RoomLink* link = nullptr;
      for (const auto& candidate : links) {
        if (candidate.door == static_cast<int>(index)) {
          link = &candidate;
          break;
        }
      }
      DoorEdge edge;
      edge.from_room = room_id;
      edge.type = DoorEdgeTypeName(door.direction);
      edge.door_type_name = std::string(
          zelda3::GetDoorTypeName(static_cast<zelda3::DoorType>(door.type)));
      edge.role = zelda3::RoomDoorRoleName(door.role);
      edge.tile_x = door.tile_x;
      edge.tile_y = door.tile_y;
      edge.is_exit = door.role == zelda3::RoomDoorRole::kExit;
      if (link != nullptr) {
        edge.to = link->to >= 0 ? RoomHex(link->to) : "none";
        edge.kind = zelda3::RoomReferenceKindName(link->kind);
        edge.strong = link->strong;
        edge.detail = link->detail;
      } else if (edge.is_exit) {
        edge.to = "exit";
      } else if (door.role == zelda3::RoomDoorRole::kKeyStairs) {
        edge.to = "stairs";
      } else if (!door.outer) {
        edge.to = "same_room";  // Middle wall: a quadrant change.
      } else {
        edge.to = "none";  // Would leave the room grid.
      }
      door_edges.push_back(edge);

      if (link == nullptr || !link->strong || link->to < 0) {
        continue;
      }
      // Optional: only follow grid-neighbor doors within the start blockset.
      if (same_blockset_filter &&
          link->kind == zelda3::RoomReferenceKind::kDoor) {
        const zelda3::RoomLinkFacts* next = cache.Get(link->to);
        if (next == nullptr || next->blockset != start_blockset) {
          continue;
        }
      }
      visit(link->to);
    }

    // Stair, warp tag and holewarp edges (teleport doors are door edges).
    for (const auto& link : links) {
      if (!UsesHeaderByte(link) ||
          link.kind == zelda3::RoomReferenceKind::kTeleportDoor) {
        continue;
      }
      if (link.to < 0) {
        out_of_range.push_back(link);
        continue;
      }
      stair_edges.push_back(link);
      if (link.strong) {
        visit(link.to);
      }
    }
  }

  formatter.BeginObject("room_graph");
  formatter.AddField("entrance_id", absl::StrFormat("0x%02X", entrance_id));
  formatter.AddField("entrance_table", EntranceTableName(start->expanded));
  formatter.AddField("start_room", RoomHex(start_room));
  formatter.AddField("dungeon_id",
                     absl::StrFormat("0x%02X", start->dungeon_id));
  formatter.AddField("rooms_discovered", static_cast<int>(visited.size()));

  formatter.BeginArray("rooms");
  for (int rid : visited) {
    formatter.BeginObject();
    formatter.AddField("room_id", RoomHex(rid));
    formatter.AddField("name", RoomName(rid));
    formatter.EndObject();
  }
  formatter.EndArray();

  formatter.BeginArray("door_edges");
  for (const auto& edge : door_edges) {
    formatter.BeginObject();
    formatter.AddField("from", RoomHex(edge.from_room));
    formatter.AddField("to", edge.to);
    formatter.AddField("type", edge.type);
    formatter.AddField("door_type", edge.door_type_name);
    formatter.AddField("role", edge.role);
    formatter.AddField("tile_x", edge.tile_x);
    formatter.AddField("tile_y", edge.tile_y);
    formatter.AddField("is_exit", edge.is_exit);
    formatter.AddField("strong", edge.strong);
    if (!edge.kind.empty()) {
      formatter.AddField("kind", edge.kind);
      formatter.AddField("detail", edge.detail);
    }
    formatter.EndObject();
  }
  formatter.EndArray();

  formatter.BeginArray("stair_edges");
  for (const auto& edge : stair_edges) {
    AddLinkObject(formatter, edge);
  }
  formatter.EndArray();
  AddOutOfRange(formatter, out_of_range);

  int exit_count = 0;
  for (const auto& edge : door_edges) {
    exit_count += edge.is_exit ? 1 : 0;
  }
  formatter.BeginObject("stats");
  formatter.AddField("door_edges", static_cast<int>(door_edges.size()));
  formatter.AddField("exit_doors", exit_count);
  formatter.AddField("stair_edges", static_cast<int>(stair_edges.size()));
  formatter.AddField("out_of_range_edges",
                     static_cast<int>(out_of_range.size()));
  formatter.EndObject();

  formatter.EndObject();
  return absl::OkStatus();
}

}  // namespace handlers
}  // namespace cli
}  // namespace yaze
