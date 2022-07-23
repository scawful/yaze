#include "snes_tile.h"

#include <cstdint>
#include <vector>

#include "app/core/constants.h"

namespace yaze {
namespace app {
namespace gfx {

TileInfo GetTilesInfo(ushort tile) {
  // vhopppcc cccccccc
  ushort o = 0;
  ushort v = 0;
  ushort h = 0;
  auto tid = (ushort)(tile & 0x3FF);
  auto p = (uchar)((tile >> 10) & 0x07);

  o = (ushort)((tile & 0x2000) >> 13);
  h = (ushort)((tile & 0x4000) >> 14);
  v = (ushort)((tile & 0x8000) >> 15);

  return TileInfo(tid, p, v, h, o);
}

void BuildTiles16Gfx(uchar *mapblockset16, uchar *currentOWgfx16Ptr,
                     std::vector<Tile16> &allTiles) {
  uchar *gfx16Data = mapblockset16;
  uchar *gfx8Data = currentOWgfx16Ptr;
  const int offsets[4] = {0, 8, 1024, 1032};
  auto yy = 0;
  auto xx = 0;

  // Number of tiles16 3748? // its 3752
  for (auto i = 0; i < core::NumberOfMap16; i++) {
    // 8x8 tile draw
    // gfx8 = 4bpp so everyting is /2
    auto tiles = allTiles[i];

    for (auto tile = 0; tile < 4; tile++) {
      TileInfo info = tiles.tiles_info[tile];
      int offset = offsets[tile];

      for (auto y = 0; y < 8; y++) {
        for (auto x = 0; x < 4; x++) {
          CopyTile16(x, y, xx, yy, offset, info, gfx16Data, gfx8Data);
        }
      }
    }

    xx += 16;
    if (xx >= 128) {
      yy += 2048;
      xx = 0;
    }
  }
}

void CopyTile16(int x, int y, int xx, int yy, int offset, TileInfo tile,
                uchar *gfx16Pointer, uchar *gfx8Pointer)  // map,current
{
  int mx = x;
  int my = y;
  uchar r = 0;

  if (tile.horizontal_mirror_) {
    mx = 3 - x;
    r = 1;
  }
  if (tile.vertical_mirror_) {
    my = 7 - y;
  }

  int tx = ((tile.id_ / 16) * 512) + ((tile.id_ - ((tile.id_ / 16) * 16)) * 4);
  auto index = xx + yy + offset + (mx * 2) + (my * 128);
  uchar pixel = gfx8Pointer[tx + (y * 64) + x];

  gfx16Pointer[index + r ^ 1] = (uchar)((pixel & 0x0F) + tile.palette_ * 16);
  gfx16Pointer[index + r] = (uchar)(((pixel >> 4) & 0x0F) + tile.palette_ * 16);
}

}  // namespace gfx
}  // namespace app
}  // namespace yaze