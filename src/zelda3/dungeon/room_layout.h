#ifndef YAZE_APP_ZELDA3_DUNGEON_ROOM_LAYOUT_H
#define YAZE_APP_ZELDA3_DUNGEON_ROOM_LAYOUT_H

#include <vector>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "app/rom.h"
#include "zelda3/dungeon/room_object.h"

namespace yaze::zelda3 {

class RoomLayout {
 public:
  RoomLayout() = default;
  explicit RoomLayout(Rom* rom) : rom_(rom) {}

  void set_rom(Rom* rom) { rom_ = rom; }

  absl::Status LoadLayout(int layout_id);

  const std::vector<RoomObject>& GetObjects() const { return objects_; }

 private:
  absl::StatusOr<int> GetLayoutAddress(int layout_id) const;

  Rom* rom_ = nullptr;
  std::vector<RoomObject> objects_;
};

}  // namespace yaze::zelda3

#endif  // YAZE_APP_ZELDA3_DUNGEON_ROOM_LAYOUT_H
