#ifndef YAZE_ZELDA3_DUNGEON_ROOM_COLLISION_H
#define YAZE_ZELDA3_DUNGEON_ROOM_COLLISION_H

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <vector>

#include "zelda3/dungeon/game_tilemap_comparison.h"
#include "zelda3/dungeon/room.h"
#include "zelda3/dungeon/room_object.h"

namespace yaze {

class Rom;

namespace zelda3 {

// Vanilla underworld collision ("tile attribute") maps.
//
// After a room is drawn, Underworld_LoadAttributeTable (usdasm bank_01
// #_01B8BF) builds one attribute byte per tilemap tile:
//   COLMAPA $7F2000 (64x64) from TILEMAPA $7E2000 (BG1, the upper layer)
//   COLMAPB $7F3000 (64x64) from TILEMAPB $7E4000 (BG2, the lower layer)
// in four passes:
//   1. Underworld_LoadBasicAttribute_full: TILEATTR[tile number], plus the
//      tile's flip bits for attributes 0x10-0x1B.
//   2. Underworld_LoadObjectAttribute: stars, stairs, layer stairs, water
//      stairs, pots/blocks/pegs, torches, chests and big key locks, from lists
//      the object draw routines recorded while the room was drawn.
//   3. Underworld_LoadDoorAttribute: doorways, locked doors, layer and
//      dungeon toggle doors, the door switch, from the door slot tables.
//   4. Underworld_FlipCrystalPegAttribute when $7EC172 != 0.
// yaze reproduces the recorded lists by replaying the list bookkeeping of
// each RoomDraw_* routine over the room's own objects, doors, pushable blocks
// and torches, in the game's draw order.

inline constexpr size_t kTileAttributeTableSize = 0x200;  // $7EFE00
inline constexpr size_t kCollisionMapTiles = 64 * 64;
// COLMAPA then COLMAPB, indexed like the game: tile = y * 64 + x, plus
// 0x1000 for the lower (BG2) map.
inline constexpr size_t kRoomCollisionBytes = 2 * kCollisionMapTiles;

// Which pass last wrote a collision byte.
enum class CollisionSource : uint8_t {
  kTilemap = 0,      // basic pass (tile attribute table)
  kStar,             // star tiles
  kStairs,           // in-room, spiral and straight stairs
  kLayerStairs,      // auto multi-layer / merged stairs
  kWater,            // water hop stairs, water overlay stairs, water ladders
  kManipulable,      // pots, pegs, blocks, big gray rocks
  kTorch,            // lightable torches
  kChest,            // chests and big chests
  kBigKeyLock,       // big key locks
  kDoor,             // doorway properties
  kLockedDoor,       // closed doors (F0 | slot)
  kDoorLayerToggle,  // layer / dungeon toggle doors (OR 0x10 / 0x20)
  kDoorSwitch,       // the door switcher object
  kCrystalPeg,       // crystal peg swap
};

const char* CollisionSourceName(CollisionSource source);

struct RoomCollisionMaps {
  std::array<uint8_t, kRoomCollisionBytes> attributes{};
  std::array<CollisionSource, kRoomCollisionBytes> sources{};

  uint8_t upper(int x, int y) const { return attributes[y * 64 + x]; }
  uint8_t lower(int x, int y) const {
    return attributes[kCollisionMapTiles + y * 64 + x];
  }
};

// Save-file and runtime state that changes the maps. The default is a room
// entered for the first time on a new file: no room flags, pegs not swapped.
struct UnderworldRoomLoadState {
  // The room's save word ($7EF000 + room * 2). Underworld_LoadHeader derives
  // $0400 (= word & 0xF000), $068C (= $0400 | 0x0F00) and
  // $0402 (= (word & 0x0FF0) << 4) from it.
  uint16_t room_save_flags = 0;
  // $068C, the open-door mask, when it differs from the one LoadHeader
  // derives (a room whose shutters opened after it loaded).
  std::optional<uint16_t> door_open_mask;
  // $0468: shutter doors stay closed while it is nonzero. LoadHeader sets 1.
  uint16_t shutter_controller = 1;
  // $7EC172 != 0: Underworld_FlipCrystalPegAttribute swaps 0x66 and 0x67.
  bool crystal_pegs_swapped = false;
};

// TILEATTR ($7EFE00, 512 bytes): LoadDefaultTileTypes (#_0E97D9) copies
// UnderworldTileTypes ($0E:9659) entries 0x000-0x13F to 0x000 and
// 0x140-0x17F to 0x1C0; Underworld_LoadCustomTileTypes (#_0E942A) copies 0x80
// bytes from CustomUnderworldTileTypes ($0E:902A) +
// CustomTileTypesOffset[blockset] ($0E:9000) to 0x140. `blockset` is room
// header byte 2 (Room::blockset()).
std::array<uint8_t, kTileAttributeTableSize> LoadUnderworldTileAttributeTable(
    const Rom& rom, uint8_t blockset);

// Pass 1 only: the attribute of each tile word in `tilemaps`.
RoomCollisionMaps ComputeBasicCollisionMaps(
    const std::array<uint8_t, kTileAttributeTableSize>& table,
    const RoomTilemaps& tilemaps);

// Everything the passes read from a room.
struct RoomCollisionInput {
  int room_id = 0;
  uint8_t blockset = 0;
  uint8_t tag1 = 0;  // $AE
  uint8_t tag2 = 0;  // $AF
  // Room objects as Room holds them: stream objects (list = GetLayerValue())
  // plus pushable blocks and torches (ObjectOption::Block / ::Torch).
  std::vector<RoomObject> objects;
  std::vector<Room::Door> doors;  // stream order
  RoomTilemaps tilemaps;          // BG1 / BG2 words after drawing
};

// All four passes. Tables (tile types, door positions, doorway properties)
// are read from `rom`.
RoomCollisionMaps ComputeRoomCollisionMaps(
    const Rom& rom, const RoomCollisionInput& input,
    const UnderworldRoomLoadState& state = {});

// Convenience for a loaded, rendered room: its objects, doors and header, and
// the tilemaps yaze composed (ComposeYazeRoomTilemaps) unless `tilemaps` is
// given.
RoomCollisionInput MakeRoomCollisionInput(const Room& room);
RoomCollisionInput MakeRoomCollisionInput(const Room& room,
                                          const RoomTilemaps& tilemaps);

}  // namespace zelda3
}  // namespace yaze

#endif  // YAZE_ZELDA3_DUNGEON_ROOM_COLLISION_H
