#ifndef YAZE_APP_ZELDA3_OVERWORLD_MAP_H
#define YAZE_APP_ZELDA3_OVERWORLD_MAP_H

#include <imgui/imgui.h>

#include <cstddef>
#include <cstdint>
#include <memory>
#include <unordered_map>
#include <vector>

#include "absl/status/status.h"
#include "app/core/common.h"
#include "app/gfx/bitmap.h"
#include "app/gfx/snes_palette.h"
#include "app/gfx/snes_tile.h"
#include "app/rom.h"

namespace yaze {
namespace app {
namespace zelda3 {

static constexpr int kTileOffsets[] = {0, 8, 4096, 4104};

class OverworldMap {
 public:
  OverworldMap() = default;
  OverworldMap(int index, ROM& rom, std::vector<gfx::Tile16>& tiles16);

  absl::Status BuildMap(int count, int game_state, int world, uchar* map_parent,
                        OWBlockset& world_blockset);

  auto Tile16Blockset() const { return current_blockset_; }
  auto AreaGraphics() const { return current_gfx_; }
  auto AreaPalette() const { return current_palette_; }
  auto BitmapData() const { return bitmap_data_; }
  auto SetLargeMap(bool is_set) { large_map_ = is_set; }
  auto IsLargeMap() const { return large_map_; }
  auto IsInitialized() const { return initialized_; }
  auto Parent() const { return parent_; }

  auto area_graphics() const { return area_graphics_; }
  auto area_palette() const { return area_palette_; }
  auto sprite_graphics(int i) const { return sprite_graphics_[i]; }
  auto sprite_palette(int i) const { return sprite_palette_[i]; }
  auto message_id() const { return message_id_; }
  auto area_music(int i) const { return area_music_[i]; }
  auto static_graphics(int i) const { return static_graphics_[i]; }

  auto mutable_area_graphics() { return &area_graphics_; }
  auto mutable_area_palette() { return &area_palette_; }
  auto mutable_sprite_graphics(int i) { return &sprite_graphics_[i]; }
  auto mutable_sprite_palette(int i) { return &sprite_palette_[i]; }
  auto mutable_message_id() { return &message_id_; }
  auto mutable_area_music(int i) { return &area_music_[i]; }
  auto mutable_static_graphics(int i) { return &static_graphics_[i]; }

 private:
  void LoadAreaInfo();

  void LoadWorldIndex();
  void LoadSpritesBlocksets();
  void LoadMainBlocksets();
  void LoadAreaGraphicsBlocksets();
  void LoadDeathMountainGFX();
  void LoadAreaGraphics();

  void LoadPalette();

  void ProcessGraphicsBuffer(int index, int static_graphics_offset, int size);
  gfx::SNESPalette GetPalette(const std::string& group, int index,
                              int previousIndex, int limit);

  absl::Status BuildTileset();
  absl::Status BuildTiles16Gfx(int count);
  absl::Status BuildBitmap(OWBlockset& world_blockset);

  bool built_ = false;
  bool large_map_ = false;
  bool initialized_ = false;

  int parent_ = 0;
  int index_ = 0;
  int world_ = 0;
  int game_state_ = 0;
  int world_index_ = 0;

  uint16_t message_id_ = 0;
  uint8_t area_graphics_ = 0;
  uint8_t area_palette_ = 0;

  uchar sprite_graphics_[3];
  uchar sprite_palette_[3];
  uchar area_music_[4];
  uchar static_graphics_[16];

  ROM rom_;
  Bytes all_gfx_;
  Bytes current_blockset_;
  Bytes current_gfx_;
  Bytes bitmap_data_;
  OWMapTiles map_tiles_;

  gfx::SNESPalette current_palette_;
  std::vector<gfx::Tile16> tiles16_;
};

}  // namespace zelda3
}  // namespace app
}  // namespace yaze

#endif