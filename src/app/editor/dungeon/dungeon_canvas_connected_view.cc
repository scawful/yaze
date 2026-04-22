#include "dungeon_canvas_viewer.h"

#include <algorithm>
#include <cmath>
#include <functional>
#include <limits>
#include <map>
#include <queue>
#include <set>
#include <tuple>
#include <utility>

namespace yaze::editor {

namespace {

bool IsExitDoorType(zelda3::DoorType type) {
  switch (type) {
    case zelda3::DoorType::FancyDungeonExit:
    case zelda3::DoorType::FancyDungeonExitLower:
    case zelda3::DoorType::CaveExit:
    case zelda3::DoorType::LitCaveExitLower:
    case zelda3::DoorType::ExitLower:
    case zelda3::DoorType::UnusedCaveExit:
    case zelda3::DoorType::BombableCaveExit:
    case zelda3::DoorType::WaterfallDoor:
    case zelda3::DoorType::ExitMarker:
      return true;
    default:
      return false;
  }
}

std::pair<int, int> ConnectedDoorDelta(zelda3::DoorDirection dir) {
  switch (dir) {
    case zelda3::DoorDirection::North:
      return {0, -1};
    case zelda3::DoorDirection::South:
      return {0, 1};
    case zelda3::DoorDirection::West:
      return {-1, 0};
    case zelda3::DoorDirection::East:
      return {1, 0};
  }
  return {0, 0};
}

bool IsConnectedSlotOccupied(
    const std::map<std::pair<int, int>, int>& occupied_slots, int col,
    int row) {
  return occupied_slots.contains({col, row});
}

std::pair<int, int> FindConnectedDoorPlacement(
    const std::map<std::pair<int, int>, int>& occupied_slots, int source_col,
    int source_row, zelda3::DoorDirection dir) {
  const auto [dx, dy] = ConnectedDoorDelta(dir);
  int best_score = std::numeric_limits<int>::max();
  std::pair<int, int> best = {source_col + dx, source_row + dy};

  for (int radius = 1; radius <= 24; ++radius) {
    for (int row = source_row - radius; row <= source_row + radius; ++row) {
      for (int col = source_col - radius; col <= source_col + radius; ++col) {
        if (IsConnectedSlotOccupied(occupied_slots, col, row)) {
          continue;
        }

        const int rel_col = col - source_col;
        const int rel_row = row - source_row;
        const int forward = (rel_col * dx) + (rel_row * dy);
        if (forward <= 0) {
          continue;
        }

        const int lateral = std::abs((rel_col * dy) - (rel_row * dx));
        const int distance_from_ideal =
            std::abs(rel_col - dx) + std::abs(rel_row - dy);
        const int overshoot = std::max(0, forward - 1);
        const int score =
            (distance_from_ideal * 16) + (lateral * 20) + (overshoot * 6);
        if (score < best_score) {
          best_score = score;
          best = {col, row};
        }
      }
    }
    if (best_score != std::numeric_limits<int>::max()) {
      return best;
    }
  }

  return best;
}

std::pair<int, int> FindConnectedTransportPlacement(
    const std::map<std::pair<int, int>, int>& occupied_slots, int source_col,
    int source_row) {
  int best_score = std::numeric_limits<int>::max();
  std::pair<int, int> best = {source_col + 1, source_row + 1};

  for (int radius = 1; radius <= 24; ++radius) {
    for (int row = source_row - radius; row <= source_row + radius; ++row) {
      for (int col = source_col - radius; col <= source_col + radius; ++col) {
        if (IsConnectedSlotOccupied(occupied_slots, col, row)) {
          continue;
        }

        const int rel_col = col - source_col;
        const int rel_row = row - source_row;
        const int distance = std::abs(rel_col) + std::abs(rel_row);
        if (distance == 0) {
          continue;
        }

        const bool diagonal = rel_col != 0 && rel_row != 0;
        const bool axis_aligned = rel_col == 0 || rel_row == 0;
        const int distance_bias = std::abs(distance - 2);
        const int score = (distance_bias * 10) +
                          (diagonal       ? 0
                           : axis_aligned ? 8
                                          : 4) +
                          ((std::abs(rel_col) + std::abs(rel_row)) > 3 ? 4 : 0);
        if (score < best_score) {
          best_score = score;
          best = {col, row};
        }
      }
    }
    if (best_score != std::numeric_limits<int>::max()) {
      return best;
    }
  }

  return best;
}

std::tuple<int, int, DungeonConnectedLinkType> MakeConnectedLinkKey(
    const DungeonConnectedRoomLink& link) {
  const auto ordered_rooms = std::minmax(link.from_room_id, link.to_room_id);
  return std::make_tuple(ordered_rooms.first, ordered_rooms.second, link.type);
}

}  // namespace

std::vector<DungeonConnectedRoomLink> CollectDungeonConnectedRoomLinks(
    int room_id, const zelda3::Room& room,
    const std::function<bool(int, zelda3::DoorDirection)>&
        has_reciprocal_door) {
  std::vector<DungeonConnectedRoomLink> links;

  for (const auto& door : room.GetDoors()) {
    if (IsExitDoorType(door.type)) {
      continue;
    }

    const int neighbor = NeighborRoomId(room_id, door.direction);
    if (neighbor < 0) {
      continue;
    }
    if (has_reciprocal_door &&
        !has_reciprocal_door(neighbor, OppositeDir(door.direction))) {
      continue;
    }

    links.push_back(DungeonConnectedRoomLink{
        room_id, neighbor, DungeonConnectedLinkType::Door, door.direction});
  }

  for (int i = 0; i < 4; ++i) {
    const int stair_room = static_cast<int>(room.staircase_room(i));
    if (stair_room <= 0 || stair_room >= zelda3::kNumberOfRooms) {
      continue;
    }
    links.push_back(DungeonConnectedRoomLink{
        room_id, stair_room, DungeonConnectedLinkType::Staircase,
        zelda3::DoorDirection::North});
  }

  const int holewarp_room = static_cast<int>(room.holewarp());
  if (holewarp_room > 0 && holewarp_room < zelda3::kNumberOfRooms) {
    links.push_back(DungeonConnectedRoomLink{room_id, holewarp_room,
                                             DungeonConnectedLinkType::Holewarp,
                                             zelda3::DoorDirection::North});
  }

  return links;
}

zelda3::Room* DungeonCanvasViewer::EnsureRoomLoadedForConnectedView(
    int room_id) {
  if (!rooms_ || !rom_ || !rom_->is_loaded() || room_id < 0 ||
      room_id >= zelda3::kNumberOfRooms) {
    return nullptr;
  }

  auto& room = (*rooms_)[room_id];
  room.SetRom(rom_);
  room.SetGameData(game_data_);
  if (!room.IsLoaded()) {
    room = zelda3::LoadRoomFromRom(rom_, room_id);
    room.SetGameData(game_data_);
  }
  return &room;
}

bool DungeonCanvasViewer::RoomHasNonExitDoorInDirection(
    int room_id, zelda3::DoorDirection dir) {
  zelda3::Room* room = EnsureRoomLoadedForConnectedView(room_id);
  if (!room) {
    return false;
  }

  for (const auto& door : room->GetDoors()) {
    if (door.direction == dir && !IsExitDoorType(door.type)) {
      return true;
    }
  }
  return false;
}

DungeonCanvasViewer::ConnectedRoomGraphData
DungeonCanvasViewer::BuildConnectedRoomGraph(int start_room_id) {
  ConnectedRoomGraphData graph;
  if (start_room_id < 0 || start_room_id >= zelda3::kNumberOfRooms) {
    return graph;
  }

  zelda3::Room* start_room = EnsureRoomLoadedForConnectedView(start_room_id);
  if (!start_room) {
    return graph;
  }

  const uint8_t start_blockset = start_room->blockset();
  graph.room_mask[static_cast<size_t>(start_room_id)] = true;
  graph.room_positions[static_cast<size_t>(start_room_id)] = {0, 0, true};
  graph.room_count = 1;
  graph.min_col = graph.max_col = 0;
  graph.min_row = graph.max_row = 0;

  std::map<std::pair<int, int>, int> occupied_slots;
  occupied_slots[{0, 0}] = start_room_id;
  std::queue<int> to_visit;
  std::set<std::tuple<int, int, DungeonConnectedLinkType>> seen_links;
  to_visit.push(start_room_id);

  auto room_matches_cluster = [&](int room_id) {
    zelda3::Room* room = EnsureRoomLoadedForConnectedView(room_id);
    return room != nullptr && room->blockset() == start_blockset;
  };

  auto track_room_bounds = [&](int room_id) {
    const auto& placement = graph.room_positions[static_cast<size_t>(room_id)];
    graph.min_col = std::min(graph.min_col, placement.col);
    graph.max_col = std::max(graph.max_col, placement.col);
    graph.min_row = std::min(graph.min_row, placement.row);
    graph.max_row = std::max(graph.max_row, placement.row);
  };

  while (!to_visit.empty()) {
    const int room_id = to_visit.front();
    to_visit.pop();

    zelda3::Room* room = EnsureRoomLoadedForConnectedView(room_id);
    if (!room || room->blockset() != start_blockset) {
      continue;
    }

    const auto outgoing_links = CollectDungeonConnectedRoomLinks(
        room_id, *room,
        [this](int neighbor_room_id, zelda3::DoorDirection dir) {
          return RoomHasNonExitDoorInDirection(neighbor_room_id, dir);
        });

    for (const auto& link : outgoing_links) {
      if (link.to_room_id < 0 || link.to_room_id >= zelda3::kNumberOfRooms) {
        continue;
      }
      if (!room_matches_cluster(link.to_room_id)) {
        continue;
      }

      if (seen_links.insert(MakeConnectedLinkKey(link)).second) {
        graph.links.push_back(link);
      }

      if (!graph.room_mask[static_cast<size_t>(link.to_room_id)]) {
        const auto& source_placement =
            graph.room_positions[static_cast<size_t>(room_id)];
        const std::pair<int, int> placement =
            (link.type == DungeonConnectedLinkType::Door)
                ? FindConnectedDoorPlacement(
                      occupied_slots, source_placement.col,
                      source_placement.row, link.direction)
                : FindConnectedTransportPlacement(occupied_slots,
                                                  source_placement.col,
                                                  source_placement.row);
        graph.room_mask[static_cast<size_t>(link.to_room_id)] = true;
        graph.room_positions[static_cast<size_t>(link.to_room_id)] = {
            placement.first, placement.second, true};
        occupied_slots[placement] = link.to_room_id;
        ++graph.room_count;
        track_room_bounds(link.to_room_id);
        to_visit.push(link.to_room_id);
      }
    }
  }

  return graph;
}

}  // namespace yaze::editor
