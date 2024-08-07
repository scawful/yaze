#ifndef YAZE_APP_ZELDA3_OVERWORLD_MAP_H
#define YAZE_APP_ZELDA3_OVERWORLD_MAP_H

#include "imgui/imgui.h"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <unordered_map>
#include <vector>

#include "absl/status/status.h"
#include "app/core/common.h"
#include "app/editor/utils/gfx_context.h"
#include "app/gfx/bitmap.h"
#include "app/gfx/snes_palette.h"
#include "app/gfx/snes_tile.h"
#include "app/rom.h"

namespace yaze {
namespace app {
namespace zelda3 {
namespace overworld {

static constexpr int kTileOffsets[] = {0, 8, 4096, 4104};

/**
 * @brief Represents a single Overworld map screen.
 */
class OverworldMap : public editor::context::GfxContext {
 public:
  OverworldMap() = default;
  OverworldMap(int index, Rom& rom, std::vector<gfx::Tile16>& tiles16);

  absl::Status BuildMap(int count, int game_state, int world,
                        OWBlockset& world_blockset);

  void LoadAreaGraphics();
  absl::Status LoadPalette();
  absl::Status BuildTileset();
  absl::Status BuildTiles16Gfx(int count);
  absl::Status BuildBitmap(OWBlockset& world_blockset);

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
    tiles16_.clear();
  }

 private:
  void LoadAreaInfo();

  void LoadMainBlocksetId();
  void LoadSpritesBlocksets();
  void LoadMainBlocksets();
  void LoadAreaGraphicsBlocksets();
  void LoadDeathMountainGFX();

  void ProcessGraphicsBuffer(int index, int static_graphics_offset, int size);
  absl::StatusOr<gfx::SnesPalette> GetPalette(const gfx::PaletteGroup& group,
                                              int index, int previous_index,
                                              int limit);

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

  uchar sprite_graphics_[3];
  uchar sprite_palette_[3];
  uchar area_music_[4];
  uchar static_graphics_[16];

  Rom rom_;
  Bytes all_gfx_;
  Bytes current_blockset_;
  Bytes current_gfx_;
  Bytes bitmap_data_;
  OWMapTiles map_tiles_;

  gfx::SnesPalette current_palette_;
  std::vector<gfx::Tile16> tiles16_;
};

}  // namespace overworld
}  // namespace zelda3
}  // namespace app
}  // namespace yaze

#endif