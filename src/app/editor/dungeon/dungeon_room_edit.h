#ifndef YAZE_APP_EDITOR_DUNGEON_DUNGEON_ROOM_EDIT_H_
#define YAZE_APP_EDITOR_DUNGEON_DUNGEON_ROOM_EDIT_H_

#include "absl/status/status.h"

namespace yaze::zelda3 {
class Room;
}

namespace yaze::editor {

enum class RoomMetadataField {
  kLayout,
  kBlockset,
  kFloor1,
  kFloor2,
  kPalette,
  kSpriteset,
  kMessage,
  kBg2,
  kEffect,
  kCollision,
  kTag1,
  kTag2,
  kHolewarp,
  kStaircaseRoom,
  kStaircasePlane,
};

struct RoomMetadataEdit {
  RoomMetadataField field;
  int value;
  int index = 0;  // Only staircase fields have multiple slots (0-3).
};

struct RoomMetadataRequest {
  int room_id;
  RoomMetadataEdit edit;
};

// A batch validates every request before any target room is changed.
absl::Status ValidateRoomMetadataEdit(const RoomMetadataEdit& edit);

// Validates the edited field before changing the room. Limits describe the
// existing supported authoring controls, not every raw value a hack may load.
// Unrelated values are preserved; undo must use Room::RestoreMetadataSnapshot
// to restore historical values even if they are outside the authoring range.
absl::Status ApplyRoomMetadataEdit(zelda3::Room& room,
                                   const RoomMetadataEdit& edit);

}  // namespace yaze::editor

#endif  // YAZE_APP_EDITOR_DUNGEON_DUNGEON_ROOM_EDIT_H_
