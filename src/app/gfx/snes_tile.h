#ifndef YAZE_APP_GFX_SNES_TILE_H
#define YAZE_APP_GFX_SNES_TILE_H

#include <cstdint>
#include <cstring>
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
  uint32_t id;
  char data[64];
  uint32_t palette_id;
};
using tile8 = struct tile8;

tile8 UnpackBppTile(const Bytes& data, const uint32_t offset,
                    const uint32_t bpp);

Bytes PackBppTile(const tile8& tile, const uint32_t bpp);

std::vector<uchar> ConvertBpp(const std::vector<uchar>& tiles,
                              uint32_t from_bpp, uint32_t to_bpp);

std::vector<uchar> Convert3bppTo4bpp(const std::vector<uchar>& tiles);
std::vector<uchar> Convert4bppTo3bpp(const std::vector<uchar>& tiles);

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
};

uint16_t TileInfoToWord(TileInfo tile_info);
TileInfo WordToTileInfo(uint16_t word);
ushort TileInfoToShort(TileInfo tile_info);

TileInfo GetTilesInfo(ushort tile);

class Tile32 {
 public:
  uint16_t tile0_;
  uint16_t tile1_;
  uint16_t tile2_;
  uint16_t tile3_;

  // Default constructor
  Tile32() : tile0_(0), tile1_(0), tile2_(0), tile3_(0) {}

  // Parameterized constructor
  Tile32(uint16_t t0, uint16_t t1, uint16_t t2, uint16_t t3)
      : tile0_(t0), tile1_(t1), tile2_(t2), tile3_(t3) {}

  // Copy constructor
  Tile32(const Tile32& other)
      : tile0_(other.tile0_),
        tile1_(other.tile1_),
        tile2_(other.tile2_),
        tile3_(other.tile3_) {}

  // Constructor from packed value
  Tile32(uint64_t packedVal) {
    tile0_ = (packedVal >> 48) & 0xFFFF;
    tile1_ = (packedVal >> 32) & 0xFFFF;
    tile2_ = (packedVal >> 16) & 0xFFFF;
    tile3_ = packedVal & 0xFFFF;
  }

  // Equality operator
  bool operator==(const Tile32& other) const {
    return tile0_ == other.tile0_ && tile1_ == other.tile1_ &&
           tile2_ == other.tile2_ && tile3_ == other.tile3_;
  }

  // Inequality operator
  bool operator!=(const Tile32& other) const { return !(*this == other); }

  // Get packed uint64_t representation
  uint64_t GetPackedValue() const {
    return (static_cast<uint64_t>(tile0_) << 48) |
           (static_cast<uint64_t>(tile1_) << 32) |
           (static_cast<uint64_t>(tile2_) << 16) |
           static_cast<uint64_t>(tile3_);
  }
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