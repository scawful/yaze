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
  Overworld() = default;
  ~Overworld();

  void Load(app::rom::ROM& rom);

  char* overworldMapPointer = new char[0x40000];
  gfx::Bitmap* overworldMapBitmap;

 private:
  app::rom::ROM rom_;
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

  std::shared_ptr<uchar> mapblockset16;
  std::shared_ptr<uchar> currentOWgfx16Ptr;

  gfx::Bitmap mapblockset16Bitmap;

  SDL_Texture* overworld_map_texture;

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

}  // namespace zelda3
}  // namespace app
}  // namespace yaze

#endif