#ifndef YAZE_APP_DATA_OVERWORLD_H
#define YAZE_APP_DATA_OVERWORLD_H

#include <vector>

#include "absl/container/flat_hash_map.h"
#include "absl/status/status.h"
#include "app/core/common.h"
#include "app/core/constants.h"
#include "app/gfx/snes_tile.h"
#include "app/rom.h"
#include "app/zelda3/common.h"
#include "app/zelda3/overworld/overworld_entrance.h"
#include "app/zelda3/overworld/overworld_exit.h"
#include "app/zelda3/overworld/overworld_item.h"
#include "app/zelda3/overworld/overworld_map.h"
#include "app/zelda3/sprite/sprite.h"

namespace yaze {
namespace zelda3 {

constexpr int GravesYTilePos = 0x49968;          // short (0x0F entries)
constexpr int GravesXTilePos = 0x49986;          // short (0x0F entries)
constexpr int GravesTilemapPos = 0x499A4;        // short (0x0F entries)
constexpr int GravesGFX = 0x499C2;               // short (0x0F entries)
constexpr int GravesXPos = 0x4994A;              // short (0x0F entries)
constexpr int GravesYLine = 0x4993A;             // short (0x08 entries)
constexpr int GravesCountOnY = 0x499E0;          // Byte 0x09 entries
constexpr int GraveLinkSpecialHole = 0x46DD9;    // short
constexpr int GraveLinkSpecialStairs = 0x46DE0;  // short

constexpr int kOverworldMapPaletteIds = 0x7D1C;
constexpr int kOverworldSpritePaletteIds = 0x7B41;
constexpr int kOverworldSpritePaletteGroup = 0x75580;
constexpr int kOverworldSpriteset = 0x7A41;
constexpr int kOverworldSpecialGfxGroup = 0x16821;
constexpr int kOverworldSpecialPalGroup = 0x16831;
constexpr int kOverworldSpritesBeginning = 0x4C881;
constexpr int kOverworldSpritesAgahnim = 0x4CA21;
constexpr int kOverworldSpritesZelda = 0x4C901;

constexpr int kAreaGfxIdPtr = 0x7C9C;
constexpr int kOverworldMessageIds = 0x3F51D;

constexpr int kOverworldMusicBeginning = 0x14303;
constexpr int kOverworldMusicZelda = 0x14303 + 0x40;
constexpr int kOverworldMusicMasterSword = 0x14303 + 0x80;
constexpr int kOverworldMusicAgahnim = 0x14303 + 0xC0;
constexpr int kOverworldMusicDarkWorld = 0x14403;
constexpr int kOverworldEntranceAllowedTilesLeft = 0xDB8C1;
constexpr int kOverworldEntranceAllowedTilesRight = 0xDB917;

// 0x00 = small maps, 0x20 = large maps
constexpr int kOverworldMapSize = 0x12844;

// 0x01 = small maps, 0x03 = large maps
constexpr int kOverworldMapSizeHighByte = 0x12884;

// relative to the WORLD + 0x200 per map
// large map that are not == parent id = same position as their parent!
// eg for X position small maps :
// 0000, 0200, 0400, 0600, 0800, 0A00, 0C00, 0E00
// all Large map would be :
// 0000, 0000, 0400, 0400, 0800, 0800, 0C00, 0C00
constexpr int kOverworldMapParentId = 0x125EC;
constexpr int kOverworldTransitionPositionY = 0x128C4;
constexpr int kOverworldTransitionPositionX = 0x12944;
constexpr int kOverworldScreenSize = 0x1788D;
constexpr int kOverworldScreenSizeForLoading = 0x4C635;

constexpr int kOverworldScreenTileMapChangeByScreen1 = 0x12634;
constexpr int kOverworldScreenTileMapChangeByScreen2 = 0x126B4;
constexpr int kOverworldScreenTileMapChangeByScreen3 = 0x12734;
constexpr int kOverworldScreenTileMapChangeByScreen4 = 0x127B4;

constexpr int kOverworldMapDataOverflow = 0x130000;

constexpr int kTransitionTargetNorth = 0x13EE2;
constexpr int kTransitionTargetWest = 0x13F62;
constexpr int overworldCustomMosaicASM = 0x1301D0;
constexpr int overworldCustomMosaicArray = 0x1301F0;

// Expanded tile16 and tile32
constexpr int kMap16TilesExpanded = 0x1E8000;
constexpr int kMap32TileTRExpanded = 0x020000;
constexpr int kMap32TileBLExpanded = 0x1F0000;
constexpr int kMap32TileBRExpanded = 0x1F8000;
constexpr int kMap32TileCountExpanded = 0x0067E0;
constexpr int kMap32ExpandedFlagPos = 0x01772E;              // 0x04
constexpr int kMap16ExpandedFlagPos = 0x02FD28;              // 0x0F
constexpr int kOverworldEntranceExpandedFlagPos = 0x0DB895;  // 0xB8

constexpr int kOverworldCompressedMapPos = 0x058000;
constexpr int kOverworldCompressedOverflowPos = 0x137FFF;

constexpr int kNumTileTypes = 0x200;
constexpr int kMap16Tiles = 0x78000;
constexpr int kNumOverworldMaps = 160;
constexpr int kNumTile16Individual = 4096;
constexpr int Map32PerScreen = 256;
constexpr int NumberOfMap16 = 3752;    // 4096
constexpr int NumberOfMap16Ex = 4096;  // 4096
constexpr int LimitOfMap32 = 8864;
constexpr int NumberOfOWSprites = 352;
constexpr int NumberOfMap32 = Map32PerScreen * kNumOverworldMaps;

/**
 * @brief Represents the full Overworld data, light and dark world.
 *
 * This class is responsible for loading and saving the overworld data,
 * as well as creating the tilesets and tilemaps for the overworld.
 */
class Overworld : public SharedRom {
 public:
  absl::Status Load(Rom &rom);
  absl::Status LoadOverworldMaps();
  void LoadTileTypes();
  void LoadEntrances();

  absl::Status LoadExits();
  absl::Status LoadItems();
  absl::Status LoadSprites();
  absl::Status LoadSpritesFromMap(int spriteStart, int spriteCount,
                                  int spriteIndex);

  absl::Status Save(Rom &rom);
  absl::Status SaveOverworldMaps();
  absl::Status SaveLargeMaps();
  absl::Status SaveEntrances();
  absl::Status SaveExits();
  absl::Status SaveItems();

  absl::Status CreateTile32Tilemap();
  absl::Status SaveMap16Expanded();
  absl::Status SaveMap16Tiles();
  absl::Status SaveMap32Expanded();
  absl::Status SaveMap32Tiles();

  absl::Status SaveMapProperties();

  void Destroy() {
    for (auto &map : overworld_maps_) {
      map.Destroy();
    }
    overworld_maps_.clear();
    all_entrances_.clear();
    all_exits_.clear();
    all_items_.clear();
    all_sprites_.clear();
    is_loaded_ = false;
  }

  int GetTileFromPosition(ImVec2 position) const {
    if (current_world_ == 0) {
      return map_tiles_.light_world[position.x][position.y];
    } else if (current_world_ == 1) {
      return map_tiles_.dark_world[position.x][position.y];
    } else {
      return map_tiles_.special_world[position.x][position.y];
    }
  }

  OverworldBlockset &GetMapTiles(int world_type) {
    switch (world_type) {
      case 0:
        return map_tiles_.light_world;
      case 1:
        return map_tiles_.dark_world;
      case 2:
        return map_tiles_.special_world;
      default:
        return map_tiles_.light_world;
    }
  }

  auto overworld_maps() const { return overworld_maps_; }
  auto overworld_map(int i) const { return &overworld_maps_[i]; }
  auto mutable_overworld_map(int i) { return &overworld_maps_[i]; }
  auto exits() const { return &all_exits_; }
  auto mutable_exits() { return &all_exits_; }
  std::vector<gfx::Tile16> tiles16() const { return tiles16_; }
  auto mutable_tiles16() { return &tiles16_; }
  auto sprites(int state) const { return all_sprites_[state]; }
  auto mutable_sprites(int state) { return &all_sprites_[state]; }
  auto current_graphics() const {
    return overworld_maps_[current_map_].current_graphics();
  }
  auto &entrances() { return all_entrances_; }
  auto mutable_entrances() { return &all_entrances_; }
  auto &holes() { return all_holes_; }
  auto mutable_holes() { return &all_holes_; }
  auto deleted_entrances() const { return deleted_entrances_; }
  auto mutable_deleted_entrances() { return &deleted_entrances_; }
  auto current_area_palette() const {
    return overworld_maps_[current_map_].current_palette();
  }
  auto current_map_bitmap_data() const {
    return overworld_maps_[current_map_].bitmap_data();
  }
  auto tile16_blockset_data() const {
    return overworld_maps_[current_map_].current_tile16_blockset();
  }
  auto is_loaded() const { return is_loaded_; }
  void set_current_map(int i) { current_map_ = i; }
  auto map_tiles() const { return map_tiles_; }
  auto mutable_map_tiles() { return &map_tiles_; }
  auto all_items() const { return all_items_; }
  auto mutable_all_items() { return &all_items_; }
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
  absl::StatusOr<uint16_t> GetTile16ForTile32(int index, int quadrant,
                                              int dimension,
                                              const uint32_t *map32address);
  absl::Status AssembleMap32Tiles();
  void AssembleMap16Tiles();
  void AssignWorldTiles(int x, int y, int sx, int sy, int tpos,
                        OverworldBlockset &world);
  void OrganizeMapTiles(std::vector<uint8_t> &bytes,
                        std::vector<uint8_t> &bytes2, int i, int sx, int sy,
                        int &ttpos);
  absl::Status DecompressAllMapTiles();

  Rom rom_;

  bool is_loaded_ = false;
  bool expanded_tile16_ = false;
  bool expanded_tile32_ = false;
  bool expanded_entrances_ = false;

  int game_state_ = 0;
  int current_map_ = 0;
  int current_world_ = 0;

  OverworldMapTiles map_tiles_;

  std::array<uint8_t, kNumOverworldMaps> map_parent_;
  std::array<uint8_t, kNumTileTypes> all_tiles_types_;
  std::vector<gfx::Tile16> tiles16_;
  std::vector<gfx::Tile32> tiles32_;
  std::vector<uint16_t> tiles32_list_;
  std::vector<gfx::Tile32> tiles32_unique_;
  std::vector<OverworldMap> overworld_maps_;
  std::vector<OverworldEntrance> all_entrances_;
  std::vector<OverworldEntrance> all_holes_;
  std::vector<OverworldExit> all_exits_;
  std::vector<OverworldItem> all_items_;
  std::vector<std::vector<Sprite>> all_sprites_;
  std::vector<uint64_t> deleted_entrances_;

  std::vector<std::vector<uint8_t>> map_data_p1 =
      std::vector<std::vector<uint8_t>>(kNumOverworldMaps);
  std::vector<std::vector<uint8_t>> map_data_p2 =
      std::vector<std::vector<uint8_t>>(kNumOverworldMaps);

  std::vector<int> map_pointers1_id = std::vector<int>(kNumOverworldMaps);
  std::vector<int> map_pointers2_id = std::vector<int>(kNumOverworldMaps);

  std::vector<int> map_pointers1 = std::vector<int>(kNumOverworldMaps);
  std::vector<int> map_pointers2 = std::vector<int>(kNumOverworldMaps);

  std::vector<absl::flat_hash_map<uint16_t, int>> usage_stats_;
};

}  // namespace zelda3
}  // namespace yaze

#endif
