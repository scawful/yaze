#ifndef YAZE_ZELDA3_DUNGEON_ROOM_LINKS_H_
#define YAZE_ZELDA3_DUNGEON_ROOM_LINKS_H_

// Room links: the ways the game moves Link from one underworld room to
// another. The room census (room_census.h) and the z3ed graph commands
// (dungeon-graph, dungeon-discover, dungeon-room-graph) build their edges
// here, so they agree on:
// - Entrances: the game reads ZScream's bank $0F tables when $02:D99F is
//   hooked (ReadDungeonEntranceTarget).
// - Header destinations: a stair or holewarp byte keeps the source room's
//   high byte ($A1). Byte 0 is room $x00 when a stair, hole or warp uses it;
//   an unused byte 0 is no reference. Results past 0x127 are not rooms.
// - Grid neighbors: edge doors lead to +-1 / +-16 on the same $A1 page, with
//   no wrap across a row or page edge (DungeonNeighborRoom).
// - Teleport doors (door type 0x46 on an east or west wall): the edge
//   transition loads stair slot 4 (east, $02:B670 LDA $7EC004) or slot 3
//   (west, $02:B711 LDA $7EC003) instead of the grid neighbor.
// - Key-stair doors (0x20-0x26) lock a stair object; the stair object is the
//   link, not the door.
// - Holes: every fall runs DetermineConsequencesOfFalling, which costs a
//   heart in rooms listed in RoomsWithPitDamage ($07:94AA) and otherwise
//   loads the header holewarp ($07:94BA). A room's holewarp is live when it
//   has pit objects, pit tiles, tag-driven holes (RoomTag_TriggerHoles),
//   falling-floor overlords (0x0A-0x0F) or Ganon (0xD6, whose phase 3 spawns
//   overlords 0x0C-0x0F). Warp tiles ($07:D146) use the holewarp in any room.
//   Pit tiles include the bowls of large braziers (0x11C), entered by landing
//   from the room above: vanilla 0x31 drops into 0x77, whose braziers drop
//   into the Hera fairy room 0xA7 (Module07_07_0F_FallingFadeIn, $02:8EC3).

#include <array>
#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <set>
#include <string>
#include <vector>

namespace yaze {
class Rom;
namespace zelda3 {

class PitDamageTable;
class Room;

inline constexpr int kDungeonRoomCount = 0x128;
// Entrance table rows 0x00-0x84 (vanilla and ZScream's bank $0F copy).
inline constexpr int kDungeonEntranceCount = 0x85;
// Door type 0x46: tile attribute 0x89 on east and west walls.
inline constexpr uint8_t kTeleportDoorType = 0x46;

enum class RoomReferenceKind : uint8_t {
  kEntrance,          // Overworld entrance placed on a map.
  kHole,              // Overworld hole placed on a map.
  kSpawn,             // Spawn point (save/start location).
  kUnplacedEntrance,  // Entrance table row with no overworld placement.
  kDoor,              // Door from an adjacent room.
  kStair,             // Stair object resolved to a header slot.
  kHolewarp,          // Pits, holes or warp tiles using the header holewarp.
  kWarpTag,           // Project warp tag using header stair slots.
  kHeaderOnly,        // Header byte points here but nothing uses it.
  kProjectListing,    // Listed in the project's dungeon registry.
  kDungeonMap,        // Drawn on a dungeon's pause map (bank $0A room grids).
  kTeleportDoor,      // Door type 0x46 using header stair slot 3 or 4.
};

const char* RoomReferenceKindName(RoomReferenceKind kind);

// What a door does when Link walks through it.
enum class RoomDoorRole : uint8_t {
  kNeighbor,   // Edge transition to the grid neighbor.
  kTeleport,   // Type 0x46 on an east or west wall: stair slot 4 or 3.
  kExit,       // Leaves the dungeon (IsExitDoorType).
  kKeyStairs,  // Locks a stair object; the stair object is the link.
};
const char* RoomDoorRoleName(RoomDoorRole role);

// Per-room facts that decide the room's outgoing links. Plain data so tests
// can build them.
struct RoomLinkFacts {
  int room_id = 0;
  uint8_t blockset = 0;
  uint8_t tag1 = 0;
  uint8_t tag2 = 0;
  // Pit objects (room stream or layout), pit tiles in the drawn tilemaps or
  // custom collision.
  bool has_pits = false;
  // The only pit tiles are the bowls of large braziers (object 0x11C), which
  // Link enters only by landing there from the room above.
  bool pits_only_in_braziers = false;
  // Warp tile objects (including 0xFCF, drawn disabled and enabled at run
  // time), warp tiles in the drawn tilemaps or custom collision.
  bool has_warp_tiles = false;
  // Tag 1 or tag 2 is a vanilla "Holes" tag. CollectRoomLinks ignores a
  // hole tag that the project redefines as a warp tag.
  bool has_hole_tag = false;
  // Falling-floor overlord or Ganon; empty when none. Names the sprite.
  std::string hole_sprite;
  bool in_pit_damage_table = false;
  uint8_t holewarp_byte = 0;
  std::array<uint8_t, 4> stair_bytes{};
  // Header slots used by stair objects in this room.
  std::array<bool, 4> stair_slot_used{};
  std::array<bool, 4> stair_slot_estimated{};  // Placement order, not replay.
  struct Door {
    int direction = 0;  // zelda3::DoorDirection value.
    int along = 0;      // Tile coordinate along the wall.
    bool outer = false;
    RoomDoorRole role = RoomDoorRole::kNeighbor;
    uint8_t type = 0;  // zelda3::DoorType value.
    int tile_x = 0;
    int tile_y = 0;
  };
  std::vector<Door> doors;  // Every door except layer/dungeon swap markers.
};

struct RoomLink {
  int from = -1;
  int to = -1;      // 0x000-0x127, or -1 when the destination is not a room.
  int raw_to = -1;  // Destination before the range check.
  RoomReferenceKind kind = RoomReferenceKind::kDoor;
  int slot = -1;          // Header stair slot 0-3, or -1.
  int direction = -1;     // Door wall for door links, or -1.
  int door = -1;          // Index into RoomLinkFacts::doors for door links.
  bool strong = false;    // The game can take it.
  bool owner_ok = false;  // Same area: may carry dungeon ownership.
  std::string detail;
};

// Facts of a room, or nullptr when `room_id` is not a room or not known.
using RoomLinkFactsLookup = std::function<const RoomLinkFacts*(int room_id)>;

// Every outgoing link of `room_id`: doors (mutual or one-sided), teleport
// doors, stairs, warp tags, the holewarp, and header bytes nothing uses.
// Links whose destination is not a room keep `raw_to` and have `to` = -1.
std::vector<RoomLink> CollectRoomLinks(
    int room_id, const RoomLinkFactsLookup& facts,
    const std::set<uint8_t>& warp_tag_ids = {});

bool IsDungeonRoomId(int room_id);

// Grid neighbor for an edge transition, or -1 when it would leave the row or
// the $A1 page, or is not a room.
int DungeonNeighborRoom(int room_id, int direction);

// ResolveHeaderDestinationRoom, or -1 when the result is not a room.
int CheckedHeaderDestinationRoom(int source_room_id, uint8_t header_byte);

// True when the entrance loader at $02:D99F is ZScream's JSL $0FF008, which
// reads rooms from $0F:8000 and dungeon IDs from $0F:9800.
bool UsesExpandedEntranceTables(const Rom& rom);

struct DungeonEntranceTarget {
  int room_id = -1;  // Raw 16-bit room word; check IsDungeonRoomId.
  int dungeon_id = -1;
  bool expanded = false;  // Read from ZScream's bank $0F tables.
};

// Room and dungeon ID of entrance `entrance_id`, from the table the game
// reads.
std::optional<DungeonEntranceTarget> ReadDungeonEntranceTarget(const Rom& rom,
                                                               int entrance_id);

// True for objects whose tiles are pits (vanilla object IDs).
bool IsPitObjectId(int object_id);
// Warp tile 0xFCA and its disabled form 0xFCF.
bool IsWarpTileObjectId(int object_id);
// Vanilla "Holes" room tags (RoomTag_TriggerHoles and the chest holes).
bool IsHoleRoomTag(uint8_t tag);
// Pit (0x20, 0xB0-0xBD) and warp (0x4B) tile attributes, per the
// underworld TileBehavior table at $07:D7D8.
bool IsPitTileAttribute(uint8_t attribute);
bool IsWarpTileAttribute(uint8_t attribute);

// Reads the link facts of `room`, which must be loaded with its objects and
// sprites (LoadRoomFromRom + LoadSprites). Draws the room's layout and
// objects without graphics to find pit and warp tiles. `pit_table` may be
// null (no room counts as a pit-damage room).
RoomLinkFacts CollectRoomLinkFacts(Rom* rom, const Room& room,
                                   const PitDamageTable* pit_table);

// Loads rooms on demand and keeps their link facts.
class RoomLinkFactsCache {
 public:
  explicit RoomLinkFactsCache(Rom* rom);
  ~RoomLinkFactsCache();
  RoomLinkFactsCache(const RoomLinkFactsCache&) = delete;
  RoomLinkFactsCache& operator=(const RoomLinkFactsCache&) = delete;

  // nullptr when `room_id` is not a room.
  const RoomLinkFacts* Get(int room_id);
  RoomLinkFactsLookup AsLookup();

 private:
  Rom* rom_;
  std::unique_ptr<PitDamageTable> pit_table_;  // null when unreadable
  std::vector<std::optional<RoomLinkFacts>> facts_;
};

}  // namespace zelda3
}  // namespace yaze

#endif  // YAZE_ZELDA3_DUNGEON_ROOM_LINKS_H_
