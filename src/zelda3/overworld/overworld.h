#ifndef YAZE_APP_DATA_OVERWORLD_H
#define YAZE_APP_DATA_OVERWORLD_H

#include <array>
#include <cstdint>
#include <deque>
#include <mutex>
#include <unordered_map>
#include <vector>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "app/gfx/types/snes_tile.h"
#include "imgui.h"
#include "rom/rom.h"
#include "zelda3/common.h"
#include "zelda3/game_data.h"
#include "zelda3/overworld/diggable_tiles.h"
#include "zelda3/overworld/overworld_entrance.h"
#include "zelda3/overworld/overworld_exit.h"
#include "zelda3/overworld/overworld_item.h"
#include "zelda3/overworld/overworld_map.h"
#include "zelda3/sprite/sprite.h"

namespace yaze::zelda3 {

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
constexpr int kMap32ExpandedFlagPos = 0x01772E;  // 0x04
constexpr int kMap16ExpandedFlagPos = 0x02FD28;  // 0x0F

constexpr int overworldSpritesBeginingExpanded = 0x141438;
constexpr int overworldSpritesZeldaExpanded = 0x141578;
constexpr int overworldSpritesAgahnimExpanded = 0x1416B8;
constexpr int overworldSpritesDataStartExpanded = 0x04C881;

constexpr int overworldSpecialSpriteGFXGroupExpandedTemp = 0x0166E1;
constexpr int overworldSpecialSpritePaletteExpandedTemp = 0x016701;

constexpr int ExpandedOverlaySpace = 0x120000;

// Expanded pointer table markers for tail map support (maps 0xA0-0xBF)
// Set by TailMapExpansion.asm patch after ZSCustomOverworld v3
constexpr int kExpandedPtrTableMarker = 0x1423FF;   // Location of marker byte
constexpr uint8_t kExpandedPtrTableMagic = 0xEA;    // Marker value when applied
constexpr int kExpandedPtrTableHigh = 0x142400;     // New high table location
constexpr int kExpandedPtrTableLow = 0x142640;      // New low table location
constexpr int kExpandedMapCount = 192;              // 0x00-0xBF

constexpr int overworldTilesType = 0x071459;
constexpr int overworldMessages = 0x03F51D;
constexpr int overworldMessagesExpanded = 0x1417F8;

constexpr int kOverworldCompressedMapPos = 0x058000;
constexpr int kOverworldCompressedOverflowPos = 0x137FFF;

constexpr int kNumTileTypes = 0x200;
constexpr int kMap16Tiles = 0x78000;

constexpr int kNumTile16Individual = 4096;
constexpr int Map32PerScreen = 256;
constexpr int NumberOfMap16 = 3752;    // 4096
constexpr int NumberOfMap16Ex = 4096;  // 4096
constexpr int LimitOfMap32 = 8864;
constexpr int NumberOfOWSprites = 352;
constexpr int NumberOfMap32 = Map32PerScreen * kNumOverworldMaps;
constexpr int kNumMapsPerWorld = 0x40;

/**
 * @brief Represents the full Overworld data, light and dark world.
 *
 * This class is responsible for loading and saving the overworld data,
 * as well as creating the tilesets and tilemaps for the overworld.
 */
class Overworld {
 public:
  Overworld(Rom* rom, GameData* game_data = nullptr) 
      : rom_(rom), game_data_(game_data) {}

  void set_game_data(GameData* game_data) { game_data_ = game_data; }
  
  // Helper to get version constants from game_data or default to US
  zelda3_version_pointers version_constants() const {
    return kVersionConstantsMap.at(game_data_ ? game_data_->version : zelda3_version::US);
  }
  
  absl::Status Load(Rom* rom);
  absl::Status LoadOverworldMaps();
  void LoadTileTypes();

  absl::Status LoadSprites();
  absl::Status LoadSpritesFromMap(int sprite_start, int sprite_count,
                                  int sprite_index);

  /**
   * @brief Build a map on-demand if it hasn't been built yet
   *
   * This method checks if the specified map needs to be built and builds it
   * if necessary. Used for lazy loading optimization.
   */
  absl::Status EnsureMapBuilt(int map_index);

  /**
   * @brief Compute hash of graphics configuration for cache lookup
   */
  uint64_t ComputeGraphicsConfigHash(int map_index);

  /**
   * @brief Try to get cached tileset data for a graphics configuration
   * @return nullptr if not cached, pointer to cached data if available
   */
  const std::vector<uint8_t>* GetCachedTileset(uint64_t config_hash);

  /**
   * @brief Cache tileset data for future reuse
   */
  void CacheTileset(uint64_t config_hash, const std::vector<uint8_t>& tileset);

  absl::Status Save(Rom* rom);
  absl::Status SaveOverworldMaps();
  absl::Status SaveLargeMaps();
  absl::Status SaveLargeMapsExpanded();
  absl::Status SaveSmallAreaTransitions(
      int i, int parent_x_pos, int parent_y_pos, int transition_target_north,
      int transition_target_west, int transition_pos_x, int transition_pos_y,
      int screen_change_1, int screen_change_2, int screen_change_3,
      int screen_change_4);
  absl::Status SaveLargeAreaTransitions(
      int i, int parent_x_pos, int parent_y_pos, int transition_target_north,
      int transition_target_west, int transition_pos_x, int transition_pos_y,
      int screen_change_1, int screen_change_2, int screen_change_3,
      int screen_change_4);
  absl::Status SaveWideAreaTransitions(
      int i, int parent_x_pos, int parent_y_pos, int transition_target_north,
      int transition_target_west, int transition_pos_x, int transition_pos_y,
      int screen_change_1, int screen_change_2, int screen_change_3,
      int screen_change_4);
  absl::Status SaveTallAreaTransitions(
      int i, int parent_x_pos, int parent_y_pos, int transition_target_north,
      int transition_target_west, int transition_pos_x, int transition_pos_y,
      int screen_change_1, int screen_change_2, int screen_change_3,
      int screen_change_4);
  absl::Status SaveEntrances();
  absl::Status SaveExits();
  absl::Status SaveItems();
  absl::Status SaveMapOverlays();
  absl::Status SaveOverworldTilesType();
  absl::Status SaveCustomOverworldASM(bool enable_bg_color,
                                      bool enable_main_palette,
                                      bool enable_mosaic,
                                      bool enable_gfx_groups,
                                      bool enable_subscreen_overlay,
                                      bool enable_animated);
  absl::Status SaveAreaSpecificBGColors();

  absl::Status CreateTile32Tilemap();
  absl::Status SaveMap16Expanded();
  absl::Status SaveMap16Tiles();
  absl::Status SaveMap32Expanded();
  absl::Status SaveMap32Tiles();

  absl::Status SaveMapProperties();
  absl::Status SaveMusic();
  absl::Status SaveAreaSizes();
  void AssignMapSizes(std::vector<OverworldMap>& maps);

  /**
   * @brief Configure a multi-area map structure (Large/Wide/Tall)
   * @param parent_index The parent map index
   * @param size The area size to configure
   * @return Status of the configuration
   *
   * Properly sets up sibling relationships and updates ROM data for v3+.
   */
  absl::Status ConfigureMultiAreaMap(int parent_index, AreaSizeEnum size);

  auto rom() const { return rom_; }
  auto mutable_rom() { return rom_; }

  /**
   * @brief Check if the ROM has expanded pointer tables for tail maps
   *
   * Returns true if the TailMapExpansion.asm patch has been applied,
   * enabling support for maps 0xA0-0xBF. Detection is based on the
   * marker byte at 0x1423FF being 0xEA.
   */
  bool HasExpandedPointerTables() const {
    if (!rom_ || kExpandedPtrTableMarker >= rom_->size()) {
      return false;
    }
    return rom_->data()[kExpandedPtrTableMarker] == kExpandedPtrTableMagic;
  }

  void Destroy() {
    for (auto& map : overworld_maps_) {
      map.Destroy();
    }
    overworld_maps_.clear();
    all_entrances_.clear();
    all_exits_.clear();
    all_items_.clear();
    for (auto& sprites : all_sprites_) {
      sprites.clear();
    }
    tiles16_.clear();
    tiles32_.clear();
    tiles32_unique_.clear();
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

  OverworldBlockset& GetMapTiles(int world_type) {
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
  auto overworld_map(int i) const {
    if (i < 0 || i >= static_cast<int>(overworld_maps_.size())) {
      return static_cast<const OverworldMap*>(nullptr);
    }
    return &overworld_maps_[i];
  }
  auto mutable_overworld_map(int i) {
    if (i < 0 || i >= static_cast<int>(overworld_maps_.size())) {
      return static_cast<OverworldMap*>(nullptr);
    }
    return &overworld_maps_[i];
  }
  auto exits() const { return &all_exits_; }
  auto mutable_exits() { return &all_exits_; }
  std::vector<gfx::Tile16> tiles16() const { return tiles16_; }
  auto tiles32_unique() const { return tiles32_unique_; }
  auto mutable_tiles16() { return &tiles16_; }
  auto sprites(int state) const {
    if (state < 0 || state >= 3) return std::vector<Sprite>{};
    return all_sprites_[state];
  }
  auto mutable_sprites(int state) {
    if (state < 0 || state >= 3) return static_cast<std::vector<Sprite>*>(nullptr);
    return &all_sprites_[state];
  }
  auto current_graphics() const {
    if (!is_current_map_valid()) return std::vector<uint8_t>{};
    return overworld_maps_[current_map_].current_graphics();
  }
  const std::vector<OverworldEntrance>& entrances() const {
    return all_entrances_;
  }
  auto& entrances() { return all_entrances_; }
  auto mutable_entrances() { return &all_entrances_; }
  const std::vector<OverworldEntrance>& holes() const { return all_holes_; }
  auto& holes() { return all_holes_; }
  auto mutable_holes() { return &all_holes_; }
  auto deleted_entrances() const { return deleted_entrances_; }
  auto mutable_deleted_entrances() { return &deleted_entrances_; }
  auto current_area_palette() const {
    if (!is_current_map_valid()) return gfx::SnesPalette{};
    return overworld_maps_[current_map_].current_palette();
  }
  auto current_map_bitmap_data() const {
    if (!is_current_map_valid()) return std::vector<uint8_t>{};
    return overworld_maps_[current_map_].bitmap_data();
  }
  auto tile16_blockset_data() const {
    if (!is_current_map_valid()) return std::vector<uint8_t>{};
    return overworld_maps_[current_map_].current_tile16_blockset();
  }

  bool is_current_map_valid() const {
    return current_map_ >= 0 && current_map_ < static_cast<int>(overworld_maps_.size());
  }
  auto is_loaded() const { return is_loaded_; }
  auto expanded_tile16() const { return expanded_tile16_; }
  auto expanded_tile32() const { return expanded_tile32_; }
  auto expanded_entrances() const { return expanded_entrances_; }
  int current_map_id() const { return current_map_; }
  int current_world() const { return current_world_; }
  void set_current_map(int i) { current_map_ = i; }
  void set_current_world(int world) { current_world_ = world; }
  uint16_t GetTile(int x, int y) const {
    if (current_world_ == 0) {
      return map_tiles_.light_world[y][x];
    } else if (current_world_ == 1) {
      return map_tiles_.dark_world[y][x];
    } else {
      return map_tiles_.special_world[y][x];
    }
  }
  void SetTile(int x, int y, uint16_t tile_id) {
    if (current_world_ == 0) {
      map_tiles_.light_world[y][x] = tile_id;
    } else if (current_world_ == 1) {
      map_tiles_.dark_world[y][x] = tile_id;
    } else {
      map_tiles_.special_world[y][x] = tile_id;
    }
  }
  auto map_tiles() const { return map_tiles_; }
  auto mutable_map_tiles() { return &map_tiles_; }
  auto all_items() const { return all_items_; }
  auto mutable_all_items() { return &all_items_; }
  auto all_tiles_types() const { return all_tiles_types_; }
  auto mutable_all_tiles_types() { return &all_tiles_types_; }
  auto all_sprites() const { return all_sprites_; }

  // Diggable tiles management
  const DiggableTiles& diggable_tiles() const { return diggable_tiles_; }
  DiggableTiles* mutable_diggable_tiles() { return &diggable_tiles_; }
  absl::Status LoadDiggableTiles();
  absl::Status SaveDiggableTiles();
  absl::Status AutoDetectDiggableTiles();

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
                                              const uint32_t* map32address);
  absl::Status AssembleMap32Tiles();
  absl::Status AssembleMap16Tiles();
  void AssignWorldTiles(int x, int y, int sx, int sy, int tpos,
                        OverworldBlockset& world);
  void FillBlankMapTiles(int map_index);
  OverworldBlockset& SelectWorldBlockset(int world_type);
  void OrganizeMapTiles(std::vector<uint8_t>& bytes,
                        std::vector<uint8_t>& bytes2, int i, int sx, int sy,
                        int& ttpos);
  absl::Status DecompressAllMapTilesParallel();

  Rom* rom_;
  GameData* game_data_ = nullptr;

  bool is_loaded_ = false;
  bool expanded_tile16_ = false;
  bool expanded_tile32_ = false;
  bool expanded_entrances_ = false;

  int game_state_ = 0;
  int current_map_ = 0;
  int current_world_ = 0;

  OverworldMapTiles map_tiles_;

  // Thread safety for parallel operations
  mutable std::mutex map_tiles_mutex_;

  // LRU cache for built maps to prevent memory exhaustion
  // Max ~20 maps = ~25MB of memory (1.25MB per map)
  static constexpr int kMaxBuiltMaps = 20;
  std::deque<int> built_map_lru_;

  // Graphics config cache for blockset reuse
  // Key: Hash of static_graphics array, Value: Precomputed current_gfx data
  // This avoids rebuilding the same tileset for maps with identical graphics
  struct GraphicsConfigCache {
    std::vector<uint8_t> current_gfx;  // 64KB tileset
    int reference_count = 0;
  };
  std::unordered_map<uint64_t, GraphicsConfigCache> gfx_config_cache_;
#ifdef __EMSCRIPTEN__
  // WASM: Smaller cache to reduce memory pressure (4 × 64KB = 256KB)
  static constexpr int kMaxCachedConfigs = 4;
#else
  // Native: Larger cache for better performance (8 × 64KB = 512KB)
  static constexpr int kMaxCachedConfigs = 8;
#endif

  std::vector<OverworldMap> overworld_maps_;
  std::vector<OverworldEntrance> all_entrances_;
  std::vector<OverworldEntrance> all_holes_;
  std::vector<OverworldExit> all_exits_;
  std::vector<OverworldItem> all_items_;

  std::vector<gfx::Tile16> tiles16_;
  std::vector<gfx::Tile32> tiles32_;
  std::vector<gfx::Tile32> tiles32_unique_;

  std::vector<uint16_t> tiles32_list_;
  std::vector<uint64_t> deleted_entrances_;

  std::array<uint8_t, kNumOverworldMaps> map_parent_ = {0};
  std::array<uint8_t, kNumTileTypes> all_tiles_types_ = {0};
  std::array<std::vector<Sprite>, 3> all_sprites_;
  DiggableTiles diggable_tiles_;
  std::array<std::vector<uint8_t>, kNumOverworldMaps> map_data_p1;
  std::array<std::vector<uint8_t>, kNumOverworldMaps> map_data_p2;
  std::array<int, kNumOverworldMaps> map_pointers1_id;
  std::array<int, kNumOverworldMaps> map_pointers2_id;
  std::array<int, kNumOverworldMaps> map_pointers1;
  std::array<int, kNumOverworldMaps> map_pointers2;
};

}  // namespace yaze::zelda3

#endif
