#ifndef YAZE_APP_ZELDA3_DUNGEON_ROOM_H
#define YAZE_APP_ZELDA3_DUNGEON_ROOM_H

#include <cstdint>
#include <vector>

#include "app/core/common.h"
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

constexpr int entrance_gfx_group = 0x5D97;
constexpr int dungeons_main_bg_palette_pointers = 0xDEC4B;  // JP Same
constexpr int dungeons_palettes = 0xDD734;
constexpr int room_items_pointers = 0xDB69;     // JP 0xDB67
constexpr int rooms_sprite_pointer = 0x4C298;   // JP Same //2byte bank 09D62E
constexpr int kRoomHeaderPointer = 0xB5DD;      // LONG
constexpr int kRoomHeaderPointerBank = 0xB5E7;  // JP Same
constexpr int gfx_groups_pointer = 0x6237;
constexpr int room_object_layout_pointer = 0x882D;
constexpr int room_object_pointer = 0x874C;  // Long pointer
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

constexpr ushort stairsObjects[] = {0x139, 0x138, 0x13B, 0x12E, 0x12D};

void DrawDungeonRoomBG1(std::vector<uint8_t>& tiles_bg1_buffer,
                        std::vector<uint8_t>& current_gfx16,
                        std::vector<uint8_t>& room_bg1_ptr);

void DrawDungeonRoomBG2(std::vector<uint8_t>& tiles_bg2_buffer,
                        std::vector<uint8_t>& current_gfx16,
                        std::vector<uint8_t>& room_bg2_ptr);

class DungeonDestination {
 public:
  DungeonDestination() = default;
  ~DungeonDestination() = default;
  DungeonDestination(uint8_t i) : Index(i) {}

  uint8_t Index;
  uint8_t Target = 0;
  uint8_t TargetLayer = 0;
  // RoomObject* AssociatedObject = nullptr;
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

class Room : public SharedROM {
 public:
  Room() = default;
  Room(int room_id) : room_id_(room_id) {}
  ~Room() = default;
  void LoadHeader();
  void LoadRoomGraphics(uchar entrance_blockset = 0xFF);
  void LoadAnimatedGraphics();

  void LoadSprites();
  void LoadChests();
  void LoadObjects();

  void LoadRoomFromROM();

  RoomObject AddObject(short oid, uint8_t x, uint8_t y, uint8_t size,
                       uint8_t layer) {
    return RoomObject(oid, x, y, size, layer);
  }

  uint8_t floor1 = 0;
  uint8_t floor2 = 0;
  uint8_t blockset = 0;
  uint8_t spriteset = 0;
  uint8_t palette = 0;
  uint8_t layout = 0;

  uint16_t message_id_ = 0;

  gfx::Bitmap current_graphics_;
  std::vector<uint8_t> bg1_buffer_;
  std::vector<uint8_t> bg2_buffer_;
  std::vector<uint8_t> current_gfx16_;

 private:
  bool is_loaded_ = false;

  int animated_frame = 0;

  int room_id_ = 0;

  bool light;
  Background2 bg2;

  uint8_t staircase_plane[4];
  uint8_t staircase_rooms[4];

  uchar BackgroundTileset;
  uchar SpriteTileset;
  uchar Layer2Behavior;
  uchar Palette;
  uchar Floor1Graphics;
  uchar Floor2Graphics;
  uchar Layer2Mode;
  std::array<uchar, 16> blocks;
  std::array<uchar, 16> ChestList;

  std::vector<zelda3::Sprite> sprites_;
  std::vector<StaircaseRooms> staircaseRooms;

  DungeonDestination Pits;
  DungeonDestination Stair1;
  DungeonDestination Stair2;
  DungeonDestination Stair3;
  DungeonDestination Stair4;

  uchar Tag1;
  uchar Tag2;
  bool IsDark;

  bool floor;

  // std::vector<Chest> chest_list;
  std::vector<ChestData> chests_in_room;
  std::vector<RoomObject> tilesObjects;
};

}  // namespace dungeon
}  // namespace zelda3
}  // namespace app
}  // namespace yaze

#endif