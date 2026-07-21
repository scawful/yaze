#ifndef YAZE_APP_ZELDA3_DUNGEON_ROOM_ENTRANCE_H
#define YAZE_APP_ZELDA3_DUNGEON_ROOM_ENTRANCE_H

#include <array>
#include <cstddef>
#include <cstdint>
#include <utility>
#include <vector>

#include "absl/status/status.h"
#include "absl/strings/str_format.h"
#include "rom/rom.h"
#include "util/macro.h"
#include "zelda3/dungeon/dungeon_rom_addresses.h"

namespace yaze {
namespace zelda3 {

// 0x14577 word value for each room
constexpr int kEntranceRoom = 0x14813;

// 8 bytes per room, HU, FU, HD, FD, HL, FL, HR, FR
constexpr int kEntranceScrollEdge = 0x1491D;      // 0x14681
constexpr int kEntranceYScroll = 0x14D45;         // 0x14AA9 2 bytes each room
constexpr int kEntranceXScroll = 0x14E4F;         // 0x14BB3 2 bytes
constexpr int kEntranceYPosition = 0x14F59;       // 0x14CBD 2bytes
constexpr int kEntranceXPosition = 0x15063;       // 0x14DC7 2bytes
constexpr int kEntranceCameraYTrigger = 0x1516D;  // 0x14ED1 2bytes
constexpr int kEntranceCameraXTrigger = 0x15277;  // 0x14FDB 2bytes

constexpr int kEntranceBlockset = 0x15381;  // 0x150E5 1byte
constexpr int kEntranceFloor = 0x15406;     // 0x1516A 1byte
constexpr int kEntranceDungeon = 0x1548B;   // 0x151EF 1byte (dungeon id)
constexpr int kEntranceDoor = 0x15510;      // 0x15274 1byte

// 1 byte, ---b ---a b = bg2, a = need to check
constexpr int kEntranceLadderBG = 0x15595;        // 0x152F9
constexpr int kEntrancescrolling = 0x1561A;       // 0x1537E 1byte --h- --v-
constexpr int kEntranceScrollQuadrant = 0x1569F;  // 0x15403 1byte
constexpr int kEntranceExit = 0x15724;            // 0x15488 2byte word
constexpr int kEntranceMusic = 0x1582E;           // 0x15592

// word value for each room
constexpr int kStartingEntranceroom = 0x15B6E;  // 0x158D2

// 8 bytes per room, HU, FU, HD, FD, HL, FL, HR, FR
constexpr int kStartingEntranceScrollEdge = 0x15B7C;  // 0x158E0
constexpr int kStartingEntranceYScroll = 0x15BB4;  // 0x14AA9 //2bytes each room
constexpr int kStartingEntranceXScroll = 0x15BC2;  // 0x14BB3 //2bytes
constexpr int kStartingEntranceYPosition = 0x15BD0;       // 0x14CBD 2bytes
constexpr int kStartingEntranceXPosition = 0x15BDE;       // 0x14DC7 2bytes
constexpr int kStartingEntranceCameraYTrigger = 0x15BEC;  // 0x14ED1 2bytes
constexpr int kStartingEntranceCameraXTrigger = 0x15BFA;  // 0x14FDB 2bytes

constexpr int kStartingEntranceBlockset = 0x15C08;  // 0x150E5 1byte
constexpr int kStartingEntranceFloor = 0x15C0F;     // 0x1516A 1byte
constexpr int kStartingEntranceDungeon = 0x15C16;  // 0x151EF 1byte (dungeon id)

constexpr int kStartingEntranceDoor = 0x15C2B;  // 0x15274 1byte

// 1 byte, ---b ---a b = bg2, a = need to check
constexpr int kStartingEntranceLadderBG = 0x15C1D;  // 0x152F9
// 1byte --h- --v-
constexpr int kStartingEntrancescrolling = 0x15C24;       // 0x1537E
constexpr int kStartingEntranceScrollQuadrant = 0x15C2B;  // 0x15403 1byte
constexpr int kStartingEntranceexit = 0x15C32;   // 0x15488 //2byte word
constexpr int kStartingEntrancemusic = 0x15C4E;  // 0x15592
constexpr int kStartingEntranceentrance = 0x15C40;

constexpr int kNumDungeonSpawnPoints = 0x07;
constexpr int kNumRegularDungeonEntrances = 0x85;
constexpr int kNumDungeonEntranceSlots =
    kNumDungeonSpawnPoints + kNumRegularDungeonEntrances;

using DungeonEntranceWriteRange = std::pair<uint32_t, uint32_t>;

// One regular entrance is stored as 17 discontiguous records: eight words,
// eight scalar bytes, and one eight-byte camera-boundary record. Keep this
// layout shared by save validation, manifest preflight, and write prediction so
// those safety surfaces cannot drift apart.
constexpr std::array<std::pair<uint32_t, uint32_t>, 17>
    kRegularDungeonEntranceTableLayout = {{
        {kEntranceRoom, 2},
        {kEntranceScrollEdge, 8},
        {kEntranceYScroll, 2},
        {kEntranceXScroll, 2},
        {kEntranceYPosition, 2},
        {kEntranceXPosition, 2},
        {kEntranceCameraYTrigger, 2},
        {kEntranceCameraXTrigger, 2},
        {kEntranceBlockset, 1},
        {kEntranceFloor, 1},
        {kEntranceDungeon, 1},
        {kEntranceDoor, 1},
        {kEntranceLadderBG, 1},
        {kEntrancescrolling, 1},
        {kEntranceScrollQuadrant, 1},
        {kEntranceExit, 2},
        {kEntranceMusic, 1},
    }};

inline std::vector<DungeonEntranceWriteRange> RegularDungeonEntranceWriteRanges(
    int entrance_id) {
  if (entrance_id < 0 || entrance_id >= kNumRegularDungeonEntrances) {
    return {};
  }

  std::vector<DungeonEntranceWriteRange> ranges;
  ranges.reserve(kRegularDungeonEntranceTableLayout.size());
  for (const auto& [table_pc, record_width] :
       kRegularDungeonEntranceTableLayout) {
    const uint32_t begin =
        table_pc + static_cast<uint32_t>(entrance_id) * record_width;
    ranges.emplace_back(begin, begin + record_width);
  }
  return ranges;
}

class RoomEntrance;

absl::Status ValidateRegularDungeonEntranceForSave(const Rom& rom,
                                                   const RoomEntrance& entrance,
                                                   int entrance_id);

constexpr int items_data_start = 0xDDE9;  // save purpose
constexpr int items_data_end = 0xE6B2;    // save purpose
constexpr int initial_equipement = 0x271A6;

// item id you get instead if you already have that item
constexpr int chests_backupitems = 0x3B528;
constexpr int chests_yoffset = 0x4836C;
constexpr int chests_xoffset = 0x4836C + (76 * 1);
constexpr int chests_itemsgfx = 0x4836C + (76 * 2);
constexpr int chests_itemswide = 0x4836C + (76 * 3);
constexpr int chests_itemsproperties = 0x4836C + (76 * 4);
constexpr int chests_sramaddress = 0x4836C + (76 * 5);
constexpr int chests_sramvalue = 0x4836C + (76 * 7);
constexpr int chests_msgid = 0x442DD;

constexpr int dungeons_startrooms = 0x7939;
constexpr int dungeons_endrooms = 0x792D;
constexpr int dungeons_bossrooms = 0x10954;  // short value

// Bed Related Values (Starting location)
constexpr int bedPositionX = 0x039A37;  // short value
constexpr int bedPositionY = 0x039A32;  // short value

// short value (on 2 different bytes)
constexpr int bedPositionResetXLow = 0x02DE53;
constexpr int bedPositionResetXHigh = 0x02DE58;

// short value (on 2 different bytes)
constexpr int bedPositionResetYLow = 0x02DE5D;
constexpr int bedPositionResetYHigh = 0x02DE62;

constexpr int bedSheetPositionX = 0x0480BD;  // short value
constexpr int bedSheetPositionY = 0x0480B8;  // short value

/**
 * @brief Dungeon Room Entrance or Spawn Point
 */
class RoomEntrance {
 public:
  RoomEntrance() = default;
  RoomEntrance(Rom* rom, uint8_t entrance_id, bool is_spawn_point = false)
      : entrance_id_(entrance_id), is_spawn_point_(is_spawn_point) {
    room_ = static_cast<short>(
        (rom->data()[kEntranceRoom + (entrance_id * 2) + 1] << 8) +
        rom->data()[kEntranceRoom + (entrance_id * 2)]);
    y_position_ = static_cast<uint16_t>(
        (rom->data()[kEntranceYPosition + (entrance_id * 2) + 1] << 8) +
        rom->data()[kEntranceYPosition + (entrance_id * 2)]);
    x_position_ = static_cast<uint16_t>(
        (rom->data()[kEntranceXPosition + (entrance_id * 2) + 1] << 8) +
        rom->data()[kEntranceXPosition + (entrance_id * 2)]);
    camera_x_ = static_cast<uint16_t>(
        (rom->data()[kEntranceXScroll + (entrance_id * 2) + 1] << 8) +
        rom->data()[kEntranceXScroll + (entrance_id * 2)]);
    camera_y_ = static_cast<uint16_t>(
        (rom->data()[kEntranceYScroll + (entrance_id * 2) + 1] << 8) +
        rom->data()[kEntranceYScroll + (entrance_id * 2)]);
    camera_trigger_y_ = static_cast<uint16_t>(
        (rom->data()[(kEntranceCameraYTrigger + (entrance_id * 2)) + 1] << 8) +
        rom->data()[kEntranceCameraYTrigger + (entrance_id * 2)]);
    camera_trigger_x_ = static_cast<uint16_t>(
        (rom->data()[(kEntranceCameraXTrigger + (entrance_id * 2)) + 1] << 8) +
        rom->data()[kEntranceCameraXTrigger + (entrance_id * 2)]);
    blockset_ = rom->data()[kEntranceBlockset + entrance_id];
    music_ = rom->data()[kEntranceMusic + entrance_id];
    dungeon_id_ = rom->data()[kEntranceDungeon + entrance_id];
    floor_ = rom->data()[kEntranceFloor + entrance_id];
    door_ = rom->data()[kEntranceDoor + entrance_id];
    ladder_bg_ = rom->data()[kEntranceLadderBG + entrance_id];
    scrolling_ = rom->data()[kEntrancescrolling + entrance_id];
    scroll_quadrant_ = rom->data()[kEntranceScrollQuadrant + entrance_id];
    exit_ = static_cast<short>(
        (rom->data()[kEntranceExit + (entrance_id * 2) + 1] << 8) +
        rom->data()[kEntranceExit + (entrance_id * 2)]);

    camera_boundary_qn_ =
        rom->data()[kEntranceScrollEdge + 0 + (entrance_id * 8)];
    camera_boundary_fn_ =
        rom->data()[kEntranceScrollEdge + 1 + (entrance_id * 8)];
    camera_boundary_qs_ =
        rom->data()[kEntranceScrollEdge + 2 + (entrance_id * 8)];
    camera_boundary_fs_ =
        rom->data()[kEntranceScrollEdge + 3 + (entrance_id * 8)];
    camera_boundary_qw_ =
        rom->data()[kEntranceScrollEdge + 4 + (entrance_id * 8)];
    camera_boundary_fw_ =
        rom->data()[kEntranceScrollEdge + 5 + (entrance_id * 8)];
    camera_boundary_qe_ =
        rom->data()[kEntranceScrollEdge + 6 + (entrance_id * 8)];
    camera_boundary_fe_ =
        rom->data()[kEntranceScrollEdge + 7 + (entrance_id * 8)];

    if (is_spawn_point) {
      room_ = static_cast<short>(
          (rom->data()[kStartingEntranceroom + (entrance_id * 2) + 1] << 8) +
          rom->data()[kStartingEntranceroom + (entrance_id * 2)]);

      y_position_ = static_cast<uint16_t>(
          (rom->data()[kStartingEntranceYPosition + (entrance_id * 2) + 1]
           << 8) +
          rom->data()[kStartingEntranceYPosition + (entrance_id * 2)]);

      x_position_ = static_cast<uint16_t>(
          (rom->data()[kStartingEntranceXPosition + (entrance_id * 2) + 1]
           << 8) +
          rom->data()[kStartingEntranceXPosition + (entrance_id * 2)]);

      camera_x_ = static_cast<uint16_t>(
          (rom->data()[kStartingEntranceXScroll + (entrance_id * 2) + 1] << 8) +
          rom->data()[kStartingEntranceXScroll + (entrance_id * 2)]);

      camera_y_ = static_cast<uint16_t>(
          (rom->data()[kStartingEntranceYScroll + (entrance_id * 2) + 1] << 8) +
          rom->data()[kStartingEntranceYScroll + (entrance_id * 2)]);

      camera_trigger_y_ = static_cast<uint16_t>(
          (rom->data()[kStartingEntranceCameraYTrigger + (entrance_id * 2) + 1]
           << 8) +
          rom->data()[kStartingEntranceCameraYTrigger + (entrance_id * 2)]);

      camera_trigger_x_ = static_cast<uint16_t>(
          (rom->data()[kStartingEntranceCameraXTrigger + (entrance_id * 2) + 1]
           << 8) +
          rom->data()[kStartingEntranceCameraXTrigger + (entrance_id * 2)]);

      blockset_ = rom->data()[kStartingEntranceBlockset + entrance_id];
      music_ = rom->data()[kStartingEntrancemusic + entrance_id];
      dungeon_id_ = rom->data()[kStartingEntranceDungeon + entrance_id];
      floor_ = rom->data()[kStartingEntranceFloor + entrance_id];
      door_ = rom->data()[kStartingEntranceDoor + entrance_id];

      ladder_bg_ = rom->data()[kStartingEntranceLadderBG + entrance_id];
      scrolling_ = rom->data()[kStartingEntrancescrolling + entrance_id];
      scroll_quadrant_ =
          rom->data()[kStartingEntranceScrollQuadrant + entrance_id];

      exit_ = static_cast<short>(
          ((rom->data()[kStartingEntranceexit + (entrance_id * 2) + 1] & 0x01)
           << 8) +
          rom->data()[kStartingEntranceexit + (entrance_id * 2)]);

      camera_boundary_qn_ =
          rom->data()[kStartingEntranceScrollEdge + 0 + (entrance_id * 8)];
      camera_boundary_fn_ =
          rom->data()[kStartingEntranceScrollEdge + 1 + (entrance_id * 8)];
      camera_boundary_qs_ =
          rom->data()[kStartingEntranceScrollEdge + 2 + (entrance_id * 8)];
      camera_boundary_fs_ =
          rom->data()[kStartingEntranceScrollEdge + 3 + (entrance_id * 8)];
      camera_boundary_qw_ =
          rom->data()[kStartingEntranceScrollEdge + 4 + (entrance_id * 8)];
      camera_boundary_fw_ =
          rom->data()[kStartingEntranceScrollEdge + 5 + (entrance_id * 8)];
      camera_boundary_qe_ =
          rom->data()[kStartingEntranceScrollEdge + 6 + (entrance_id * 8)];
      camera_boundary_fe_ =
          rom->data()[kStartingEntranceScrollEdge + 7 + (entrance_id * 8)];
    }
  }

  bool dirty() const { return dirty_; }
  void MarkDirty() { dirty_ = true; }
  void ClearDirty() { dirty_ = false; }
  bool is_spawn_point() const { return is_spawn_point_; }

  absl::Status Save(Rom* rom, int entrance_id, bool is_spawn_point = false) {
    if (!rom || !rom->is_loaded()) {
      return absl::FailedPreconditionError("ROM not loaded");
    }
    if (is_spawn_point) {
      return absl::FailedPreconditionError(absl::StrFormat(
          "Dungeon spawn point 0x%02X cannot be saved: spawn writes are "
          "disabled until the dedicated spawn ROM schema is modeled safely",
          entrance_id));
    }

    // Deliberate defense in depth for direct callers outside the bulk editor
    // preflight: reject model/ID mismatches before the first regular write.
    RETURN_IF_ERROR(
        ValidateRegularDungeonEntranceForSave(*rom, *this, entrance_id));
    RETURN_IF_ERROR(rom->WriteShort(kEntranceRoom + (entrance_id * 2), room_));
    RETURN_IF_ERROR(
        rom->WriteShort(kEntranceYPosition + (entrance_id * 2), y_position_));
    RETURN_IF_ERROR(
        rom->WriteShort(kEntranceXPosition + (entrance_id * 2), x_position_));
    RETURN_IF_ERROR(
        rom->WriteShort(kEntranceYScroll + (entrance_id * 2), camera_y_));
    RETURN_IF_ERROR(
        rom->WriteShort(kEntranceXScroll + (entrance_id * 2), camera_x_));
    RETURN_IF_ERROR(rom->WriteShort(kEntranceCameraXTrigger + (entrance_id * 2),
                                    camera_trigger_x_));
    RETURN_IF_ERROR(rom->WriteShort(kEntranceCameraYTrigger + (entrance_id * 2),
                                    camera_trigger_y_));
    RETURN_IF_ERROR(rom->WriteShort(kEntranceExit + (entrance_id * 2), exit_));
    RETURN_IF_ERROR(rom->WriteByte(kEntranceBlockset + entrance_id,
                                   (uint8_t)(blockset_ & 0xFF)));
    RETURN_IF_ERROR(
        rom->WriteByte(kEntranceMusic + entrance_id, (uint8_t)(music_ & 0xFF)));
    RETURN_IF_ERROR(rom->WriteByte(kEntranceDungeon + entrance_id,
                                   (uint8_t)(dungeon_id_ & 0xFF)));
    RETURN_IF_ERROR(
        rom->WriteByte(kEntranceDoor + entrance_id, (uint8_t)(door_ & 0xFF)));
    RETURN_IF_ERROR(
        rom->WriteByte(kEntranceFloor + entrance_id, (uint8_t)(floor_ & 0xFF)));
    RETURN_IF_ERROR(rom->WriteByte(kEntranceLadderBG + entrance_id,
                                   (uint8_t)(ladder_bg_ & 0xFF)));
    RETURN_IF_ERROR(rom->WriteByte(kEntrancescrolling + entrance_id,
                                   (uint8_t)(scrolling_ & 0xFF)));
    RETURN_IF_ERROR(rom->WriteByte(kEntranceScrollQuadrant + entrance_id,
                                   (uint8_t)(scroll_quadrant_ & 0xFF)));
    RETURN_IF_ERROR(rom->WriteByte(kEntranceScrollEdge + 0 + (entrance_id * 8),
                                   camera_boundary_qn_));
    RETURN_IF_ERROR(rom->WriteByte(kEntranceScrollEdge + 1 + (entrance_id * 8),
                                   camera_boundary_fn_));
    RETURN_IF_ERROR(rom->WriteByte(kEntranceScrollEdge + 2 + (entrance_id * 8),
                                   camera_boundary_qs_));
    RETURN_IF_ERROR(rom->WriteByte(kEntranceScrollEdge + 3 + (entrance_id * 8),
                                   camera_boundary_fs_));
    RETURN_IF_ERROR(rom->WriteByte(kEntranceScrollEdge + 4 + (entrance_id * 8),
                                   camera_boundary_qw_));
    RETURN_IF_ERROR(rom->WriteByte(kEntranceScrollEdge + 5 + (entrance_id * 8),
                                   camera_boundary_fw_));
    RETURN_IF_ERROR(rom->WriteByte(kEntranceScrollEdge + 6 + (entrance_id * 8),
                                   camera_boundary_qe_));
    RETURN_IF_ERROR(rom->WriteByte(kEntranceScrollEdge + 7 + (entrance_id * 8),
                                   camera_boundary_fe_));
    ClearDirty();
    return absl::OkStatus();
  }

  uint16_t entrance_id_ = 0;
  uint16_t x_position_ = 0;
  uint16_t y_position_ = 0;
  uint16_t camera_x_ = 0;
  uint16_t camera_y_ = 0;
  uint16_t camera_trigger_x_ = 0;
  uint16_t camera_trigger_y_ = 0;

  int16_t room_ = 0;
  uint8_t blockset_ = 0;
  uint8_t floor_ = 0;
  uint8_t dungeon_id_ = 0;
  uint8_t ladder_bg_ = 0;
  uint8_t scrolling_ = 0;
  uint8_t scroll_quadrant_ = 0;
  int16_t exit_ = 0;
  uint8_t music_ = 0;
  uint8_t door_ = 0;

  uint8_t camera_boundary_qn_ = 0;
  uint8_t camera_boundary_fn_ = 0;
  uint8_t camera_boundary_qs_ = 0;
  uint8_t camera_boundary_fs_ = 0;
  uint8_t camera_boundary_qw_ = 0;
  uint8_t camera_boundary_fw_ = 0;
  uint8_t camera_boundary_qe_ = 0;
  uint8_t camera_boundary_fe_ = 0;

 private:
  bool dirty_ = false;
  bool is_spawn_point_ = false;
};

inline std::vector<DungeonEntranceWriteRange>
CollectDirtyRegularDungeonEntranceWriteRanges(
    const std::array<RoomEntrance, kNumDungeonEntranceSlots>& entrances) {
  std::vector<DungeonEntranceWriteRange> ranges;
  for (int entrance_id = 0; entrance_id < kNumRegularDungeonEntrances;
       ++entrance_id) {
    const auto& entrance = entrances[kNumDungeonSpawnPoints + entrance_id];
    if (!entrance.dirty()) {
      continue;
    }
    auto entrance_ranges = RegularDungeonEntranceWriteRanges(entrance_id);
    ranges.insert(ranges.end(), entrance_ranges.begin(), entrance_ranges.end());
  }
  return ranges;
}

inline absl::Status ValidateRegularDungeonEntranceForSave(
    const Rom& rom, const RoomEntrance& entrance, int entrance_id) {
  if (entrance_id < 0 || entrance_id >= kNumRegularDungeonEntrances) {
    return absl::OutOfRangeError(
        absl::StrFormat("Regular dungeon entrance ID %d is outside [0, %d)",
                        entrance_id, kNumRegularDungeonEntrances));
  }
  if (entrance.is_spawn_point()) {
    return absl::InvalidArgumentError(absl::StrFormat(
        "Regular dungeon entrance 0x%02X is marked as a spawn point",
        entrance_id));
  }
  if (entrance.entrance_id_ != static_cast<uint16_t>(entrance_id)) {
    return absl::InvalidArgumentError(absl::StrFormat(
        "Regular dungeon entrance model ID 0x%02X does not match requested "
        "save ID 0x%02X",
        static_cast<int>(entrance.entrance_id_), entrance_id));
  }
  if (entrance.room_ < 0 || entrance.room_ >= kNumberOfRooms) {
    return absl::OutOfRangeError(absl::StrFormat(
        "Regular dungeon entrance 0x%02X room ID %d is outside [0, %d)",
        entrance_id, entrance.room_, kNumberOfRooms));
  }

  for (const auto& [begin, end] :
       RegularDungeonEntranceWriteRanges(entrance_id)) {
    if (end > rom.size()) {
      return absl::FailedPreconditionError(absl::StrFormat(
          "Regular dungeon entrance 0x%02X write range "
          "0x%06X-0x%06X exceeds ROM size 0x%X",
          entrance_id, begin, end, static_cast<uint32_t>(rom.size())));
    }
  }
  return absl::OkStatus();
}

inline absl::Status ValidateRegularDungeonEntrancesForSave(
    const Rom& rom,
    const std::array<RoomEntrance, kNumDungeonEntranceSlots>& entrances,
    bool dirty_only = true) {
  for (int entrance_id = 0; entrance_id < kNumRegularDungeonEntrances;
       ++entrance_id) {
    const auto& entrance = entrances[kNumDungeonSpawnPoints + entrance_id];
    if (dirty_only && !entrance.dirty()) {
      continue;
    }
    RETURN_IF_ERROR(
        ValidateRegularDungeonEntranceForSave(rom, entrance, entrance_id));
  }
  return absl::OkStatus();
}

inline absl::Status RejectUnsupportedDungeonSpawnPointSaves(
    const std::array<RoomEntrance, kNumDungeonEntranceSlots>& entrances,
    bool dirty_only = true) {
  for (int spawn_id = 0; spawn_id < kNumDungeonSpawnPoints; ++spawn_id) {
    if (dirty_only && !entrances[spawn_id].dirty()) {
      continue;
    }
    return absl::FailedPreconditionError(absl::StrFormat(
        "Dungeon spawn point 0x%02X is scheduled for save, but spawn saving "
        "is deferred until its distinct ROM schema is modeled safely",
        spawn_id));
  }
  return absl::OkStatus();
}

inline absl::Status SaveAllDungeonEntrances(
    Rom* rom, std::array<RoomEntrance, kNumDungeonEntranceSlots>& entrances,
    bool dirty_only = true) {
  if (!rom || !rom->is_loaded()) {
    return absl::FailedPreconditionError("ROM not loaded");
  }

  // Spawn writes are not permitted through this coordinator until their
  // distinct ROM schema is modeled. Reject them before validating or writing
  // any regular record so prediction remains a complete list of allowed writes.
  RETURN_IF_ERROR(
      RejectUnsupportedDungeonSpawnPointSaves(entrances, dirty_only));

  // Validate every participating regular record before the first write.
  RETURN_IF_ERROR(
      ValidateRegularDungeonEntrancesForSave(*rom, entrances, dirty_only));

  for (int entrance_id = 0; entrance_id < kNumRegularDungeonEntrances;
       ++entrance_id) {
    auto& entrance = entrances[kNumDungeonSpawnPoints + entrance_id];
    if (dirty_only && !entrance.dirty()) {
      continue;
    }
    RETURN_IF_ERROR(entrance.Save(rom, entrance_id, false));
  }

  return absl::OkStatus();
}

}  // namespace zelda3
}  // namespace yaze

#endif  // YAZE_APP_ZELDA3_DUNGEON_ROOM_ENTRANCE_H
