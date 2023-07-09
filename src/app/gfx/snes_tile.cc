#include "snes_tile.h"

#include <cstdint>
#include <vector>

#include "app/core/constants.h"

namespace yaze {
namespace app {
namespace gfx {

Bytes SnesTo8bppSheet(Bytes sheet, int bpp) {
  int xx = 0;  // positions where we are at on the sheet
  int yy = 0;
  int pos = 0;
  int ypos = 0;
  int num_tiles = 64;
  int buffer_size = 0x1000;
  if (bpp == 2) {
    bpp = 16;
    num_tiles = 128;
    buffer_size = 0x2000;
  } else if (bpp == 3) {
    bpp = 24;
  }
  Bytes sheet_buffer_out(buffer_size);

  for (int i = 0; i < num_tiles; i++) {  // for each tiles, 16 per line
    for (int y = 0; y < 8; y++) {        // for each line
      for (int x = 0; x < 8; x++) {      //[0] + [1] + [16]
        auto b1 = (sheet[(y * 2) + (bpp * pos)] & (kGraphicsBitmap[x]));
        auto b2 = (sheet[((y * 2) + (bpp * pos)) + 1] & (kGraphicsBitmap[x]));
        auto b3 = (sheet[(16 + y) + (bpp * pos)] & (kGraphicsBitmap[x]));
        unsigned char b = 0;
        if (b1 != 0) {
          b |= 1;
        }
        if (b2 != 0) {
          b |= 2;
        }
        if (b3 != 0 && bpp != 16) {
          b |= 4;
        }
        sheet_buffer_out[x + xx + (y * 128) + (yy * 1024)] = b;
      }
    }
    pos++;
    ypos++;
    xx += 8;
    if (ypos >= 16) {
      yy++;
      xx = 0;
      ypos = 0;
    }
  }
  return sheet_buffer_out;
}

TileInfo GetTilesInfo(ushort tile) {
  // vhopppcc cccccccc
  bool o = false;
  bool v = false;
  bool h = false;
  auto tid = (ushort)(tile & core::TileNameMask);
  auto p = (uchar)((tile >> 10) & 0x07);

  o = ((tile & core::TilePriorityBit) == core::TilePriorityBit);
  h = ((tile & core::TileHFlipBit) == core::TileHFlipBit);
  v = ((tile & core::TileVFlipBit) == core::TileVFlipBit);

  return TileInfo(tid, p, v, h, o);
}

}  // namespace gfx
}  // namespace app
}  // namespace yaze