#ifndef YAZE_APP_ZELDA3_OVERWORLD_MAP_H
#define YAZE_APP_ZELDA3_OVERWORLD_MAP_H

#include <array>
#include <cstddef>
#include <cstdint>
#include <vector>

#include "absl/status/status.h"
#include "app/gfx/snes_palette.h"
#include "app/gfx/snes_tile.h"
#include "app/rom.h"

namespace yaze {
namespace zelda3 {

static constexpr int kTileOffsets[] = {0, 8, 4096, 4104};

// 1 byte, not 0 if enabled
constexpr int OverworldCustomASMHasBeenApplied = 0x140145;

// 2 bytes for each overworld area (0x140)
constexpr int OverworldCustomAreaSpecificBGPalette = 0x140000;

// 1 byte, not 0 if enabled
constexpr int OverworldCustomAreaSpecificBGEnabled = 0x140140;

// 1 byte for each overworld area (0xA0)
constexpr int OverworldCustomMainPaletteArray = 0x140160;
// 1 byte, not 0 if enabled
constexpr int OverworldCustomMainPaletteEnabled = 0x140141;

// 1 byte for each overworld area (0xA0)
constexpr int OverworldCustomMosaicArray = 0x140200;

// 1 byte, not 0 if enabled
constexpr int OverworldCustomMosaicEnabled = 0x140142;

// 1 byte for each overworld area (0xA0)
constexpr int OverworldCustomAnimatedGFXArray = 0x1402A0;

// 1 byte, not 0 if enabled
constexpr int OverworldCustomAnimatedGFXEnabled = 0x140143;

// 2 bytes for each overworld area (0x140)
constexpr int OverworldCustomSubscreenOverlayArray = 0x140340;

// 1 byte, not 0 if enabled
constexpr int OverworldCustomSubscreenOverlayEnabled = 0x140144;

// 8 bytes for each overworld area (0x500)
constexpr int OverworldCustomTileGFXGroupArray = 0x140480;

// 1 byte, not 0 if enabled
constexpr int OverworldCustomTileGFXGroupEnabled = 0x140148;

constexpr int kDarkWorldMapIdStart = 0x40;
constexpr int kSpecialWorldMapIdStart = 0x80;

/**
 * @brief Represents tile32 data for the overworld.
 */
using OverworldBlockset = std::vector<std::vector<uint16_t>>;

/**
 * @brief Overworld map tile32 data.
 */
typedef struct OverworldMapTiles {
  OverworldBlockset light_world;    // 64 maps
  OverworldBlockset dark_world;     // 64 maps
  OverworldBlockset special_world;  // 32 maps
} OverworldMapTiles;

/**
 * @brief Represents a single Overworld map screen.
 */
class OverworldMap : public gfx::GfxContext {
 public:
  OverworldMap() = default;
  OverworldMap(int index, Rom* rom);

  absl::Status BuildMap(int count, int game_state, int world,
                        std::vector<gfx::Tile16>& tiles16,
                        OverworldBlockset& world_blockset);

  void LoadAreaGraphics();
  absl::Status LoadPalette();
  absl::Status BuildTileset();
  absl::Status BuildTiles16Gfx(std::vector<gfx::Tile16>& tiles16, int count);
  absl::Status BuildBitmap(OverworldBlockset& world_blockset);

  void DrawAnimatedTiles();

  auto current_tile16_blockset() const { return current_blockset_; }
  auto current_graphics() const { return current_gfx_; }
  auto current_palette() const { return current_palette_; }
  auto bitmap_data() const { return bitmap_data_; }
  auto is_large_map() const { return large_map_; }
  auto is_initialized() const { return initialized_; }
  auto parent() const { return parent_; }
  auto mutable_mosaic() { return &mosaic_; }
  auto mutable_current_palette() { return &current_palette_; }

  auto area_graphics() const { return area_graphics_; }
  auto area_palette() const { return area_palette_; }
  auto sprite_graphics(int i) const { return sprite_graphics_[i]; }
  auto sprite_palette(int i) const { return sprite_palette_[i]; }
  auto message_id() const { return message_id_; }
  auto area_music(int i) const { return area_music_[i]; }
  auto static_graphics(int i) const { return static_graphics_[i]; }
  auto large_index() const { return large_index_; }

  auto mutable_current_graphics() { return &current_gfx_; }
  auto mutable_area_graphics() { return &area_graphics_; }
  auto mutable_area_palette() { return &area_palette_; }
  auto mutable_sprite_graphics(int i) { return &sprite_graphics_[i]; }
  auto mutable_sprite_palette(int i) { return &sprite_palette_[i]; }
  auto mutable_message_id() { return &message_id_; }
  auto mutable_area_music(int i) { return &area_music_[i]; }
  auto mutable_static_graphics(int i) { return &static_graphics_[i]; }

  auto set_area_graphics(uint8_t value) { area_graphics_ = value; }
  auto set_area_palette(uint8_t value) { area_palette_ = value; }
  auto set_sprite_graphics(int i, uint8_t value) {
    sprite_graphics_[i] = value;
  }
  auto set_sprite_palette(int i, uint8_t value) { sprite_palette_[i] = value; }
  auto set_message_id(uint16_t value) { message_id_ = value; }

  uint8_t* mutable_custom_tileset(int index) { return &custom_gfx_ids_[index]; }

  void SetAsLargeMap(int parent_index, int quadrant) {
    parent_ = parent_index;
    large_index_ = quadrant;
    large_map_ = true;
  }

  void SetAsSmallMap(int index = -1) {
    if (index != -1)
      parent_ = index;
    else
      parent_ = index_;
    large_index_ = 0;
    large_map_ = false;
  }

  void Destroy() {
    current_blockset_.clear();
    current_gfx_.clear();
    bitmap_data_.clear();
    map_tiles_.light_world.clear();
    map_tiles_.dark_world.clear();
    map_tiles_.special_world.clear();
    built_ = false;
    initialized_ = false;
    large_map_ = false;
    mosaic_ = false;
    index_ = 0;
    parent_ = 0;
    large_index_ = 0;
    world_ = 0;
    game_state_ = 0;
    main_gfx_id_ = 0;
    message_id_ = 0;
    area_graphics_ = 0;
    area_palette_ = 0;
    animated_gfx_ = 0;
    subscreen_overlay_ = 0;
  }

 private:
  void LoadAreaInfo();
  void LoadCustomOverworldData();
  void SetupCustomTileset(uint8_t asm_version);

  void LoadMainBlocksetId();
  void LoadSpritesBlocksets();
  void LoadMainBlocksets();
  void LoadAreaGraphicsBlocksets();
  void LoadDeathMountainGFX();

  void ProcessGraphicsBuffer(int index, int static_graphics_offset, int size,
                             uint8_t* all_gfx);
  absl::StatusOr<gfx::SnesPalette> GetPalette(const gfx::PaletteGroup& group,
                                              int index, int previous_index,
                                              int limit);

  Rom *rom_;

  bool built_ = false;
  bool large_map_ = false;
  bool initialized_ = false;
  bool mosaic_ = false;

  int index_ = 0;        // Map index
  int parent_ = 0;       // Parent map index
  int large_index_ = 0;  // Quadrant ID [0-3]
  int world_ = 0;        // World ID [0-2]
  int game_state_ = 0;   // Game state [0-2]
  int main_gfx_id_ = 0;  // Main Gfx ID

  uint16_t message_id_ = 0;
  uint8_t area_graphics_ = 0;
  uint8_t area_palette_ = 0;
  uint8_t animated_gfx_ = 0;        // Custom Overworld Animated ID
  uint16_t subscreen_overlay_ = 0;  // Custom Overworld Subscreen Overlay ID

  std::array<uint8_t, 8> custom_gfx_ids_;
  std::array<uint8_t, 3> sprite_graphics_;
  std::array<uint8_t, 3> sprite_palette_;
  std::array<uint8_t, 4> area_music_;
  std::array<uint8_t, 16> static_graphics_;

  std::array<bool, 4> mosaic_expanded_;

  std::vector<uint8_t> current_blockset_;
  std::vector<uint8_t> current_gfx_;
  std::vector<uint8_t> bitmap_data_;

  OverworldMapTiles map_tiles_;
  gfx::SnesPalette current_palette_;
};

}  // namespace zelda3
}  // namespace yaze

#endif
