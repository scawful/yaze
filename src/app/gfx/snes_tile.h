#ifndef YAZE_APP_GFX_SNES_TILE_H
#define YAZE_APP_GFX_SNES_TILE_H

#include <cstdint>
#include <cstring>
#include <vector>

#include "app/core/constants.h"

namespace yaze {
namespace app {
namespace gfx {

constexpr int kTilesheetWidth = 128;
constexpr int kTilesheetHeight = 32;
constexpr int kTilesheetDepth = 8;

constexpr uint8_t kGraphicsBitmap[8] = {0x80, 0x40, 0x20, 0x10,
                                        0x08, 0x04, 0x02, 0x01};

std::vector<uint8_t> SnesTo8bppSheet(const std::vector<uint8_t>& sheet,
                                     int bpp);
std::vector<uint8_t> Bpp8SnesToIndexed(std::vector<uint8_t> data,
                                       uint64_t bpp = 0);

struct tile8 {
  uint32_t id;
  char data[64];
  uint32_t palette_id;
};
using tile8 = struct tile8;

tile8 UnpackBppTile(const std::vector<uint8_t>& data, const uint32_t offset,
                    const uint32_t bpp);

std::vector<uint8_t> PackBppTile(const tile8& tile, const uint32_t bpp);

std::vector<uint8_t> ConvertBpp(const std::vector<uint8_t>& tiles,
                                uint32_t from_bpp, uint32_t to_bpp);

std::vector<uint8_t> Convert3bppTo4bpp(const std::vector<uint8_t>& tiles);
std::vector<uint8_t> Convert4bppTo3bpp(const std::vector<uint8_t>& tiles);

/**
 * @brief SNES 16-bit tile metadata container
 *
 * Format:
 *  vhopppcc cccccccc
 *     [0, 1]
 *     [2, 3]
 */
class TileInfo {
 public:
  uint16_t id_;
  uint8_t palette_;
  bool over_;
  bool vertical_mirror_;
  bool horizontal_mirror_;
  TileInfo() = default;
  TileInfo(uint16_t id, uint8_t palette, bool v, bool h, bool o)
      : id_(id),
        over_(o),
        vertical_mirror_(v),
        horizontal_mirror_(h),
        palette_(palette) {}

  bool operator==(const TileInfo& other) const {
    return id_ == other.id_ && over_ == other.over_ &&
           vertical_mirror_ == other.vertical_mirror_ &&
           horizontal_mirror_ == other.horizontal_mirror_ &&
           palette_ == other.palette_;
  }
};

uint16_t TileInfoToWord(TileInfo tile_info);
TileInfo WordToTileInfo(uint16_t word);
uint16_t TileInfoToShort(TileInfo tile_info);

TileInfo GetTilesInfo(uint16_t tile);

/**
 * @brief Tile composition of four 16x16 tiles.
 */
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
    tile0_ = (uint16_t)packedVal;
    tile1_ = (uint16_t)(packedVal >> 16);
    tile2_ = (uint16_t)(packedVal >> 32);
    tile3_ = (uint16_t)(packedVal >> 48);
  }

  // Get packed uint64_t representation
  uint64_t GetPackedValue() const {
    return static_cast<uint64_t>(tile3_) << 48 |
           (static_cast<uint64_t>(tile2_) << 32) |
           (static_cast<uint64_t>(tile1_) << 16) | tile0_;
  }

  // Equality operator
  bool operator==(const Tile32& other) const {
    return tile0_ == other.tile0_ && tile1_ == other.tile1_ &&
           tile2_ == other.tile2_ && tile3_ == other.tile3_;
  }

  // Inequality operator
  bool operator!=(const Tile32& other) const { return !(*this == other); }
};

/**
 * @brief Tile composition of four 8x8 tiles.
 */
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

  bool operator==(const Tile16& other) const {
    return tile0_ == other.tile0_ && tile1_ == other.tile1_ &&
           tile2_ == other.tile2_ && tile3_ == other.tile3_;
  }

  bool operator!=(const Tile16& other) const { return !(*this == other); }
};

/**
 * @brief Object Attribute Memory tile abstraction container
 */
class OamTile {
 public:
  int x_;
  int y_;
  int mx_;
  int my_;
  int pal_;
  uint16_t tile_;
  OamTile() = default;
  OamTile(int x, int y, uint16_t tile, int pal, bool upper = false, int mx = 0,
          int my = 0)
      : x_(x), y_(y), mx_(mx), my_(my), pal_(pal) {
    if (upper) {
      tile_ = (uint16_t)(tile + 512);
    } else {
      tile_ = (uint16_t)(tile + 256 + 512);
    }
  }
};

}  // namespace gfx
}  // namespace app
}  // namespace yaze

#endif  // YAZE_APP_GFX_SNES_TILE_H
