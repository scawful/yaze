#ifndef YAZE_APP_ZELDA3_DUNGEON_ROOM_OBJECT_H
#define YAZE_APP_ZELDA3_DUNGEON_ROOM_OBJECT_H

#include <array>
#include <cstdint>
#include <stdexcept>
#include <string>
#include <vector>

#include "app/gfx/snes_palette.h"
#include "app/gfx/snes_tile.h"
#include "app/rom.h"

namespace yaze {
namespace app {
namespace zelda3 {
namespace dungeon {

enum class SpecialObjectType { Chest, BigChest, InterroomStairs };

struct Tile {};

enum Background2 {
  Off,
  Parallax,
  Dark,
  OnTop,
  Translucent,
  Addition,
  Normal,
  Transparent,
  DarkRoom  // TODO: Determine if DarkRoom will stay there or not
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

enum ObjectOption {
  Nothing = 0,
  Door = 1,
  Chest = 2,
  Block = 4,
  Torch = 8,
  Bgr = 16,
  Stairs = 32
};

struct LayerType {
  LayerType(uint8_t t) : type(t) {}

  uint8_t type;
};

class RoomObject : public SharedROM {
 public:
  enum LayerType { BG1 = 0, BG2 = 1, BG3 = 2 };

  RoomObject(int16_t id, uint8_t x, uint8_t y, uint8_t size, uint8_t layer = 0)
      : id(id),
        x_(x),
        y_(y),
        size_(size),
        Layer(static_cast<LayerType>(layer)),
        nx(x),
        ny(y),
        ox(x),
        oy(y),
        width(16),
        height(16),
        uniqueID(0) {}

  void getObjectSize() {
    previousSize = size_;
    size_ = 1;
    // Draw();
    getBaseSize();
    UpdateSize();
    size_ = 2;
    // Draw();
    getSizeSized();
    UpdateSize();
    size_ = previousSize;
    // collisionPoint.clear();
  }

  void getBaseSize() {
    basewidth = width;
    baseheight = height;
  }

  void getSizeSized() {
    sizeheight = height - baseheight;
    sizewidth = width - basewidth;
  }

  // virtual void Draw() { collisionPoint.clear(); }

  void UpdateSize() {
    width = 8;
    height = 8;
  }

  void addTiles(int nbr, int pos) {
    auto rom_data = rom()->data();
    for (int i = 0; i < nbr; i++) {
      // tiles.push_back(
      //     gfx::Tile16(rom_data[pos + (i * 2)], rom_data[pos + (i * 2) + 1]));
    }
  }

  void DrawTile(Tile t, int xx, int yy, std::vector<uint8_t>& current_gfx16,
                std::vector<uint8_t>& tiles_bg1_buffer, std::vector<uint8_t>& tiles_bg2_buffer,
                ushort tileUnder = 0xFFFF);

 private:
  int16_t id;

  uint8_t x_;
  uint8_t y_;
  uint8_t size_;

  LayerType Layer;

  std::vector<uint8_t> preview_object_data_;

  bool allBgs = false;
  bool lit = false;
  std::vector<gfx::Tile16> tiles;
  int tileIndex = 0;
  std::string name;
  uint8_t nx;
  uint8_t ny;
  uint8_t ox;
  uint8_t oy;
  int width;
  int height;
  int basewidth;
  int baseheight;
  int sizewidth;
  int sizeheight;
  ObjectOption options = ObjectOption::Nothing;
  int offsetX = 0;
  int offsetY = 0;
  bool diagonalFix = false;
  bool selected = false;
  bool redraw = false;
  bool preview = false;
  int previewId = 0;
  uint8_t previousSize = 0;
  bool showRectangle = false;
  // std::vector<Point> collisionPoint;
  int uniqueID = 0;
  uint8_t z = 0;
  bool deleted = false;
};
}  // namespace dungeon
}  // namespace zelda3
}  // namespace app
}  // namespace yaze

#endif  // YAZE_APP_ZELDA3_DUNGEON_ROOM_OBJECT_H