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
#include "app/gfx/snes_tile.h"
#include "app/rom.h"

namespace yaze {
namespace app {
namespace zelda3 {

static constexpr int kTileOffsets[] = {0, 8, 4096, 4104};

class OverworldMap {
 public:
  OverworldMap(int index, ROM& rom, std::vector<gfx::Tile16>& tiles16);

  absl::Status BuildMap(int count, int game_state, int world, uchar* map_parent,
                        OWBlockset& world_blockset);

  auto GetCurrentGraphicsSet() const { return current_graphics_sheet_set; }
  auto GetCurrentBlockset() const { return current_blockset_; }
  auto GetCurrentGraphics() const { return current_gfx_; }
  auto GetBitmapData() const { return bitmap_data_; }
  auto SetLargeMap(bool is_set) { large_map_ = is_set; }
  auto IsLargeMap() const { return large_map_; }
  auto IsInitialized() const { return initialized_; }
  auto IsBuilt() const { return built_; }

 private:
  void LoadAreaInfo();
  void LoadAreaGraphics(int game_state, int world_index);

  absl::Status BuildTileset();
  absl::Status BuildTiles16Gfx(int count);
  absl::Status BuildBitmap(OWBlockset& world_blockset);

  int parent_ = 0;
  int index_ = 0;
  int world_ = 0;
  int message_id_ = 0;
  int area_graphics_ = 0;
  int area_palette_ = 0;

  uchar sprite_graphics_[3];
  uchar sprite_palette_[3];
  uchar area_music_[4];
  uchar static_graphics_[16];

  bool initialized_ = false;
  bool built_ = false;
  bool large_map_ = false;

  ROM rom_;
  Bytes all_gfx_;
  Bytes current_blockset_;
  Bytes current_gfx_;
  Bytes bitmap_data_;
  OWMapTiles map_tiles_;

  std::vector<gfx::Tile16> tiles16_;
  std::unordered_map<int, gfx::Bitmap> current_graphics_sheet_set;
};

}  // namespace zelda3
}  // namespace app
}  // namespace yaze

#endif