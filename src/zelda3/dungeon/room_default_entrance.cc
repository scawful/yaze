#include "zelda3/dungeon/room_default_entrance.h"

#include <map>

#include "rom/rom.h"
#include "zelda3/dungeon/dungeon_rom_addresses.h"
#include "zelda3/dungeon/dungeon_spawn_point.h"
#include "zelda3/dungeon/room_entrance.h"
#include "zelda3/screen/dungeon_map.h"

namespace yaze::zelda3 {
namespace {

constexpr uint8_t kEmptyMapRoom = 0x0F;

}  // namespace

std::vector<RoomDefaultEntrance> ComputeRoomDefaultEntrances(Rom& rom) {
  std::vector<RoomDefaultEntrance> result(kNumberOfRooms);
  if (!rom.is_loaded()) {
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
      slot = {entrance.id, entrance.main_blockset, RoomEntranceSource::kDirect,
              -1};
    }
  }

  // 2. Pause-menu dungeon maps.
  DungeonMapLabels labels;
  auto maps = LoadDungeonMaps(rom, labels);
  if (maps.ok()) {
    for (const DungeonMap& map : *maps) {
      std::vector<int> rooms;
      for (const auto& floor : map.floor_rooms) {
        for (uint8_t room : floor) {
          if (room != kEmptyMapRoom) {
            rooms.push_back(room);
          }
        }
      }
      // Which dungeon ID this map belongs to: the most common one among
      // entrances that lead straight into its rooms.
      std::map<uint8_t, int> votes;
      for (int room : rooms) {
        for (const EntranceInfo& entrance : entrances) {
          if (entrance.room == room && entrance.dungeon_id != 0xFF) {
            ++votes[entrance.dungeon_id];
          }
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
      const EntranceInfo* representative = nullptr;
      for (const EntranceInfo& entrance : entrances) {
        if (entrance.dungeon_id == dungeon_id) {
          representative = &entrance;
          break;
        }
      }
      if (representative == nullptr) {
        continue;
      }
      for (int room : rooms) {
        RoomDefaultEntrance& slot = result[static_cast<size_t>(room)];
        if (slot.source == RoomEntranceSource::kNone) {
          slot = {representative->id, representative->main_blockset,
                  RoomEntranceSource::kDungeonMap, -1};
        }
      }
    }
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
        result[static_cast<size_t>(room)] = {
            from.entrance_id, from.main_blockset,
            RoomEntranceSource::kNeighbour, neighbour};
        changed = true;
        break;
      }
    }
  }
  return result;
}

}  // namespace yaze::zelda3
