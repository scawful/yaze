#ifndef YAZE_APP_DATA_OVERWORLD_H
#define YAZE_APP_DATA_OVERWORLD_H

#include <rommapping.h>

#include <memory>
#include <vector>

#include "Core/constants.h"
#include "gfx/bitmap.h"
#include "gfx/tile.h"
#include "overworld_map.h"
#include "rom.h"

namespace yaze {
namespace app {
namespace Data {

class Overworld {
 public:
  Overworld() = default;
  ~Overworld();

  void Load(Data::ROM& rom);

  char* overworldMapPointer = new char[0x40000];
  gfx::Bitmap* overworldMapBitmap;

  char* owactualMapPointer = new char[0x40000];
  gfx::Bitmap* owactualMapBitmap;

 private:
  Data::ROM rom_;
  int gameState = 1;
  bool isLoaded = false;
  uchar mapParent[160];

  ushort** allmapsTilesLW;  // 64 maps * (32*32 tiles)
  ushort** allmapsTilesDW;  // 64 maps * (32*32 tiles)
  ushort** allmapsTilesSP;  // 32 maps * (32*32 tiles)

  std::vector<gfx::Tile16> tiles16;
  std::vector<gfx::Tile32> tiles32;
  std::vector<gfx::Tile32> map16tiles;

  std::vector<OverworldMap> allmaps;

  std::vector<ushort> tileLeftEntrance;
  std::vector<ushort> tileRightEntrance;

  int map32address[4] = {
      core::constants::map32TilesTL, core::constants::map32TilesTR,
      core::constants::map32TilesBL, core::constants::map32TilesBR};

  enum Dimension {
    map32TilesTL = 0,
    map32TilesTR = 1,
    map32TilesBL = 2,
    map32TilesBR = 3
  };

  ushort GenerateTile32(int i, int k, int dimension);
  void AssembleMap32Tiles();
  void AssembleMap16Tiles();
  void DecompressAllMapTiles();
  void FetchLargeMaps();
  void LoadOverworldMap();
};

}  // namespace Data
}  // namespace app
}  // namespace yaze

#endif