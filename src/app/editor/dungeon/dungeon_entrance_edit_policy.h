#ifndef YAZE_APP_EDITOR_DUNGEON_DUNGEON_ENTRANCE_EDIT_POLICY_H_
#define YAZE_APP_EDITOR_DUNGEON_DUNGEON_ENTRANCE_EDIT_POLICY_H_

#include "zelda3/dungeon/room_entrance.h"

namespace yaze::editor {

inline constexpr char kDungeonSpawnReadOnlyReason[] =
    "Spawn properties are read-only until the UI binds to the dedicated "
    "DungeonSpawnPoint model.";

inline bool CanEditDungeonEntrance(int slot_index,
                                   const zelda3::RoomEntrance& entrance) {
  return slot_index >= zelda3::kNumDungeonSpawnPoints &&
         slot_index < zelda3::kNumDungeonEntranceSlots &&
         !entrance.is_spawn_point();
}

inline bool MarkDungeonEntranceDirtyIfEditable(int slot_index,
                                               zelda3::RoomEntrance& entrance,
                                               bool properties_changed) {
  if (!properties_changed || !CanEditDungeonEntrance(slot_index, entrance)) {
    return false;
  }
  entrance.MarkDirty();
  return true;
}

}  // namespace yaze::editor

#endif  // YAZE_APP_EDITOR_DUNGEON_DUNGEON_ENTRANCE_EDIT_POLICY_H_
