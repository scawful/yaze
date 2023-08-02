#ifndef YAZE_APP_GFX_SNES_TILE_H
#define YAZE_APP_GFX_SNES_TILE_H

#include <cstdint>
#include <vector>

#include "app/core/constants.h"

namespace yaze {
namespace app {
namespace gfx {

constexpr uchar kGraphicsBitmap[8] = {0x80, 0x40, 0x20, 0x10,
                                      0x08, 0x04, 0x02, 0x01};

Bytes SnesTo8bppSheet(Bytes sheet, int bpp);
Bytes BPP8SNESToIndexed(Bytes data, uint64_t bpp = 0);

struct tile8 {
  unsigned int id;
  char data[64];
  unsigned int palette_id;
};
using tile8 = struct tile8;

// vhopppcc cccccccc
// [0, 1]
// [2, 3]
class TileInfo {
 public:
  ushort id_;
  bool over_;
  bool vertical_mirror_;
  bool horizontal_mirror_;
  uchar palette_;
  TileInfo() = default;
  TileInfo(ushort id, uchar palette, bool v, bool h, bool o)
      : id_(id),
        over_(o),
        vertical_mirror_(v),
        horizontal_mirror_(h),
        palette_(palette) {}

  ushort ToShort() const {
    ushort result = 0;

    // Copy the id_ value
    result |= id_ & 0x3FF;  // ids are 10 bits

    // Set the vertical_mirror_, horizontal_mirror_, and over_ flags
    result |= (vertical_mirror_ ? 1 : 0) << 10;
    result |= (horizontal_mirror_ ? 1 : 0) << 11;
    result |= (over_ ? 1 : 0) << 12;

    // Set the palette_
    result |= (palette_ & 0x07) << 13;  // palettes are 3 bits

    return result;
  }
};

TileInfo GetTilesInfo(ushort tile);

class Tile32 {
 public:
  ushort tile0_;
  ushort tile1_;
  ushort tile2_;
  ushort tile3_;

  Tile32(ushort t0, ushort t1, ushort t2, ushort t3)
      : tile0_(t0), tile1_(t1), tile2_(t2), tile3_(t3) {}
};

class Tile16 {
 public:
  TileInfo tile0_;
  TileInfo tile1_;
  TileInfo tile2_;
  TileInfo tile3_;
  std::vector<TileInfo> tiles_info;

  Tile16() = default;
  Tile16(TileInfo t0, TileInfo t1, TileInfo t2, TileInfo t3)
      : tile0_(t0), tile1_(t1), tile2_(t2), tile3_(t3) {
    tiles_info.push_back(tile0_);
    tiles_info.push_back(tile1_);
    tiles_info.push_back(tile2_);
    tiles_info.push_back(tile3_);
  }
};

class OAMTile {
 public:
  int x_;
  int y_;
  int mx_;
  int my_;
  int pal_;
  ushort tile_;
  OAMTile() = default;
  OAMTile(int x, int y, ushort tile, int pal, bool upper = false, int mx = 0,
          int my = 0)
      : x_(x), y_(y), mx_(mx), my_(my), pal_(pal) {
    if (upper) {
      tile_ = (ushort)(tile + 512);
    } else {
      tile_ = (ushort)(tile + 256 + 512);
    }
  }
};

}  // namespace gfx
}  // namespace app
}  // namespace yaze

#endif  // YAZE_APP_GFX_SNES_TILE_H