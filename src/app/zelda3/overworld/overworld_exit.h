#ifndef YAZE_APP_ZELDA3_OVERWORLD_EXIT_H
#define YAZE_APP_ZELDA3_OVERWORLD_EXIT_H

#include <cstdint>
#include <iostream>

#include "app/core/constants.h"
#include "app/zelda3/common.h"

namespace yaze {
namespace app {
namespace zelda3 {
namespace overworld {

constexpr int OWExitRoomId = 0x15D8A;  // 0x15E07 Credits sequences
// 105C2 Ending maps
// 105E2 Sprite Group Table for Ending
constexpr int OWExitMapId = 0x15E28;
constexpr int OWExitVram = 0x15E77;
constexpr int OWExitYScroll = 0x15F15;
constexpr int OWExitXScroll = 0x15FB3;
constexpr int OWExitYPlayer = 0x16051;
constexpr int OWExitXPlayer = 0x160EF;
constexpr int OWExitYCamera = 0x1618D;
constexpr int OWExitXCamera = 0x1622B;
constexpr int OWExitDoorPosition = 0x15724;
constexpr int OWExitUnk1 = 0x162C9;
constexpr int OWExitUnk2 = 0x16318;
constexpr int OWExitDoorType1 = 0x16367;
constexpr int OWExitDoorType2 = 0x16405;

constexpr int OWExitMapIdWhirlpool = 0x16AE5;    //  JP = ;016849
constexpr int OWExitVramWhirlpool = 0x16B07;     //  JP = ;01686B
constexpr int OWExitYScrollWhirlpool = 0x16B29;  // JP = ;01688D
constexpr int OWExitXScrollWhirlpool = 0x16B4B;  // JP = ;016DE7
constexpr int OWExitYPlayerWhirlpool = 0x16B6D;  // JP = ;016E09
constexpr int OWExitXPlayerWhirlpool = 0x16B8F;  // JP = ;016E2B
constexpr int OWExitYCameraWhirlpool = 0x16BB1;  // JP = ;016E4D
constexpr int OWExitXCameraWhirlpool = 0x16BD3;  // JP = ;016E6F
constexpr int OWExitUnk1Whirlpool = 0x16BF5;     //    JP = ;016E91
constexpr int OWExitUnk2Whirlpool = 0x16C17;     //    JP = ;016EB3
constexpr int OWWhirlpoolPosition = 0x16CF8;     //    JP = ;016F94

class OverworldExit : public GameEntity {
 public:
  uint16_t y_scroll_;
  uint16_t x_scroll_;
  uchar y_player_;
  uchar x_player_;
  uchar y_camera_;
  uchar x_camera_;
  uchar scroll_mod_y_;
  uchar scroll_mod_x_;
  uint16_t door_type_1_;
  uint16_t door_type_2_;
  uint16_t room_id_;
  uint16_t map_pos_;  // Position in the vram
  uchar entrance_id_;
  uchar area_x_;
  uchar area_y_;
  bool is_hole_ = false;
  bool deleted_ = false;
  bool is_automatic_ = false;
  bool large_map_ = false;

  OverworldExit() = default;
  OverworldExit(uint16_t room_id, uchar map_id, uint16_t vram_location,
                uint16_t y_scroll, uint16_t x_scroll, uint16_t player_y,
                uint16_t player_x, uint16_t camera_y, uint16_t camera_x,
                uchar scroll_mod_y, uchar scroll_mod_x, uint16_t door_type_1,
                uint16_t door_type_2, bool deleted = false)
      : map_pos_(vram_location),
        entrance_id_(0),
        area_x_(0),
        area_y_(0),
        room_id_(room_id),
        y_scroll_(y_scroll),
        x_scroll_(x_scroll),
        y_player_(player_y),
        x_player_(player_x),
        y_camera_(camera_y),
        x_camera_(camera_x),
        scroll_mod_y_(scroll_mod_y),
        scroll_mod_x_(scroll_mod_x),
        door_type_1_(door_type_1),
        door_type_2_(door_type_2),
        is_hole_(false),
        deleted_(deleted) {
    // Initialize entity variables
    x_ = player_x;
    y_ = player_y;
    map_id_ = map_id;
    entity_type_ = kExit;

    int mapX = (map_id_ - ((map_id_ / 8) * 8));
    int mapY = (map_id_ / 8);

    area_x_ = (uchar)((std::abs(x_ - (mapX * 512)) / 16));
    area_y_ = (uchar)((std::abs(y_ - (mapY * 512)) / 16));

    if (door_type_1 != 0) {
      int p = (door_type_1 & 0x7FFF) >> 1;
      entrance_id_ = (uchar)(p % 64);
      area_y_ = (uchar)(p >> 6);
    }

    if (door_type_2 != 0) {
      int p = (door_type_2 & 0x7FFF) >> 1;
      entrance_id_ = (uchar)(p % 64);
      area_y_ = (uchar)(p >> 6);
    }

    if (map_id_ >= 64) {
      map_id_ -= 64;
    }

    mapX = (map_id_ - ((map_id_ / 8) * 8));
    mapY = (map_id_ / 8);

    area_x_ = (uchar)((std::abs(x_ - (mapX * 512)) / 16));
    area_y_ = (uchar)((std::abs(y_ - (mapY * 512)) / 16));

    map_pos_ = (uint16_t)((((area_y_) << 6) | (area_x_ & 0x3F)) << 1);
  }

  // Overworld overworld
  void UpdateMapProperties(uint16_t map_id) override {
    map_id_ = map_id;

    int large = 256;
    int mapid = map_id;

    if (map_id < 128) {
      large = large_map_ ? 768 : 256;
      // if (overworld.overworld_map(map_id)->Parent() != map_id) {
      //   mapid = overworld.overworld_map(map_id)->Parent();
      // }
    }

    int mapX = map_id - ((map_id / 8) * 8);
    int mapY = map_id / 8;

    area_x_ = (uchar)((std::abs(x_ - (mapX * 512)) / 16));
    area_y_ = (uchar)((std::abs(y_ - (mapY * 512)) / 16));

    if (map_id >= 64) {
      map_id -= 64;
    }

    int mapx = (map_id & 7) << 9;
    int mapy = (map_id & 56) << 6;

    if (is_automatic_) {
      x_ = x_ - 120;
      y_ = y_ - 80;

      if (x_ < mapx) {
        x_ = mapx;
      }

      if (y_ < mapy) {
        y_ = mapy;
      }

      if (x_ > mapx + large) {
        x_ = mapx + large;
      }

      if (y_ > mapy + large + 32) {
        y_ = mapy + large + 32;
      }

      x_camera_ = x_player_ + 0x07;
      y_camera_ = y_player_ + 0x1F;

      if (x_camera_ < mapx + 127) {
        x_camera_ = mapx + 127;
      }

      if (y_camera_ < mapy + 111) {
        y_camera_ = mapy + 111;
      }

      if (x_camera_ > mapx + 127 + large) {
        x_camera_ = mapx + 127 + large;
      }

      if (y_camera_ > mapy + 143 + large) {
        y_camera_ = mapy + 143 + large;
      }
    }

    short vram_x_scroll = (short)(x_ - mapx);
    short vram_y_scroll = (short)(y_ - mapy);

    map_pos_ = (uint16_t)(((vram_y_scroll & 0xFFF0) << 3) |
                          ((vram_x_scroll & 0xFFF0) >> 3));

    std::cout << "Exit:      " << room_id_ << " MapId: " << std::hex << mapid
              << " X: " << static_cast<int>(area_x_)
              << " Y: " << static_cast<int>(area_y_) << std::endl;
  }
};

}  // namespace overworld
}  // namespace zelda3
}  // namespace app
}  // namespace yaze

#endif  // YAZE_APP_ZELDA3_OVERWORLD_EXIT_H_
