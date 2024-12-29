#ifndef YAZE_APP_ZELDA3_DUNGEON_ROOM_ENTRANCE_H
#define YAZE_APP_ZELDA3_DUNGEON_ROOM_ENTRANCE_H

#include <cstdint>

#include "app/rom.h"

namespace yaze {
namespace zelda3 {
namespace dungeon {

// ============================================================================
// Dungeon Entrances Related Variables
// ============================================================================

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
  RoomEntrance(Rom &rom, uint8_t entrance_id, bool is_spawn_point = false)
      : entrance_id_(entrance_id) {
    room_ =
        static_cast<short>((rom[kEntranceRoom + (entrance_id * 2) + 1] << 8) +
                           rom[kEntranceRoom + (entrance_id * 2)]);
    y_position_ = static_cast<ushort>(
        (rom[kEntranceYPosition + (entrance_id * 2) + 1] << 8) +
        rom[kEntranceYPosition + (entrance_id * 2)]);
    x_position_ = static_cast<ushort>(
        (rom[kEntranceXPosition + (entrance_id * 2) + 1] << 8) +
        rom[kEntranceXPosition + (entrance_id * 2)]);
    camera_x_ = static_cast<ushort>(
        (rom[kEntranceXScroll + (entrance_id * 2) + 1] << 8) +
        rom[kEntranceXScroll + (entrance_id * 2)]);
    camera_y_ = static_cast<ushort>(
        (rom[kEntranceYScroll + (entrance_id * 2) + 1] << 8) +
        rom[kEntranceYScroll + (entrance_id * 2)]);
    camera_trigger_y_ = static_cast<ushort>(
        (rom[(kEntranceCameraYTrigger + (entrance_id * 2)) + 1] << 8) +
        rom[kEntranceCameraYTrigger + (entrance_id * 2)]);
    camera_trigger_x_ = static_cast<ushort>(
        (rom[(kEntranceCameraXTrigger + (entrance_id * 2)) + 1] << 8) +
        rom[kEntranceCameraXTrigger + (entrance_id * 2)]);
    blockset_ = rom[kEntranceBlockset + entrance_id];
    music_ = rom[kEntranceMusic + entrance_id];
    dungeon_id_ = rom[kEntranceDungeon + entrance_id];
    floor_ = rom[kEntranceFloor + entrance_id];
    door_ = rom[kEntranceDoor + entrance_id];
    ladder_bg_ = rom[kEntranceLadderBG + entrance_id];
    scrolling_ = rom[kEntrancescrolling + entrance_id];
    scroll_quadrant_ = rom[kEntranceScrollQuadrant + entrance_id];
    exit_ =
        static_cast<short>((rom[kEntranceExit + (entrance_id * 2) + 1] << 8) +
                           rom[kEntranceExit + (entrance_id * 2)]);

    camera_boundary_qn_ = rom[kEntranceScrollEdge + 0 + (entrance_id * 8)];
    camera_boundary_fn_ = rom[kEntranceScrollEdge + 1 + (entrance_id * 8)];
    camera_boundary_qs_ = rom[kEntranceScrollEdge + 2 + (entrance_id * 8)];
    camera_boundary_fs_ = rom[kEntranceScrollEdge + 3 + (entrance_id * 8)];
    camera_boundary_qw_ = rom[kEntranceScrollEdge + 4 + (entrance_id * 8)];
    camera_boundary_fw_ = rom[kEntranceScrollEdge + 5 + (entrance_id * 8)];
    camera_boundary_qe_ = rom[kEntranceScrollEdge + 6 + (entrance_id * 8)];
    camera_boundary_fe_ = rom[kEntranceScrollEdge + 7 + (entrance_id * 8)];

    if (is_spawn_point) {
      room_ = static_cast<short>(
          (rom[kStartingEntranceroom + (entrance_id * 2) + 1] << 8) +
          rom[kStartingEntranceroom + (entrance_id * 2)]);

      y_position_ = static_cast<ushort>(
          (rom[kStartingEntranceYPosition + (entrance_id * 2) + 1] << 8) +
          rom[kStartingEntranceYPosition + (entrance_id * 2)]);

      x_position_ = static_cast<ushort>(
          (rom[kStartingEntranceXPosition + (entrance_id * 2) + 1] << 8) +
          rom[kStartingEntranceXPosition + (entrance_id * 2)]);

      camera_x_ = static_cast<ushort>(
          (rom[kStartingEntranceXScroll + (entrance_id * 2) + 1] << 8) +
          rom[kStartingEntranceXScroll + (entrance_id * 2)]);

      camera_y_ = static_cast<ushort>(
          (rom[kStartingEntranceYScroll + (entrance_id * 2) + 1] << 8) +
          rom[kStartingEntranceYScroll + (entrance_id * 2)]);

      camera_trigger_y_ = static_cast<ushort>(
          (rom[kStartingEntranceCameraYTrigger + (entrance_id * 2) + 1] << 8) +
          rom[kStartingEntranceCameraYTrigger + (entrance_id * 2)]);

      camera_trigger_x_ = static_cast<ushort>(
          (rom[kStartingEntranceCameraXTrigger + (entrance_id * 2) + 1] << 8) +
          rom[kStartingEntranceCameraXTrigger + (entrance_id * 2)]);

      blockset_ = rom[kStartingEntranceBlockset + entrance_id];
      music_ = rom[kStartingEntrancemusic + entrance_id];
      dungeon_id_ = rom[kStartingEntranceDungeon + entrance_id];
      floor_ = rom[kStartingEntranceFloor + entrance_id];
      door_ = rom[kStartingEntranceDoor + entrance_id];

      ladder_bg_ = rom[kStartingEntranceLadderBG + entrance_id];
      scrolling_ = rom[kStartingEntrancescrolling + entrance_id];
      scroll_quadrant_ = rom[kStartingEntranceScrollQuadrant + entrance_id];

      exit_ = static_cast<short>(
          ((rom[kStartingEntranceexit + (entrance_id * 2) + 1] & 0x01) << 8) +
          rom[kStartingEntranceexit + (entrance_id * 2)]);

      camera_boundary_qn_ =
          rom[kStartingEntranceScrollEdge + 0 + (entrance_id * 8)];
      camera_boundary_fn_ =
          rom[kStartingEntranceScrollEdge + 1 + (entrance_id * 8)];
      camera_boundary_qs_ =
          rom[kStartingEntranceScrollEdge + 2 + (entrance_id * 8)];
      camera_boundary_fs_ =
          rom[kStartingEntranceScrollEdge + 3 + (entrance_id * 8)];
      camera_boundary_qw_ =
          rom[kStartingEntranceScrollEdge + 4 + (entrance_id * 8)];
      camera_boundary_fw_ =
          rom[kStartingEntranceScrollEdge + 5 + (entrance_id * 8)];
      camera_boundary_qe_ =
          rom[kStartingEntranceScrollEdge + 6 + (entrance_id * 8)];
      camera_boundary_fe_ =
          rom[kStartingEntranceScrollEdge + 7 + (entrance_id * 8)];
    }
  }

  absl::Status Save(Rom &rom, int entrance_id, bool is_spawn_point = false) {
    if (!is_spawn_point) {
      RETURN_IF_ERROR(
          rom.WriteShort(kEntranceYPosition + (entrance_id * 2), y_position_));
      RETURN_IF_ERROR(
          rom.WriteShort(kEntranceXPosition + (entrance_id * 2), x_position_));
      RETURN_IF_ERROR(
          rom.WriteShort(kEntranceYScroll + (entrance_id * 2), camera_y_));
      RETURN_IF_ERROR(
          rom.WriteShort(kEntranceXScroll + (entrance_id * 2), camera_x_));
      RETURN_IF_ERROR(rom.WriteShort(
          kEntranceCameraXTrigger + (entrance_id * 2), camera_trigger_x_));
      RETURN_IF_ERROR(rom.WriteShort(
          kEntranceCameraYTrigger + (entrance_id * 2), camera_trigger_y_));
      RETURN_IF_ERROR(rom.WriteShort(kEntranceExit + (entrance_id * 2), exit_));
      RETURN_IF_ERROR(rom.Write(kEntranceBlockset + entrance_id,
                                (uint8_t)(blockset_ & 0xFF)));
      RETURN_IF_ERROR(
          rom.Write(kEntranceMusic + entrance_id, (uint8_t)(music_ & 0xFF)));
      RETURN_IF_ERROR(rom.Write(kEntranceDungeon + entrance_id,
                                (uint8_t)(dungeon_id_ & 0xFF)));
      RETURN_IF_ERROR(
          rom.Write(kEntranceDoor + entrance_id, (uint8_t)(door_ & 0xFF)));
      RETURN_IF_ERROR(
          rom.Write(kEntranceFloor + entrance_id, (uint8_t)(floor_ & 0xFF)));
      RETURN_IF_ERROR(rom.Write(kEntranceLadderBG + entrance_id,
                                (uint8_t)(ladder_bg_ & 0xFF)));
      RETURN_IF_ERROR(rom.Write(kEntrancescrolling + entrance_id,
                                (uint8_t)(scrolling_ & 0xFF)));
      RETURN_IF_ERROR(rom.Write(kEntranceScrollQuadrant + entrance_id,
                                (uint8_t)(scroll_quadrant_ & 0xFF)));
      RETURN_IF_ERROR(rom.Write(kEntranceScrollEdge + 0 + (entrance_id * 8),
                                camera_boundary_qn_));
      RETURN_IF_ERROR(rom.Write(kEntranceScrollEdge + 1 + (entrance_id * 8),
                                camera_boundary_fn_));
      RETURN_IF_ERROR(rom.Write(kEntranceScrollEdge + 2 + (entrance_id * 8),
                                camera_boundary_qs_));
      RETURN_IF_ERROR(rom.Write(kEntranceScrollEdge + 3 + (entrance_id * 8),
                                camera_boundary_fs_));
      RETURN_IF_ERROR(rom.Write(kEntranceScrollEdge + 4 + (entrance_id * 8),
                                camera_boundary_qw_));
      RETURN_IF_ERROR(rom.Write(kEntranceScrollEdge + 5 + (entrance_id * 8),
                                camera_boundary_fw_));
      RETURN_IF_ERROR(rom.Write(kEntranceScrollEdge + 6 + (entrance_id * 8),
                                camera_boundary_qe_));
      RETURN_IF_ERROR(rom.Write(kEntranceScrollEdge + 7 + (entrance_id * 8),
                                camera_boundary_fe_));
    } else {
      RETURN_IF_ERROR(
          rom.WriteShort(kStartingEntranceroom + (entrance_id * 2), room_));
      RETURN_IF_ERROR(rom.WriteShort(
          kStartingEntranceYPosition + (entrance_id * 2), y_position_));
      RETURN_IF_ERROR(rom.WriteShort(
          kStartingEntranceXPosition + (entrance_id * 2), x_position_));
      RETURN_IF_ERROR(rom.WriteShort(
          kStartingEntranceYScroll + (entrance_id * 2), camera_y_));
      RETURN_IF_ERROR(rom.WriteShort(
          kStartingEntranceXScroll + (entrance_id * 2), camera_x_));
      RETURN_IF_ERROR(
          rom.WriteShort(kStartingEntranceCameraXTrigger + (entrance_id * 2),
                         camera_trigger_x_));
      RETURN_IF_ERROR(
          rom.WriteShort(kStartingEntranceCameraYTrigger + (entrance_id * 2),
                         camera_trigger_y_));
      RETURN_IF_ERROR(
          rom.WriteShort(kStartingEntranceexit + (entrance_id * 2), exit_));
      RETURN_IF_ERROR(rom.Write(kStartingEntranceBlockset + entrance_id,
                                (uint8_t)(blockset_ & 0xFF)));
      RETURN_IF_ERROR(rom.Write(kStartingEntrancemusic + entrance_id,
                                (uint8_t)(music_ & 0xFF)));
      RETURN_IF_ERROR(rom.Write(kStartingEntranceDungeon + entrance_id,
                                (uint8_t)(dungeon_id_ & 0xFF)));
      RETURN_IF_ERROR(rom.Write(kStartingEntranceDoor + entrance_id,
                                (uint8_t)(door_ & 0xFF)));
      RETURN_IF_ERROR(rom.Write(kStartingEntranceFloor + entrance_id,
                                (uint8_t)(floor_ & 0xFF)));
      RETURN_IF_ERROR(rom.Write(kStartingEntranceLadderBG + entrance_id,
                                (uint8_t)(ladder_bg_ & 0xFF)));
      RETURN_IF_ERROR(rom.Write(kStartingEntrancescrolling + entrance_id,
                                (uint8_t)(scrolling_ & 0xFF)));
      RETURN_IF_ERROR(rom.Write(kStartingEntranceScrollQuadrant + entrance_id,
                                (uint8_t)(scroll_quadrant_ & 0xFF)));
      RETURN_IF_ERROR(
          rom.Write(kStartingEntranceScrollEdge + 0 + (entrance_id * 8),
                    camera_boundary_qn_));
      RETURN_IF_ERROR(
          rom.Write(kStartingEntranceScrollEdge + 1 + (entrance_id * 8),
                    camera_boundary_fn_));
      RETURN_IF_ERROR(
          rom.Write(kStartingEntranceScrollEdge + 2 + (entrance_id * 8),
                    camera_boundary_qs_));
      RETURN_IF_ERROR(
          rom.Write(kStartingEntranceScrollEdge + 3 + (entrance_id * 8),
                    camera_boundary_fs_));
      RETURN_IF_ERROR(
          rom.Write(kStartingEntranceScrollEdge + 4 + (entrance_id * 8),
                    camera_boundary_qw_));
      RETURN_IF_ERROR(
          rom.Write(kStartingEntranceScrollEdge + 5 + (entrance_id * 8),
                    camera_boundary_fw_));
      RETURN_IF_ERROR(
          rom.Write(kStartingEntranceScrollEdge + 6 + (entrance_id * 8),
                    camera_boundary_qe_));
      RETURN_IF_ERROR(
          rom.Write(kStartingEntranceScrollEdge + 7 + (entrance_id * 8),
                    camera_boundary_fe_));
    }
    return absl::OkStatus();
  }

  uint16_t entrance_id_;
  uint16_t x_position_;
  uint16_t y_position_;
  uint16_t camera_x_;
  uint16_t camera_y_;
  uint16_t camera_trigger_x_;
  uint16_t camera_trigger_y_;

  int16_t room_;
  uint8_t blockset_;
  uint8_t floor_;
  uint8_t dungeon_id_;
  uint8_t ladder_bg_;
  uint8_t scrolling_;
  uint8_t scroll_quadrant_;
  int16_t exit_;
  uint8_t music_;
  uint8_t door_;

  uint8_t camera_boundary_qn_;
  uint8_t camera_boundary_fn_;
  uint8_t camera_boundary_qs_;
  uint8_t camera_boundary_fs_;
  uint8_t camera_boundary_qw_;
  uint8_t camera_boundary_fw_;
  uint8_t camera_boundary_qe_;
  uint8_t camera_boundary_fe_;
};

}  // namespace dungeon
}  // namespace zelda3

}  // namespace yaze

#endif  // YAZE_APP_ZELDA3_DUNGEON_ROOM_ENTRANCE_H
