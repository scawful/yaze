#include "app/editor/dungeon/dungeon_connection_edit.h"

#include <optional>

#include "absl/status/status.h"
#include "absl/strings/str_format.h"
#include "util/macro.h"
#include "zelda3/dungeon/dungeon_limits.h"

namespace yaze::editor {
namespace {

using Door = zelda3::Room::Door;
using zelda3::DoorDirection;
using zelda3::DoorType;

bool IsOrdinaryDoor(DoorType type) {
  return type == DoorType::NormalDoor || type == DoorType::NormalDoorLower;
}

bool IsOuterDoor(const Door& door) {
  if (door.position >= 12) {
    return false;
  }
  switch (door.direction) {
    case DoorDirection::North:
    case DoorDirection::West:
      return door.position < 6;
    case DoorDirection::South:
    case DoorDirection::East:
      return door.position >= 6;
  }
  return false;
}

bool SharesOuterLane(const Door& first, const Door& second) {
  // Door variants 0/3 (and 6/9) occupy the same passage at different wall
  // depths. A marker there can affect the other variant's corridor too:
  // Underworld_LoadSingleDoorAttribute ($01BE35), especially $01BED1..BF17.
  return first.direction == second.direction && IsOuterDoor(first) &&
         IsOuterDoor(second) && first.position % 3 == second.position % 3;
}

Door ReturnDoor(const Door& source, DoorType type) {
  // USDASM RoomDraw_DoorPartnerSelfLocation ($009AA2) and
  // RoomDraw_DoorPartnerLocation ($009AD2) match N/W positions 0..5 to
  // S/E positions 6..11. The internal seam slots are absent from these tables.
  const auto direction =
      static_cast<DoorDirection>(static_cast<unsigned>(source.direction) ^ 1);
  const uint8_t position =
      source.position < 6 ? source.position + 6 : source.position - 6;
  const auto [byte1, byte2] =
      zelda3::DoorPositionManager::EncodeDoorBytes(position, type, direction);
  return Door::FromRomBytes(byte1, byte2);
}

void SetLayer(Door& door, DoorType type) {
  if (door.type != type) {
    door.type = type;
    door.byte2 = static_cast<uint8_t>(type);
  }
}

}  // namespace

bool SameDungeonDoors(const std::vector<Door>& first,
                      const std::vector<Door>& second) {
  if (first.size() != second.size()) {
    return false;
  }
  for (size_t i = 0; i < first.size(); ++i) {
    const auto& a = first[i];
    const auto& b = second[i];
    if (a.position != b.position || a.type != b.type ||
        a.direction != b.direction || a.byte1 != b.byte1 ||
        a.byte2 != b.byte2) {
      return false;
    }
  }
  return true;
}

bool DungeonConnectionPlan::changed() const {
  return !SameDungeonDoors(source_before, source_after) ||
         !SameDungeonDoors(target_before, target_after);
}

absl::StatusOr<int> DungeonConnectionTargetRoom(int source_room_id,
                                                const Door& door) {
  if (source_room_id < 0 || source_room_id >= zelda3::kNumberOfRooms) {
    return absl::InvalidArgumentError("Source room is outside the room table");
  }
  if (static_cast<unsigned>(door.direction) > 3 || door.position >= 12) {
    return absl::InvalidArgumentError("Door has an invalid direction or slot");
  }
  if (!IsOrdinaryDoor(door.type)) {
    return absl::FailedPreconditionError(
        "Reciprocal authoring supports normal upper or lower doors only");
  }
  if (!IsOuterDoor(door)) {
    return absl::FailedPreconditionError(
        "This door is on an internal room seam, not an adjacent-room wall");
  }

  // USDASM HandleEdgeTransitionMovement{East,West,South,North} changes the
  // eight-bit $A0 at $02B68A/$02B72B/$02B7D7/$02B878; $A1 is unchanged.
  // Never infer a connection across a row or page boundary from linear ids.
  const int column = source_room_id & 0x0F;
  const int row = (source_room_id & 0xFF) >> 4;
  int target_room_id = -1;
  switch (door.direction) {
    case DoorDirection::North:
      if (row > 0) {
        target_room_id = source_room_id - 16;
      }
      break;
    case DoorDirection::South:
      if (row < 15) {
        target_room_id = source_room_id + 16;
      }
      break;
    case DoorDirection::West:
      if (column > 0) {
        target_room_id = source_room_id - 1;
      }
      break;
    case DoorDirection::East:
      if (column < 15) {
        target_room_id = source_room_id + 1;
      }
      break;
  }
  if (target_room_id < 0 || target_room_id >= zelda3::kNumberOfRooms) {
    return absl::FailedPreconditionError(
        "No adjacent room exists at this room-table boundary");
  }
  return target_room_id;
}

absl::StatusOr<DungeonConnectionPlan> PlanDungeonDoorConnection(
    const zelda3::Room& source, const zelda3::Room& target,
    const DungeonConnectionRequest& request) {
  if (request.source_room_id != source.id()) {
    return absl::InvalidArgumentError("Selected source room changed");
  }
  if (request.layer != DungeonConnectionLayer::kUpper &&
      request.layer != DungeonConnectionLayer::kLower) {
    return absl::InvalidArgumentError("Invalid connection layer");
  }
  const auto& source_doors = source.GetDoors();
  if (request.source_door_index >= source_doors.size()) {
    return absl::InvalidArgumentError("Selected source door no longer exists");
  }
  const auto& source_door = source_doors[request.source_door_index];
  ASSIGN_OR_RETURN(const int target_room_id,
                   DungeonConnectionTargetRoom(source.id(), source_door));
  if (target.id() != target_room_id || source.rom() != target.rom()) {
    return absl::InvalidArgumentError(
        "Target must be the adjacent room in the same ROM");
  }
  for (size_t i = 0; i < source_doors.size(); ++i) {
    if (i != request.source_door_index &&
        SharesOuterLane(source_door, source_doors[i])) {
      return absl::FailedPreconditionError(
          "Source passage contains another door or control marker");
    }
  }

  const auto type = request.layer == DungeonConnectionLayer::kUpper
                        ? DoorType::NormalDoor
                        : DoorType::NormalDoorLower;
  const Door return_door = ReturnDoor(source_door, type);
  const auto& target_doors = target.GetDoors();
  std::optional<size_t> target_index;
  for (size_t i = 0; i < target_doors.size(); ++i) {
    if (!SharesOuterLane(return_door, target_doors[i])) {
      continue;
    }
    if (target_index || target_doors[i].position != return_door.position ||
        !IsOrdinaryDoor(target_doors[i].type)) {
      return absl::FailedPreconditionError(
          "Return passage contains conflicting doors or control markers");
    }
    target_index = i;
  }
  const bool creates_return = !target_index.has_value();
  if (creates_return &&
      target_doors.size() >= static_cast<size_t>(zelda3::GetDungeonLimitMax(
                                 zelda3::DungeonLimit::kDoors))) {
    return absl::ResourceExhaustedError(absl::StrFormat(
        "Room 0x%03X has no free door slot (maximum %d)", target_room_id,
        zelda3::GetDungeonLimitMax(zelda3::DungeonLimit::kDoors)));
  }

  DungeonConnectionPlan plan;
  plan.request = request;
  plan.rom = source.rom();
  plan.target_room_id = target_room_id;
  plan.target_door_index = target_index.value_or(target_doors.size());
  plan.source_before = source_doors;
  plan.source_after = source_doors;
  plan.target_before = target_doors;
  plan.target_after = target_doors;
  plan.creates_return = creates_return;
  SetLayer(plan.source_after[request.source_door_index], type);
  if (creates_return) {
    plan.target_after.push_back(return_door);
  } else {
    SetLayer(plan.target_after[*target_index], type);
  }
  return plan;
}

}  // namespace yaze::editor
