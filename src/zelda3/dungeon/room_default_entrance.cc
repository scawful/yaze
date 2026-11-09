#include "zelda3/dungeon/room_default_entrance.h"

#include <algorithm>
#include <map>
#include <set>
#include <utility>

#include "rom/rom.h"
#include "zelda3/dungeon/dungeon_rom_addresses.h"
#include "zelda3/dungeon/dungeon_spawn_point.h"
#include "zelda3/dungeon/room.h"
#include "zelda3/dungeon/room_entrance.h"
#include "zelda3/game_data.h"
#include "zelda3/screen/dungeon_map.h"

namespace yaze::zelda3 {
namespace {

constexpr uint8_t kEmptyMapRoom = 0x0F;

}  // namespace

const char* RoomEntranceSourceName(RoomEntranceSource source) {
  switch (source) {
    case RoomEntranceSource::kNone:
      return "none";
    case RoomEntranceSource::kDirect:
      return "direct";
    case RoomEntranceSource::kDungeonMap:
      return "dungeon_map";
    case RoomEntranceSource::kNeighbour:
      return "neighbour";
  }
  return "none";
}

int SelectRoomEntranceCandidate(
    const std::vector<RoomEntranceCandidate>& candidates,
    uint8_t header_blockset) {
  for (size_t i = 0; i < candidates.size(); ++i) {
    if (candidates[i].main_blockset == header_blockset) {
      return static_cast<int>(i);
    }
  }
  return candidates.empty() ? -1 : 0;
}

int DungeonBackgroundPaletteOffset(uint8_t main_blockset,
                                   int destination_slot) {
  if (destination_slot < 0 || destination_slot >= 8)
    return 0;
  const int runtime_slot = 7 - destination_slot;
  const bool right = main_blockset < 0x20
                         ? runtime_slot >= 4
                         : runtime_slot == 2 || runtime_slot == 3 ||
                               runtime_slot == 4 || runtime_slot == 7;
  return right ? 8 : 0;
}

RoomMainBlocksetResolution ResolveRoomMainBlockset(
    int room_id, uint8_t header_blockset, uint8_t requested_main_blockset,
    const GameData& game_data) {
  const auto valid = [&game_data](uint8_t group) {
    return group != 0xFF && group < game_data.main_blockset_ids.size();
  };
  if (valid(requested_main_blockset)) {
    return {requested_main_blockset, RoomMainBlocksetSource::kExplicitPreview};
  }
  if (room_id >= 0 &&
      room_id < static_cast<int>(game_data.room_default_entrances.size())) {
    const auto& entrance = game_data.room_default_entrances[room_id];
    if (valid(entrance.main_blockset)) {
      return {entrance.main_blockset,
              entrance.source == RoomEntranceSource::kDirect
                  ? RoomMainBlocksetSource::kDirectDefault
                  : RoomMainBlocksetSource::kInferredDefault};
    }
  }
  if (valid(header_blockset)) {
    return {header_blockset, RoomMainBlocksetSource::kHeaderFallback};
  }
  return {0, RoomMainBlocksetSource::kZeroFallback};
}

std::vector<RoomDefaultEntrance> ComputeRoomDefaultEntrances(Rom& rom) {
  std::vector<RoomDefaultEntrance> result(kNumberOfRooms);
  if (!rom.is_loaded()) {
    return result;
  }
  if (!ResolveEntranceLayout(rom.vector()).ok()) {
    // Do not infer defaults from the populated but inactive legacy tables.
    return result;
  }

  struct EntranceInfo {
    int id;
    int room;
    uint8_t main_blockset;
    uint8_t dungeon_id;
  };
  std::vector<EntranceInfo> entrances;
  entrances.reserve(kNumRegularDungeonEntrances);
  for (int id = 0; id < kNumRegularDungeonEntrances; ++id) {
    RoomEntrance entrance(&rom, static_cast<uint8_t>(id), false);
    if (!entrance.load_status().ok()) {
      continue;
    }
    entrances.push_back(
        {id, entrance.room_, entrance.blockset_, entrance.dungeon_id_});
  }

  // 1. Direct entrances.
  for (const EntranceInfo& entrance : entrances) {
    if (entrance.room < 0 || entrance.room >= kNumberOfRooms) {
      continue;
    }
    RoomDefaultEntrance& slot = result[static_cast<size_t>(entrance.room)];
    if (slot.source == RoomEntranceSource::kNone) {
      slot.entrance_id = entrance.id;
      slot.main_blockset = entrance.main_blockset;
      slot.source = RoomEntranceSource::kDirect;
    }
    slot.candidates.push_back({entrance.id, entrance.main_blockset});
  }

  // 2. Pause-menu dungeon maps.
  std::map<int, std::vector<RoomEntranceCandidate>> map_candidates;
  DungeonMapLabels labels;
  auto maps = LoadDungeonMaps(rom, labels);
  if (maps.ok()) {
    for (const DungeonMap& map : *maps) {
      std::set<int> rooms;
      for (const auto& floor : map.floor_rooms) {
        for (uint8_t room : floor) {
          if (room != kEmptyMapRoom) {
            rooms.insert(room);
          }
        }
      }
      // The map's dungeon ID: the most common one among entrances that lead
      // straight into its rooms. Other entrances can share the ID without
      // being part of this map, so they never represent it.
      std::map<uint8_t, int> votes;
      for (const EntranceInfo& entrance : entrances) {
        if (rooms.contains(entrance.room) && entrance.dungeon_id != 0xFF) {
          ++votes[entrance.dungeon_id];
        }
      }
      if (votes.empty()) {
        continue;
      }
      uint8_t dungeon_id = votes.begin()->first;
      for (const auto& [id, count] : votes) {
        if (count > votes[dungeon_id]) {
          dungeon_id = id;
        }
      }
      for (int room : rooms) {
        auto& candidates = map_candidates[room];
        for (const EntranceInfo& entrance : entrances) {
          if (entrance.dungeon_id != dungeon_id ||
              !rooms.contains(entrance.room)) {
            continue;
          }
          const bool known = std::any_of(
              candidates.begin(), candidates.end(), [&](const auto& candidate) {
                return candidate.entrance_id == entrance.id;
              });
          if (!known) {
            candidates.push_back({entrance.id, entrance.main_blockset});
          }
        }
      }
    }
  }
  for (auto& [room, candidates] : map_candidates) {
    RoomDefaultEntrance& slot = result[static_cast<size_t>(room)];
    if (slot.source != RoomEntranceSource::kNone || candidates.empty()) {
      continue;
    }
    const bool same_blockset = std::all_of(
        candidates.begin(), candidates.end(), [&](const auto& candidate) {
          return candidate.main_blockset == candidates.front().main_blockset;
        });
    // The header is read only when it can change the outcome.
    const uint8_t header_blockset =
        same_blockset ? candidates.front().main_blockset
                      : LoadRoomHeaderFromRom(&rom, room).blockset();
    const auto& chosen =
        candidates[SelectRoomEntranceCandidate(candidates, header_blockset)];
    slot.entrance_id = chosen.entrance_id;
    slot.main_blockset = chosen.main_blockset;
    slot.source = RoomEntranceSource::kDungeonMap;
    slot.candidates = std::move(candidates);
  }

  // 3. Neighbours in the 16-wide room grid, until nothing changes.
  bool changed = true;
  while (changed) {
    changed = false;
    for (int room = 0; room < kNumberOfRooms; ++room) {
      if (result[static_cast<size_t>(room)].source !=
          RoomEntranceSource::kNone) {
        continue;
      }
      for (int neighbour : {room - 1, room + 1, room - 16, room + 16}) {
        const bool adjacent = neighbour / 16 == room / 16 ||
                              neighbour == room - 16 || neighbour == room + 16;
        if (neighbour < 0 || neighbour >= kNumberOfRooms || !adjacent) {
          continue;
        }
        const RoomDefaultEntrance& from =
            result[static_cast<size_t>(neighbour)];
        if (from.source == RoomEntranceSource::kNone) {
          continue;
        }
        result[static_cast<size_t>(room)] = {from.entrance_id,
                                             from.main_blockset,
                                             RoomEntranceSource::kNeighbour,
                                             neighbour,
                                             {}};
        changed = true;
        break;
      }
    }
  }
  return result;
}

}  // namespace yaze::zelda3
