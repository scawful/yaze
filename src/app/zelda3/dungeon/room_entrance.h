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
constexpr int entrance_room = 0x14813;

// 8 bytes per room, HU, FU, HD, FD, HL, FL, HR, FR
constexpr int entrance_scrolledge = 0x1491D;      // 0x14681
constexpr int entrance_yscroll = 0x14D45;         // 0x14AA9 2 bytes each room
constexpr int entrance_xscroll = 0x14E4F;         // 0x14BB3 2 bytes
constexpr int entrance_yposition = 0x14F59;       // 0x14CBD 2bytes
constexpr int entrance_xposition = 0x15063;       // 0x14DC7 2bytes
constexpr int entrance_cameraytrigger = 0x1516D;  // 0x14ED1 2bytes
constexpr int entrance_cameraxtrigger = 0x15277;  // 0x14FDB 2bytes

constexpr int entrance_blockset = 0x15381;  // 0x150E5 1byte
constexpr int entrance_floor = 0x15406;     // 0x1516A 1byte
constexpr int entrance_dungeon = 0x1548B;   // 0x151EF 1byte (dungeon id)
constexpr int entrance_door = 0x15510;      // 0x15274 1byte

// 1 byte, ---b ---a b = bg2, a = need to check
constexpr int entrance_ladderbg = 0x15595;        // 0x152F9
constexpr int entrance_scrolling = 0x1561A;       // 0x1537E 1byte --h- --v-
constexpr int entrance_scrollquadrant = 0x1569F;  // 0x15403 1byte
constexpr int entrance_exit = 0x15724;            // 0x15488 2byte word
constexpr int entrance_music = 0x1582E;           // 0x15592

// word value for each room
constexpr int startingentrance_room = 0x15B6E;  // 0x158D2

// 8 bytes per room, HU, FU, HD, FD, HL, FL, HR, FR
constexpr int startingentrance_scrolledge = 0x15B7C;  // 0x158E0
constexpr int startingentrance_yscroll = 0x15BB4;  // 0x14AA9 //2bytes each room
constexpr int startingentrance_xscroll = 0x15BC2;  // 0x14BB3 //2bytes
constexpr int startingentrance_yposition = 0x15BD0;       // 0x14CBD 2bytes
constexpr int startingentrance_xposition = 0x15BDE;       // 0x14DC7 2bytes
constexpr int startingentrance_cameraytrigger = 0x15BEC;  // 0x14ED1 2bytes
constexpr int startingentrance_cameraxtrigger = 0x15BFA;  // 0x14FDB 2bytes

constexpr int startingentrance_blockset = 0x15C08;  // 0x150E5 1byte
constexpr int startingentrance_floor = 0x15C0F;     // 0x1516A 1byte
constexpr int startingentrance_dungeon = 0x15C16;  // 0x151EF 1byte (dungeon id)

constexpr int startingentrance_door = 0x15C2B;  // 0x15274 1byte

// 1 byte, ---b ---a b = bg2, a = need to check
constexpr int startingentrance_ladderbg = 0x15C1D;  // 0x152F9
// 1byte --h- --v-
constexpr int startingentrance_scrolling = 0x15C24;       // 0x1537E
constexpr int startingentrance_scrollquadrant = 0x15C2B;  // 0x15403 1byte
constexpr int startingentrance_exit = 0x15C32;   // 0x15488 //2byte word
constexpr int startingentrance_music = 0x15C4E;  // 0x15592
constexpr int startingentrance_entrance = 0x15C40;

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
        static_cast<short>((rom[entrance_room + (entrance_id * 2) + 1] << 8) +
                           rom[entrance_room + (entrance_id * 2)]);
    y_position_ = static_cast<ushort>(
        (rom[entrance_yposition + (entrance_id * 2) + 1] << 8) +
        rom[entrance_yposition + (entrance_id * 2)]);
    x_position_ = static_cast<ushort>(
        (rom[entrance_xposition + (entrance_id * 2) + 1] << 8) +
        rom[entrance_xposition + (entrance_id * 2)]);
    camera_x_ = static_cast<ushort>(
        (rom[entrance_xscroll + (entrance_id * 2) + 1] << 8) +
        rom[entrance_xscroll + (entrance_id * 2)]);
    camera_y_ = static_cast<ushort>(
        (rom[entrance_yscroll + (entrance_id * 2) + 1] << 8) +
        rom[entrance_yscroll + (entrance_id * 2)]);
    camera_trigger_y_ = static_cast<ushort>(
        (rom[(entrance_cameraytrigger + (entrance_id * 2)) + 1] << 8) +
        rom[entrance_cameraytrigger + (entrance_id * 2)]);
    camera_trigger_x_ = static_cast<ushort>(
        (rom[(entrance_cameraxtrigger + (entrance_id * 2)) + 1] << 8) +
        rom[entrance_cameraxtrigger + (entrance_id * 2)]);
    blockset_ = rom[entrance_blockset + entrance_id];
    music_ = rom[entrance_music + entrance_id];
    dungeon_id_ = rom[entrance_dungeon + entrance_id];
    floor_ = rom[entrance_floor + entrance_id];
    door_ = rom[entrance_door + entrance_id];
    ladder_bg_ = rom[entrance_ladderbg + entrance_id];
    scrolling_ = rom[entrance_scrolling + entrance_id];
    scroll_quadrant_ = rom[entrance_scrollquadrant + entrance_id];
    exit_ =
        static_cast<short>((rom[entrance_exit + (entrance_id * 2) + 1] << 8) +
                           rom[entrance_exit + (entrance_id * 2)]);

    camera_boundary_qn_ = rom[entrance_scrolledge + 0 + (entrance_id * 8)];
    camera_boundary_fn_ = rom[entrance_scrolledge + 1 + (entrance_id * 8)];
    camera_boundary_qs_ = rom[entrance_scrolledge + 2 + (entrance_id * 8)];
    camera_boundary_fs_ = rom[entrance_scrolledge + 3 + (entrance_id * 8)];
    camera_boundary_qw_ = rom[entrance_scrolledge + 4 + (entrance_id * 8)];
    camera_boundary_fw_ = rom[entrance_scrolledge + 5 + (entrance_id * 8)];
    camera_boundary_qe_ = rom[entrance_scrolledge + 6 + (entrance_id * 8)];
    camera_boundary_fe_ = rom[entrance_scrolledge + 7 + (entrance_id * 8)];

    if (is_spawn_point) {
      room_ = static_cast<short>(
          (rom[startingentrance_room + (entrance_id * 2) + 1] << 8) +
          rom[startingentrance_room + (entrance_id * 2)]);

      y_position_ = static_cast<ushort>(
          (rom[startingentrance_yposition + (entrance_id * 2) + 1] << 8) +
          rom[startingentrance_yposition + (entrance_id * 2)]);

      x_position_ = static_cast<ushort>(
          (rom[startingentrance_xposition + (entrance_id * 2) + 1] << 8) +
          rom[startingentrance_xposition + (entrance_id * 2)]);

      camera_x_ = static_cast<ushort>(
          (rom[startingentrance_xscroll + (entrance_id * 2) + 1] << 8) +
          rom[startingentrance_xscroll + (entrance_id * 2)]);

      camera_y_ = static_cast<ushort>(
          (rom[startingentrance_yscroll + (entrance_id * 2) + 1] << 8) +
          rom[startingentrance_yscroll + (entrance_id * 2)]);

      camera_trigger_y_ = static_cast<ushort>(
          (rom[startingentrance_cameraytrigger + (entrance_id * 2) + 1] << 8) +
          rom[startingentrance_cameraytrigger + (entrance_id * 2)]);

      camera_trigger_x_ = static_cast<ushort>(
          (rom[startingentrance_cameraxtrigger + (entrance_id * 2) + 1] << 8) +
          rom[startingentrance_cameraxtrigger + (entrance_id * 2)]);

      blockset_ = rom[startingentrance_blockset + entrance_id];
      music_ = rom[startingentrance_music + entrance_id];
      dungeon_id_ = rom[startingentrance_dungeon + entrance_id];
      floor_ = rom[startingentrance_floor + entrance_id];
      door_ = rom[startingentrance_door + entrance_id];

      ladder_bg_ = rom[startingentrance_ladderbg + entrance_id];
      scrolling_ = rom[startingentrance_scrolling + entrance_id];
      scroll_quadrant_ = rom[startingentrance_scrollquadrant + entrance_id];

      exit_ = static_cast<short>(
          ((rom[startingentrance_exit + (entrance_id * 2) + 1] & 0x01) << 8) +
          rom[startingentrance_exit + (entrance_id * 2)]);

      camera_boundary_qn_ =
          rom[startingentrance_scrolledge + 0 + (entrance_id * 8)];
      camera_boundary_fn_ =
          rom[startingentrance_scrolledge + 1 + (entrance_id * 8)];
      camera_boundary_qs_ =
          rom[startingentrance_scrolledge + 2 + (entrance_id * 8)];
      camera_boundary_fs_ =
          rom[startingentrance_scrolledge + 3 + (entrance_id * 8)];
      camera_boundary_qw_ =
          rom[startingentrance_scrolledge + 4 + (entrance_id * 8)];
      camera_boundary_fw_ =
          rom[startingentrance_scrolledge + 5 + (entrance_id * 8)];
      camera_boundary_qe_ =
          rom[startingentrance_scrolledge + 6 + (entrance_id * 8)];
      camera_boundary_fe_ =
          rom[startingentrance_scrolledge + 7 + (entrance_id * 8)];
    }
  }

  absl::Status Save(Rom& rom, int entrance_id, bool is_spawn_point = false) {
    if (!is_spawn_point) {
      RETURN_IF_ERROR(
          rom.WriteShort(entrance_yposition + (entrance_id * 2), y_position_));
      RETURN_IF_ERROR(
          rom.WriteShort(entrance_xposition + (entrance_id * 2), x_position_));
      RETURN_IF_ERROR(
          rom.WriteShort(entrance_yscroll + (entrance_id * 2), camera_y_));
      RETURN_IF_ERROR(
          rom.WriteShort(entrance_xscroll + (entrance_id * 2), camera_x_));
      RETURN_IF_ERROR(rom.WriteShort(
          entrance_cameraxtrigger + (entrance_id * 2), camera_trigger_x_));
      RETURN_IF_ERROR(rom.WriteShort(
          entrance_cameraytrigger + (entrance_id * 2), camera_trigger_y_));
      RETURN_IF_ERROR(rom.WriteShort(entrance_exit + (entrance_id * 2), exit_));
      RETURN_IF_ERROR(rom.Write(entrance_blockset + entrance_id,
                                (uint8_t)(blockset_ & 0xFF)));
      RETURN_IF_ERROR(
          rom.Write(entrance_music + entrance_id, (uint8_t)(music_ & 0xFF)));
      RETURN_IF_ERROR(rom.Write(entrance_dungeon + entrance_id,
                                (uint8_t)(dungeon_id_ & 0xFF)));
      RETURN_IF_ERROR(
          rom.Write(entrance_door + entrance_id, (uint8_t)(door_ & 0xFF)));
      RETURN_IF_ERROR(
          rom.Write(entrance_floor + entrance_id, (uint8_t)(floor_ & 0xFF)));
      RETURN_IF_ERROR(rom.Write(entrance_ladderbg + entrance_id,
                                (uint8_t)(ladder_bg_ & 0xFF)));
      RETURN_IF_ERROR(rom.Write(entrance_scrolling + entrance_id,
                                (uint8_t)(scrolling_ & 0xFF)));
      RETURN_IF_ERROR(rom.Write(entrance_scrollquadrant + entrance_id,
                                (uint8_t)(scroll_quadrant_ & 0xFF)));
      RETURN_IF_ERROR(rom.Write(entrance_scrolledge + 0 + (entrance_id * 8),
                                camera_boundary_qn_));
      RETURN_IF_ERROR(rom.Write(entrance_scrolledge + 1 + (entrance_id * 8),
                                camera_boundary_fn_));
      RETURN_IF_ERROR(rom.Write(entrance_scrolledge + 2 + (entrance_id * 8),
                                camera_boundary_qs_));
      RETURN_IF_ERROR(rom.Write(entrance_scrolledge + 3 + (entrance_id * 8),
                                camera_boundary_fs_));
      RETURN_IF_ERROR(rom.Write(entrance_scrolledge + 4 + (entrance_id * 8),
                                camera_boundary_qw_));
      RETURN_IF_ERROR(rom.Write(entrance_scrolledge + 5 + (entrance_id * 8),
                                camera_boundary_fw_));
      RETURN_IF_ERROR(rom.Write(entrance_scrolledge + 6 + (entrance_id * 8),
                                camera_boundary_qe_));
      RETURN_IF_ERROR(rom.Write(entrance_scrolledge + 7 + (entrance_id * 8),
                                camera_boundary_fe_));
    } else {
      RETURN_IF_ERROR(
          rom.WriteShort(startingentrance_room + (entrance_id * 2), room_));
      RETURN_IF_ERROR(rom.WriteShort(
          startingentrance_yposition + (entrance_id * 2), y_position_));
      RETURN_IF_ERROR(rom.WriteShort(
          startingentrance_xposition + (entrance_id * 2), x_position_));
      RETURN_IF_ERROR(rom.WriteShort(
          startingentrance_yscroll + (entrance_id * 2), camera_y_));
      RETURN_IF_ERROR(rom.WriteShort(
          startingentrance_xscroll + (entrance_id * 2), camera_x_));
      RETURN_IF_ERROR(
          rom.WriteShort(startingentrance_cameraxtrigger + (entrance_id * 2),
                         camera_trigger_x_));
      RETURN_IF_ERROR(
          rom.WriteShort(startingentrance_cameraytrigger + (entrance_id * 2),
                         camera_trigger_y_));
      RETURN_IF_ERROR(
          rom.WriteShort(startingentrance_exit + (entrance_id * 2), exit_));
      RETURN_IF_ERROR(rom.Write(startingentrance_blockset + entrance_id,
                                (uint8_t)(blockset_ & 0xFF)));
      RETURN_IF_ERROR(rom.Write(startingentrance_music + entrance_id,
                                (uint8_t)(music_ & 0xFF)));
      RETURN_IF_ERROR(rom.Write(startingentrance_dungeon + entrance_id,
                                (uint8_t)(dungeon_id_ & 0xFF)));
      RETURN_IF_ERROR(rom.Write(startingentrance_door + entrance_id,
                                (uint8_t)(door_ & 0xFF)));
      RETURN_IF_ERROR(rom.Write(startingentrance_floor + entrance_id,
                                (uint8_t)(floor_ & 0xFF)));
      RETURN_IF_ERROR(rom.Write(startingentrance_ladderbg + entrance_id,
                                (uint8_t)(ladder_bg_ & 0xFF)));
      RETURN_IF_ERROR(rom.Write(startingentrance_scrolling + entrance_id,
                                (uint8_t)(scrolling_ & 0xFF)));
      RETURN_IF_ERROR(rom.Write(startingentrance_scrollquadrant + entrance_id,
                                (uint8_t)(scroll_quadrant_ & 0xFF)));
      RETURN_IF_ERROR(
          rom.Write(startingentrance_scrolledge + 0 + (entrance_id * 8),
                    camera_boundary_qn_));
      RETURN_IF_ERROR(
          rom.Write(startingentrance_scrolledge + 1 + (entrance_id * 8),
                    camera_boundary_fn_));
      RETURN_IF_ERROR(
          rom.Write(startingentrance_scrolledge + 2 + (entrance_id * 8),
                    camera_boundary_qs_));
      RETURN_IF_ERROR(
          rom.Write(startingentrance_scrolledge + 3 + (entrance_id * 8),
                    camera_boundary_fs_));
      RETURN_IF_ERROR(
          rom.Write(startingentrance_scrolledge + 4 + (entrance_id * 8),
                    camera_boundary_qw_));
      RETURN_IF_ERROR(
          rom.Write(startingentrance_scrolledge + 5 + (entrance_id * 8),
                    camera_boundary_fw_));
      RETURN_IF_ERROR(
          rom.Write(startingentrance_scrolledge + 6 + (entrance_id * 8),
                    camera_boundary_qe_));
      RETURN_IF_ERROR(
          rom.Write(startingentrance_scrolledge + 7 + (entrance_id * 8),
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