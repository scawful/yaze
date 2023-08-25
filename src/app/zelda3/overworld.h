#ifndef YAZE_APP_DATA_OVERWORLD_H
#define YAZE_APP_DATA_OVERWORLD_H

#include <SDL.h>

#include <future>
#include <memory>
#include <vector>

#include "absl/status/status.h"
#include "app/core/constants.h"
#include "app/gfx/bitmap.h"
#include "app/gfx/snes_tile.h"
#include "app/rom.h"
#include "app/zelda3/overworld_map.h"
#include "app/zelda3/sprite/sprite.h"

namespace yaze {
namespace app {
namespace zelda3 {

class OverworldEntrance {
 public:
  int x_;
  int y_;
  ushort map_pos_;
  uchar entrance_id_;
  uchar area_x_;
  uchar area_y_;
  short map_id_;
  bool is_hole_ = false;
  bool deleted = false;

  OverworldEntrance(int x, int y, uchar entranceId, short mapId, ushort mapPos,
                    bool hole)
      : x_(x),
        y_(y),
        map_pos_(mapPos),
        entrance_id_(entranceId),
        map_id_(mapId),
        is_hole_(hole) {
    int mapX = (map_id_ - ((map_id_ / 8) * 8));
    int mapY = (map_id_ / 8);

    area_x_ = (uchar)((std::abs(x - (mapX * 512)) / 16));
    area_y_ = (uchar)((std::abs(y - (mapY * 512)) / 16));
  }

  auto Copy() {
    return new OverworldEntrance(x_, y_, entrance_id_, map_id_, map_pos_,
                                 is_hole_);
  }

  void updateMapStuff(short mapId) {
    map_id_ = mapId;

    if (map_id_ >= 64) {
      map_id_ -= 64;
    }

    int mapX = (map_id_ - ((map_id_ / 8) * 8));
    int mapY = (map_id_ / 8);

    area_x_ = (uchar)((std::abs(x_ - (mapX * 512)) / 16));
    area_y_ = (uchar)((std::abs(y_ - (mapY * 512)) / 16));

    map_pos_ = (ushort)((((area_y_) << 6) | (area_x_ & 0x3F)) << 1);
  }
};

class Overworld {
 public:
  absl::Status Load(ROM &rom);
  absl::Status SaveOverworldMaps();
  void SaveMap16Tiles();
  void SaveMap32Tiles();

  auto GetTiles16() const { return tiles16; }
  auto GetOverworldMap(uint index) { return overworld_maps_[index]; }
  auto GetOverworldMaps() const { return overworld_maps_; }
  auto Sprites(int state) const { return all_sprites_[state]; }
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
  auto GameState() const { return game_state_; }
  auto isLoaded() const { return is_loaded_; }
  void SetCurrentMap(int i) { current_map_ = i; }

  absl::Status LoadPrototype(ROM &rom_, std::vector<uint8_t> &tilemap,
                             std::vector<uint8_t> tile32);

 private:
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
  void LoadSprites();
  void LoadSpritesFromMap(int spriteStart, int spriteCount, int spriteIndex);

  void LoadOverworldMap();

  int game_state_ = 0;
  int current_map_ = 0;
  uchar map_parent_[160];
  bool is_loaded_ = false;

  ROM rom_;
  OWMapTiles map_tiles_;

  std::vector<gfx::Tile16> tiles16;
  std::vector<gfx::Tile32> tiles32;
  std::vector<gfx::Tile32> tiles32_unique_;
  std::vector<OverworldMap> overworld_maps_;
  std::vector<OverworldEntrance> all_entrances_;
  std::vector<OverworldEntrance> all_holes_;
  std::vector<std::vector<Sprite>> all_sprites_;
};

}  // namespace zelda3
}  // namespace app
}  // namespace yaze

#endif