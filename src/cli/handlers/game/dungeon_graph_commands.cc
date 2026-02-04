#include "cli/handlers/game/dungeon_graph_commands.h"

#include <cstdint>
#include <set>
#include <string>
#include <vector>

#include "absl/strings/str_format.h"
#include "cli/util/hex_util.h"
#include "rom/rom.h"
#include "zelda3/dungeon/room.h"
#include "zelda3/dungeon/room_entrance.h"

namespace yaze {
namespace cli {
namespace handlers {

using util::ParseHexString;

namespace {

// Edge types for room connections
constexpr const char* kEdgeTypeStair1 = "stair1";
constexpr const char* kEdgeTypeStair2 = "stair2";
constexpr const char* kEdgeTypeStair3 = "stair3";
constexpr const char* kEdgeTypeStair4 = "stair4";
constexpr const char* kEdgeTypeHolewarp = "holewarp";

struct RoomNode {
  int room_id;
  std::string name;
  uint8_t staircase_rooms[4];
  uint8_t holewarp;
  bool has_connections;
};

struct RoomEdge {
  int from_room;
  int to_room;
  std::string type;
};

// Get the dungeon ID for a room by checking entrances
int GetRoomDungeonId(Rom* rom, int room_id) {
  // Scan entrances to find one that leads to this room
  // This is an approximation - some rooms may not have direct entrances
  for (int i = 0; i < 0x84; ++i) {
    zelda3::RoomEntrance entrance(rom, static_cast<uint8_t>(i), false);
    if (entrance.room_ == room_id) {
      return entrance.dungeon_id_;
    }
  }
  return -1;  // Unknown dungeon
}

}  // namespace

absl::Status DungeonGraphCommandHandler::Execute(
    Rom* rom, const resources::ArgumentParser& parser,
    resources::OutputFormatter& formatter) {
  // Parse optional filters
  auto room_id_opt = parser.GetString("room");
  auto dungeon_id_opt = parser.GetString("dungeon");

  int room_filter = -1;
  int dungeon_filter = -1;

  if (room_id_opt.has_value()) {
    if (!ParseHexString(room_id_opt.value(), &room_filter)) {
      return absl::InvalidArgumentError(
          "Invalid room ID format. Must be hex (e.g., 0x07).");
    }
  }

  if (dungeon_id_opt.has_value()) {
    if (!ParseHexString(dungeon_id_opt.value(), &dungeon_filter)) {
      return absl::InvalidArgumentError(
          "Invalid dungeon ID format. Must be hex (e.g., 0x02).");
    }
  }

  // Build the graph
  std::vector<RoomNode> nodes;
  std::vector<RoomEdge> edges;
  std::set<int> rooms_with_edges;

  // Determine scan range
  int start_room = (room_filter >= 0) ? room_filter : 0;
  int end_room = (room_filter >= 0) ? room_filter : zelda3::NumberOfRooms - 1;

  for (int room_id = start_room; room_id <= end_room; ++room_id) {
    // Load room header to get staircase and holewarp data
    zelda3::Room room = zelda3::LoadRoomHeaderFromRom(rom, room_id);

    // Skip if filtering by dungeon
    if (dungeon_filter >= 0) {
      int room_dungeon = GetRoomDungeonId(rom, room_id);
      if (room_dungeon != dungeon_filter) {
        continue;
      }
    }

    RoomNode node;
    node.room_id = room_id;
    // Bounds check for kRoomNames (array size is 297)
    if (room_id >= 0 && room_id < 297) {
      node.name = std::string(zelda3::kRoomNames[room_id]);
    } else {
      node.name = absl::StrFormat("Room 0x%02X", room_id);
    }
    node.holewarp = room.holewarp;
    node.has_connections = false;

    // Extract staircase destinations
    for (int i = 0; i < 4; ++i) {
      node.staircase_rooms[i] = room.staircase_room(i);

      // Create edge if destination is valid (non-zero)
      if (node.staircase_rooms[i] != 0) {
        RoomEdge edge;
        edge.from_room = room_id;
        edge.to_room = node.staircase_rooms[i];

        switch (i) {
          case 0:
            edge.type = kEdgeTypeStair1;
            break;
          case 1:
            edge.type = kEdgeTypeStair2;
            break;
          case 2:
            edge.type = kEdgeTypeStair3;
            break;
          case 3:
            edge.type = kEdgeTypeStair4;
            break;
        }

        edges.push_back(edge);
        node.has_connections = true;
        rooms_with_edges.insert(room_id);
        rooms_with_edges.insert(node.staircase_rooms[i]);
      }
    }

    // Create holewarp edge if valid
    if (node.holewarp != 0) {
      RoomEdge edge;
      edge.from_room = room_id;
      edge.to_room = node.holewarp;
      edge.type = kEdgeTypeHolewarp;
      edges.push_back(edge);
      node.has_connections = true;
      rooms_with_edges.insert(room_id);
      rooms_with_edges.insert(node.holewarp);
    }

    nodes.push_back(node);
  }

  // Output the graph
  formatter.BeginObject("dungeon_graph");

  // Nodes array - only include nodes with connections for cleaner output
  formatter.BeginArray("nodes");
  for (const auto& node : nodes) {
    // Include all nodes if filtering by room, otherwise only connected ones
    if (room_filter >= 0 || node.has_connections ||
        rooms_with_edges.count(node.room_id)) {
      formatter.BeginObject();
      formatter.AddField("room_id", absl::StrFormat("0x%02X", node.room_id));
      formatter.AddField("name", node.name);

      // Staircase array
      formatter.BeginArray("stairs");
      for (int i = 0; i < 4; ++i) {
        formatter.AddArrayItem(
            absl::StrFormat("0x%02X", node.staircase_rooms[i]));
      }
      formatter.EndArray();

      formatter.AddField("holewarp", absl::StrFormat("0x%02X", node.holewarp));
      formatter.EndObject();
    }
  }
  formatter.EndArray();

  // Edges array
  formatter.BeginArray("edges");
  for (const auto& edge : edges) {
    formatter.BeginObject();
    formatter.AddField("from", absl::StrFormat("0x%02X", edge.from_room));
    formatter.AddField("to", absl::StrFormat("0x%02X", edge.to_room));
    formatter.AddField("type", edge.type);
    formatter.EndObject();
  }
  formatter.EndArray();

  // Statistics
  formatter.BeginObject("stats");
  formatter.AddField("total_rooms_scanned",
                     static_cast<int>(end_room - start_room + 1));
  formatter.AddField("total_nodes", static_cast<int>(rooms_with_edges.size()));
  formatter.AddField("total_edges", static_cast<int>(edges.size()));

  // Count edge types
  int stair_edges = 0;
  int hole_edges = 0;
  for (const auto& edge : edges) {
    if (edge.type == kEdgeTypeHolewarp) {
      hole_edges++;
    } else {
      stair_edges++;
    }
  }
  formatter.AddField("staircase_connections", stair_edges);
  formatter.AddField("holewarp_connections", hole_edges);
  formatter.EndObject();

  formatter.EndObject();

  return absl::OkStatus();
}

}  // namespace handlers
}  // namespace cli
}  // namespace yaze
