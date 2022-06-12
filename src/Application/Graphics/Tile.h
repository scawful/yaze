#ifndef YAZE_APPLICATION_DATA_TILE_H
#define YAZE_APPLICATION_DATA_TILE_H

#include <tile.h>

#include <string>
#include <unordered_map>
#include <vector>

#include "Palette.h"

namespace yaze {
namespace Application {
namespace Graphics {

using byte = unsigned char;
using ushort = unsigned short;
using uint = unsigned int;

class TileInfo {
 public:
  ushort id_;
  ushort over_;
  ushort vertical_mirror_;
  ushort horizontal_mirror_;
  byte palette_;
  TileInfo() {}  // vhopppcc cccccccc
  TileInfo(ushort id, byte palette, ushort v, ushort h, ushort o)
      : id_(id),
        palette_(palette),
        vertical_mirror_(v),
        horizontal_mirror_(h),
        over_(o) {}
  ushort toShort();
};

class Tile32 {
 public:
  //[0,1]
  //[2,3]
  ushort tile0_;
  ushort tile1_;
  ushort tile2_;
  ushort tile3_;

  Tile32(ushort tile0, ushort tile1, ushort tile2, ushort tile3)
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
    // tile0_ = GFX.gettilesinfo((ushort)tiles);
    // tile1_ = GFX.gettilesinfo((ushort)(tiles >> 16));
    // tile2_ = GFX.gettilesinfo((ushort)(tiles >> 32));
    // tile3_ = GFX.gettilesinfo((ushort)(tiles >> 48));
  }

  unsigned long getLongValue() {
    return ((unsigned long)(tile3_.toShort()) << 48) |
           ((unsigned long)(tile2_.toShort()) << 32) |
           ((unsigned long)(tile1_.toShort()) << 16) |
           (unsigned long)((tile0_.toShort()));
    ;
  }
};

class TilesPattern {
 public:
  TilesPattern();
  std::string name;
  std::string description;
  bool custom;
  unsigned int tilesPerRow;
  unsigned int numberOfTiles;

  void default_settings();

  static bool loadPatterns();
  static TilesPattern pattern(std::string name);
  static std::unordered_map<std::string, TilesPattern> Patterns();
  static std::vector<std::vector<tile8> > transform(
      const TilesPattern& pattern, const std::vector<tile8>& tiles);
  static std::vector<std::vector<tile8> > transform(
      const std::string id, const std::vector<tile8>& tiles);
  static std::vector<tile8> reverse(const TilesPattern& pattern,
                                    const std::vector<tile8>& tiles);

 protected:
  std::vector<std::vector<tile8> > transform(
      const std::vector<tile8>& tiles) const;
  std::vector<tile8> reverse(const std::vector<tile8>& tiles) const;
  std::vector<std::vector<int> > transformVector;

 private:
  static std::unordered_map<std::string, TilesPattern> m_Patterns;
};

class TilePreset {
 public:
  TilePreset();

  bool save(const std::string& file);
  bool load(const std::string& file);

  std::string name;
  std::string romName;
  std::string romType;
  TilesPattern tilesPattern;

  unsigned int SNESTilesLocation;
  int pcTilesLocation;
  unsigned int SNESPaletteLocation;
  unsigned int pcPaletteLocation;
  bool paletteNoZeroColor;
  unsigned int length;

  unsigned int bpp;
  std::string compression;
};

}  // namespace Graphics
}  // namespace Application
}  // namespace yaze

#endif