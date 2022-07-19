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

class OverworldMap {
 public:
  OverworldMap(int index, ROM& rom, const std::vector<gfx::Tile16>& tiles16);
  void BuildMap(int count, int game_state, uchar* map_parent,
                OWMapTiles& map_tiles);

  auto SetLargeMap(bool is_set) { large_map_ = is_set; }
  auto IsLargeMap() { return large_map_; }

 private:
  void LoadAreaInfo();
  void BuildTileset(int gameState);
  void BuildTiles16Gfx(int count);
  void CopyTile(int x, int y, int xx, int yy, int offset, gfx::TileInfo tile,
                uchar* gfx16Pointer, uchar* gfx8Pointer);
  void CopyTileToMap(int x, int y, int xx, int yy, int offset,
                     gfx::TileInfo tile, uchar* gfx16Pointer,
                     uchar* gfx8Pointer);
  void CopyTile8bpp16(int x, int y, int tile, uchar* destbmpPtr,
                      uchar* sourcebmpPtr);
  void CopyTile8bpp16From8(int xP, int yP, int tileID, uchar* destbmpPtr,
                           uchar* sourcebmpPtr);

  ROM rom_;
  gfx::Bitmap gfxBitmap;
  std::vector<gfx::Tile16> tiles16_;

  uchar* staticgfx = new uchar[16];
  std::vector<std::vector<ushort>> tiles_used_;

  int parent_ = 0;
  int index_ = 0;
  int message_id_ = 0;
  int area_graphics_ = 0;
  int area_palette_ = 0;
  bool initialized_ = false;
  bool large_map_ = false;
  uchar sprite_graphics_[3];
  uchar sprite_palette_[3];
  uchar area_music_[4];
  uchar* gfxPtr = new uchar[512 * 512];
  uchar* mapblockset16_ = nullptr;
  uchar* currentOWgfx16Ptr_ = nullptr;
  uchar* allGfx16Ptr_ = nullptr;
};

}  // namespace zelda3
}  // namespace app
}  // namespace yaze