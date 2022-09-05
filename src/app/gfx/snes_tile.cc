#include "snes_tile.h"

#include <cstdint>
#include <vector>

#include "app/core/constants.h"

namespace yaze {
namespace app {
namespace gfx {

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