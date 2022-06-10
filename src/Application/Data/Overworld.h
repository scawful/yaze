#ifndef YAZE_APPLICATION_DATA_OVERWORLD_H
#define YAZE_APPLICATION_DATA_OVERWORLD_H

#include <memory>
#include <vector>

#include "Core/Constants.h"

namespace yaze {
namespace Application {
namespace Data {

class TileInfo {
 public:
  unsigned short o;
  unsigned short v;
  unsigned short h;  // o = over, v = vertical mirror, h = horizontal mirror
  std::byte palette;
  unsigned short id;
  // vhopppcc cccccccc
  TileInfo() {}
  TileInfo(unsigned short id, std::byte palette, unsigned short v,
           unsigned short h, unsigned short o) {}
  unsigned short toShort() {
    unsigned short value = 0;
    // vhopppcc cccccccc
    if (o == 1) {
      value |= 0x2000;
    };
    if (h == 1) {
      value |= 0x4000;
    };
    if (v == 1) {
      value |= 0x8000;
    };
    value |= (unsigned short)(((unsigned short)palette << 10) & 0x1C00);
    value |= (unsigned short)(id & 0x3FF);
    return value;
  }
};

class Tile32 {
  //[0,1]
  //[2,3]
  unsigned short tile0_;
  unsigned short tile1_;
  unsigned short tile2_;
  unsigned short tile3_;

 public:
  Tile32(unsigned short tile0, unsigned short tile1, unsigned short tile2,
         unsigned short tile3)
      : tile0_(tile0), tile1_(tile1), tile2_(tile2), tile3_(tile3) {}

  explicit Tile32(unsigned long tiles)
      : tile0_(tiles),
        tile1_(tiles >> 16),
        tile2_(tiles >> 32),
        tile3_(tiles >> 48) {}

  unsigned long getLongValue() {
    return ((unsigned long)tile3_ << 48) | ((unsigned long)tile2_ << 32) |
           ((unsigned long)tile1_ << 16) | (unsigned long)(tile0_);
  }
};

class Tile16 {
 public:
  TileInfo tile0_;
  TileInfo tile1_;
  TileInfo tile2_;
  TileInfo tile3_;
  std::vector<TileInfo> tiles_info;
  //[0,1]
  //[2,3]

  Tile16(TileInfo tile0, TileInfo tile1, TileInfo tile2, TileInfo tile3)
      : tile0_(tile0), tile1_(tile1), tile2_(tile2), tile3_(tile3) {
    tiles_info.push_back(tile0_);
    tiles_info.push_back(tile1_);
    tiles_info.push_back(tile2_);
    tiles_info.push_back(tile3_);
  }

  explicit Tile16(unsigned long tiles) {
    // tile0_ = GFX.gettilesinfo((unsigned short)tiles);
    // tile1_ = GFX.gettilesinfo((unsigned short)(tiles >> 16));
    // tile2_ = GFX.gettilesinfo((unsigned short)(tiles >> 32));
    // tile3_ = GFX.gettilesinfo((unsigned short)(tiles >> 48));
  }

  unsigned long getLongValue() {
    return ((unsigned long)(tile3_.toShort()) << 48) |
           ((unsigned long)(tile2_.toShort()) << 32) |
           ((unsigned long)(tile1_.toShort()) << 16) |
           (unsigned long)((tile0_.toShort()));
    ;
  }
};

class Overworld {
 public:
  Overworld();

 private:
  void AssembleMap32Tiles();
  void AssembleMap16Tiles();

  std::vector<Tile16> tiles16;
  std::vector<Tile32> tiles32;
  std::vector<Tile32> map16tiles;

  std::vector<unsigned short> tileLeftEntrance;
  std::vector<unsigned short> tileRightEntrance;

  int map32address[4] = {
      Core::Constants::map32TilesTL, Core::Constants::map32TilesTR,
      Core::Constants::map32TilesBL, Core::Constants::map32TilesBR};
};

}  // namespace Data
}  // namespace Application
}  // namespace yaze

#endif