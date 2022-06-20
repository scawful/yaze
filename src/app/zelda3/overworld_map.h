#include <imgui/imgui.h>

#include <cstddef>
#include <memory>

#include "rom.h"
#include "gfx/bitmap.h"
#include "gfx/tile.h"

namespace yaze {
namespace app {
namespace Data {

using ushort = unsigned short;

class OverworldMap {
 public:
  uchar parent = 0;
  uchar index = 0;
  uchar gfx = 0;
  uchar palette = 0;
  bool firstLoad = false;
  short messageID = 0;
  bool largeMap = false;

  uchar sprgfx[3];
  uchar sprpalette[3];
  uchar musics[4];

  int* gfxPtr = new int[512 * 512];
  int* mapblockset16 = new int[1048576];
  gfx::Bitmap mapblockset16Bitmap;
  gfx::Bitmap gfxBitmap;

  uchar* staticgfx =
      new uchar[16];  // Need to be used to display map and not pre render it!
  ushort** tilesUsed;

  bool needRefresh = false;
  Data::ROM rom_;

  uchar* currentOWgfx16Ptr = new uchar[(128 * 512) / 2];
  std::vector<gfx::Tile16> tiles16_;

  OverworldMap(Data::ROM & rom, const std::vector<gfx::Tile16> tiles16,
               uchar index);
  void BuildMap(uchar* mapParent, int count, int gameState,
                ushort** allmapsTilesLW, ushort** allmapsTilesDW,
                ushort** allmapsTilesSP);
  void CopyTile8bpp16(int x, int y, int tile, int* destbmpPtr,
                      int* sourcebmpPtr);
  void CopyTile8bpp16From8(int xP, int yP, int tileID, int* destbmpPtr,
                           int* sourcebmpPtr);

 private:
  void BuildTiles16Gfx(int count);
  // void ReloadPalettes() { LoadPalette(); }

  void CopyTile(int x, int y, int xx, int yy, int offset,
                gfx::TileInfo tile, uchar* gfx16Pointer,
                uchar* gfx8Pointer);
  void CopyTileToMap(int x, int y, int xx, int yy, int offset,
                     gfx::TileInfo tile, uchar* gfx16Pointer,
                     uchar* gfx8Pointer);

  void LoadPalette();

  void SetColorsPalette(int index, ImVec4 main, ImVec4 animated, ImVec4 aux1,
                        ImVec4 aux2, ImVec4 hud, ImVec4 bgrcolor, ImVec4 spr,
                        ImVec4 spr2);

  void BuildTileset(int gameState);
};

}  // namespace Data
}  // namespace app
}  // namespace yaze