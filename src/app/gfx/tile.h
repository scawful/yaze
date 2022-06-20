#ifndef YAZE_APP_GFX_TILE_H
#define YAZE_APP_GFX_TILE_H

#include <tile.h>

#include <cassert>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <memory>
#include <regex>
#include <string>
#include <unordered_map>
#include <vector>

#include "app/gfx/snes_palette.h"

namespace yaze {
namespace app {
namespace gfx {

using ushort = unsigned short;
using uchar = unsigned char;
using ulong = unsigned long;
using uint = unsigned int;

// vhopppcc cccccccc
// [0, 1]
// [2, 3]
class TileInfo {
 public:
  ushort id_;
  ushort over_;
  ushort vertical_mirror_;
  ushort horizontal_mirror_;
  uchar palette_;
  TileInfo() = default;
  TileInfo(ushort id, uchar palette, ushort v, ushort h, ushort o)
      : id_(id),
        over_(o),
        vertical_mirror_(v),
        horizontal_mirror_(h),
        palette_(palette) {}
};

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

  Tile16(TileInfo t0, TileInfo t1, TileInfo t2, TileInfo t3)
      : tile0_(t0), tile1_(t1), tile2_(t2), tile3_(t3) {
    tiles_info.push_back(tile0_);
    tiles_info.push_back(tile1_);
    tiles_info.push_back(tile2_);
    tiles_info.push_back(tile3_);
  }
};

static bool isbpp3[core::constants::NumberOfSheets];

int GetPCGfxAddress(char* romData, char id);
char* CreateAllGfxDataRaw(char* romData);
void CreateAllGfxData(char* romData, char* allgfx16Ptr);
void BuildTiles16Gfx(uchar* mapblockset16, uchar* currentOWgfx16Ptr,
                     std::vector<Tile16>& allTiles);
void CopyTile16(int x, int y, int xx, int yy, int offset, TileInfo tile,
                uchar* gfx16Pointer, uchar* gfx8Pointer);

class TilePreset {
 public:
  TilePreset() = default;
  bool no_zero_color_ = false;
  int pc_tiles_location_ = 0;
  int pc_palette_location_ = 0;
  int bits_per_pixel_ = 0;
  int length_ = 0;
  int SNESTilesLocation = 0;
  int SNESPaletteLocation = 0;
};

}  // namespace gfx
}  // namespace app
}  // namespace yaze

#endif  // YAZE_APP_GFX_TILE_H