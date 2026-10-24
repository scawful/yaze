#ifndef YAZE_ZELDA3_DUNGEON_ROOM_OBJECT_BATCH_H
#define YAZE_ZELDA3_DUNGEON_ROOM_OBJECT_BATCH_H

#include <cstddef>
#include <vector>

#include "zelda3/dungeon/room_object.h"

namespace yaze {
namespace zelda3 {

// Snapshot of one room object used to detect stale batch requests. A complete
// guard list holds one entry per object, ordered by source index.
struct RoomObjectGuard {
  size_t object_index;
  int object_id;
  int x_tile;
  int y_tile;
  int size;
  int stored_list;
};

struct RoomObjectMove {
  size_t object_index;
  int x_tile;
  int y_tile;
};

struct RoomObjectBatch {
  int room_id;
  std::vector<RoomObjectGuard> expected_objects;
  std::vector<RoomObjectMove> moves;
};

struct RoomObjectBatchResult {
  size_t changed_object_count;
};

inline std::vector<RoomObjectGuard> CaptureRoomObjectGuards(
    const std::vector<RoomObject>& objects) {
  std::vector<RoomObjectGuard> guards;
  guards.reserve(objects.size());
  for (size_t index = 0; index < objects.size(); ++index) {
    const RoomObject& object = objects[index];
    guards.push_back(RoomObjectGuard{index, object.id_, object.x_, object.y_,
                                     object.size_,
                                     static_cast<int>(object.layer_)});
  }
  return guards;
}

}  // namespace zelda3
}  // namespace yaze

#endif  // YAZE_ZELDA3_DUNGEON_ROOM_OBJECT_BATCH_H
