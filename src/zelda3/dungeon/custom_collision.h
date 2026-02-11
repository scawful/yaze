#ifndef YAZE_APP_ZELDA3_DUNGEON_CUSTOM_COLLISION_H
#define YAZE_APP_ZELDA3_DUNGEON_CUSTOM_COLLISION_H

#include <array>
#include <cstdint>
#include <string>
#include <vector>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "rom/rom.h"

namespace yaze {
namespace zelda3 {

struct CustomCollisionMap {
  std::array<uint8_t, 64 * 64> tiles{};
  bool has_data = false;
};

// Load the ZScream custom collision map for a dungeon room.
absl::StatusOr<CustomCollisionMap> LoadCustomCollisionMap(Rom* rom,
                                                          int room_id);

struct CustomCollisionTileEntry {
  uint16_t offset = 0;  // Y*64 + X (0..4095)
  uint8_t value = 0;    // Collision type (0..255). 0 typically means "unset".
};

struct CustomCollisionRoomEntry {
  int room_id = -1;
  std::vector<CustomCollisionTileEntry> tiles;
};

// JSON import/export helpers for authoring workflows.
//
// Format (v1):
// {
//   "version": 1,
//   "rooms": [
//     { "room_id": "0x25", "tiles": [ [ 1234, 8 ], [ 42, "0x1B" ] ] }
//   ]
// }
//
// `room_id`, offsets, and values may be provided as ints or strings (hex
// allowed). Values of 0 are allowed and can be used to explicitly clear tiles.
absl::StatusOr<std::string> DumpCustomCollisionRoomsToJsonString(
    const std::vector<CustomCollisionRoomEntry>& rooms);

absl::StatusOr<std::vector<CustomCollisionRoomEntry>>
LoadCustomCollisionRoomsFromJsonString(const std::string& json_content);

}  // namespace zelda3
}  // namespace yaze

#endif  // YAZE_APP_ZELDA3_DUNGEON_CUSTOM_COLLISION_H
