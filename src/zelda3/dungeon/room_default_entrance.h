#ifndef YAZE_ZELDA3_DUNGEON_ROOM_DEFAULT_ENTRANCE_H
#define YAZE_ZELDA3_DUNGEON_ROOM_DEFAULT_ENTRANCE_H

#include <cstdint>
#include <vector>

namespace yaze {

class Rom;

namespace zelda3 {

// How a room's default entrance was chosen.
enum class RoomEntranceSource : uint8_t {
  kNone,        // No entrance could be associated with the room.
  kDirect,      // An entrance leads straight into the room.
  kDungeonMap,  // The room is on a dungeon's pause-menu map.
  kNeighbour,   // Inherited from an adjacent room in the 16-wide room grid.
};

// The entrance the game would most plausibly load a room through, and the
// main graphics set that entrance selects.
//
// The game's main blockset ($0AA1) comes only from the entrance
// (Underworld_LoadEntrance: EntranceData.main_GFX -> $0AA1); moving between
// rooms never changes it. The room header's blockset byte is the secondary
// set ($0AA2). A room entered through a door therefore uses the main graphics
// of the dungeon it belongs to, which yaze recovers here.
struct RoomDefaultEntrance {
  int entrance_id = -1;          // Regular entrance index, or -1.
  uint8_t main_blockset = 0xFF;  // EntranceData.main_GFX, or 0xFF.
  RoomEntranceSource source = RoomEntranceSource::kNone;
  int via_room = -1;  // For kNeighbour: the room inherited from.
};

// One entry per dungeon room (kNumberOfRooms), in order:
//  1. an entrance whose room is this room;
//  2. an entrance of the dungeon whose pause-menu map lists the room (map
//     index to dungeon ID by majority over that map's direct entrances);
//  3. the choice of a neighbouring room (+-1 in the same row, +-16).
std::vector<RoomDefaultEntrance> ComputeRoomDefaultEntrances(Rom& rom);

}  // namespace zelda3
}  // namespace yaze

#endif  // YAZE_ZELDA3_DUNGEON_ROOM_DEFAULT_ENTRANCE_H
