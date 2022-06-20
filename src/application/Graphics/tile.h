#ifndef YAZE_APPLICATION_DATA_TILE_H
#define YAZE_APPLICATION_DATA_TILE_H

#include <tile.h>

#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

#include "Core/constants.h"
#include "Graphics/palette.h"

namespace yaze {
namespace application {
namespace Graphics {

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

class TilesPattern {
 public:
  TilesPattern();
  std::string name;
  std::string description;
  unsigned int tiles_per_row_;
  unsigned int number_of_tiles_;

  static TilesPattern pattern(std::string name);
  static std::vector<std::vector<tile8>> transform(
      const TilesPattern& pattern, const std::vector<tile8>& tiles);

 protected:
  std::vector<std::vector<tile8>> transform(
      const std::vector<tile8>& tiles) const;
  std::vector<tile8> reverse(const std::vector<tile8>& tiles) const;

 private:
  std::vector<std::vector<int>> transform_vector_;
};

class TilePreset {
 public:
  TilePreset() = default;
  TilesPattern tilesPattern;
  bool no_zero_color_ = false;
  int pc_tiles_location_ = 0;
  int pc_palette_location_ = 0;
  int bits_per_pixel_ = 0;
  int length_ = 0;
  int SNESTilesLocation = 0;
  int SNESPaletteLocation = 0;
};

}  // namespace Graphics
}  // namespace application
}  // namespace yaze

#endif