#ifndef YAZE_APP_GFX_SNES_TILE_H
#define YAZE_APP_GFX_SNES_TILE_H

#include <cstdint>
#include <cstring>
#include <vector>

#include "incl/snes_tile.h"

namespace yaze {
namespace app {
namespace gfx {

constexpr int kTilesheetWidth = 128;
constexpr int kTilesheetHeight = 32;
constexpr int kTilesheetDepth = 8;

constexpr uint8_t kGraphicsBitmap[8] = {0x80, 0x40, 0x20, 0x10,
                                        0x08, 0x04, 0x02, 0x01};

std::vector<uint8_t> SnesTo8bppSheet(const std::vector<uint8_t>& sheet, int bpp,
                                     int num_sheets = 1);
std::vector<uint8_t> Bpp8SnesToIndexed(std::vector<uint8_t> data,
                                       uint64_t bpp = 0);

snes_tile8 UnpackBppTile(const std::vector<uint8_t>& data,
                         const uint32_t offset, const uint32_t bpp);

std::vector<uint8_t> PackBppTile(const snes_tile8& tile, const uint32_t bpp);

std::vector<uint8_t> ConvertBpp(const std::vector<uint8_t>& tiles,
                                uint32_t from_bpp, uint32_t to_bpp);

std::vector<uint8_t> Convert3bppTo4bpp(const std::vector<uint8_t>& tiles);
std::vector<uint8_t> Convert4bppTo3bpp(const std::vector<uint8_t>& tiles);

void CopyTile8bpp16(int x, int y, int tile, std::vector<uint8_t>& bitmap,
                    std::vector<uint8_t>& blockset);

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
  std::array<TileInfo, 4> tiles_info;

  Tile16() = default;
  Tile16(TileInfo t0, TileInfo t1, TileInfo t2, TileInfo t3)
      : tile0_(t0), tile1_(t1), tile2_(t2), tile3_(t3) {
    tiles_info[0] = tile0_;
    tiles_info[1] = tile1_;
    tiles_info[2] = tile2_;
    tiles_info[3] = tile3_;
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

class GraphicsBuffer {
 public:
  GraphicsBuffer() = default;
  GraphicsBuffer(uint8_t bpp, const std::vector<uint8_t>& data)
      : bpp_(bpp), data_(data) {}
  GraphicsBuffer(uint8_t bpp, std::vector<uint8_t>&& data)
      : bpp_(bpp), data_(std::move(data)) {}

  uint8_t bpp() const { return bpp_; }
  const std::vector<uint8_t>& data() const { return data_; }

  void set_bpp(uint8_t bpp) { bpp_ = bpp; }
  void set_data(const std::vector<uint8_t>& data) { data_ = data; }
  void to_bpp(uint8_t bpp) {
    if (bpp_ == bpp) {
      return;
    }
    data_ = ConvertBpp(data_, bpp_, bpp);
    bpp_ = bpp;
  }

  // Array-like access via operator[]
  uint8_t& operator[](size_t index) {
    if (index >= data_.size()) {
      throw std::out_of_range("Index out of range");
    }
    return data_[index];
  }

  const uint8_t& operator[](size_t index) const {
    if (index >= data_.size()) {
      throw std::out_of_range("Index out of range");
    }
    return data_[index];
  }

  auto begin() { return data_.begin(); }
  auto end() { return data_.end(); }
  auto begin() const { return data_.begin(); }
  auto end() const { return data_.end(); }

 private:
  uint8_t bpp_;
  std::vector<uint8_t> data_;
};

}  // namespace gfx
}  // namespace app
}  // namespace yaze

#endif  // YAZE_APP_GFX_SNES_TILE_H
