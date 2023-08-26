#ifndef YAZE_APP_DATA_OVERWORLD_H
#define YAZE_APP_DATA_OVERWORLD_H

#include <SDL.h>

#include <future>
#include <memory>
#include <vector>

#include "absl/container/flat_hash_map.h"
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

constexpr int OWExitRoomId = 0x15D8A;  // 0x15E07 Credits sequences
// 105C2 Ending maps
// 105E2 Sprite Group Table for Ending
constexpr int OWExitMapId = 0x15E28;
constexpr int OWExitVram = 0x15E77;
constexpr int OWExitYScroll = 0x15F15;
constexpr int OWExitXScroll = 0x15FB3;
constexpr int OWExitYPlayer = 0x16051;
constexpr int OWExitXPlayer = 0x160EF;
constexpr int OWExitYCamera = 0x1618D;
constexpr int OWExitXCamera = 0x1622B;
constexpr int OWExitDoorPosition = 0x15724;
constexpr int OWExitUnk1 = 0x162C9;
constexpr int OWExitUnk2 = 0x16318;
constexpr int OWExitDoorType1 = 0x16367;
constexpr int OWExitDoorType2 = 0x16405;
constexpr int OWEntranceMap = 0xDB96F;
constexpr int OWEntrancePos = 0xDBA71;
constexpr int OWEntranceEntranceId = 0xDBB73;
constexpr int OWHolePos = 0xDB800;  //(0x13 entries, 2 bytes each) modified(less
                                    // 0x400) map16 coordinates for each hole
constexpr int OWHoleArea =
    0xDB826;  //(0x13 entries, 2 bytes each) corresponding
              // area numbers for each hole
constexpr int OWHoleEntrance =
    0xDB84C;  //(0x13 entries, 1 byte each)  corresponding entrance numbers

constexpr int OWExitMapIdWhirlpool = 0x16AE5;    //  JP = ;016849
constexpr int OWExitVramWhirlpool = 0x16B07;     //  JP = ;01686B
constexpr int OWExitYScrollWhirlpool = 0x16B29;  // JP = ;01688D
constexpr int OWExitXScrollWhirlpool = 0x16B4B;  // JP = ;016DE7
constexpr int OWExitYPlayerWhirlpool = 0x16B6D;  // JP = ;016E09
constexpr int OWExitXPlayerWhirlpool = 0x16B8F;  // JP = ;016E2B
constexpr int OWExitYCameraWhirlpool = 0x16BB1;  // JP = ;016E4D
constexpr int OWExitXCameraWhirlpool = 0x16BD3;  // JP = ;016E6F
constexpr int OWExitUnk1Whirlpool = 0x16BF5;     //    JP = ;016E91
constexpr int OWExitUnk2Whirlpool = 0x16C17;     //    JP = ;016EB3
constexpr int OWWhirlpoolPosition = 0x16CF8;     //    JP = ;016F94

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

constexpr int compressedAllMap32PointersHigh = 0x1794D;
constexpr int compressedAllMap32PointersLow = 0x17B2D;
constexpr int overworldgfxGroups = 0x05D97;
constexpr int overworldPalGroup1 = 0xDE6C8;
constexpr int overworldPalGroup2 = 0xDE86C;
constexpr int overworldPalGroup3 = 0xDE604;
constexpr int overworldMapPalette = 0x7D1C;
constexpr int overworldSpritePalette = 0x7B41;
constexpr int overworldMapPaletteGroup = 0x75504;
constexpr int overworldSpritePaletteGroup = 0x75580;
constexpr int overworldSpriteset = 0x7A41;
constexpr int overworldSpecialGFXGroup = 0x16821;
constexpr int OverworldMapDataOverflow = 0x130000;
constexpr int overworldSpecialPALGroup = 0x16831;
constexpr int overworldSpritesBegining = 0x4C881;
constexpr int overworldSpritesAgahnim = 0x4CA21;
constexpr int overworldSpritesZelda = 0x4C901;
constexpr int overworldItemsPointers = 0xDC2F9;
constexpr int overworldItemsAddress = 0xDC8B9;  // 1BC2F9
constexpr int overworldItemsBank = 0xDC8BF;
constexpr int overworldItemsEndData = 0xDC89C;  // 0DC89E
constexpr int mapGfx = 0x7C9C;
constexpr int overlayPointers = 0x77664;
constexpr int overlayPointersBank = 0x0E;
constexpr int overworldTilesType = 0x71459;
constexpr int overworldMessages = 0x3F51D;
constexpr int overworldMusicBegining = 0x14303;
constexpr int overworldMusicZelda = 0x14303 + 0x40;
constexpr int overworldMusicMasterSword = 0x14303 + 0x80;
constexpr int overworldMusicAgahim = 0x14303 + 0xC0;
constexpr int overworldMusicDW = 0x14403;
constexpr int overworldEntranceAllowedTilesLeft = 0xDB8C1;
constexpr int overworldEntranceAllowedTilesRight = 0xDB917;

// 0x00 = small maps, 0x20 = large maps
constexpr int overworldMapSize = 0x12844;

// 0x01 = small maps, 0x03 = large maps
constexpr int overworldMapSizeHighByte = 0x12884;

// relative to the WORLD + 0x200 per map
// large map that are not == parent id = same position as their parent!
// eg for X position small maps :
// 0000, 0200, 0400, 0600, 0800, 0A00, 0C00, 0E00
// all Large map would be :
// 0000, 0000, 0400, 0400, 0800, 0800, 0C00, 0C00
constexpr int overworldMapParentId = 0x125EC;
constexpr int overworldTransitionPositionY = 0x128C4;
constexpr int overworldTransitionPositionX = 0x12944;
constexpr int overworldScreenSize = 0x1788D;
constexpr int OverworldScreenSizeForLoading = 0x4C635;
constexpr int OverworldScreenTileMapChangeByScreen = 0x12634;
constexpr int transition_target_north = 0x13ee2;
constexpr int transition_target_west = 0x13f62;
constexpr int overworldCustomMosaicASM = 0x1301D0;
constexpr int overworldCustomMosaicArray = 0x1301F0;

constexpr int kMap16Tiles = 0x78000;
constexpr int kNumOverworldMaps = 160;
constexpr int Map32PerScreen = 256;
constexpr int NumberOfMap16 = 3752;  // 4096
constexpr int LimitOfMap32 = 8864;
constexpr int NumberOfOWSprites = 352;
constexpr int NumberOfMap32 = Map32PerScreen * kNumOverworldMaps;

struct MapData {
  std::vector<uint8_t> highData;
  std::vector<uint8_t> lowData;
};

class Overworld : public SharedROM {
 public:
  absl::Status Load(ROM &rom);

  absl::Status SaveOverworldMaps();
  absl::Status SaveLargeMaps();

  bool CreateTile32Tilemap(bool onlyShow = false);
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
  auto AreaPaletteById(int id) const {
    return overworld_maps_[id].AreaPalette();
  }
  auto BitmapData() const { return overworld_maps_[current_map_].BitmapData(); }
  auto Tile16Blockset() const {
    return overworld_maps_[current_map_].Tile16Blockset();
  }
  auto GameState() const { return game_state_; }
  auto isLoaded() const { return is_loaded_; }
  void SetCurrentMap(int i) { current_map_ = i; }

  absl::Status LoadPrototype(ROM &rom_, const std::string &tilemap_filename);

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
  absl::Status DecompressProtoMapTiles(const std::string &filename);
  void FetchLargeMaps();
  void LoadEntrances();
  void LoadSprites();
  void LoadSpritesFromMap(int spriteStart, int spriteCount, int spriteIndex);

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

  absl::flat_hash_map<int, MapData> proto_map_data_;

  std::vector<std::vector<uint8_t>> mapDatap1 =
      std::vector<std::vector<uint8_t>>(kNumOverworldMaps);
  std::vector<std::vector<uint8_t>> mapDatap2 =
      std::vector<std::vector<uint8_t>>(kNumOverworldMaps);

  std::vector<int> mapPointers1id = std::vector<int>(kNumOverworldMaps);
  std::vector<int> mapPointers2id = std::vector<int>(kNumOverworldMaps);

  std::vector<int> mapPointers1 = std::vector<int>(kNumOverworldMaps);
  std::vector<int> mapPointers2 = std::vector<int>(kNumOverworldMaps);
};

}  // namespace zelda3
}  // namespace app
}  // namespace yaze

#endif