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

const char* RoomEntranceSourceName(RoomEntranceSource source);

// An entrance that could have loaded the room's graphics. A room can be
// reached from several entrances, and a room can be drawn on several pause
// maps, so the default below is only the most plausible of these.
struct RoomEntranceCandidate {
  int entrance_id = -1;
  uint8_t main_blockset = 0xFF;
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
  // Every direct or pause-map entrance considered, in table order. Empty for
  // neighbour defaults.
  std::vector<RoomEntranceCandidate> candidates;
};

struct GameData;

// The source selected by the same main-blockset precedence as
// Room::LoadRoomGraphics. A default is direct only when the default entrance
// actually leads into this room; map and neighbour defaults are inferred.
enum class RoomMainBlocksetSource : uint8_t {
  kExplicitPreview,
  kDirectDefault,
  kInferredDefault,
  kHeaderFallback,
  kZeroFallback,
};

struct RoomMainBlocksetResolution {
  uint8_t main_blockset = 0;
  RoomMainBlocksetSource source = RoomMainBlocksetSource::kZeroFallback;
};

RoomMainBlocksetResolution ResolveRoomMainBlockset(
    int room_id, uint8_t header_blockset, uint8_t requested_main_blockset,
    const GameData& game_data);

// Palette half for a destination background slot (tile ID / 64). The game's
// runtime upload slot runs in the opposite direction. Index zero remains
// transparent; apply this offset only to nonzero 3bpp pixel indices.
int DungeonBackgroundPaletteOffset(uint8_t main_blockset, int destination_slot);

// Index of the candidate to use for a room reached only through its dungeon.
// Prefers a main blockset equal to the room's header (secondary) blockset,
// which is how hacks and vanilla usually pair them, then the first candidate.
// Returns -1 when `candidates` is empty.
int SelectRoomEntranceCandidate(
    const std::vector<RoomEntranceCandidate>& candidates,
    uint8_t header_blockset);

// One entry per dungeon room (kNumberOfRooms), in order:
//  1. an entrance whose room is this room;
//  2. an entrance on a pause-menu map listing the room. Only entrances that
//     lead into a room of that map count; the map's dungeon ID is the majority
//     among them. A dungeon ID shared with an entrance elsewhere in the ROM
//     must not choose the graphics. A room on several maps chooses with
//     SelectRoomEntranceCandidate;
//  3. the choice of a neighbouring room (+-1 in the same row, +-16).
std::vector<RoomDefaultEntrance> ComputeRoomDefaultEntrances(Rom& rom);

}  // namespace zelda3
}  // namespace yaze

#endif  // YAZE_ZELDA3_DUNGEON_ROOM_DEFAULT_ENTRANCE_H
