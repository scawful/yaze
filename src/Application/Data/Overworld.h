#ifndef YAZE_APPLICATION_DATA_OVERWORLD_H
#define YAZE_APPLICATION_DATA_OVERWORLD_H

#include <memory>
#include <vector>

#include "Core/Constants.h"
#include "Tile.h"
#include "Utils/Bitmap.h"
#include "Utils/Compression.h"
#include "Utils/ROM.h"

namespace yaze {
namespace Application {
namespace Data {

using ushort = ushort;
using byte = unsigned char;

class Overworld {
 public:
  Overworld(Utils::ROM rom);

 private:
  Utils::ROM rom_;
  Utils::ALTTPCompression alttp_compressor_;

  std::vector<Tile16> tiles16;
  std::vector<Tile32> tiles32;
  std::vector<Tile32> map16tiles;

  std::vector<ushort> tileLeftEntrance;
  std::vector<ushort> tileRightEntrance;

  int map32address[4] = {
      Core::Constants::map32TilesTL, Core::Constants::map32TilesTR,
      Core::Constants::map32TilesBL, Core::Constants::map32TilesBR};

  std::unique_ptr<int> overworldMapPointer;
  Utils::Bitmap overworldMapBitmap;

  std::unique_ptr<int> owactualMapPointer;
  Utils::Bitmap owactualMapBitmap;

  enum Dimension {
    map32TilesTL = 0,
    map32TilesTR = 1,
    map32TilesBL = 2,
    map32TilesBR = 3
  };

  unsigned short **allmapsTilesLW; 
  
  // 64 maps * (32*32 tiles) ushort[512, 512]
  std::vector<std::vector<ushort>> allmapsTilesDW;  // 64 maps * (32*32 tiles)
  std::vector<std::vector<ushort>> allmapsTilesSP;  // 32 maps * (32*32 tiles)

  ushort GenerateTile32(int i, int k, int dimension);
  void AssembleMap32Tiles();
  void AssembleMap16Tiles();
  void DecompressAllMapTiles();
  void LoadOverworldMap();
};

}  // namespace Data
}  // namespace Application
}  // namespace yaze

#endif