#ifndef YAZE_APP_DATA_OVERWORLD_H
#define YAZE_APP_DATA_OVERWORLD_H

#include <SDL.h>

#include <memory>
#include <vector>

#include "absl/status/status.h"
#include "app/core/constants.h"
#include "app/gfx/bitmap.h"
#include "app/gfx/snes_tile.h"
#include "app/rom.h"
#include "app/zelda3/overworld_map.h"

namespace yaze {
namespace app {
namespace zelda3 {

class EntranceOWEditor {
 public:
  int x_;
  int y_;
  ushort mapPos;
  uchar entranceId_, AreaX, AreaY;
  short mapId_;
  bool isHole = false;
  bool deleted = false;

  EntranceOWEditor(int x, int y, uchar entranceId, short mapId, ushort mapPos,
                   bool hole) {
    x_ = x;
    y_ = y;
    entranceId_ = entranceId;
    mapId_ = mapId;
    mapPos = mapPos;

    int mapX = (mapId_ - ((mapId / 8) * 8));
    int mapY = (mapId_ / 8);

    AreaX = (uchar)((std::abs(x - (mapX * 512)) / 16));
    AreaY = (uchar)((std::abs(y - (mapY * 512)) / 16));

    isHole = hole;
  }

  auto Copy() {
    return new EntranceOWEditor(x_, y_, entranceId_, mapId_, mapPos, isHole);
  }

  void updateMapStuff(short mapId) {
    mapId_ = mapId;

    if (mapId_ >= 64) {
      mapId_ -= 64;
    }

    int mapX = (mapId_ - ((mapId_ / 8) * 8));
    int mapY = (mapId_ / 8);

    AreaX = (uchar)((std::abs(x_ - (mapX * 512)) / 16));
    AreaY = (uchar)((std::abs(y_ - (mapY * 512)) / 16));

    int mx = (mapId_ - ((mapId_ / 8) * 8));
    int my = (mapId_ / 8);

    uchar xx = (uchar)((x_ - (mx * 512)) / 16);
    uchar yy = (uchar)((y_ - (my * 512)) / 16);

    mapPos = (ushort)((((AreaY) << 6) | (AreaX & 0x3F)) << 1);
  }
};

class Overworld {
 public:
  absl::Status Load(ROM &rom);
  auto GetTiles16() const { return tiles16; }
  auto GetOverworldMap(uint index) { return overworld_maps_[index]; }
  auto GetOverworldMaps() const { return overworld_maps_; }

  auto AreaGraphics() const {
    return overworld_maps_[current_map_].AreaGraphics();
  }
  auto Entrances() const { return all_entrances_; }
  auto AreaPalette() const {
    return overworld_maps_[current_map_].AreaPalette();
  }
  auto BitmapData() const { return overworld_maps_[current_map_].BitmapData(); }
  auto Tile16Blockset() const {
    return overworld_maps_[current_map_].Tile16Blockset();
  }

  auto isLoaded() const { return is_loaded_; }
  void SetCurrentMap(int i) { current_map_ = i; }

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
  void AssignWorldTiles(int x, int y, int sx, int sy, int tpos,
                        OWBlockset &world);
  void OrganizeMapTiles(Bytes &bytes, Bytes &bytes2, int i, int sx, int sy,
                        int &ttpos);
  absl::Status DecompressAllMapTiles();
  void FetchLargeMaps();
  void LoadEntrances();

  void LoadOverworldMap();

  int game_state_ = 1;
  int current_map_ = 0;
  uchar map_parent_[160];
  bool is_loaded_ = false;

  ROM rom_;
  OWMapTiles map_tiles_;

  std::vector<gfx::Tile16> tiles16;
  std::vector<gfx::Tile32> tiles32;
  std::vector<OverworldMap> overworld_maps_;
  std::vector<EntranceOWEditor> all_entrances_;
  std::vector<EntranceOWEditor> all_holes_;
};

}  // namespace zelda3
}  // namespace app
}  // namespace yaze

#endif