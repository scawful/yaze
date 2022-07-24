#ifndef YAZE_APP_DATA_OVERWORLD_H
#define YAZE_APP_DATA_OVERWORLD_H

#include <SDL2/SDL.h>
#include <rommapping.h>

#include <memory>
#include <vector>

#include "app/core/constants.h"
#include "app/gfx/bitmap.h"
#include "app/gfx/pseudo_vram.h"
#include "app/gfx/snes_tile.h"
#include "app/rom.h"
#include "app/zelda3/overworld_map.h"

namespace yaze {
namespace app {
namespace zelda3 {

class Overworld {
 public:
  void Load(ROM &rom, uchar *ow_blockset);
  auto GetTiles16() const { return tiles16; }
  auto GetOverworldMap(uint index) { return overworld_maps_[index]; }
  auto isLoaded() const { return is_loaded_; }

 private:
  const int map32address[4] = {core::map32TilesTL, core::map32TilesTR,
                               core::map32TilesBL, core::map32TilesBR};
  enum Dimension {
    map32TilesTL = 0,
    map32TilesTR = 1,
    map32TilesBL = 2,
    map32TilesBR = 3
  };

  ushort GenerateTile32(int i, int k, int dimension);
  void AssembleMap32Tiles();
  void AssembleMap16Tiles();
  void AssignWorldTiles(std::vector<std::vector<ushort>> &world, int x, int y,
                        int sx, int sy, int tpos);
  void DecompressAllMapTiles();
  void FetchLargeMaps();

  int game_state_ = 1;
  uchar map_parent_[160];
  bool is_loaded_ = false;

  ROM rom_;
  OWMapTiles map_tiles_;

  std::vector<gfx::Tile16> tiles16;
  std::vector<gfx::Tile32> tiles32;
  std::vector<gfx::Tile32> map16tiles;
  std::vector<OverworldMap> overworld_maps_;
};

}  // namespace zelda3
}  // namespace app
}  // namespace yaze

#endif