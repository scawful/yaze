#include "cli/handlers/game/overworld_graph_commands.h"

#include <algorithm>
#include <chrono>
#include <ctime>
#include <iomanip>
#include <map>
#include <set>
#include <sstream>
#include <string>
#include <vector>

#include "absl/status/status.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/str_format.h"
#include "cli/handlers/game/overworld_inspect.h"
#include "util/macro.h"
#include "zelda3/common.h"
#include "zelda3/dungeon/room_entrance.h"
#include "zelda3/overworld/overworld.h"
#include "zelda3/overworld/overworld_entrance.h"
#include "zelda3/overworld/overworld_exit.h"

namespace yaze {
namespace cli {
namespace handlers {

namespace {

constexpr int kDarkWorldOffset = 0x40;
constexpr int kSpecialWorldOffset = 0x80;

// Screen dimensions in pixels
constexpr int kScreenWidth = 512;
constexpr int kScreenHeight = 512;

// Maps per row in each world
constexpr int kMapsPerRow = 8;

std::string GetWorldName(int world) {
  switch (world) {
    case 0: return "light";
    case 1: return "dark";
    case 2: return "special";
    default: return "unknown";
  }
}

std::string GetAreaSizeName(zelda3::AreaSizeEnum size) {
  switch (size) {
    case zelda3::AreaSizeEnum::SmallArea: return "small";
    case zelda3::AreaSizeEnum::LargeArea: return "large";
    case zelda3::AreaSizeEnum::WideArea: return "wide";
    case zelda3::AreaSizeEnum::TallArea: return "tall";
    default: return "small";
  }
}

int WorldFromMapId(int map_id) {
  if (map_id >= kSpecialWorldOffset) return 2;
  if (map_id >= kDarkWorldOffset) return 1;
  return 0;
}

int LocalIndexFromMapId(int map_id) {
  int world = WorldFromMapId(map_id);
  switch (world) {
    case 0: return map_id;
    case 1: return map_id - kDarkWorldOffset;
    case 2: return map_id - kSpecialWorldOffset;
    default: return map_id;
  }
}

std::string GetDefaultAreaName(int map_id) {
  int world = WorldFromMapId(map_id);
  int local = LocalIndexFromMapId(map_id);
  int grid_x = local % kMapsPerRow;
  int grid_y = local / kMapsPerRow;

  std::string world_name;
  switch (world) {
    case 0: world_name = "Light World"; break;
    case 1: world_name = "Dark World"; break;
    case 2: world_name = "Special Area"; break;
    default: world_name = "Unknown"; break;
  }

  return absl::StrFormat("%s (%d,%d)", world_name, grid_x, grid_y);
}

std::string GetCurrentTimestamp() {
  auto now = std::chrono::system_clock::now();
  auto time = std::chrono::system_clock::to_time_t(now);
  std::stringstream ss;
  ss << std::put_time(std::gmtime(&time), "%Y-%m-%dT%H:%M:%SZ");
  return ss.str();
}

}  // namespace

absl::Status OverworldExportGraphCommandHandler::Execute(
    Rom* rom, const resources::ArgumentParser& parser,
    resources::OutputFormatter& formatter) {

  // Parse world filter
  std::optional<int> world_filter;
  if (auto world_str = parser.GetString("world")) {
    std::string w = *world_str;
    if (w == "light" || w == "0") {
      world_filter = 0;
    } else if (w == "dark" || w == "1") {
      world_filter = 1;
    } else if (w == "special" || w == "2") {
      world_filter = 2;
    } else if (w != "all") {
      return absl::InvalidArgumentError(
          absl::StrCat("Invalid world: ", w,
                       ". Use: light, dark, special, or all"));
    }
  }

  // Load overworld data
  zelda3::Overworld overworld(rom);
  RETURN_IF_ERROR(overworld.Load(rom));

  // Collect area information
  std::vector<AreaInfo> areas;
  std::map<int, int> map_to_parent;  // Track parent relationships

  for (int map_id = 0; map_id < zelda3::kNumOverworldMaps; ++map_id) {
    int world = WorldFromMapId(map_id);

    // Apply world filter
    if (world_filter.has_value() && world != *world_filter) {
      continue;
    }

    const auto* map = overworld.overworld_map(map_id);
    if (map == nullptr) continue;

    int local = LocalIndexFromMapId(map_id);
    int grid_x = local % kMapsPerRow;
    int grid_y = local / kMapsPerRow;

    // Calculate pixel bounds
    int min_x = grid_x * kScreenWidth;
    int min_y = grid_y * kScreenHeight;
    int max_x = min_x + kScreenWidth;
    int max_y = min_y + kScreenHeight;

    // Adjust for large/wide/tall maps
    auto area_size = map->area_size();
    if (area_size == zelda3::AreaSizeEnum::LargeArea) {
      max_x = min_x + kScreenWidth * 2;
      max_y = min_y + kScreenHeight * 2;
    } else if (area_size == zelda3::AreaSizeEnum::WideArea) {
      max_x = min_x + kScreenWidth * 2;
    } else if (area_size == zelda3::AreaSizeEnum::TallArea) {
      max_y = min_y + kScreenHeight * 2;
    }

    int parent = map->parent();
    map_to_parent[map_id] = parent;

    AreaInfo area;
    area.id = map_id;
    area.name = GetDefaultAreaName(map_id);
    area.world = world;
    area.grid_x = grid_x;
    area.grid_y = grid_y;
    area.size = GetAreaSizeName(area_size);
    area.parent_id = parent == map_id ? -1 : parent;  // -1 if no parent
    area.min_x = min_x;
    area.min_y = min_y;
    area.max_x = max_x;
    area.max_y = max_y;

    areas.push_back(area);
  }

  // Extract connections from transition tables
  std::vector<AreaConnection> connections;
  std::set<std::pair<int, int>> seen_connections;

  // Read north transition targets
  for (int map_id = 0; map_id < zelda3::kNumOverworldMaps; ++map_id) {
    int world = WorldFromMapId(map_id);
    if (world_filter.has_value() && world != *world_filter) continue;

    // North transitions
    ASSIGN_OR_RETURN(auto north_target,
        rom->ReadWord(zelda3::kTransitionTargetNorth + map_id * 2));
    int north_area = north_target & 0xFF;

    // Only add if target is different and valid
    if (north_area != map_id && north_area < zelda3::kNumOverworldMaps) {
      int target_world = WorldFromMapId(north_area);
      // Only connect within same world (unless special crossing)
      if (target_world == world || world == 2 || target_world == 2) {
        auto key = std::minmax(map_id, north_area);
        if (seen_connections.find(key) == seen_connections.end()) {
          seen_connections.insert(key);

          AreaConnection conn;
          conn.from_area = map_id;
          conn.to_area = north_area;
          conn.direction = "north";

          // Calculate edge position
          int local = LocalIndexFromMapId(map_id);
          int grid_x = local % kMapsPerRow;
          int grid_y = local / kMapsPerRow;
          conn.edge_x = grid_x * kScreenWidth + kScreenWidth / 2;
          conn.edge_y = grid_y * kScreenHeight;
          conn.bidirectional = true;

          connections.push_back(conn);
        }
      }
    }

    // West transitions
    ASSIGN_OR_RETURN(auto west_target,
        rom->ReadWord(zelda3::kTransitionTargetWest + map_id * 2));
    int west_area = west_target & 0xFF;

    if (west_area != map_id && west_area < zelda3::kNumOverworldMaps) {
      int target_world = WorldFromMapId(west_area);
      if (target_world == world || world == 2 || target_world == 2) {
        auto key = std::minmax(map_id, west_area);
        if (seen_connections.find(key) == seen_connections.end()) {
          seen_connections.insert(key);

          AreaConnection conn;
          conn.from_area = map_id;
          conn.to_area = west_area;
          conn.direction = "west";

          int local = LocalIndexFromMapId(map_id);
          int grid_x = local % kMapsPerRow;
          int grid_y = local / kMapsPerRow;
          conn.edge_x = grid_x * kScreenWidth;
          conn.edge_y = grid_y * kScreenHeight + kScreenHeight / 2;
          conn.bidirectional = true;

          connections.push_back(conn);
        }
      }
    }
  }

  // Also add implicit connections for adjacent areas
  for (int map_id = 0; map_id < zelda3::kNumOverworldMaps; ++map_id) {
    int world = WorldFromMapId(map_id);
    if (world_filter.has_value() && world != *world_filter) continue;

    int local = LocalIndexFromMapId(map_id);
    int grid_x = local % kMapsPerRow;
    int grid_y = local / kMapsPerRow;

    // Get world offset for neighbor calculation
    int world_offset = (world == 0) ? 0 : (world == 1) ? kDarkWorldOffset : kSpecialWorldOffset;
    int maps_in_world = (world == 2) ? 32 : 64;

    // East neighbor
    if (grid_x < kMapsPerRow - 1) {
      int east_local = local + 1;
      if (east_local < maps_in_world) {
        int east_area = world_offset + east_local;
        auto key = std::minmax(map_id, east_area);
        if (seen_connections.find(key) == seen_connections.end()) {
          seen_connections.insert(key);

          AreaConnection conn;
          conn.from_area = map_id;
          conn.to_area = east_area;
          conn.direction = "east";
          conn.edge_x = (grid_x + 1) * kScreenWidth;
          conn.edge_y = grid_y * kScreenHeight + kScreenHeight / 2;
          conn.bidirectional = true;

          connections.push_back(conn);
        }
      }
    }

    // South neighbor
    int rows_in_world = maps_in_world / kMapsPerRow;
    if (grid_y < rows_in_world - 1) {
      int south_local = local + kMapsPerRow;
      if (south_local < maps_in_world) {
        int south_area = world_offset + south_local;
        auto key = std::minmax(map_id, south_area);
        if (seen_connections.find(key) == seen_connections.end()) {
          seen_connections.insert(key);

          AreaConnection conn;
          conn.from_area = map_id;
          conn.to_area = south_area;
          conn.direction = "south";
          conn.edge_x = grid_x * kScreenWidth + kScreenWidth / 2;
          conn.edge_y = (grid_y + 1) * kScreenHeight;
          conn.bidirectional = true;

          connections.push_back(conn);
        }
      }
    }
  }

  // Collect entrance information
  std::vector<EntranceInfo> entrances;

  const auto& ow_entrances = overworld.entrances();
  for (size_t i = 0; i < ow_entrances.size(); ++i) {
    const auto& entrance = ow_entrances[i];
    if (entrance.deleted) continue;

    int area_id = entrance.map_id_ & 0xFF;
    int world = WorldFromMapId(area_id);

    if (world_filter.has_value() && world != *world_filter) continue;

    EntranceInfo info;
    info.id = static_cast<int>(i);
    info.area_id = area_id;
    info.position_x = entrance.x_;
    info.position_y = entrance.y_;
    info.is_hole = entrance.is_hole_;

    // Get entrance name from labels
    constexpr size_t kNumEntranceLabels =
        sizeof(zelda3::kEntranceNames) / sizeof(zelda3::kEntranceNames[0]);
    if (entrance.entrance_id_ < kNumEntranceLabels) {
      info.name = zelda3::kEntranceNames[entrance.entrance_id_];
    } else {
      info.name = absl::StrFormat("Entrance %d", entrance.entrance_id_);
    }

    // Read destination room ID from ROM
    // kEntranceRoom is 2 bytes per entrance
    uint8_t entrance_id = entrance.entrance_id_;
    ASSIGN_OR_RETURN(auto room_id,
        rom->ReadWord(zelda3::kEntranceRoom + entrance_id * 2));
    info.dest_room_id = room_id;

    entrances.push_back(info);
  }

  // Also add holes
  const auto& holes = overworld.holes();
  for (size_t i = 0; i < holes.size(); ++i) {
    const auto& hole = holes[i];

    int area_id = hole.map_id_ & 0xFF;
    int world = WorldFromMapId(area_id);

    if (world_filter.has_value() && world != *world_filter) continue;

    EntranceInfo info;
    info.id = static_cast<int>(ow_entrances.size() + i);
    info.area_id = area_id;
    info.position_x = hole.x_;
    info.position_y = hole.y_;
    info.is_hole = true;

    constexpr size_t kNumEntranceLabels =
        sizeof(zelda3::kEntranceNames) / sizeof(zelda3::kEntranceNames[0]);
    if (hole.entrance_id_ < kNumEntranceLabels) {
      info.name = absl::StrFormat("%s (Hole)",
          zelda3::kEntranceNames[hole.entrance_id_]);
    } else {
      info.name = absl::StrFormat("Hole %d", hole.entrance_id_);
    }

    uint8_t entrance_id = hole.entrance_id_;
    ASSIGN_OR_RETURN(auto room_id,
        rom->ReadWord(zelda3::kEntranceRoom + entrance_id * 2));
    info.dest_room_id = room_id;

    entrances.push_back(info);
  }

  // Collect exit information
  std::vector<ExitInfo> exits;
  const auto* ow_exits = overworld.exits();

  if (ow_exits != nullptr) {
    for (const auto& exit : *ow_exits) {
      int area_id = exit.map_id_ & 0xFF;
      int world = WorldFromMapId(area_id);

      if (world_filter.has_value() && world != *world_filter) continue;

      ExitInfo info;
      info.room_id = exit.room_id_;
      info.name = absl::StrFormat("Exit from room 0x%02X", exit.room_id_);
      info.return_area_id = area_id;
      info.return_x = exit.x_;
      info.return_y = exit.y_;

      exits.push_back(info);
    }
  }

  // Output the graph
  formatter.BeginObject("world_graph");

  formatter.AddField("version", "1.0.0");
  formatter.AddField("generated", GetCurrentTimestamp());
  formatter.AddField("rom_name", rom->title());

  // Areas array
  formatter.BeginArray("areas");
  for (const auto& area : areas) {
    formatter.BeginObject();
    formatter.AddField("id", area.id);
    formatter.AddField("name", area.name);
    formatter.AddField("world", GetWorldName(area.world));

    formatter.BeginObject("grid");
    formatter.AddField("x", area.grid_x);
    formatter.AddField("y", area.grid_y);
    formatter.EndObject();

    formatter.AddField("size", area.size);

    if (area.parent_id >= 0) {
      formatter.AddField("parent_id", area.parent_id);
    }

    formatter.BeginObject("bounds");
    formatter.AddField("min_x", area.min_x);
    formatter.AddField("min_y", area.min_y);
    formatter.AddField("max_x", area.max_x);
    formatter.AddField("max_y", area.max_y);
    formatter.EndObject();

    formatter.EndObject();
  }
  formatter.EndArray();

  // Connections array
  formatter.BeginArray("connections");
  for (const auto& conn : connections) {
    formatter.BeginObject();
    formatter.AddField("from_area", conn.from_area);
    formatter.AddField("to_area", conn.to_area);
    formatter.AddField("direction", conn.direction);

    formatter.BeginObject("edge");
    formatter.AddField("x", conn.edge_x);
    formatter.AddField("y", conn.edge_y);
    formatter.EndObject();

    formatter.AddField("bidirectional", conn.bidirectional);
    formatter.EndObject();
  }
  formatter.EndArray();

  // Entrances array
  formatter.BeginArray("entrances");
  for (const auto& entrance : entrances) {
    formatter.BeginObject();
    formatter.AddField("id", entrance.id);
    formatter.AddField("name", entrance.name);
    formatter.AddField("area_id", entrance.area_id);

    formatter.BeginObject("position");
    formatter.AddField("x", entrance.position_x);
    formatter.AddField("y", entrance.position_y);
    formatter.EndObject();

    formatter.AddField("dest_room_id", entrance.dest_room_id);
    formatter.AddField("is_hole", entrance.is_hole);
    formatter.EndObject();
  }
  formatter.EndArray();

  // Exits array
  formatter.BeginArray("exits");
  for (const auto& exit : exits) {
    formatter.BeginObject();
    formatter.AddField("room_id", exit.room_id);
    formatter.AddField("name", exit.name);
    formatter.AddField("return_area_id", exit.return_area_id);

    formatter.BeginObject("return_position");
    formatter.AddField("x", exit.return_x);
    formatter.AddField("y", exit.return_y);
    formatter.EndObject();

    formatter.EndObject();
  }
  formatter.EndArray();

  // Summary stats
  formatter.BeginObject("stats");
  formatter.AddField("total_areas", static_cast<int>(areas.size()));
  formatter.AddField("total_connections", static_cast<int>(connections.size()));
  formatter.AddField("total_entrances", static_cast<int>(entrances.size()));
  formatter.AddField("total_exits", static_cast<int>(exits.size()));
  formatter.EndObject();

  formatter.EndObject();

  return absl::OkStatus();
}

}  // namespace handlers
}  // namespace cli
}  // namespace yaze
