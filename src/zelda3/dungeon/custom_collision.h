#ifndef YAZE_APP_ZELDA3_DUNGEON_CUSTOM_COLLISION_H
#define YAZE_APP_ZELDA3_DUNGEON_CUSTOM_COLLISION_H

#include <array>
#include <bitset>
#include <cstddef>
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
  // Presence is independent of value: 0x00 is an explicit floor override.
  std::bitset<64 * 64> overrides;
  bool has_data = false;

  bool HasOverride(size_t offset) const {
    // Keep nonzero maps constructed by legacy track producers compatible.
    return offset < tiles.size() &&
           (overrides.test(offset) || tiles[offset] != 0);
  }

  void SetOverride(size_t offset, uint8_t tile) {
    if (offset >= tiles.size()) {
      return;
    }
    tiles[offset] = tile;
    overrides.set(offset);
    has_data = true;
  }

  void ClearOverride(size_t offset) {
    if (offset >= tiles.size()) {
      return;
    }
    tiles[offset] = 0;
    overrides.reset(offset);
  }

  bool HasOverrides() const {
    for (size_t offset = 0; offset < tiles.size(); ++offset) {
      if (HasOverride(offset)) {
        return true;
      }
    }
    return false;
  }

  bool operator==(const CustomCollisionMap& other) const {
    if (has_data != other.has_data || tiles != other.tiles) {
      return false;
    }
    for (size_t offset = 0; offset < tiles.size(); ++offset) {
      if (HasOverride(offset) != other.HasOverride(offset)) {
        return false;
      }
    }
    return true;
  }
};

// Load the ZScream custom collision map for a dungeon room.
absl::StatusOr<CustomCollisionMap> LoadCustomCollisionMap(Rom* rom,
                                                          int room_id);

struct CustomCollisionTileEntry {
  uint16_t offset = 0;  // Y*64 + X (0..4095)
  uint8_t value = 0;    // Collision type (0..255), including explicit floor 0.
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
// allowed). A value of 0 is an explicit floor override. Omitted coordinates
// have no override; importing an empty tile list clears the room's overrides.
absl::StatusOr<std::string> DumpCustomCollisionRoomsToJsonString(
    const std::vector<CustomCollisionRoomEntry>& rooms);

absl::StatusOr<std::vector<CustomCollisionRoomEntry>>
LoadCustomCollisionRoomsFromJsonString(const std::string& json_content);

// Every room with custom collision in `rom`, serialized as the canonical
// custom_collision.json source (rooms ascending, present tiles including zero).
// This is the exact byte format Oracle of Secrets' collision source validator
// requires the tracked file to match.
absl::StatusOr<std::string> DumpCustomCollisionSourceFromRom(Rom* rom);

}  // namespace zelda3
}  // namespace yaze

#endif  // YAZE_APP_ZELDA3_DUNGEON_CUSTOM_COLLISION_H
