#include "dungeon_canvas_viewer.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <functional>
#include <limits>
#include <map>
#include <queue>
#include <set>
#include <string>
#include <tuple>
#include <utility>

#include "absl/strings/str_format.h"
#include "app/editor/dungeon/dungeon_project_labels.h"
#include "zelda3/dungeon/draw_routines/draw_routine_registry.h"

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

bool IsHeaderBackedInterroomStaircaseObject(int16_t object_id) {
  const int routine_id =
      zelda3::DrawRoutineRegistry::Get().GetRoutineIdForObject(object_id);
  switch (routine_id) {
    case zelda3::DrawRoutineIds::kInterRoomFatStairsUp:
    case zelda3::DrawRoutineIds::kInterRoomFatStairsDownA:
    case zelda3::DrawRoutineIds::kInterRoomFatStairsDownB:
    case zelda3::DrawRoutineIds::kStraightInterRoomStairs:
    case zelda3::DrawRoutineIds::kSpiralStairsGoingUpUpper:
    case zelda3::DrawRoutineIds::kSpiralStairsGoingDownUpper:
    case zelda3::DrawRoutineIds::kSpiralStairsGoingUpLower:
    case zelda3::DrawRoutineIds::kSpiralStairsGoingDownLower:
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

// Dedup key for the connected-mode graph.
//
// Door / Holewarp links are undirected — emitting from either side produces
// the same logical edge — so we order the room pair (minmax) and treat both
// emissions as one. There can only be one of either type per room pair.
//
// Staircase links carry per-instance provenance (slot_index + object_id) and
// are *directed*: a stair object lives in exactly one room and points to
// another. Two stair objects in the same source room targeting the same
// destination, or two reciprocal stairs (A->B at slot 0 + B->A at slot 1),
// are distinct edges that must each be visible in the matrix. Including
// from_room/slot_index/object_id in the key preserves them.
std::tuple<int, int, DungeonConnectedLinkType, int, int16_t>
MakeConnectedLinkKey(const DungeonConnectedRoomLink& link) {
  if (link.type == DungeonConnectedLinkType::Staircase) {
    return std::make_tuple(link.from_room_id, link.to_room_id, link.type,
                           link.slot_index, link.object_id);
  }
  const auto ordered_rooms = std::minmax(link.from_room_id, link.to_room_id);
  return std::make_tuple(ordered_rooms.first, ordered_rooms.second, link.type,
                         -1, static_cast<int16_t>(-1));
}

}  // namespace

DungeonConnectedRoomLinkDiagnostics CollectDungeonConnectedRoomLinkDiagnostics(
    int room_id, const zelda3::Room& room,
    const std::function<bool(int, zelda3::DoorDirection)>&
        has_reciprocal_door) {
  DungeonConnectedRoomLinkDiagnostics result;

  const auto& doors = room.GetDoors();
  for (size_t door_index = 0; door_index < doors.size(); ++door_index) {
    const auto& door = doors[door_index];
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

    DungeonConnectedRoomLink link;
    link.from_room_id = room_id;
    link.to_room_id = neighbor;
    link.type = DungeonConnectedLinkType::Door;
    link.direction = door.direction;
    link.door_index = static_cast<int>(door_index);
    link.door_type = door.type;
    result.links.push_back(link);
  }

  // Walk placed header-backed staircase objects in placement order. The Nth
  // such object consumes header slot N (room.staircase_room(N)). Mismatches
  // are surfaced as diagnostic entries instead of being silently skipped:
  //
  //   - Slot consumed + valid header  → real Staircase link.
  //   - Slot consumed + zero/invalid header → MissingDestination diagnostic
  //     (placed object would be a dead-end stair at runtime).
  //   - >4 placed objects → ExtraPlacedObject diagnostic per surplus object.
  //   - Header non-zero but no consuming object → UnusedHeader diagnostic.
  //
  // ASSUMPTION (load-bearing for diagnostics): the placement-order → header-
  // slot-index mapping mirrors how the runtime walks Room_LoadDungeonState.
  // ZScream's `Dungeon_LoadStaircaseRooms` and the usdasm dungeon-load path
  // both consume `Object_Tile_Staircase*` writers in placement order and
  // index `staircase_rooms[]` with an internal counter. If any custom
  // sub-routine ever indexes the slot table by a parameter byte instead of
  // placement order, this mapping will misreport which placed object
  // collides with which header destination. Verify against the ROM with
  // `staircase_room_position_select.asm` (usdasm bank-01) when adding
  // ROM-backed parity tests; the synthetic AddObject fixtures here do not
  // exercise the real load path.
  std::array<bool, 4> slot_consumed{false, false, false, false};
  std::array<int16_t, 4> slot_object_id{
      static_cast<int16_t>(-1), static_cast<int16_t>(-1),
      static_cast<int16_t>(-1), static_cast<int16_t>(-1)};
  int next_slot = 0;
  for (const auto& object : room.GetTileObjects()) {
    if (!IsHeaderBackedInterroomStaircaseObject(object.id_)) {
      continue;
    }
    if (next_slot >= 4) {
      DungeonStaircaseIssue extra;
      extra.from_room_id = room_id;
      extra.kind = DungeonStaircaseIssueKind::ExtraPlacedObject;
      extra.object_id = object.id_;
      result.staircase_issues.push_back(extra);
      continue;
    }
    slot_consumed[next_slot] = true;
    slot_object_id[next_slot] = object.id_;
    ++next_slot;
  }

  for (int slot = 0; slot < 4; ++slot) {
    const int stair_room = static_cast<int>(room.staircase_room(slot));
    const bool header_valid =
        stair_room > 0 && stair_room < zelda3::kNumberOfRooms;
    if (slot_consumed[slot]) {
      if (header_valid) {
        DungeonConnectedRoomLink link;
        link.from_room_id = room_id;
        link.to_room_id = stair_room;
        link.type = DungeonConnectedLinkType::Staircase;
        link.direction = zelda3::DoorDirection::North;
        link.slot_index = slot;
        link.object_id = slot_object_id[slot];
        result.links.push_back(link);
      } else {
        DungeonStaircaseIssue issue;
        issue.from_room_id = room_id;
        issue.kind = DungeonStaircaseIssueKind::MissingDestination;
        issue.slot_index = slot;
        issue.header_room_id = stair_room;
        issue.object_id = slot_object_id[slot];
        result.staircase_issues.push_back(issue);
      }
    } else if (header_valid) {
      DungeonStaircaseIssue issue;
      issue.from_room_id = room_id;
      issue.kind = DungeonStaircaseIssueKind::UnusedHeader;
      issue.slot_index = slot;
      issue.header_room_id = stair_room;
      result.staircase_issues.push_back(issue);
    }
  }

  const int holewarp_room = static_cast<int>(room.holewarp());
  if (holewarp_room > 0 && holewarp_room < zelda3::kNumberOfRooms) {
    DungeonConnectedRoomLink link;
    link.from_room_id = room_id;
    link.to_room_id = holewarp_room;
    link.type = DungeonConnectedLinkType::Holewarp;
    link.direction = zelda3::DoorDirection::North;
    result.links.push_back(link);
  }

  return result;
}

std::vector<DungeonConnectedRoomLink> CollectDungeonConnectedRoomLinks(
    int room_id, const zelda3::Room& room,
    const std::function<bool(int, zelda3::DoorDirection)>&
        has_reciprocal_door) {
  return CollectDungeonConnectedRoomLinkDiagnostics(room_id, room,
                                                    has_reciprocal_door)
      .links;
}

namespace {

const char* DoorDirectionLabel(zelda3::DoorDirection dir) {
  switch (dir) {
    case zelda3::DoorDirection::North:
      return "North";
    case zelda3::DoorDirection::South:
      return "South";
    case zelda3::DoorDirection::West:
      return "West";
    case zelda3::DoorDirection::East:
      return "East";
  }
  return "?";
}

}  // namespace

std::string FormatDungeonConnectedLinkDescription(
    const DungeonConnectedRoomLink& link) {
  switch (link.type) {
    case DungeonConnectedLinkType::Door:
      return absl::StrFormat("Door (%s, type 0x%02X) -> [%03X]",
                             DoorDirectionLabel(link.direction),
                             static_cast<unsigned>(link.door_type),
                             static_cast<unsigned>(link.to_room_id));
    case DungeonConnectedLinkType::Staircase: {
      std::string object_str =
          (link.object_id >= 0)
              ? absl::StrFormat("0x%03X", static_cast<unsigned>(link.object_id))
              : std::string("(unknown)");
      const int slot_index = link.slot_index >= 0 ? link.slot_index : -1;
      if (slot_index < 0) {
        return absl::StrFormat("Staircase obj %s -> [%03X]", object_str,
                               static_cast<unsigned>(link.to_room_id));
      }
      return absl::StrFormat("Staircase slot %d obj %s -> [%03X]", slot_index,
                             object_str,
                             static_cast<unsigned>(link.to_room_id));
    }
    case DungeonConnectedLinkType::Holewarp:
      return absl::StrFormat("Holewarp -> [%03X]",
                             static_cast<unsigned>(link.to_room_id));
  }
  return absl::StrFormat("Link -> [%03X]",
                         static_cast<unsigned>(link.to_room_id));
}

std::string FormatDungeonStaircaseIssueDescription(
    const DungeonStaircaseIssue& issue) {
  switch (issue.kind) {
    case DungeonStaircaseIssueKind::UnusedHeader:
      return absl::StrFormat(
          "Stale staircase slot %d -> [%03X] (no placed interroom-stair "
          "object consumes this slot)",
          issue.slot_index, static_cast<unsigned>(issue.header_room_id));
    case DungeonStaircaseIssueKind::MissingDestination: {
      const std::string object_str =
          (issue.object_id >= 0)
              ? absl::StrFormat("0x%03X",
                                static_cast<unsigned>(issue.object_id))
              : std::string("(unknown)");
      const std::string header_str =
          (issue.header_room_id == 0)
              ? std::string("0 (unset)")
              : absl::StrFormat("0x%03X (out of range)",
                                static_cast<unsigned>(issue.header_room_id));
      return absl::StrFormat(
          "Missing staircase destination at slot %d (placed object %s, "
          "header value %s)",
          issue.slot_index, object_str, header_str);
    }
    case DungeonStaircaseIssueKind::ExtraPlacedObject: {
      const std::string object_str =
          (issue.object_id >= 0)
              ? absl::StrFormat("0x%03X",
                                static_cast<unsigned>(issue.object_id))
              : std::string("(unknown)");
      return absl::StrFormat(
          "Extra staircase object %s beyond the 4 header slots (runtime "
          "cannot reach this stair)",
          object_str);
    }
  }
  return std::string("Unknown staircase issue");
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

  graph.room_mask[static_cast<size_t>(start_room_id)] = true;
  graph.room_positions[static_cast<size_t>(start_room_id)] = {0, 0, true};
  graph.room_count = 1;
  graph.min_col = graph.max_col = 0;
  graph.min_row = graph.max_row = 0;

  // Project-aware scoping: when the project registry maps the start room to
  // a dungeon entry, restrict BFS to that dungeon's rooms by default.
  // Cross-blockset / cross-dungeon resolved links are still surfaced via
  // out_of_scope_links so the diagnostic remains visible without polluting
  // the visual matrix with rooms from other dungeons.
  std::set<int> scoped_room_ids;
  if (const auto* dungeon =
          dungeon_project_labels::FindDungeonForRoom(project_, start_room_id)) {
    graph.dungeon_scope_active = true;
    for (const auto& dungeon_room : dungeon->rooms) {
      scoped_room_ids.insert(dungeon_room.id);
    }
  }

  std::map<std::pair<int, int>, int> occupied_slots;
  occupied_slots[{0, 0}] = start_room_id;
  std::queue<int> to_visit;
  std::set<std::tuple<int, int, DungeonConnectedLinkType, int, int16_t>>
      seen_links;
  to_visit.push(start_room_id);

  auto track_room_bounds = [&](int room_id) {
    const auto& placement = graph.room_positions[static_cast<size_t>(room_id)];
    graph.min_col = std::min(graph.min_col, placement.col);
    graph.max_col = std::max(graph.max_col, placement.col);
    graph.min_row = std::min(graph.min_row, placement.row);
    graph.max_row = std::max(graph.max_row, placement.row);
  };

  auto in_dungeon_scope = [&](int candidate_room_id) {
    if (!graph.dungeon_scope_active) {
      return true;
    }
    return scoped_room_ids.find(candidate_room_id) != scoped_room_ids.end();
  };

  while (!to_visit.empty()) {
    const int room_id = to_visit.front();
    to_visit.pop();

    zelda3::Room* room = EnsureRoomLoadedForConnectedView(room_id);
    if (!room) {
      continue;
    }

    const auto outgoing_diagnostics =
        CollectDungeonConnectedRoomLinkDiagnostics(
            room_id, *room,
            [this](int neighbor_room_id, zelda3::DoorDirection dir) {
              return RoomHasNonExitDoorInDirection(neighbor_room_id, dir);
            });
    for (const auto& issue : outgoing_diagnostics.staircase_issues) {
      graph.staircase_issues.push_back(issue);
    }
    const auto& outgoing_links = outgoing_diagnostics.links;

    for (const auto& link : outgoing_links) {
      if (link.to_room_id < 0 || link.to_room_id >= zelda3::kNumberOfRooms) {
        continue;
      }

      // Out-of-scope link (target room is not in the current dungeon group):
      // record as a diagnostic, do not recurse, do not add the target to the
      // visual graph. We still de-dup against `seen_links` so a reciprocal
      // edge from the other side won't double-list it later.
      if (!in_dungeon_scope(link.to_room_id)) {
        if (seen_links.insert(MakeConnectedLinkKey(link)).second) {
          graph.out_of_scope_links.push_back(link);
        }
        continue;
      }

      if (!EnsureRoomLoadedForConnectedView(link.to_room_id)) {
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
