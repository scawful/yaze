#ifndef YAZE_APP_ZELDA3_DUNGEON_ROOM_H
#define YAZE_APP_ZELDA3_DUNGEON_ROOM_H

#include <cstdint>
#include <vector>

#include "app/core/constants.h"
#include "app/gfx/bitmap.h"
#include "app/gfx/snes_palette.h"
#include "app/gfx/snes_tile.h"
#include "app/gui/canvas.h"
#include "app/rom.h"
#include "app/zelda3/dungeon/room_names.h"
#include "app/zelda3/dungeon/room_object.h"
#include "app/zelda3/sprite/sprite.h"

namespace yaze {
namespace app {
namespace zelda3 {
namespace dungeon {

// room_object_layout_pointer   0x882D
// room_object_pointer          0x874C
// 0x882D -> readlong() -> 2FEF04 (04EF2F -> toPC->026F2F) ->

// 47EF04 ; layout00 ptr
// AFEF04 ; layout01 ptr
// F0EF04 ; layout02 ptr
// 4CF004 ; layout03 ptr
// A8F004 ; layout04 ptr
// ECF004 ; layout05 ptr
// 48F104 ; layout06 ptr
// A4F104 ; layout07 ptr
// also they are not exactly the same as rooms
// the object array is terminated by a 0xFFFF there's no layers
// in normal room when you encounter a 0xFFFF it goes to the next layer

constexpr int room_object_layout_pointer = 0x882D;
constexpr int room_object_pointer = 0x874C;  // Long pointer

constexpr int dungeons_main_bg_palette_pointers = 0xDEC4B;  // JP Same
constexpr int dungeons_palettes = 0xDD734;
constexpr int room_items_pointers = 0xDB69;     // JP 0xDB67
constexpr int rooms_sprite_pointer = 0x4C298;   // JP Same //2byte bank 09D62E
constexpr int kRoomHeaderPointer = 0xB5DD;      // LONG
constexpr int kRoomHeaderPointerBank = 0xB5E7;  // JP Same
constexpr int gfx_groups_pointer = 0x6237;
constexpr int chests_length_pointer = 0xEBF6;
constexpr int chests_data_pointer1 = 0xEBFB;

constexpr int messages_id_dungeon = 0x3F61D;

constexpr int blocks_length = 0x8896;  // Word value
constexpr int blocks_pointer1 = 0x15AFA;
constexpr int blocks_pointer2 = 0x15B01;
constexpr int blocks_pointer3 = 0x15B08;
constexpr int blocks_pointer4 = 0x15B0F;
constexpr int torch_data = 0x2736A;  // JP 0x2704A
constexpr int torches_length_pointer = 0x88C1;
constexpr int sprite_blockset_pointer = 0x5B57;

constexpr int sprites_data = 0x4D8B0;
constexpr int sprites_data_empty_room = 0x4D8AE;
constexpr int sprites_end_data = 0x4EC9E;
constexpr int pit_pointer = 0x394AB;
constexpr int pit_count = 0x394A6;
constexpr int doorPointers = 0xF83C0;

// doors
constexpr int door_gfx_up = 0x4D9E;
constexpr int door_gfx_down = 0x4E06;
constexpr int door_gfx_cavexit_down = 0x4E06;
constexpr int door_gfx_left = 0x4E66;
constexpr int door_gfx_right = 0x4EC6;
constexpr int door_pos_up = 0x197E;
constexpr int door_pos_down = 0x1996;
constexpr int door_pos_left = 0x19AE;
constexpr int door_pos_right = 0x19C6;

constexpr int dungeon_spr_ptrs = 0x090000;

constexpr int NumberOfRooms = 296;

constexpr ushort stairsObjects[] = {0x139, 0x138, 0x13B, 0x12E, 0x12D};

class DungeonDestination {
 public:
  DungeonDestination() = default;
  ~DungeonDestination() = default;
  DungeonDestination(uint8_t i) : Index(i) {}

  uint8_t Index;
  uint8_t Target = 0;
  uint8_t TargetLayer = 0;
};

struct object_door {
  object_door() = default;
  object_door(short id, uint8_t x, uint8_t y, uint8_t size, uint8_t layer)
      : id_(id), x_(x), y_(y), size_(size), layer_(layer) {}

  short id_;
  uint8_t x_;
  uint8_t y_;
  uint8_t size_;
  uint8_t type_;
  uint8_t layer_;
};

struct ChestData {
  ChestData() = default;
  ChestData(uchar i, bool s) : id_(i), size_(s){};

  uchar id_;
  bool size_;
};

struct StaircaseRooms {};

class Room : public SharedRom {
 public:
  Room() = default;
  Room(int room_id) : room_id_(room_id) {}
  ~Room() = default;
  void LoadHeader();
  void LoadRoomFromROM();

  void LoadRoomGraphics(uchar entrance_blockset = 0xFF);
  void CopyRoomGraphicsToBuffer();
  void LoadAnimatedGraphics();

  void LoadObjects();
  void LoadSprites();
  void LoadChests();

  auto blocks() const { return blocks_; }
  auto& mutable_blocks() { return blocks_; }
  auto layer1() const { return background_bmps_[0]; }
  auto layer2() const { return background_bmps_[1]; }
  auto layer3() const { return background_bmps_[2]; }
  auto room_size() const { return room_size_; }
  auto room_size_ptr() const { return room_size_pointer_; }
  auto set_room_size(uint64_t size) { room_size_ = size; }

  RoomObject AddObject(short oid, uint8_t x, uint8_t y, uint8_t size,
                       uint8_t layer) {
    return RoomObject(oid, x, y, size, layer);
  }

  uint8_t blockset = 0;
  uint8_t spriteset = 0;
  uint8_t palette = 0;
  uint8_t layout = 0;
  uint8_t holewarp = 0;
  uint8_t floor1 = 0;
  uint8_t floor2 = 0;

  uint16_t message_id_ = 0;

  gfx::Bitmap current_graphics_;
  std::vector<uint8_t> bg1_buffer_;
  std::vector<uint8_t> bg2_buffer_;
  std::vector<uint8_t> current_gfx16_;

 private:
  bool is_light_;
  bool is_loaded_;
  bool is_dark_;
  bool is_floor_;

  int room_id_;
  int animated_frame_;

  uchar tag1_;
  uchar tag2_;

  uint8_t staircase_plane_[4];
  uint8_t staircase_rooms_[4];

  uint8_t background_tileset_;
  uint8_t sprite_tileset_;
  uint8_t layer2_behavior_;
  uint8_t palette_;
  uint8_t floor1_graphics_;
  uint8_t floor2_graphics_;
  uint8_t layer2_mode_;

  uint64_t room_size_;
  int64_t room_size_pointer_;

  std::array<uint8_t, 16> blocks_;
  std::array<uchar, 16> chest_list_;

  std::array<gfx::Bitmap, 3> background_bmps_;
  std::vector<zelda3::Sprite> sprites_;
  std::vector<StaircaseRooms> staircase_rooms_vec_;

  Background2 bg2_;
  DungeonDestination pits_;
  DungeonDestination stair1_;
  DungeonDestination stair2_;
  DungeonDestination stair3_;
  DungeonDestination stair4_;

  std::vector<ChestData> chests_in_room_;
  std::vector<RoomObject> tile_objects_;

  std::vector<int> room_addresses_;
};

}  // namespace dungeon
}  // namespace zelda3
}  // namespace app
}  // namespace yaze

#endif