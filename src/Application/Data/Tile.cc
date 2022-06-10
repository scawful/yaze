#include "Tile.h"

namespace yaze {
namespace Application {
namespace Data {

ushort TileInfo::toShort() {
  ushort value = 0;
  // vhopppcc cccccccc
  if (over_ == 1) {
    value |= 0x2000;
  };
  if (horizontal_mirror_ == 1) {
    value |= 0x4000;
  };
  if (vertical_mirror_ == 1) {
    value |= 0x8000;
  };
  value |= (ushort)((palette_ << 10) & 0x1C00);
  value |= (ushort)(id_ & 0x3FF);
  return value;
}

}  // namespace Data
}  // namespace Application
}  // namespace yaze
