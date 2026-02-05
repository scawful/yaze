#ifndef YAZE_APP_ZELDA3_DUNGEON_CUSTOM_COLLISION_H
#define YAZE_APP_ZELDA3_DUNGEON_CUSTOM_COLLISION_H

#include <array>
#include <cstdint>

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

}  // namespace zelda3
}  // namespace yaze

#endif  // YAZE_APP_ZELDA3_DUNGEON_CUSTOM_COLLISION_H
