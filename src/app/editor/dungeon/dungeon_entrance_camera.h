#ifndef YAZE_APP_EDITOR_DUNGEON_DUNGEON_ENTRANCE_CAMERA_H_
#define YAZE_APP_EDITOR_DUNGEON_DUNGEON_ENTRANCE_CAMERA_H_

#include <array>
#include <cstdint>
#include <string>
#include <vector>

namespace yaze::zelda3 {
class DungeonSpawnPoint;
class RoomEntrance;
}  // namespace yaze::zelda3

namespace yaze::editor {

enum class DungeonEntranceRecordKind {
  kRegularEntrance,
  kSpawnPoint,
};

struct DungeonEntranceCameraState {
  DungeonEntranceRecordKind kind = DungeonEntranceRecordKind::kRegularEntrance;
  int room_id = -1;
  uint16_t player_x = 0;
  uint16_t player_y = 0;
  uint16_t camera_x = 0;
  uint16_t camera_y = 0;
  uint16_t trigger_x = 0;
  uint16_t trigger_y = 0;
  // QN, FN, QS, FS, QW, FW, QE, FE.
  std::array<uint8_t, 8> boundaries{};
  uint8_t quadrant = 0;
  bool dirty = false;

  bool operator==(const DungeonEntranceCameraState&) const = default;
};

struct DungeonEntranceCameraDerived {
  uint16_t camera_x = 0;
  uint16_t camera_y = 0;
  uint16_t trigger_x = 0;
  uint16_t trigger_y = 0;
  std::array<uint8_t, 8> boundaries{};
  uint8_t quadrant = 0;
};

struct DungeonEntranceCameraValidation {
  DungeonEntranceCameraDerived expected;
  std::vector<std::string> errors;
  std::vector<std::string> differences;
  bool quadrant_repair_safe = true;
  bool has_repairable_difference = false;
  bool repair_source_valid = true;

  bool geometry_valid() const { return errors.empty(); }
  bool matches_derived() const { return differences.empty(); }
  bool can_repair() const {
    return repair_source_valid && has_repairable_difference;
  }
};

DungeonEntranceCameraDerived DeriveDungeonEntranceCamera(int room_id,
                                                         uint16_t player_x,
                                                         uint16_t player_y);

DungeonEntranceCameraValidation ValidateDungeonEntranceCamera(
    const DungeonEntranceCameraState& state);

// Returns a copy with only camera-derived fields changed. Room 0x104 has a
// known vanilla quadrant exception, so its quadrant is preserved.
DungeonEntranceCameraState BuildRepairedDungeonEntranceCamera(
    const DungeonEntranceCameraState& state);

DungeonEntranceCameraState CaptureDungeonEntranceCamera(
    const zelda3::RoomEntrance& entrance);
DungeonEntranceCameraState CaptureDungeonSpawnCamera(
    const zelda3::DungeonSpawnPoint& spawn);
void ApplyDungeonEntranceCameraState(const DungeonEntranceCameraState& state,
                                     zelda3::RoomEntrance* entrance);
void ApplyDungeonSpawnCameraState(const DungeonEntranceCameraState& state,
                                  zelda3::DungeonSpawnPoint* spawn);

}  // namespace yaze::editor

#endif  // YAZE_APP_EDITOR_DUNGEON_DUNGEON_ENTRANCE_CAMERA_H_
