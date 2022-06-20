#include "overworld_map.h"

#include "gfx/tile.h"
#include "rom.h"

namespace yaze {
namespace app {
namespace zelda3 {

using namespace core;
using namespace gfx;

OverworldMap::OverworldMap(ROM& rom, const std::vector<gfx::Tile16> tiles16,
                           uchar index)
    : rom_(rom), index(index), tiles16_(tiles16), parent(index) {
  if (index != 0x80) {
    if (index <= 150) {
      if (rom_.GetRawData()[constants::overworldMapSize + (index & 0x3F)] !=
          0) {
        largeMap = true;
      }
    }
  }

  if (index < 64) {
    sprgfx[0] = rom_.GetRawData()[constants::overworldSpriteset + parent];
    sprgfx[1] = rom_.GetRawData()[constants::overworldSpriteset + parent + 64];
    sprgfx[2] = rom_.GetRawData()[constants::overworldSpriteset + parent + 128];
    gfx = rom_.GetRawData()[constants::mapGfx + parent];
    palette = rom_.GetRawData()[constants::overworldMapPalette + parent];
    sprpalette[0] =
        rom_.GetRawData()[constants::overworldSpritePalette + parent];
    sprpalette[1] =
        rom_.GetRawData()[constants::overworldSpritePalette + parent + 64];
    sprpalette[2] =
        rom_.GetRawData()[constants::overworldSpritePalette + parent + 128];
    musics[0] = rom_.GetRawData()[constants::overworldMusicBegining + parent];
    musics[1] = rom_.GetRawData()[constants::overworldMusicZelda + parent];
    musics[2] =
        rom_.GetRawData()[constants::overworldMusicMasterSword + parent];
    musics[3] = rom_.GetRawData()[constants::overworldMusicAgahim + parent];
  } else if (index < 128) {
    sprgfx[0] = rom_.GetRawData()[constants::overworldSpriteset + parent + 128];
    sprgfx[1] = rom_.GetRawData()[constants::overworldSpriteset + parent + 128];
    sprgfx[2] = rom_.GetRawData()[constants::overworldSpriteset + parent + 128];
    gfx = rom_.GetRawData()[constants::mapGfx + parent];
    palette = rom_.GetRawData()[constants::overworldMapPalette + parent];
    sprpalette[0] =
        rom_.GetRawData()[constants::overworldSpritePalette + parent + 128];
    sprpalette[1] =
        rom_.GetRawData()[constants::overworldSpritePalette + parent + 128];
    sprpalette[2] =
        rom_.GetRawData()[constants::overworldSpritePalette + parent + 128];

    musics[0] = rom_.GetRawData()[constants::overworldMusicDW + (parent - 64)];
  } else {
    if (index == 0x94) {
      parent = 128;
    } else if (index == 0x95) {
      parent = 03;
    } else if (index == 0x96)  // pyramid bg use 0x5B map
    {
      parent = 0x5B;
    } else if (index == 0x97)  // pyramid bg use 0x5B map
    {
      parent = 0x00;
    } else if (index == 156) {
      parent = 67;
    } else if (index == 157) {
      parent = 0;
    } else if (index == 158) {
      parent = 0;
    } else if (index == 159) {
      parent = 44;
    } else if (index == 136) {
      parent = 136;
    }

    messageID = rom_.GetRawData()[constants::overworldMessages + parent];

    sprgfx[0] = rom_.GetRawData()[constants::overworldSpriteset + parent + 128];
    sprgfx[1] = rom_.GetRawData()[constants::overworldSpriteset + parent + 128];
    sprgfx[2] = rom_.GetRawData()[constants::overworldSpriteset + parent + 128];
    sprpalette[0] =
        rom_.GetRawData()[constants::overworldSpritePalette + parent + 128];
    sprpalette[1] =
        rom_.GetRawData()[constants::overworldSpritePalette + parent + 128];
    sprpalette[2] =
        rom_.GetRawData()[constants::overworldSpritePalette + parent + 128];

    palette =
        rom_.GetRawData()[constants::overworldSpecialPALGroup + parent - 128];
    if (index >= 0x80 && index <= 0x8A && index != 0x88) {
      gfx = rom_.GetRawData()[constants::overworldSpecialGFXGroup +
                              (parent - 128)];
      palette = rom_.GetRawData()[constants::overworldSpecialPALGroup + 1];
    } else if (index == 0x88) {
      gfx = 81;
      palette = 0;
    } else  // pyramid bg use 0x5B map
    {
      gfx = rom_.GetRawData()[constants::mapGfx + parent];
      palette = rom_.GetRawData()[constants::overworldMapPalette + parent];
    }
  }
}

void OverworldMap::BuildMap(uchar* mapParent, int count, int gameState,
                            ushort** allmapsTilesLW, ushort** allmapsTilesDW,
                            ushort** allmapsTilesSP) {
  tilesUsed = new ushort*[32];
  for (int i = 0; i < 32; i++) tilesUsed[i] = new ushort;

  if (largeMap) {
    this->parent = mapParent[index];

    if (parent != index) {
      if (!firstLoad) {
        gfx = rom_.GetRawData()[constants::mapGfx + parent];
        palette = rom_.GetRawData()[constants::overworldMapPalette + parent];
        firstLoad = true;
      }
    }
  }

  BuildTileset(gameState);
  BuildTiles16Gfx(count);  // build on GFX.mapgfx16Ptr

  int world = 0;

  if (index < 64) {
    tilesUsed = allmapsTilesLW;
  } else if (index < 128 && index >= 64) {
    tilesUsed = allmapsTilesDW;
    world = 1;
  } else {
    tilesUsed = allmapsTilesSP;
    world = 2;
  }

  int superY = ((index - (world * 64)) / 8);
  int superX = index - (world * 64) - (superY * 8);

  for (int y = 0; y < 32; y++) {
    for (int x = 0; x < 32; x++) {
      CopyTile8bpp16((x * 16), (y * 16),
                     tilesUsed[x + (superX * 32)][y + (superY * 32)], gfxPtr,
                     mapblockset16);
    }
  }
}

void OverworldMap::CopyTile8bpp16(int x, int y, int tile, int* destbmpPtr,
                                  int* sourcebmpPtr) {
  int sourceY = (tile / 8);
  int sourceX = (tile) - ((sourceY)*8);
  int sourcePtrPos = ((tile - ((tile / 8) * 8)) * 16) +
                     ((tile / 8) * 2048);  //(sourceX * 16) + (sourceY * 128);
  uchar* sourcePtr = (uchar*)sourcebmpPtr;

  int destPtrPos = (x + (y * 512));
  uchar* destPtr = (uchar*)destbmpPtr;

  for (int ystrip = 0; ystrip < 16; ystrip++) {
    for (int xstrip = 0; xstrip < 16; xstrip++) {
      destPtr[destPtrPos + xstrip + (ystrip * 512)] =
          sourcePtr[sourcePtrPos + xstrip + (ystrip * 128)];
    }
  }
}

void OverworldMap::CopyTile8bpp16From8(int xP, int yP, int tileID,
                                       int* destbmpPtr, int* sourcebmpPtr) {
  auto gfx16Data = (uchar*)destbmpPtr;

  auto gfx8Data = currentOWgfx16Ptr;

  int offsets[] = {0, 8, 4096, 4104};

  auto tiles = tiles16_[tileID];

  for (auto tile = 0; tile < 4; tile++) {
    TileInfo info = tiles.tiles_info[tile];
    int offset = offsets[tile];

    for (auto y = 0; y < 8; y++) {
      for (auto x = 0; x < 4; x++) {
        CopyTileToMap(x, y, xP, yP, offset, info, gfx16Data, gfx8Data);
      }
    }
  }
}

void OverworldMap::BuildTiles16Gfx(int count) {
  uchar* gfx16Data = (uchar*)mapblockset16;
  uchar* gfx8Data = currentOWgfx16Ptr;

  int offsets[] = {0, 8, 1024, 1032};
  auto yy = 0;
  auto xx = 0;

  for (auto i = 0; i < count; i++)  // number of tiles16 3748?
  {
    // 8x8 tile draw
    // gfx8 = 4bpp so everyting is /2F
    auto tiles = tiles16_[i];

    for (auto tile = 0; tile < 4; tile++) {
      TileInfo info = tiles16_[i].tiles_info[tile];
      int offset = offsets[tile];

      for (auto y = 0; y < 8; y++) {
        for (auto x = 0; x < 4; x++) {
          CopyTile(x, y, xx, yy, offset, info, gfx16Data, gfx8Data);
        }
      }
    }

    xx += 16;
    if (xx >= 128) {
      yy += 2048;
      xx = 0;
    }
  }
}

void OverworldMap::CopyTile(int x, int y, int xx, int yy, int offset,
                            TileInfo tile, uchar* gfx16Pointer,
                            uchar* gfx8Pointer)  // map,current
{
  int mx = x;
  int my = y;
  uchar r = 0;

  if (tile.horizontal_mirror_ != 0) {
    mx = 3 - x;
    r = 1;
  }

  if (tile.vertical_mirror_ != 0) {
    my = 7 - y;
  }

  int tx = ((tile.id_ / 16) * 512) + ((tile.id_ - ((tile.id_ / 16) * 16)) * 4);
  auto index = xx + yy + offset + (mx * 2) + (my * 128);
  auto pixel = gfx8Pointer[tx + (y * 64) + x];

  gfx16Pointer[index + r ^ 1] = (uchar)((pixel & 0x0F) + tile.palette_ * 16);
  gfx16Pointer[index + r] = (uchar)(((pixel >> 4) & 0x0F) + tile.palette_ * 16);
}

void OverworldMap::CopyTileToMap(int x, int y, int xx, int yy, int offset,
                                 TileInfo tile, uchar* gfx16Pointer,
                                 uchar* gfx8Pointer)  // map,current
{
  int mx = x;
  int my = y;
  uchar r = 0;

  if (tile.horizontal_mirror_ != 0) {
    mx = 3 - x;
    r = 1;
  }

  if (tile.vertical_mirror_ != 0) {
    my = 7 - y;
  }

  int tx = ((tile.id_ / 16) * 512) + ((tile.id_ - ((tile.id_ / 16) * 16)) * 4);
  auto index = xx + (yy * 512) + offset + (mx * 2) + (my * 512);
  auto pixel = gfx8Pointer[tx + (y * 64) + x];

  gfx16Pointer[index + r ^ 1] = (uchar)((pixel & 0x0F) + tile.palette_ * 16);
  gfx16Pointer[index + r] = (uchar)(((pixel >> 4) & 0x0F) + tile.palette_ * 16);
}

void OverworldMap::BuildTileset(int gameState) {
  int indexWorld = 0x20;
  if (parent < 0x40) {
    indexWorld = 0x20;
  } else if (parent >= 0x40 && parent < 0x80) {
    indexWorld = 0x21;
  } else if (parent == 0x88) {
    indexWorld = 36;
  }

  // Sprites Blocksets
  staticgfx[8] = 115 + 0;
  staticgfx[9] = 115 + 1;
  staticgfx[10] = 115 + 6;
  staticgfx[11] = 115 + 7;
  for (int i = 0; i < 4; i++) {
    staticgfx[12 + i] =
        (uchar)(rom_.GetRawData()[constants::sprite_blockset_pointer +
                                  (sprgfx[gameState] * 4) + i] +
                115);
  }

  // Main Blocksets
  for (int i = 0; i < 8; i++) {
    staticgfx[i] = rom_.GetRawData()[constants::overworldgfxGroups2 +
                                     (indexWorld * 8) + i];
  }

  if (rom_.GetRawData()[constants::overworldgfxGroups + (gfx * 4)] != 0) {
    staticgfx[3] = rom_.GetRawData()[constants::overworldgfxGroups + (gfx * 4)];
  }
  if (rom_.GetRawData()[constants::overworldgfxGroups + (gfx * 4) + 1] != 0) {
    staticgfx[4] =
        rom_.GetRawData()[constants::overworldgfxGroups + (gfx * 4) + 1];
  }
  if (rom_.GetRawData()[constants::overworldgfxGroups + (gfx * 4) + 2] != 0) {
    staticgfx[5] =
        rom_.GetRawData()[constants::overworldgfxGroups + (gfx * 4) + 2];
  }
  if (rom_.GetRawData()[constants::overworldgfxGroups + (gfx * 4) + 3] != 0) {
    staticgfx[6] =
        rom_.GetRawData()[constants::overworldgfxGroups + (gfx * 4) + 3];
  }

  // Hardcoded overworld GFX Values, for death mountain
  if ((parent >= 0x03 && parent <= 0x07) ||
      (parent >= 0x0B && parent <= 0x0E)) {
    staticgfx[7] = 89;
  } else if ((parent >= 0x43 && parent <= 0x47) ||
             (parent >= 0x4B && parent <= 0x4E)) {
    staticgfx[7] = 89;
  } else {
    staticgfx[7] = 91;
  }

  uchar* currentmapgfx8Data = new uchar[(128 * 512) / 2];
  // (uchar*)GFX.currentOWgfx16Ptr.ToPointer();  // loaded gfx for the current
  //                                            // map (empty at this point)
  uchar* allgfxData = new uchar[(128 * 7136) / 2];
  // (uchar*)GFX.allgfx16Ptr
  //     .ToPointer();  // all gfx of the game pack of 2048 uchars (4bpp)

  for (int i = 0; i < 16; i++) {
    for (int j = 0; j < 2048; j++) {
      uchar mapByte = allgfxData[j + (staticgfx[i] * 2048)];
      switch (i) {
        case 0:
        case 3:
        case 4:
        case 5:
          mapByte += 0x88;
          break;
      }

      currentmapgfx8Data[(i * 2048) + j] = mapByte;  // Upload used gfx data
    }
  }
}

}  // namespace zelda3
}  // namespace app
}  // namespace yaze