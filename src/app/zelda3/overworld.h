#ifndef YAZE_APP_DATA_OVERWORLD_H
#define YAZE_APP_DATA_OVERWORLD_H

#include <SDL2/SDL.h>
#include <rommapping.h>

#include <memory>
#include <vector>

#include "app/core/constants.h"
#include "app/gfx/bitmap.h"
#include "app/gfx/tile.h"
#include "app/rom.h"
#include "app/zelda3/overworld_map.h"

namespace yaze {
namespace app {
namespace zelda3 {

class Overworld {
 public:
  void Load(app::rom::ROM& rom, uchar* allGfxPtr);
  auto GetTiles16() const { return tiles16; }
  auto GetCurrentGfxSetPtr() { return currentOWgfx16Ptr; }
  auto GetMapBlockset16Ptr() { return mapblockset16; }

  std::shared_ptr<uchar[]> overworldMapPointer;
  gfx::Bitmap overworldMapBitmap;

 private:
  ushort GenerateTile32(int i, int k, int dimension);
  void AssembleMap32Tiles();
  void AssembleMap16Tiles();
  void DecompressAllMapTiles();
  void FetchLargeMaps();
  void LoadOverworldMap();

  app::rom::ROM rom_;
  int gameState = 1;
  bool isLoaded = false;
  uchar mapParent[160];

  std::vector<std::vector<ushort>> allmapsTilesLW;  // 64 maps * (32*32 tiles)
  std::vector<std::vector<ushort>> allmapsTilesDW;  // 64 maps * (32*32 tiles)
  std::vector<std::vector<ushort>> allmapsTilesSP;  // 32 maps * (32*32 tiles)

  uchar* allGfx16Ptr = nullptr;
  std::shared_ptr<uchar[]> mapblockset16;
  std::shared_ptr<uchar[]> currentOWgfx16Ptr;

  std::vector<gfx::Tile16> tiles16;
  std::vector<gfx::Tile32> tiles32;
  std::vector<gfx::Tile32> map16tiles;
  std::vector<OverworldMap> overworld_maps_;

  const int map32address[4] = {
      core::constants::map32TilesTL, core::constants::map32TilesTR,
      core::constants::map32TilesBL, core::constants::map32TilesBR};

  enum Dimension {
    map32TilesTL = 0,
    map32TilesTR = 1,
    map32TilesBL = 2,
    map32TilesBR = 3
  };
};

}  // namespace zelda3
}  // namespace app
}  // namespace yaze

#endif