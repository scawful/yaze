#ifndef YAZE_APP_ZELDA3_DUNGEON_ROOM_OBJECT_H
#define YAZE_APP_ZELDA3_DUNGEON_ROOM_OBJECT_H

#include <cstdint>
#include <string>
#include <vector>

#include "app/emu/cpu/cpu.h"
#include "app/emu/memory/memory.h"
#include "app/emu/video/ppu.h"
#include "app/gfx/snes_palette.h"
#include "app/gfx/snes_tile.h"
#include "app/rom.h"
#include "app/zelda3/dungeon/object_names.h"
#include "app/zelda3/dungeon/object_renderer.h"

namespace yaze {
namespace app {
namespace zelda3 {
namespace dungeon {

struct SubtypeInfo {
  uint32_t subtype_ptr;
  uint32_t routine_ptr;
};

SubtypeInfo FetchSubtypeInfo(uint16_t object_id);

enum class SpecialObjectType { Chest, BigChest, InterroomStairs };

enum Background2 {
  Off,
  Parallax,
  Dark,
  OnTop,
  Translucent,
  Addition,
  Normal,
  Transparent,
  DarkRoom
};

enum Sorting {
  All = 0,
  Wall = 1,
  Horizontal = 2,
  Vertical = 4,
  NonScalable = 8,
  Dungeons = 16,
  Floors = 32,
  SortStairs = 64
};

enum class ObjectOption {
  Nothing = 0,
  Door = 1,
  Chest = 2,
  Block = 4,
  Torch = 8,
  Bgr = 16,
  Stairs = 32
};

ObjectOption operator|(ObjectOption lhs, ObjectOption rhs);
ObjectOption operator&(ObjectOption lhs, ObjectOption rhs);
ObjectOption operator^(ObjectOption lhs, ObjectOption rhs);
ObjectOption operator~(ObjectOption option);

constexpr int kRoomObjectSubtype1 = 0x8000;          // JP = Same
constexpr int kRoomObjectSubtype2 = 0x83F0;          // JP = Same
constexpr int kRoomObjectSubtype3 = 0x84F0;          // JP = Same
constexpr int kRoomObjectTileAddress = 0x1B52;       // JP = Same
constexpr int kRoomObjectTileAddressFloor = 0x1B5A;  // JP = Same

class RoomObject : public SharedRom {
 public:
  enum LayerType { BG1 = 0, BG2 = 1, BG3 = 2 };

  RoomObject(int16_t id, uint8_t x, uint8_t y, uint8_t size, uint8_t layer = 0)
      : id_(id),
        x_(x),
        y_(y),
        size_(size),
        layer_(static_cast<LayerType>(layer)),
        nx_(x),
        ny_(y),
        ox_(x),
        oy_(y),
        width_(16),
        height_(16),
        unique_id_(0) {}

  void GetObjectSize() {
    previous_size_ = size_;
    size_ = 1;
    GetBaseSize();
    UpdateSize();
    size_ = 2;
    GetSizeSized();
    UpdateSize();
    size_ = previous_size_;
  }

  void GetBaseSize() {
    base_width_ = width_;
    base_height_ = height_;
  }

  void GetSizeSized() {
    size_height_ = height_ - base_height_;
    size_width_ = width_ - base_width_;
  }

  void UpdateSize() {
    width_ = 8;
    height_ = 8;
  }

  void AddTiles(int nbr, int pos) {
    for (int i = 0; i < nbr; i++) {
      ASSIGN_OR_LOG_ERROR(auto tile, rom()->ReadTile16(pos + (i * 2)));
      tiles_.push_back(tile);
    }
  }

  void DrawTile(gfx::Tile16 t, int xx, int yy,
                std::vector<uint8_t>& current_gfx16,
                std::vector<uint8_t>& tiles_bg1_buffer,
                std::vector<uint8_t>& tiles_bg2_buffer,
                ushort tile_under = 0xFFFF);

  auto options() const { return options_; }
  void set_options(ObjectOption options) { options_ = options; }

 protected:
  bool all_bgs_ = false;
  bool lit_ = false;
  bool deleted_ = false;
  bool show_rectangle_ = false;
  bool diagonal_fix_ = false;
  bool selected_ = false;

  int16_t id_;
  uint8_t x_;
  uint8_t y_;
  uint8_t size_;
  uint8_t nx_;
  uint8_t ny_;
  uint8_t ox_;
  uint8_t oy_;
  uint8_t z_ = 0;
  uint8_t previous_size_ = 0;

  int width_;
  int height_;
  int base_width_;
  int base_height_;
  int size_width_;
  int size_height_;
  int tile_index_ = 0;
  int offset_x_ = 0;
  int offset_y_ = 0;
  int preview_id_ = 0;
  int unique_id_ = 0;

  std::string name_;

  std::vector<uint8_t> preview_object_data_;
  std::vector<gfx::Tile16> tiles_;

  LayerType layer_;
  ObjectOption options_ = ObjectOption::Nothing;
};

class Subtype1 : public RoomObject {
 public:
  bool allBgs;
  int tile_count_;
  std::string name;
  Sorting sort;

  Subtype1(int16_t id, uint8_t x, uint8_t y, uint8_t size, uint8_t layer,
           int tileCount)
      : RoomObject(id, x, y, size, layer), tile_count_(tileCount) {
    auto rom_data = rom()->data();
    int pos = kRoomObjectTileAddress +
              static_cast<int16_t>(
                  (rom_data[kRoomObjectSubtype1 + ((id & 0xFF) * 2) + 1] << 8) +
                  rom_data[kRoomObjectSubtype1 + ((id & 0xFF) * 2)]);
    AddTiles(tile_count_, pos);
    sort = (Sorting)(Sorting::Horizontal | Sorting::Wall);
  }

  void Draw(std::vector<uint8_t>& current_gfx16,
            std::vector<uint8_t>& tiles_bg1_buffer,
            std::vector<uint8_t>& tiles_bg2_buffer) {
    for (int s = 0; s < size_ + (tile_count_ == 8 ? 1 : 0); s++) {
      for (int i = 0; i < tile_count_; i++) {
        DrawTile(tiles_[i], ((s * 2)) * 8, (i / 2) * 8, current_gfx16,
                 tiles_bg1_buffer, tiles_bg2_buffer);
      }
    }
  }
};

class Subtype2 : public RoomObject {
 public:
  std::string name;
  bool allBgs;
  Sorting sort;

  Subtype2(int16_t id, uint8_t x, uint8_t y, uint8_t size, uint8_t layer)
      : RoomObject(id, x, y, size, layer) {
    auto rom_data = rom()->data();
    int pos = kRoomObjectTileAddress +
              static_cast<int16_t>(
                  (rom_data[kRoomObjectSubtype2 + ((id & 0x7F) * 2) + 1] << 8) +
                  rom_data[kRoomObjectSubtype2 + ((id & 0x7F) * 2)]);
    AddTiles(8, pos);
    sort = (Sorting)(Sorting::Horizontal | Sorting::Wall);
  }

  void Draw(std::vector<uint8_t>& current_gfx16,
            std::vector<uint8_t>& tiles_bg1_buffer,
            std::vector<uint8_t>& tiles_bg2_buffer) {
    for (int i = 0; i < 8; i++) {
      DrawTile(tiles_[i], x_ * 8, (y_ + i) * 8, current_gfx16, tiles_bg1_buffer,
               tiles_bg2_buffer);
    }
  }
};

class Subtype3 : public RoomObject {
 public:
  bool allBgs;
  std::string name;
  Sorting sort;

  Subtype3(int16_t id, uint8_t x, uint8_t y, uint8_t size, uint8_t layer)
      : RoomObject(id, x, y, size, layer) {
    auto rom_data = rom()->data();
    int pos = kRoomObjectTileAddress +
              static_cast<int16_t>(
                  (rom_data[kRoomObjectSubtype3 + ((id & 0xFF) * 2) + 1] << 8) +
                  rom_data[kRoomObjectSubtype3 + ((id & 0xFF) * 2)]);
    AddTiles(8, pos);
    sort = (Sorting)(Sorting::Horizontal | Sorting::Wall);
  }

  void Draw(std::vector<uint8_t>& current_gfx16,
            std::vector<uint8_t>& tiles_bg1_buffer,
            std::vector<uint8_t>& tiles_bg2_buffer) {
    for (int i = 0; i < 8; i++) {
      DrawTile(tiles_[i], x_ * 8, (y_ + i) * 8, current_gfx16, tiles_bg1_buffer,
               tiles_bg2_buffer);
    }
  }
};

}  // namespace dungeon
}  // namespace zelda3
}  // namespace app
}  // namespace yaze

#endif  // YAZE_APP_ZELDA3_DUNGEON_ROOM_OBJECT_H
