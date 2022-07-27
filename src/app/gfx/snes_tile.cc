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

}  // namespace gfx
}  // namespace app
}  // namespace yaze