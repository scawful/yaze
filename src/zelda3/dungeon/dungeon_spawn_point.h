#ifndef YAZE_ZELDA3_DUNGEON_DUNGEON_SPAWN_POINT_H
#define YAZE_ZELDA3_DUNGEON_DUNGEON_SPAWN_POINT_H

#include <array>
#include <cstddef>
#include <cstdint>
#include <utility>
#include <vector>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/str_format.h"
#include "rom/rom.h"
#include "util/macro.h"

namespace yaze::zelda3 {

// Underworld_LoadSpawnEntrance reads seven records from these discontiguous
// tables. The names mirror usdasm's SpawnPointData labels. In particular,
// 0x15C2B is only the packed quadrant byte and 0x15C32 is a full 16-bit
// overworld door tilemap value; neither is a regular-entrance door/exit field.
inline constexpr uint32_t kDungeonSpawnRoom = 0x15B6E;
inline constexpr uint32_t kDungeonSpawnCameraScrollBoundaries = 0x15B7C;
inline constexpr uint32_t kDungeonSpawnHorizontalScroll = 0x15BB4;
inline constexpr uint32_t kDungeonSpawnVerticalScroll = 0x15BC2;
inline constexpr uint32_t kDungeonSpawnYCoordinate = 0x15BD0;
inline constexpr uint32_t kDungeonSpawnXCoordinate = 0x15BDE;
inline constexpr uint32_t kDungeonSpawnCameraTriggerY = 0x15BEC;
inline constexpr uint32_t kDungeonSpawnCameraTriggerX = 0x15BFA;
inline constexpr uint32_t kDungeonSpawnMainGfx = 0x15C08;
inline constexpr uint32_t kDungeonSpawnFloor = 0x15C0F;
inline constexpr uint32_t kDungeonSpawnDungeonId = 0x15C16;
inline constexpr uint32_t kDungeonSpawnLayer = 0x15C1D;
inline constexpr uint32_t kDungeonSpawnCameraScrollController = 0x15C24;
inline constexpr uint32_t kDungeonSpawnQuadrant = 0x15C2B;
inline constexpr uint32_t kDungeonSpawnOverworldDoorTilemap = 0x15C32;
inline constexpr uint32_t kDungeonSpawnEntranceId = 0x15C40;
inline constexpr uint32_t kDungeonSpawnSong = 0x15C4E;

inline constexpr int kNumDungeonSpawnPoints = 0x07;
inline constexpr int kNumRegularDungeonEntrances = 0x85;
inline constexpr int kNumDungeonEntranceSlots =
    kNumDungeonSpawnPoints + kNumRegularDungeonEntrances;
inline constexpr uint16_t kDungeonSpawnRoomIdMask = 0x01FF;

// Compatibility names retained for existing read-only callers. New spawn code
// must use the dedicated names above. kStartingEntranceDoor is intentionally a
// documented alias only; DungeonSpawnPoint never treats that byte as a door.
inline constexpr int kStartingEntranceroom = kDungeonSpawnRoom;
inline constexpr int kStartingEntranceScrollEdge =
    kDungeonSpawnCameraScrollBoundaries;
inline constexpr int kStartingEntranceYScroll = kDungeonSpawnHorizontalScroll;
inline constexpr int kStartingEntranceXScroll = kDungeonSpawnVerticalScroll;
inline constexpr int kStartingEntranceYPosition = kDungeonSpawnYCoordinate;
inline constexpr int kStartingEntranceXPosition = kDungeonSpawnXCoordinate;
inline constexpr int kStartingEntranceCameraYTrigger =
    kDungeonSpawnCameraTriggerY;
inline constexpr int kStartingEntranceCameraXTrigger =
    kDungeonSpawnCameraTriggerX;
inline constexpr int kStartingEntranceBlockset = kDungeonSpawnMainGfx;
inline constexpr int kStartingEntranceFloor = kDungeonSpawnFloor;
inline constexpr int kStartingEntranceDungeon = kDungeonSpawnDungeonId;
inline constexpr int kStartingEntranceLadderBG = kDungeonSpawnLayer;
inline constexpr int kStartingEntrancescrolling =
    kDungeonSpawnCameraScrollController;
inline constexpr int kStartingEntranceScrollQuadrant = kDungeonSpawnQuadrant;
inline constexpr int kStartingEntranceDoor = kDungeonSpawnQuadrant;
inline constexpr int kStartingEntranceexit = kDungeonSpawnOverworldDoorTilemap;
inline constexpr int kStartingEntranceentrance = kDungeonSpawnEntranceId;
inline constexpr int kStartingEntrancemusic = kDungeonSpawnSong;
static_assert(kStartingEntranceDoor == kStartingEntranceScrollQuadrant);

using DungeonSpawnPointWriteRange = std::pair<uint32_t, uint32_t>;

// One spawn point occupies 17 discontiguous records totaling 33 bytes: nine
// words, seven scalar bytes, and one eight-byte camera-boundary record.
inline constexpr std::array<std::pair<uint32_t, uint32_t>, 17>
    kDungeonSpawnPointTableLayout = {{
        {kDungeonSpawnRoom, 2},
        {kDungeonSpawnCameraScrollBoundaries, 8},
        {kDungeonSpawnHorizontalScroll, 2},
        {kDungeonSpawnVerticalScroll, 2},
        {kDungeonSpawnYCoordinate, 2},
        {kDungeonSpawnXCoordinate, 2},
        {kDungeonSpawnCameraTriggerY, 2},
        {kDungeonSpawnCameraTriggerX, 2},
        {kDungeonSpawnMainGfx, 1},
        {kDungeonSpawnFloor, 1},
        {kDungeonSpawnDungeonId, 1},
        {kDungeonSpawnLayer, 1},
        {kDungeonSpawnCameraScrollController, 1},
        {kDungeonSpawnQuadrant, 1},
        {kDungeonSpawnOverworldDoorTilemap, 2},
        {kDungeonSpawnEntranceId, 2},
        {kDungeonSpawnSong, 1},
    }};

inline std::vector<DungeonSpawnPointWriteRange> DungeonSpawnPointWriteRanges(
    int spawn_id) {
  if (spawn_id < 0 || spawn_id >= kNumDungeonSpawnPoints) {
    return {};
  }

  std::vector<DungeonSpawnPointWriteRange> ranges;
  ranges.reserve(kDungeonSpawnPointTableLayout.size());
  for (const auto& [table_pc, record_width] : kDungeonSpawnPointTableLayout) {
    const uint32_t begin =
        table_pc + static_cast<uint32_t>(spawn_id) * record_width;
    ranges.emplace_back(begin, begin + record_width);
  }
  return ranges;
}

class DungeonSpawnPoint;

absl::Status ValidateDungeonSpawnPointForSave(const Rom& rom,
                                              const DungeonSpawnPoint& spawn,
                                              int spawn_id);

class DungeonSpawnPoint {
 public:
  DungeonSpawnPoint() = default;

  static absl::StatusOr<DungeonSpawnPoint> Load(const Rom& rom, int spawn_id) {
    if (!rom.is_loaded()) {
      return absl::FailedPreconditionError("ROM not loaded");
    }
    if (spawn_id < 0 || spawn_id >= kNumDungeonSpawnPoints) {
      return absl::OutOfRangeError(
          absl::StrFormat("Dungeon spawn point ID %d is outside [0, %d)",
                          spawn_id, kNumDungeonSpawnPoints));
    }
    for (const auto& [begin, end] : DungeonSpawnPointWriteRanges(spawn_id)) {
      if (end > rom.size()) {
        return absl::FailedPreconditionError(absl::StrFormat(
            "Dungeon spawn point 0x%02X read range 0x%06X-0x%06X exceeds "
            "ROM size 0x%X",
            spawn_id, begin, end, static_cast<uint32_t>(rom.size())));
      }
    }

    const auto read_word = [&rom](uint32_t pc) {
      return static_cast<uint16_t>(
          rom.data()[pc] | (static_cast<uint16_t>(rom.data()[pc + 1]) << 8));
    };

    DungeonSpawnPoint spawn;
    spawn.spawn_id_ = spawn_id;
    spawn.room_id = read_word(kDungeonSpawnRoom + spawn_id * 2);
    for (size_t i = 0; i < spawn.camera_scroll_boundaries.size(); ++i) {
      spawn.camera_scroll_boundaries[i] =
          rom.data()[kDungeonSpawnCameraScrollBoundaries + spawn_id * 8 + i];
    }
    spawn.horizontal_scroll =
        read_word(kDungeonSpawnHorizontalScroll + spawn_id * 2);
    spawn.vertical_scroll =
        read_word(kDungeonSpawnVerticalScroll + spawn_id * 2);
    spawn.y_coordinate = read_word(kDungeonSpawnYCoordinate + spawn_id * 2);
    spawn.x_coordinate = read_word(kDungeonSpawnXCoordinate + spawn_id * 2);
    spawn.camera_trigger_y =
        read_word(kDungeonSpawnCameraTriggerY + spawn_id * 2);
    spawn.camera_trigger_x =
        read_word(kDungeonSpawnCameraTriggerX + spawn_id * 2);
    spawn.main_gfx = rom.data()[kDungeonSpawnMainGfx + spawn_id];
    spawn.floor = rom.data()[kDungeonSpawnFloor + spawn_id];
    spawn.dungeon_id = rom.data()[kDungeonSpawnDungeonId + spawn_id];
    spawn.layer = rom.data()[kDungeonSpawnLayer + spawn_id];
    spawn.camera_scroll_controller =
        rom.data()[kDungeonSpawnCameraScrollController + spawn_id];
    spawn.quadrant = rom.data()[kDungeonSpawnQuadrant + spawn_id];
    spawn.overworld_door_tilemap =
        read_word(kDungeonSpawnOverworldDoorTilemap + spawn_id * 2);
    spawn.entrance_id = read_word(kDungeonSpawnEntranceId + spawn_id * 2);
    spawn.song = rom.data()[kDungeonSpawnSong + spawn_id];
    return spawn;
  }

  absl::Status Save(Rom* rom, int spawn_id);

  int spawn_id() const { return spawn_id_; }
  bool dirty() const { return dirty_; }
  void MarkDirty() { dirty_ = true; }
  void ClearDirty() { dirty_ = false; }

  uint16_t room_id = 0;
  std::array<uint8_t, 8> camera_scroll_boundaries{};
  uint16_t horizontal_scroll = 0;
  uint16_t vertical_scroll = 0;
  uint16_t y_coordinate = 0;
  uint16_t x_coordinate = 0;
  uint16_t camera_trigger_y = 0;
  uint16_t camera_trigger_x = 0;
  uint8_t main_gfx = 0;
  uint8_t floor = 0;
  uint8_t dungeon_id = 0;
  uint8_t layer = 0;
  uint8_t camera_scroll_controller = 0;
  uint8_t quadrant = 0;
  uint16_t overworld_door_tilemap = 0;
  uint16_t entrance_id = 0;
  uint8_t song = 0;

 private:
  int spawn_id_ = -1;
  bool dirty_ = false;
};

inline absl::Status ValidateDungeonSpawnPointForSave(
    const Rom& rom, const DungeonSpawnPoint& spawn, int spawn_id) {
  if (spawn_id < 0 || spawn_id >= kNumDungeonSpawnPoints) {
    return absl::OutOfRangeError(
        absl::StrFormat("Dungeon spawn point ID %d is outside [0, %d)",
                        spawn_id, kNumDungeonSpawnPoints));
  }
  if (spawn.spawn_id() != spawn_id) {
    return absl::InvalidArgumentError(absl::StrFormat(
        "Dungeon spawn point model ID %d does not match requested save ID %d",
        spawn.spawn_id(), spawn_id));
  }
  if (spawn.room_id > kDungeonSpawnRoomIdMask) {
    return absl::OutOfRangeError(absl::StrFormat(
        "Dungeon spawn point 0x%02X room ID 0x%04X exceeds the 9-bit "
        "runtime range",
        spawn_id, spawn.room_id));
  }
  if (spawn.entrance_id >= kNumRegularDungeonEntrances) {
    return absl::OutOfRangeError(absl::StrFormat(
        "Dungeon spawn point 0x%02X entrance ID 0x%04X is outside [0, "
        "0x%02X)",
        spawn_id, spawn.entrance_id, kNumRegularDungeonEntrances));
  }
  for (const auto& [begin, end] : DungeonSpawnPointWriteRanges(spawn_id)) {
    if (end > rom.size()) {
      return absl::FailedPreconditionError(absl::StrFormat(
          "Dungeon spawn point 0x%02X write range 0x%06X-0x%06X exceeds "
          "ROM size 0x%X",
          spawn_id, begin, end, static_cast<uint32_t>(rom.size())));
    }
  }
  return absl::OkStatus();
}

inline absl::Status DungeonSpawnPoint::Save(Rom* rom, int spawn_id) {
  if (rom == nullptr || !rom->is_loaded()) {
    return absl::FailedPreconditionError("ROM not loaded");
  }
  RETURN_IF_ERROR(ValidateDungeonSpawnPointForSave(*rom, *this, spawn_id));

  RETURN_IF_ERROR(rom->WriteShort(kDungeonSpawnRoom + spawn_id * 2, room_id));
  for (size_t i = 0; i < camera_scroll_boundaries.size(); ++i) {
    RETURN_IF_ERROR(
        rom->WriteByte(kDungeonSpawnCameraScrollBoundaries + spawn_id * 8 + i,
                       camera_scroll_boundaries[i]));
  }
  RETURN_IF_ERROR(rom->WriteShort(kDungeonSpawnHorizontalScroll + spawn_id * 2,
                                  horizontal_scroll));
  RETURN_IF_ERROR(rom->WriteShort(kDungeonSpawnVerticalScroll + spawn_id * 2,
                                  vertical_scroll));
  RETURN_IF_ERROR(
      rom->WriteShort(kDungeonSpawnYCoordinate + spawn_id * 2, y_coordinate));
  RETURN_IF_ERROR(
      rom->WriteShort(kDungeonSpawnXCoordinate + spawn_id * 2, x_coordinate));
  RETURN_IF_ERROR(rom->WriteShort(kDungeonSpawnCameraTriggerY + spawn_id * 2,
                                  camera_trigger_y));
  RETURN_IF_ERROR(rom->WriteShort(kDungeonSpawnCameraTriggerX + spawn_id * 2,
                                  camera_trigger_x));
  RETURN_IF_ERROR(rom->WriteByte(kDungeonSpawnMainGfx + spawn_id, main_gfx));
  RETURN_IF_ERROR(rom->WriteByte(kDungeonSpawnFloor + spawn_id, floor));
  RETURN_IF_ERROR(
      rom->WriteByte(kDungeonSpawnDungeonId + spawn_id, dungeon_id));
  RETURN_IF_ERROR(rom->WriteByte(kDungeonSpawnLayer + spawn_id, layer));
  RETURN_IF_ERROR(rom->WriteByte(kDungeonSpawnCameraScrollController + spawn_id,
                                 camera_scroll_controller));
  RETURN_IF_ERROR(rom->WriteByte(kDungeonSpawnQuadrant + spawn_id, quadrant));
  RETURN_IF_ERROR(
      rom->WriteShort(kDungeonSpawnOverworldDoorTilemap + spawn_id * 2,
                      overworld_door_tilemap));
  RETURN_IF_ERROR(
      rom->WriteShort(kDungeonSpawnEntranceId + spawn_id * 2, entrance_id));
  RETURN_IF_ERROR(rom->WriteByte(kDungeonSpawnSong + spawn_id, song));

  ClearDirty();
  return absl::OkStatus();
}

inline std::vector<DungeonSpawnPointWriteRange>
CollectDirtyDungeonSpawnPointWriteRanges(
    const std::array<DungeonSpawnPoint, kNumDungeonSpawnPoints>& spawn_points) {
  std::vector<DungeonSpawnPointWriteRange> ranges;
  for (int spawn_id = 0; spawn_id < kNumDungeonSpawnPoints; ++spawn_id) {
    if (!spawn_points[spawn_id].dirty()) {
      continue;
    }
    auto spawn_ranges = DungeonSpawnPointWriteRanges(spawn_id);
    ranges.insert(ranges.end(), spawn_ranges.begin(), spawn_ranges.end());
  }
  return ranges;
}

inline absl::Status ValidateDungeonSpawnPointsForSave(
    const Rom& rom,
    const std::array<DungeonSpawnPoint, kNumDungeonSpawnPoints>& spawn_points,
    bool dirty_only = true) {
  for (int spawn_id = 0; spawn_id < kNumDungeonSpawnPoints; ++spawn_id) {
    const auto& spawn = spawn_points[spawn_id];
    if (dirty_only && !spawn.dirty()) {
      continue;
    }
    RETURN_IF_ERROR(ValidateDungeonSpawnPointForSave(rom, spawn, spawn_id));
  }
  return absl::OkStatus();
}

inline absl::Status SaveAllDungeonSpawnPoints(
    Rom* rom,
    std::array<DungeonSpawnPoint, kNumDungeonSpawnPoints>& spawn_points,
    bool dirty_only = true) {
  if (rom == nullptr || !rom->is_loaded()) {
    return absl::FailedPreconditionError("ROM not loaded");
  }

  // Validate every participating record before the first write so malformed
  // data and truncated ROMs fail without partially updating another spawn.
  RETURN_IF_ERROR(
      ValidateDungeonSpawnPointsForSave(*rom, spawn_points, dirty_only));
  for (int spawn_id = 0; spawn_id < kNumDungeonSpawnPoints; ++spawn_id) {
    auto& spawn = spawn_points[spawn_id];
    if (dirty_only && !spawn.dirty()) {
      continue;
    }
    RETURN_IF_ERROR(spawn.Save(rom, spawn_id));
  }
  return absl::OkStatus();
}

}  // namespace yaze::zelda3

#endif  // YAZE_ZELDA3_DUNGEON_DUNGEON_SPAWN_POINT_H
