#ifndef YAZE_APP_EDITOR_DUNGEON_DUNGEON_ENTRANCE_EDIT_POLICY_H_
#define YAZE_APP_EDITOR_DUNGEON_DUNGEON_ENTRANCE_EDIT_POLICY_H_

#include "zelda3/dungeon/dungeon_spawn_point.h"
#include "zelda3/dungeon/room_entrance.h"

namespace yaze::editor {

inline constexpr char kDungeonSpawnReadOnlyReason[] =
    "Spawn properties are read-only in this legacy view. Use Entrance "
    "Properties to edit the dedicated DungeonSpawnPoint record.";

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

inline bool CanEditDungeonSpawnPoint(int slot_index,
                                     const zelda3::DungeonSpawnPoint& spawn) {
  return slot_index >= 0 && slot_index < zelda3::kNumDungeonSpawnPoints &&
         spawn.spawn_id() == slot_index;
}

inline bool MarkDungeonSpawnPointDirtyIfEditable(
    int slot_index, zelda3::DungeonSpawnPoint& spawn, bool properties_changed) {
  if (!properties_changed || !CanEditDungeonSpawnPoint(slot_index, spawn)) {
    return false;
  }
  spawn.MarkDirty();
  return true;
}

}  // namespace yaze::editor

#endif  // YAZE_APP_EDITOR_DUNGEON_DUNGEON_ENTRANCE_EDIT_POLICY_H_
