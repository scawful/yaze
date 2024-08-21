#ifndef YAZE_APP_ZELDA3_DUNGEON_ROOM_ENTRANCE_H
#define YAZE_APP_ZELDA3_DUNGEON_ROOM_ENTRANCE_H

#include <cstdint>

#include "app/rom.h"

namespace yaze {
namespace app {
namespace zelda3 {
namespace dungeon {

// ============================================================================
// Dungeon Entrances Related Variables
// ============================================================================

// 0x14577 word value for each room
constexpr int kEntranceroom = 0x14813;

// 8 bytes per room, HU, FU, HD, FD, HL, FL, HR, FR
constexpr int kEntrancescrolledge = 0x1491D;      // 0x14681
constexpr int kEntranceyscroll = 0x14D45;         // 0x14AA9 2 bytes each room
constexpr int kEntrancexscroll = 0x14E4F;         // 0x14BB3 2 bytes
constexpr int kEntranceyposition = 0x14F59;       // 0x14CBD 2bytes
constexpr int kEntrancexposition = 0x15063;       // 0x14DC7 2bytes
constexpr int kEntrancecameraytrigger = 0x1516D;  // 0x14ED1 2bytes
constexpr int kEntrancecameraxtrigger = 0x15277;  // 0x14FDB 2bytes

constexpr int kEntranceblockset = 0x15381;  // 0x150E5 1byte
constexpr int kEntrancefloor = 0x15406;     // 0x1516A 1byte
constexpr int kEntrancedungeon = 0x1548B;   // 0x151EF 1byte (dungeon id)
constexpr int kEntrancedoor = 0x15510;      // 0x15274 1byte

// 1 byte, ---b ---a b = bg2, a = need to check
constexpr int kEntranceladderbg = 0x15595;        // 0x152F9
constexpr int kEntrancescrolling = 0x1561A;       // 0x1537E 1byte --h- --v-
constexpr int kEntrancescrollquadrant = 0x1569F;  // 0x15403 1byte
constexpr int kEntranceexit = 0x15724;            // 0x15488 2byte word
constexpr int kEntrancemusic = 0x1582E;           // 0x15592

// word value for each room
constexpr int kStartingEntranceroom = 0x15B6E;  // 0x158D2

// 8 bytes per room, HU, FU, HD, FD, HL, FL, HR, FR
constexpr int kStartingEntrancescrolledge = 0x15B7C;  // 0x158E0
constexpr int kStartingEntranceyscroll = 0x15BB4;  // 0x14AA9 //2bytes each room
constexpr int kStartingEntrancexscroll = 0x15BC2;  // 0x14BB3 //2bytes
constexpr int kStartingEntranceyposition = 0x15BD0;       // 0x14CBD 2bytes
constexpr int kStartingEntrancexposition = 0x15BDE;       // 0x14DC7 2bytes
constexpr int kStartingEntrancecameraytrigger = 0x15BEC;  // 0x14ED1 2bytes
constexpr int kStartingEntrancecameraxtrigger = 0x15BFA;  // 0x14FDB 2bytes

constexpr int kStartingEntranceblockset = 0x15C08;  // 0x150E5 1byte
constexpr int kStartingEntrancefloor = 0x15C0F;     // 0x1516A 1byte
constexpr int kStartingEntrancedungeon = 0x15C16;  // 0x151EF 1byte (dungeon id)

constexpr int kStartingEntrancedoor = 0x15C2B;  // 0x15274 1byte

// 1 byte, ---b ---a b = bg2, a = need to check
constexpr int kStartingEntranceladderbg = 0x15C1D;  // 0x152F9
// 1byte --h- --v-
constexpr int kStartingEntrancescrolling = 0x15C24;       // 0x1537E
constexpr int kStartingEntrancescrollquadrant = 0x15C2B;  // 0x15403 1byte
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

class RoomEntrance {
 public:
  RoomEntrance() = default;

  RoomEntrance(Rom& rom, uint8_t entrance_id, bool is_spawn_point = false)
      : entrance_id_(entrance_id) {
    room_ =
        static_cast<short>((rom[kEntranceroom + (entrance_id * 2) + 1] << 8) +
                           rom[kEntranceroom + (entrance_id * 2)]);
    y_position_ = static_cast<ushort>(
        (rom[kEntranceyposition + (entrance_id * 2) + 1] << 8) +
        rom[kEntranceyposition + (entrance_id * 2)]);
    x_position_ = static_cast<ushort>(
        (rom[kEntrancexposition + (entrance_id * 2) + 1] << 8) +
        rom[kEntrancexposition + (entrance_id * 2)]);
    camera_x_ = static_cast<ushort>(
        (rom[kEntrancexscroll + (entrance_id * 2) + 1] << 8) +
        rom[kEntrancexscroll + (entrance_id * 2)]);
    camera_y_ = static_cast<ushort>(
        (rom[kEntranceyscroll + (entrance_id * 2) + 1] << 8) +
        rom[kEntranceyscroll + (entrance_id * 2)]);
    camera_trigger_y_ = static_cast<ushort>(
        (rom[(kEntrancecameraytrigger + (entrance_id * 2)) + 1] << 8) +
        rom[kEntrancecameraytrigger + (entrance_id * 2)]);
    camera_trigger_x_ = static_cast<ushort>(
        (rom[(kEntrancecameraxtrigger + (entrance_id * 2)) + 1] << 8) +
        rom[kEntrancecameraxtrigger + (entrance_id * 2)]);
    blockset_ = rom[kEntranceblockset + entrance_id];
    music_ = rom[kEntrancemusic + entrance_id];
    dungeon_id_ = rom[kEntrancedungeon + entrance_id];
    floor_ = rom[kEntrancefloor + entrance_id];
    door_ = rom[kEntrancedoor + entrance_id];
    ladder_bg_ = rom[kEntranceladderbg + entrance_id];
    scrolling_ = rom[kEntrancescrolling + entrance_id];
    scroll_quadrant_ = rom[kEntrancescrollquadrant + entrance_id];
    exit_ =
        static_cast<short>((rom[kEntranceexit + (entrance_id * 2) + 1] << 8) +
                           rom[kEntranceexit + (entrance_id * 2)]);

    camera_boundary_qn_ = rom[kEntrancescrolledge + 0 + (entrance_id * 8)];
    camera_boundary_fn_ = rom[kEntrancescrolledge + 1 + (entrance_id * 8)];
    camera_boundary_qs_ = rom[kEntrancescrolledge + 2 + (entrance_id * 8)];
    camera_boundary_fs_ = rom[kEntrancescrolledge + 3 + (entrance_id * 8)];
    camera_boundary_qw_ = rom[kEntrancescrolledge + 4 + (entrance_id * 8)];
    camera_boundary_fw_ = rom[kEntrancescrolledge + 5 + (entrance_id * 8)];
    camera_boundary_qe_ = rom[kEntrancescrolledge + 6 + (entrance_id * 8)];
    camera_boundary_fe_ = rom[kEntrancescrolledge + 7 + (entrance_id * 8)];

    if (is_spawn_point) {
      room_ = static_cast<short>(
          (rom[kStartingEntranceroom + (entrance_id * 2) + 1] << 8) +
          rom[kStartingEntranceroom + (entrance_id * 2)]);

      y_position_ = static_cast<ushort>(
          (rom[kStartingEntranceyposition + (entrance_id * 2) + 1] << 8) +
          rom[kStartingEntranceyposition + (entrance_id * 2)]);

      x_position_ = static_cast<ushort>(
          (rom[kStartingEntrancexposition + (entrance_id * 2) + 1] << 8) +
          rom[kStartingEntrancexposition + (entrance_id * 2)]);

      camera_x_ = static_cast<ushort>(
          (rom[kStartingEntrancexscroll + (entrance_id * 2) + 1] << 8) +
          rom[kStartingEntrancexscroll + (entrance_id * 2)]);

      camera_y_ = static_cast<ushort>(
          (rom[kStartingEntranceyscroll + (entrance_id * 2) + 1] << 8) +
          rom[kStartingEntranceyscroll + (entrance_id * 2)]);

      camera_trigger_y_ = static_cast<ushort>(
          (rom[kStartingEntrancecameraytrigger + (entrance_id * 2) + 1] << 8) +
          rom[kStartingEntrancecameraytrigger + (entrance_id * 2)]);

      camera_trigger_x_ = static_cast<ushort>(
          (rom[kStartingEntrancecameraxtrigger + (entrance_id * 2) + 1] << 8) +
          rom[kStartingEntrancecameraxtrigger + (entrance_id * 2)]);

      blockset_ = rom[kStartingEntranceblockset + entrance_id];
      music_ = rom[kStartingEntrancemusic + entrance_id];
      dungeon_id_ = rom[kStartingEntrancedungeon + entrance_id];
      floor_ = rom[kStartingEntrancefloor + entrance_id];
      door_ = rom[kStartingEntrancedoor + entrance_id];

      ladder_bg_ = rom[kStartingEntranceladderbg + entrance_id];
      scrolling_ = rom[kStartingEntrancescrolling + entrance_id];
      scroll_quadrant_ = rom[kStartingEntrancescrollquadrant + entrance_id];

      exit_ = static_cast<short>(
          ((rom[kStartingEntranceexit + (entrance_id * 2) + 1] & 0x01) << 8) +
          rom[kStartingEntranceexit + (entrance_id * 2)]);

      camera_boundary_qn_ =
          rom[kStartingEntrancescrolledge + 0 + (entrance_id * 8)];
      camera_boundary_fn_ =
          rom[kStartingEntrancescrolledge + 1 + (entrance_id * 8)];
      camera_boundary_qs_ =
          rom[kStartingEntrancescrolledge + 2 + (entrance_id * 8)];
      camera_boundary_fs_ =
          rom[kStartingEntrancescrolledge + 3 + (entrance_id * 8)];
      camera_boundary_qw_ =
          rom[kStartingEntrancescrolledge + 4 + (entrance_id * 8)];
      camera_boundary_fw_ =
          rom[kStartingEntrancescrolledge + 5 + (entrance_id * 8)];
      camera_boundary_qe_ =
          rom[kStartingEntrancescrolledge + 6 + (entrance_id * 8)];
      camera_boundary_fe_ =
          rom[kStartingEntrancescrolledge + 7 + (entrance_id * 8)];
    }
  }

  absl::Status Save(Rom& rom, int entrance_id, bool is_spawn_point = false) {
    if (!is_spawn_point) {
      RETURN_IF_ERROR(
          rom.WriteShort(kEntranceyposition + (entrance_id * 2), y_position_));
      RETURN_IF_ERROR(
          rom.WriteShort(kEntrancexposition + (entrance_id * 2), x_position_));
      RETURN_IF_ERROR(
          rom.WriteShort(kEntranceyscroll + (entrance_id * 2), camera_y_));
      RETURN_IF_ERROR(
          rom.WriteShort(kEntrancexscroll + (entrance_id * 2), camera_x_));
      RETURN_IF_ERROR(rom.WriteShort(
          kEntrancecameraxtrigger + (entrance_id * 2), camera_trigger_x_));
      RETURN_IF_ERROR(rom.WriteShort(
          kEntrancecameraytrigger + (entrance_id * 2), camera_trigger_y_));
      RETURN_IF_ERROR(rom.WriteShort(kEntranceexit + (entrance_id * 2), exit_));
      RETURN_IF_ERROR(rom.Write(kEntranceblockset + entrance_id,
                                (uint8_t)(blockset_ & 0xFF)));
      RETURN_IF_ERROR(
          rom.Write(kEntrancemusic + entrance_id, (uint8_t)(music_ & 0xFF)));
      RETURN_IF_ERROR(rom.Write(kEntrancedungeon + entrance_id,
                                (uint8_t)(dungeon_id_ & 0xFF)));
      RETURN_IF_ERROR(
          rom.Write(kEntrancedoor + entrance_id, (uint8_t)(door_ & 0xFF)));
      RETURN_IF_ERROR(
          rom.Write(kEntrancefloor + entrance_id, (uint8_t)(floor_ & 0xFF)));
      RETURN_IF_ERROR(rom.Write(kEntranceladderbg + entrance_id,
                                (uint8_t)(ladder_bg_ & 0xFF)));
      RETURN_IF_ERROR(rom.Write(kEntrancescrolling + entrance_id,
                                (uint8_t)(scrolling_ & 0xFF)));
      RETURN_IF_ERROR(rom.Write(kEntrancescrollquadrant + entrance_id,
                                (uint8_t)(scroll_quadrant_ & 0xFF)));
      RETURN_IF_ERROR(rom.Write(kEntrancescrolledge + 0 + (entrance_id * 8),
                                camera_boundary_qn_));
      RETURN_IF_ERROR(rom.Write(kEntrancescrolledge + 1 + (entrance_id * 8),
                                camera_boundary_fn_));
      RETURN_IF_ERROR(rom.Write(kEntrancescrolledge + 2 + (entrance_id * 8),
                                camera_boundary_qs_));
      RETURN_IF_ERROR(rom.Write(kEntrancescrolledge + 3 + (entrance_id * 8),
                                camera_boundary_fs_));
      RETURN_IF_ERROR(rom.Write(kEntrancescrolledge + 4 + (entrance_id * 8),
                                camera_boundary_qw_));
      RETURN_IF_ERROR(rom.Write(kEntrancescrolledge + 5 + (entrance_id * 8),
                                camera_boundary_fw_));
      RETURN_IF_ERROR(rom.Write(kEntrancescrolledge + 6 + (entrance_id * 8),
                                camera_boundary_qe_));
      RETURN_IF_ERROR(rom.Write(kEntrancescrolledge + 7 + (entrance_id * 8),
                                camera_boundary_fe_));
    } else {
      RETURN_IF_ERROR(
          rom.WriteShort(kStartingEntranceroom + (entrance_id * 2), room_));
      RETURN_IF_ERROR(rom.WriteShort(
          kStartingEntranceyposition + (entrance_id * 2), y_position_));
      RETURN_IF_ERROR(rom.WriteShort(
          kStartingEntrancexposition + (entrance_id * 2), x_position_));
      RETURN_IF_ERROR(rom.WriteShort(
          kStartingEntranceyscroll + (entrance_id * 2), camera_y_));
      RETURN_IF_ERROR(rom.WriteShort(
          kStartingEntrancexscroll + (entrance_id * 2), camera_x_));
      RETURN_IF_ERROR(
          rom.WriteShort(kStartingEntrancecameraxtrigger + (entrance_id * 2),
                         camera_trigger_x_));
      RETURN_IF_ERROR(
          rom.WriteShort(kStartingEntrancecameraytrigger + (entrance_id * 2),
                         camera_trigger_y_));
      RETURN_IF_ERROR(
          rom.WriteShort(kStartingEntranceexit + (entrance_id * 2), exit_));
      RETURN_IF_ERROR(rom.Write(kStartingEntranceblockset + entrance_id,
                                (uint8_t)(blockset_ & 0xFF)));
      RETURN_IF_ERROR(rom.Write(kStartingEntrancemusic + entrance_id,
                                (uint8_t)(music_ & 0xFF)));
      RETURN_IF_ERROR(rom.Write(kStartingEntrancedungeon + entrance_id,
                                (uint8_t)(dungeon_id_ & 0xFF)));
      RETURN_IF_ERROR(rom.Write(kStartingEntrancedoor + entrance_id,
                                (uint8_t)(door_ & 0xFF)));
      RETURN_IF_ERROR(rom.Write(kStartingEntrancefloor + entrance_id,
                                (uint8_t)(floor_ & 0xFF)));
      RETURN_IF_ERROR(rom.Write(kStartingEntranceladderbg + entrance_id,
                                (uint8_t)(ladder_bg_ & 0xFF)));
      RETURN_IF_ERROR(rom.Write(kStartingEntrancescrolling + entrance_id,
                                (uint8_t)(scrolling_ & 0xFF)));
      RETURN_IF_ERROR(rom.Write(kStartingEntrancescrollquadrant + entrance_id,
                                (uint8_t)(scroll_quadrant_ & 0xFF)));
      RETURN_IF_ERROR(
          rom.Write(kStartingEntrancescrolledge + 0 + (entrance_id * 8),
                    camera_boundary_qn_));
      RETURN_IF_ERROR(
          rom.Write(kStartingEntrancescrolledge + 1 + (entrance_id * 8),
                    camera_boundary_fn_));
      RETURN_IF_ERROR(
          rom.Write(kStartingEntrancescrolledge + 2 + (entrance_id * 8),
                    camera_boundary_qs_));
      RETURN_IF_ERROR(
          rom.Write(kStartingEntrancescrolledge + 3 + (entrance_id * 8),
                    camera_boundary_fs_));
      RETURN_IF_ERROR(
          rom.Write(kStartingEntrancescrolledge + 4 + (entrance_id * 8),
                    camera_boundary_qw_));
      RETURN_IF_ERROR(
          rom.Write(kStartingEntrancescrolledge + 5 + (entrance_id * 8),
                    camera_boundary_fw_));
      RETURN_IF_ERROR(
          rom.Write(kStartingEntrancescrolledge + 6 + (entrance_id * 8),
                    camera_boundary_qe_));
      RETURN_IF_ERROR(
          rom.Write(kStartingEntrancescrolledge + 7 + (entrance_id * 8),
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
}  // namespace app
}  // namespace yaze

#endif  // YAZE_APP_ZELDA3_DUNGEON_ROOM_ENTRANCE_H