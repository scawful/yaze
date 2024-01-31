#ifndef YAZE_APP_DATA_OVERWORLD_H
#define YAZE_APP_DATA_OVERWORLD_H

#include <SDL.h>

#include <future>
#include <memory>
#include <vector>

#include "absl/container/flat_hash_map.h"
#include "absl/status/status.h"
#include "app/core/common.h"
#include "app/core/constants.h"
#include "app/gfx/bitmap.h"
#include "app/gfx/snes_tile.h"
#include "app/rom.h"
#include "app/zelda3/common.h"
#include "app/zelda3/overworld_map.h"
#include "app/zelda3/sprite/sprite.h"

namespace yaze {
namespace app {
namespace zelda3 {

// List of secret item names
const std::vector<std::string> kSecretItemNames = {
    "Nothing",        // 0
    "Green Rupee",    // 1
    "Rock hoarder",   // 2
    "Bee",            // 3
    "Health pack",    // 4
    "Bomb",           // 5
    "Heart ",         // 6
    "Blue Rupee",     // 7
    "Key",            // 8
    "Arrow",          // 9
    "Bomb",           // 10
    "Heart",          // 11
    "Magic",          // 12
    "Full Magic",     // 13
    "Cucco",          // 14
    "Green Soldier",  // 15
    "Bush Stal",      // 16
    "Blue Soldier",   // 17
    "Landmine",       // 18
    "Heart",          // 19
    "Fairy",          // 20
    "Heart",          // 21
    "Nothing ",       // 22
    "Hole",           // 23
    "Warp",           // 24
    "Staircase",      // 25
    "Bombable",       // 26
    "Switch"          // 27
};

constexpr int overworldItemsPointers = 0xDC2F9;
constexpr int overworldItemsAddress = 0xDC8B9;  // 1BC2F9
constexpr int overworldItemsBank = 0xDC8BF;
constexpr int overworldItemsEndData = 0xDC89C;  // 0DC89E

class OverworldItem : public OverworldEntity {
 public:
  bool bg2 = false;
  uint8_t game_x;
  uint8_t game_y;
  uint8_t id;
  uint16_t room_map_id;
  int unique_id = 0;
  bool deleted = false;
  OverworldItem() = default;

  OverworldItem(uint8_t id, uint16_t room_map_id, int x, int y, bool bg2) {
    this->id = id;
    this->x_ = x;
    this->y_ = y;
    this->bg2 = bg2;
    this->room_map_id = room_map_id;
    this->map_id_ = room_map_id;
    this->entity_id_ = id;
    this->type_ = kItem;

    int map_x = room_map_id - ((room_map_id / 8) * 8);
    int map_y = room_map_id / 8;

    this->game_x = static_cast<uint8_t>(std::abs(x - (map_x * 512)) / 16);
    this->game_y = static_cast<uint8_t>(std::abs(y - (map_y * 512)) / 16);
    // this->unique_id = ROM.unique_item_id++;
  }

  void UpdateMapProperties(int16_t room_map_id) override {
    this->room_map_id = static_cast<uint16_t>(room_map_id);

    if (room_map_id >= 64) {
      room_map_id -= 64;
    }

    int map_x = room_map_id - ((room_map_id / 8) * 8);
    int map_y = room_map_id / 8;

    this->game_x =
        static_cast<uint8_t>(std::abs(this->x_ - (map_x * 512)) / 16);
    this->game_y =
        static_cast<uint8_t>(std::abs(this->y_ - (map_y * 512)) / 16);

    std::cout << "Item:      " << std::hex << std::setw(2) << std::setfill('0')
              << static_cast<int>(this->id) << " MapId: " << std::hex
              << std::setw(2) << std::setfill('0')
              << static_cast<int>(this->room_map_id)
              << " X: " << static_cast<int>(this->game_x)
              << " Y: " << static_cast<int>(this->game_y) << std::endl;
  }

  OverworldItem Copy() {
    return OverworldItem(this->id, this->room_map_id, this->x_, this->y_,
                         this->bg2);
  }
};

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

class OverworldExit : public OverworldEntity {
 public:
  ushort y_scroll_;
  ushort x_scroll_;
  uchar y_player_;
  uchar x_player_;
  uchar y_camera_;
  uchar x_camera_;
  uchar scroll_mod_y_;
  uchar scroll_mod_x_;
  ushort door_type_1_;
  ushort door_type_2_;
  ushort room_id_;
  ushort map_pos_;  // Position in the vram
  uchar entrance_id_;
  uchar area_x_;
  uchar area_y_;
  bool is_hole_ = false;
  bool deleted_ = false;
  bool is_automatic_ = false;
  bool large_map_ = false;

  OverworldExit() = default;
  OverworldExit(ushort room_id, uchar map_id, ushort vram_location,
                ushort y_scroll, ushort x_scroll, ushort player_y,
                ushort player_x, ushort camera_y, ushort camera_x,
                uchar scroll_mod_y, uchar scroll_mod_x, ushort door_type_1,
                ushort door_type_2, bool deleted = false)
      : map_pos_(vram_location),
        entrance_id_(0),
        area_x_(0),
        area_y_(0),
        is_hole_(false),
        room_id_(room_id),
        y_scroll_(y_scroll),
        x_scroll_(x_scroll),
        y_player_(player_y),
        x_player_(player_x),
        y_camera_(camera_y),
        x_camera_(camera_x),
        scroll_mod_y_(scroll_mod_y),
        scroll_mod_x_(scroll_mod_x),
        door_type_1_(door_type_1),
        door_type_2_(door_type_2),
        deleted_(deleted) {
    // Initialize entity variables
    this->x_ = player_x;
    this->y_ = player_y;
    this->map_id_ = map_id;
    this->type_ = kExit;

    int mapX = (map_id_ - ((map_id_ / 8) * 8));
    int mapY = (map_id_ / 8);

    area_x_ = (uchar)((std::abs(x_ - (mapX * 512)) / 16));
    area_y_ = (uchar)((std::abs(y_ - (mapY * 512)) / 16));

    if (door_type_1 != 0) {
      int p = (door_type_1 & 0x7FFF) >> 1;
      entrance_id_ = (uchar)(p % 64);
      area_y_ = (uchar)(p >> 6);
    }

    if (door_type_2 != 0) {
      int p = (door_type_2 & 0x7FFF) >> 1;
      entrance_id_ = (uchar)(p % 64);
      area_y_ = (uchar)(p >> 6);
    }

    if (map_id_ >= 64) {
      map_id_ -= 64;
    }

    mapX = (map_id_ - ((map_id_ / 8) * 8));
    mapY = (map_id_ / 8);

    area_x_ = (uchar)((std::abs(x_ - (mapX * 512)) / 16));
    area_y_ = (uchar)((std::abs(y_ - (mapY * 512)) / 16));

    map_pos_ = (ushort)((((area_y_) << 6) | (area_x_ & 0x3F)) << 1);
  }

  // Overworld overworld
  void UpdateMapProperties(short map_id) override {
    map_id_ = map_id;

    int large = 256;
    int mapid = map_id;

    if (map_id < 128) {
      large = large_map_ ? 768 : 256;
      // if (overworld.overworld_map(map_id)->Parent() != map_id) {
      //   mapid = overworld.overworld_map(map_id)->Parent();
      // }
    }

    int mapX = map_id - ((map_id / 8) * 8);
    int mapY = map_id / 8;

    area_x_ = (uchar)((std::abs(x_ - (mapX * 512)) / 16));
    area_y_ = (uchar)((std::abs(y_ - (mapY * 512)) / 16));

    if (map_id >= 64) {
      map_id -= 64;
    }

    int mapx = (map_id & 7) << 9;
    int mapy = (map_id & 56) << 6;

    if (is_automatic_) {
      x_ = x_ - 120;
      y_ = y_ - 80;

      if (x_ < mapx) {
        x_ = mapx;
      }

      if (y_ < mapy) {
        y_ = mapy;
      }

      if (x_ > mapx + large) {
        x_ = mapx + large;
      }

      if (y_ > mapy + large + 32) {
        y_ = mapy + large + 32;
      }

      x_camera_ = x_player_ + 0x07;
      y_camera_ = y_player_ + 0x1F;

      if (x_camera_ < mapx + 127) {
        x_camera_ = mapx + 127;
      }

      if (y_camera_ < mapy + 111) {
        y_camera_ = mapy + 111;
      }

      if (x_camera_ > mapx + 127 + large) {
        x_camera_ = mapx + 127 + large;
      }

      if (y_camera_ > mapy + 143 + large) {
        y_camera_ = mapy + 143 + large;
      }
    }

    short vram_x_scroll = (short)(x_ - mapx);
    short vram_y_scroll = (short)(y_ - mapy);

    map_pos_ = (ushort)(((vram_y_scroll & 0xFFF0) << 3) |
                        ((vram_x_scroll & 0xFFF0) >> 3));

    std::cout << "Exit:      " << room_id_ << " MapId: " << std::hex << mapid
              << " X: " << static_cast<int>(area_x_)
              << " Y: " << static_cast<int>(area_y_) << std::endl;
  }
};

constexpr int OWEntranceMap = 0xDB96F;
constexpr int OWEntrancePos = 0xDBA71;
constexpr int OWEntranceEntranceId = 0xDBB73;

// (0x13 entries, 2 bytes each) modified(less 0x400)
// map16 coordinates for each hole
constexpr int OWHolePos = 0xDB800;

// (0x13 entries, 2 bytes each) corresponding
// area numbers for each hole
constexpr int OWHoleArea = 0xDB826;

//(0x13 entries, 1 byte each)  corresponding entrance numbers
constexpr int OWHoleEntrance = 0xDB84C;

class OverworldEntrance : public OverworldEntity {
 public:
  ushort map_pos_;
  uchar entrance_id_;
  uchar area_x_;
  uchar area_y_;
  bool is_hole_ = false;
  bool deleted = false;

  OverworldEntrance() = default;
  OverworldEntrance(int x, int y, uchar entrance_id, short map_id,
                    ushort map_pos, bool hole)
      : map_pos_(map_pos), entrance_id_(entrance_id), is_hole_(hole) {
    x_ = x;
    y_ = y;
    map_id_ = map_id;
    entity_id_ = entrance_id;
    type_ = kEntrance;

    int mapX = (map_id_ - ((map_id_ / 8) * 8));
    int mapY = (map_id_ / 8);
    area_x_ = (uchar)((std::abs(x - (mapX * 512)) / 16));
    area_y_ = (uchar)((std::abs(y - (mapY * 512)) / 16));
  }

  auto Copy() {
    return new OverworldEntrance(x_, y_, entrance_id_, map_id_, map_pos_,
                                 is_hole_);
  }

  void UpdateMapProperties(short map_id) override {
    map_id_ = map_id;

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

constexpr int kCompressedAllMap32PointersHigh = 0x1794D;
constexpr int kCompressedAllMap32PointersLow = 0x17B2D;
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

// constexpr int OverworldScreenTileMapChangeByScreen = 0x12634;
constexpr int OverworldScreenTileMapChangeByScreen1 = 0x12634;
constexpr int OverworldScreenTileMapChangeByScreen2 = 0x126B4;
constexpr int OverworldScreenTileMapChangeByScreen3 = 0x12734;
constexpr int OverworldScreenTileMapChangeByScreen4 = 0x127B4;

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

class Overworld : public SharedROM, public core::ExperimentFlags {
 public:
  OWBlockset &GetMapTiles(int world_type);
  absl::Status Load(ROM &rom);
  absl::Status LoadOverworldMaps();
  void LoadTileTypes();
  void LoadEntrances();

  absl::Status LoadExits();
  absl::Status LoadItems();
  absl::Status LoadSprites();
  absl::Status LoadSpritesFromMap(int spriteStart, int spriteCount,
                                  int spriteIndex);

  absl::Status Save(ROM &rom);
  absl::Status SaveOverworldMaps();
  absl::Status SaveLargeMaps();
  absl::Status SaveEntrances();
  absl::Status SaveExits();
  absl::Status SaveItems();

  absl::Status CreateTile32Tilemap();
  absl::Status SaveMap16Tiles();
  absl::Status SaveMap32Tiles();

  absl::Status SaveMapProperties();
  absl::Status LoadPrototype(ROM &rom_, const std::string &tilemap_filename);

  int current_world_ = 0;
  int GetTileFromPosition(ImVec2 position) const {
    if (current_world_ == 0) {
      return map_tiles_.light_world[position.x][position.y];
    } else if (current_world_ == 1) {
      return map_tiles_.dark_world[position.x][position.y];
    } else {
      return map_tiles_.special_world[position.x][position.y];
    }
  }

  auto overworld_maps() const { return overworld_maps_; }
  auto overworld_map(int i) const { return &overworld_maps_[i]; }
  auto mutable_overworld_map(int i) { return &overworld_maps_[i]; }
  auto exits() const { return &all_exits_; }
  auto mutable_exits() { return &all_exits_; }
  std::vector<gfx::Tile16> tiles16() const { return tiles16_; }

  auto Sprites(int state) const { return all_sprites_[state]; }
  auto mutable_sprites(int state) { return &all_sprites_[state]; }
  auto AreaGraphics() const {
    return overworld_maps_[current_map_].AreaGraphics();
  }
  auto &Entrances() { return all_entrances_; }
  auto mutable_entrances() { return &all_entrances_; }
  auto &holes() { return all_holes_; }
  auto mutable_holes() { return &all_holes_; }
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
  auto is_loaded() const { return is_loaded_; }
  void set_current_map(int i) { current_map_ = i; }

  auto map_tiles() const { return map_tiles_; }
  auto mutable_map_tiles() { return &map_tiles_; }
  auto all_items() const { return all_items_; }
  auto mutable_all_items() { return &all_items_; }
  auto &ref_all_items() { return all_items_; }
  auto all_tiles_types() const { return all_tiles_types_; }
  auto mutable_all_tiles_types() { return &all_tiles_types_; }

 private:
  enum Dimension {
    map32TilesTL = 0,
    map32TilesTR = 1,
    map32TilesBL = 2,
    map32TilesBR = 3
  };

  void FetchLargeMaps();
  void AssembleMap32Tiles();
  void AssembleMap16Tiles();
  void AssignWorldTiles(int x, int y, int sx, int sy, int tpos,
                        OWBlockset &world);
  void OrganizeMapTiles(Bytes &bytes, Bytes &bytes2, int i, int sx, int sy,
                        int &ttpos);
  absl::Status DecompressAllMapTiles();

  absl::Status DecompressProtoMapTiles(const std::string &filename);

  bool is_loaded_ = false;

  int game_state_ = 0;
  int current_map_ = 0;
  uchar map_parent_[160];

  ROM rom_;
  OWMapTiles map_tiles_;

  uint8_t all_tiles_types_[0x200];

  std::vector<gfx::Tile16> tiles16_;
  std::vector<gfx::Tile32> tiles32_;
  std::vector<ushort> tiles32_list_;
  std::vector<gfx::Tile32> tiles32_unique_;
  std::vector<OverworldMap> overworld_maps_;
  std::vector<OverworldEntrance> all_entrances_;
  std::vector<OverworldEntrance> all_holes_;
  std::vector<OverworldExit> all_exits_;
  std::vector<OverworldItem> all_items_;
  std::vector<std::vector<Sprite>> all_sprites_;

  std::vector<std::vector<uint8_t>> map_data_p1 =
      std::vector<std::vector<uint8_t>>(kNumOverworldMaps);
  std::vector<std::vector<uint8_t>> map_data_p2 =
      std::vector<std::vector<uint8_t>>(kNumOverworldMaps);

  std::vector<int> map_pointers1_id = std::vector<int>(kNumOverworldMaps);
  std::vector<int> map_pointers2_id = std::vector<int>(kNumOverworldMaps);

  std::vector<int> map_pointers1 = std::vector<int>(kNumOverworldMaps);
  std::vector<int> map_pointers2 = std::vector<int>(kNumOverworldMaps);

  std::vector<absl::flat_hash_map<uint16_t, int>> usage_stats_;
  absl::flat_hash_map<int, MapData> proto_map_data_;
};

}  // namespace zelda3
}  // namespace app
}  // namespace yaze

#endif