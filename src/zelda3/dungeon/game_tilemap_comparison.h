#ifndef YAZE_ZELDA3_DUNGEON_GAME_TILEMAP_COMPARISON_H
#define YAZE_ZELDA3_DUNGEON_GAME_TILEMAP_COMPARISON_H

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include "absl/status/statusor.h"
#include "zelda3/dungeon/room_object.h"

namespace yaze {

class Rom;

namespace zelda3 {

class Room;

// Compares yaze's room tilemaps with tilemaps captured from the running game.
//
// When the game loads a room it builds both 64x64 tilemaps in WRAM:
// TILEMAPA ($7E2000) is BG1 and TILEMAPB ($7E4000) is BG2 (usdasm wram.asm).
// scripts/agents/capture-game-room-tilemaps.py saves them from Mesen. A tile
// word is vhopppcc cccccccc: flips, priority, palette and tile number, so a
// match proves an object's geometry, tiles, flips, palette row and priority.
// It does not prove the colors in CGRAM or the graphics sheets.

inline constexpr int kRoomTilemapSize = 64;
inline constexpr size_t kRoomTilemapWords = 64 * 64;
inline constexpr size_t kGameRoomTilemapBytes = 2 * kRoomTilemapWords * 2;

struct RoomTilemaps {
  std::vector<uint16_t> bg1;  // kRoomTilemapWords, row-major
  std::vector<uint16_t> bg2;
};

// Parses a capture file: BG1 then BG2, little-endian words.
absl::StatusOr<RoomTilemaps> ParseGameRoomTilemaps(
    const std::vector<uint8_t>& bytes);

// yaze's final tilemaps for a rendered room: the object buffer's word where an
// object wrote one, otherwise the layout buffer's word.
RoomTilemaps ComposeYazeRoomTilemaps(const Room& room);

// For each tile, the index (into `objects`) of the room object whose own
// drawing produced the word yaze shows there, or -1 when no object did
// (layout, doors, or an object whose tile was later overwritten).
struct TileOwners {
  std::vector<int> bg1;
  std::vector<int> bg2;
};

// Draws each object alone, in the room's draw order, to find which tiles it
// writes, then keeps an owner only where its word equals `yaze`'s final word.
TileOwners ComputeObjectTileOwners(Rom* rom, int room_id,
                                   const std::vector<RoomObject>& objects,
                                   const RoomTilemaps& yaze);

struct TileDifference {
  int layer = 1;  // 1 = BG1, 2 = BG2
  int x = 0;
  int y = 0;
  uint16_t game = 0;
  uint16_t yaze = 0;
  int owner = -1;  // object index, or -1
};

struct ObjectPlacementCheck {
  size_t object_index = 0;
  int object_id = 0;
  int tiles_owned = 0;           // tiles this object's drawing produced
  int tiles_different = 0;       // of those, how many differ from the game
  uint16_t difference_bits = 0;  // OR of (game ^ yaze) over those tiles
};

struct RoomTilemapCheck {
  int room_id = -1;
  int bg1_matching = 0;
  int bg2_matching = 0;
  std::vector<TileDifference> differences;
  // One entry per object, in `objects` order.
  std::vector<ObjectPlacementCheck> placements;
  // Differing tiles no object owns (layout, doors, runtime changes).
  int unowned_differences = 0;
};

RoomTilemapCheck CompareRoomTilemaps(int room_id, const RoomTilemaps& game,
                                     const RoomTilemaps& yaze,
                                     const TileOwners& owners,
                                     const std::vector<RoomObject>& objects);

// Names the tile-word fields in `xor_bits`, e.g. "priority, palette".
std::string DescribeTileWordDifference(uint16_t xor_bits);

}  // namespace zelda3
}  // namespace yaze

#endif  // YAZE_ZELDA3_DUNGEON_GAME_TILEMAP_COMPARISON_H
