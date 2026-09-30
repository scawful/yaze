#ifndef YAZE_ZELDA3_ATLAS_DUNGEON_DOOR_EDGES_H_
#define YAZE_ZELDA3_ATLAS_DUNGEON_DOOR_EDGES_H_

// Door markers for the Dungeon Atlas: which doors sit on a room's outer wall,
// their category, and the pairing of doors on facing walls of grid
// neighbours. Ported from Oracle of Secrets Scripts/Maps/room_data.py
// (DOOR_CAT, pair_doors) so both pictures agree; these are display
// heuristics, not game rules.

#include <cstdint>
#include <string>
#include <vector>

namespace yaze::zelda3::atlas {

enum class DoorCategory : uint8_t {
  kOpen,
  kOneSided,
  kShutter,
  kKey,
  kBigKey,
  kBomb,
  kExit,
  kKeyStairs,
  kOther,
  kIgnore,  // Layer swap markers: never drawn.
};

const char* DoorCategoryName(DoorCategory category);
DoorCategory DoorCategoryForType(int door_type);

// A door as the room stores it (zelda3::Room::Door).
struct AtlasDoorFact {
  int direction = 0;  // zelda3::DoorDirection: 0 N, 1 S, 2 W, 3 E.
  int tile_x = 0;
  int tile_y = 0;
  int type = 0;  // zelda3::DoorType byte.
  std::string type_name;
};

// A door on the outer wall, as drawn in the gutter.
struct AtlasDoor {
  char wall = 'N';  // N, S, W, E
  int pos = 0;      // Tile position along the wall (0-63).
  int type = 0;
  std::string type_name;
  DoorCategory category = DoorCategory::kOther;
};

// Doors on the outer wall with their category. Inner doors and layer swap
// markers are dropped; a normal door within 2 tiles of an exit marker on the
// same wall is that exit.
std::vector<AtlasDoor> OuterWallDoors(const std::vector<AtlasDoorFact>& doors);

struct DoorEdge {
  int a = 0;  // Room with the door.
  int b = 0;  // Grid neighbour (N = a-0x10, E = a+1).
  char dir = 'E';
  int pos = 0;
  DoorCategory category = DoorCategory::kOpen;
  bool both_sides = false;      // The neighbour has a matching door.
  bool one_sided_type = false;  // One of the doors is a one-sided type.
  std::vector<std::string> types;
};

// Pairs doors on facing walls (within 3 tiles). The stronger category wins
// (big key > key > shutter > bomb > open > one-sided); a door with no partner
// is a one-side edge. `doors_by_room` has one entry per room.
std::vector<DoorEdge> PairDoorEdges(
    const std::vector<std::vector<AtlasDoor>>& doors_by_room);

}  // namespace yaze::zelda3::atlas

#endif  // YAZE_ZELDA3_ATLAS_DUNGEON_DOOR_EDGES_H_
