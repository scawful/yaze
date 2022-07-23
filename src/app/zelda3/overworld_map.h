#ifndef YAZE_APP_ZELDA3_OVERWORLD_MAP_H
#define YAZE_APP_ZELDA3_OVERWORLD_MAP_H

#include <imgui/imgui.h>

#include <cstddef>
#include <cstdint>
#include <memory>
#include <vector>

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
  OverworldMap(int index, ROM& rom, const std::vector<gfx::Tile16>& tiles16);
  void BuildMap(int count, int game_state, uchar* map_parent,
                uchar* ow_blockset, uchar* current_gfx, OWMapTiles& map_tiles);

  auto GetBitmap() { return bitmap_; }
  auto SetLargeMap(bool is_set) { large_map_ = is_set; }
  auto IsLargeMap() { return large_map_; }

 private:
  void LoadAreaInfo();
  void BuildTileset(int game_state, uchar* current_gfx);
  void BuildTiles16Gfx(int count, uchar* ow_blockset);

  void CopyTile(int x, int y, int xx, int yy, int offset, gfx::TileInfo tile,
                uchar* gfx16Pointer, uchar* gfx8Pointer);

  void CopyTile8bpp16(int x, int y, int tile, uchar* ow_blockset);

  void CopyTileToMap(int x, int y, int xx, int yy, int offset,
                     gfx::TileInfo tile, uchar* gfx16Pointer,
                     uchar* gfx8Pointer);
  void CopyTile8bpp16From8(int xP, int yP, int tileID, uchar* destbmpPtr);

  int parent_ = 0;
  int index_ = 0;
  int message_id_ = 0;
  int area_graphics_ = 0;
  int area_palette_ = 0;

  uchar sprite_graphics_[3];
  uchar sprite_palette_[3];
  uchar area_music_[4];
  uchar static_graphics_[16];

  bool initialized_ = false;
  bool large_map_ = false;

  gfx::Bitmap bitmap_;

  std::vector<gfx::Tile16> tiles16_;
  std::vector<std::vector<ushort>> tiles_used_;

  ROM rom_;
};

}  // namespace zelda3
}  // namespace app
}  // namespace yaze

#endif