#ifndef YAZE_APP_EDITOR_DUNGEON_DUNGEON_CONNECTION_EDIT_H_
#define YAZE_APP_EDITOR_DUNGEON_DUNGEON_CONNECTION_EDIT_H_

#include <cstddef>
#include <vector>

#include "absl/status/statusor.h"
#include "zelda3/dungeon/room.h"

namespace yaze::editor {

enum class DungeonConnectionLayer { kUpper, kLower };

struct DungeonConnectionRequest {
  int source_room_id = -1;
  size_t source_door_index = 0;
  DungeonConnectionLayer layer = DungeonConnectionLayer::kUpper;
};

struct DungeonConnectionPlan {
  DungeonConnectionRequest request;
  const Rom* rom = nullptr;
  int target_room_id = -1;
  size_t target_door_index = 0;
  std::vector<zelda3::Room::Door> source_before;
  std::vector<zelda3::Room::Door> source_after;
  std::vector<zelda3::Room::Door> target_before;
  std::vector<zelda3::Room::Door> target_after;
  bool creates_return = false;
  bool changed() const;
};

// Ordinary outer-wall normal doors only. Internal seams, exits, markers,
// shutters, key doors and explicit destinations require other authoring rules.
// Geometric adjacency never wraps rows or crosses a 256-room engine page.
absl::StatusOr<int> DungeonConnectionTargetRoom(int source_room_id,
                                                const zelda3::Room::Door& door);

// Pure two-room preflight: retains unrelated records, never writes room or ROM
// state, and rejects ambiguous/overlapping endpoints rather than choosing one.
// Object-stream allocation remains the editor persistence boundary's concern.
absl::StatusOr<DungeonConnectionPlan> PlanDungeonDoorConnection(
    const zelda3::Room& source, const zelda3::Room& target,
    const DungeonConnectionRequest& request);

bool SameDungeonDoors(const std::vector<zelda3::Room::Door>& first,
                      const std::vector<zelda3::Room::Door>& second);

}  // namespace yaze::editor

#endif  // YAZE_APP_EDITOR_DUNGEON_DUNGEON_CONNECTION_EDIT_H_
