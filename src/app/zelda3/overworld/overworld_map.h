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
// vanilla, v2, v3
constexpr int OverworldCustomASMHasBeenApplied = 0x140145;

// 2 bytes for each overworld area (0x140)
constexpr int OverworldCustomAreaSpecificBGPalette = 0x140000;

// 1 byte, not 0 if enabled
constexpr int OverworldCustomAreaSpecificBGEnabled = 0x140140;

// Additional v3 constants
constexpr int OverworldCustomSubscreenOverlayArray = 0x140340;  // 2 bytes for each overworld area (0x140)
constexpr int OverworldCustomSubscreenOverlayEnabled = 0x140144;  // 1 byte, not 0 if enabled
constexpr int OverworldCustomAnimatedGFXArray = 0x1402A0;  // 1 byte for each overworld area (0xA0)
constexpr int OverworldCustomAnimatedGFXEnabled = 0x140143;  // 1 byte, not 0 if enabled
constexpr int OverworldCustomTileGFXGroupArray = 0x140480;  // 8 bytes for each overworld area (0x500)
constexpr int OverworldCustomTileGFXGroupEnabled = 0x140148;  // 1 byte, not 0 if enabled
constexpr int OverworldCustomMosaicArray = 0x140200;  // 1 byte for each overworld area (0xA0)
constexpr int OverworldCustomMosaicEnabled = 0x140142;  // 1 byte, not 0 if enabled

// Vanilla overlay constants
constexpr int kOverlayPointers = 0x77664;  // 2 bytes for each overworld area (0x100)
constexpr int kOverlayPointersBank = 0x0E;  // Bank for overlay pointers
constexpr int kOverlayData1 = 0x77676;  // Check for custom overlay code
constexpr int kOverlayData2 = 0x77677;  // Custom overlay data pointer
constexpr int kOverlayCodeStart = 0x77657;  // Start of overlay code

// 1 byte for each overworld area (0xA0)
constexpr int OverworldCustomMainPaletteArray = 0x140160;
// 1 byte, not 0 if enabled
constexpr int OverworldCustomMainPaletteEnabled = 0x140141;


// v3 expanded constants
constexpr int kOverworldMessagesExpanded = 0x1417F8;
constexpr int kOverworldMapParentIdExpanded = 0x140998;
constexpr int kOverworldTransitionPositionYExpanded = 0x140F38;
constexpr int kOverworldTransitionPositionXExpanded = 0x141078;
constexpr int kOverworldScreenTileMapChangeByScreen1Expanded = 0x140A38;
constexpr int kOverworldScreenTileMapChangeByScreen2Expanded = 0x140B78;
constexpr int kOverworldScreenTileMapChangeByScreen3Expanded = 0x140CB8;
constexpr int kOverworldScreenTileMapChangeByScreen4Expanded = 0x140DF8;

constexpr int kOverworldSpecialSpriteGFXGroup = 0x016811;
constexpr int kOverworldSpecialGFXGroup = 0x016821;
constexpr int kOverworldSpecialPALGroup = 0x016831;
constexpr int kOverworldSpecialSpritePalette = 0x016841;
constexpr int kOverworldPalettesScreenToSetNew = 0x4C635;
constexpr int kOverworldSpecialSpriteGfxGroupExpandedTemp = 0x0166E1;
constexpr int kOverworldSpecialSpritePaletteExpandedTemp = 0x016701;

constexpr int transition_target_northExpanded = 0x1411B8;
constexpr int transition_target_westExpanded = 0x1412F8;

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

enum class AreaSizeEnum {
  SmallArea = 0,
  LargeArea = 1,
  WideArea = 2,
  TallArea = 3,
};

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
  absl::Status LoadOverlay();
  absl::Status LoadVanillaOverlayData();
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
  auto area_size() const { return area_size_; }

  auto main_palette() const { return main_palette_; }
  void set_main_palette(uint8_t palette) { main_palette_ = palette; }

  auto area_specific_bg_color() const { return area_specific_bg_color_; }
  void set_area_specific_bg_color(uint16_t color) {
    area_specific_bg_color_ = color;
  }

  auto subscreen_overlay() const { return subscreen_overlay_; }
  void set_subscreen_overlay(uint16_t overlay) { subscreen_overlay_ = overlay; }

  auto animated_gfx() const { return animated_gfx_; }
  void set_animated_gfx(uint8_t gfx) { animated_gfx_ = gfx; }

  auto custom_tileset(int index) const { return custom_gfx_ids_[index]; }
  
  // Overlay accessors (interactive overlays)
  auto overlay_id() const { return overlay_id_; }
  auto has_overlay() const { return has_overlay_; }
  const auto& overlay_data() const { return overlay_data_; }

  // Mosaic expanded accessors
  const std::array<bool, 4>& mosaic_expanded() const { return mosaic_expanded_; }
  void set_mosaic_expanded(int index, bool value) { mosaic_expanded_[index] = value; }
  void set_custom_tileset(int index, uint8_t value) { custom_gfx_ids_[index] = value; }

  auto mutable_current_graphics() { return &current_gfx_; }
  auto mutable_area_graphics() { return &area_graphics_; }
  auto mutable_area_palette() { return &area_palette_; }
  auto mutable_sprite_graphics(int i) { return &sprite_graphics_[i]; }
  auto mutable_sprite_palette(int i) { return &sprite_palette_[i]; }
  auto mutable_message_id() { return &message_id_; }
  auto mutable_main_palette() { return &main_palette_; }
  auto mutable_animated_gfx() { return &animated_gfx_; }
  auto mutable_subscreen_overlay() { return &subscreen_overlay_; }
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
    area_size_ = AreaSizeEnum::LargeArea;
  }

  void SetAsSmallMap(int index = -1) {
    if (index != -1)
      parent_ = index;
    else
      parent_ = index_;
    large_index_ = 0;
    large_map_ = false;
    area_size_ = AreaSizeEnum::SmallArea;
  }

  void SetAreaSize(AreaSizeEnum size) {
    area_size_ = size;
    large_map_ = (size == AreaSizeEnum::LargeArea);
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
    main_palette_ = 0;
    animated_gfx_ = 0;
    subscreen_overlay_ = 0;
    area_specific_bg_color_ = 0;
    custom_gfx_ids_.fill(0);
    sprite_graphics_.fill(0);
    sprite_palette_.fill(0);
    area_music_.fill(0);
    static_graphics_.fill(0);
    mosaic_expanded_.fill(false);
    area_size_ = AreaSizeEnum::SmallArea;
    overlay_id_ = 0;
    has_overlay_ = false;
    overlay_data_.clear();
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

  Rom* rom_;

  bool built_ = false;
  bool large_map_ = false;
  bool initialized_ = false;
  bool mosaic_ = false;

  int index_ = 0;                                     // Map index
  int parent_ = 0;                                    // Parent map index
  int large_index_ = 0;                               // Quadrant ID [0-3]
  int world_ = 0;                                     // World ID [0-2]
  int game_state_ = 0;                                // Game state [0-2]
  int main_gfx_id_ = 0;                               // Main Gfx ID
  AreaSizeEnum area_size_ = AreaSizeEnum::SmallArea;  // Area size for v3

  uint16_t message_id_ = 0;
  uint8_t area_graphics_ = 0;
  uint8_t area_palette_ = 0;
  uint8_t main_palette_ = 0;        // Custom Overworld Main Palette ID
  uint8_t animated_gfx_ = 0;        // Custom Overworld Animated ID
  uint16_t subscreen_overlay_ = 0;  // Custom Overworld Subscreen Overlay ID
  uint16_t area_specific_bg_color_ =
      0;  // Custom Overworld Area-Specific Background Color

  std::array<uint8_t, 8> custom_gfx_ids_;
  std::array<uint8_t, 3> sprite_graphics_;
  std::array<uint8_t, 3> sprite_palette_;
  std::array<uint8_t, 4> area_music_;
  std::array<uint8_t, 16> static_graphics_;

  std::array<bool, 4> mosaic_expanded_;

  // Overlay support (interactive overlays that reveal holes/change elements)
  uint16_t overlay_id_ = 0;
  bool has_overlay_ = false;
  std::vector<uint8_t> overlay_data_;

  std::vector<uint8_t> current_blockset_;
  std::vector<uint8_t> current_gfx_;
  std::vector<uint8_t> bitmap_data_;

  OverworldMapTiles map_tiles_;
  gfx::SnesPalette current_palette_;
};

}  // namespace zelda3
}  // namespace yaze

#endif
