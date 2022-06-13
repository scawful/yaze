#include "overworld_map.h"

#include "Data/rom.h"
#include "Graphics/tile.h"

namespace yaze {
namespace Application {
namespace Data {

using namespace Core;
using namespace Graphics;

OverworldMap::OverworldMap(Data::ROM rom,
                           const std::vector<Graphics::Tile16> tiles16,
                           byte index)
    : rom_(rom), index(index), tiles16_(tiles16), parent(index) {
  if (index != 0x80) {
    if (index <= 150) {
      if (rom_.GetRawData()[Constants::overworldMapSize + (index & 0x3F)] !=
          0) {
        largeMap = true;
      }
    }
  }

  if (index < 64) {
    sprgfx[0] = rom_.GetRawData()[Constants::overworldSpriteset + parent];
    sprgfx[1] = rom_.GetRawData()[Constants::overworldSpriteset + parent + 64];
    sprgfx[2] = rom_.GetRawData()[Constants::overworldSpriteset + parent + 128];
    gfx = rom_.GetRawData()[Constants::mapGfx + parent];
    palette = rom_.GetRawData()[Constants::overworldMapPalette + parent];
    sprpalette[0] =
        rom_.GetRawData()[Constants::overworldSpritePalette + parent];
    sprpalette[1] =
        rom_.GetRawData()[Constants::overworldSpritePalette + parent + 64];
    sprpalette[2] =
        rom_.GetRawData()[Constants::overworldSpritePalette + parent + 128];
    musics[0] = rom_.GetRawData()[Constants::overworldMusicBegining + parent];
    musics[1] = rom_.GetRawData()[Constants::overworldMusicZelda + parent];
    musics[2] =
        rom_.GetRawData()[Constants::overworldMusicMasterSword + parent];
    musics[3] = rom_.GetRawData()[Constants::overworldMusicAgahim + parent];
  } else if (index < 128) {
    sprgfx[0] = rom_.GetRawData()[Constants::overworldSpriteset + parent + 128];
    sprgfx[1] = rom_.GetRawData()[Constants::overworldSpriteset + parent + 128];
    sprgfx[2] = rom_.GetRawData()[Constants::overworldSpriteset + parent + 128];
    gfx = rom_.GetRawData()[Constants::mapGfx + parent];
    palette = rom_.GetRawData()[Constants::overworldMapPalette + parent];
    sprpalette[0] =
        rom_.GetRawData()[Constants::overworldSpritePalette + parent + 128];
    sprpalette[1] =
        rom_.GetRawData()[Constants::overworldSpritePalette + parent + 128];
    sprpalette[2] =
        rom_.GetRawData()[Constants::overworldSpritePalette + parent + 128];

    musics[0] = rom_.GetRawData()[Constants::overworldMusicDW + (parent - 64)];
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

    messageID = rom_.GetRawData()[Constants::overworldMessages + parent];

    sprgfx[0] = rom_.GetRawData()[Constants::overworldSpriteset + parent + 128];
    sprgfx[1] = rom_.GetRawData()[Constants::overworldSpriteset + parent + 128];
    sprgfx[2] = rom_.GetRawData()[Constants::overworldSpriteset + parent + 128];
    sprpalette[0] =
        rom_.GetRawData()[Constants::overworldSpritePalette + parent + 128];
    sprpalette[1] =
        rom_.GetRawData()[Constants::overworldSpritePalette + parent + 128];
    sprpalette[2] =
        rom_.GetRawData()[Constants::overworldSpritePalette + parent + 128];

    palette =
        rom_.GetRawData()[Constants::overworldSpecialPALGroup + parent - 128];
    if (index >= 0x80 && index <= 0x8A && index != 0x88) {
      gfx = rom_.GetRawData()[Constants::overworldSpecialGFXGroup +
                              (parent - 128)];
      palette = rom_.GetRawData()[Constants::overworldSpecialPALGroup + 1];
    } else if (index == 0x88) {
      gfx = 81;
      palette = 0;
    } else  // pyramid bg use 0x5B map
    {
      gfx = rom_.GetRawData()[Constants::mapGfx + parent];
      palette = rom_.GetRawData()[Constants::overworldMapPalette + parent];
    }
  }
}

void OverworldMap::BuildMap(byte* mapParent, int count, int gameState,
                            ushort** allmapsTilesLW, ushort** allmapsTilesDW,
                            ushort** allmapsTilesSP) {
  tilesUsed = new ushort*[32];
  for (int i = 0; i < 32; i++) tilesUsed[i] = new ushort;

  if (largeMap) {
    this->parent = mapParent[index];

    if (parent != index) {
      if (!firstLoad) {
        gfx = rom_.GetRawData()[Constants::mapGfx + parent];
        palette = rom_.GetRawData()[Constants::overworldMapPalette + parent];
        firstLoad = true;
      }
    }
  }

  BuildTileset(gameState);
  BuildTiles16Gfx(count);  // build on GFX.mapgfx16Ptr
  // LoadPalette();

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
  byte* sourcePtr = (byte*)sourcebmpPtr;

  int destPtrPos = (x + (y * 512));
  byte* destPtr = (byte*)destbmpPtr;

  for (int ystrip = 0; ystrip < 16; ystrip++) {
    for (int xstrip = 0; xstrip < 16; xstrip++) {
      destPtr[destPtrPos + xstrip + (ystrip * 512)] =
          sourcePtr[sourcePtrPos + xstrip + (ystrip * 128)];
    }
  }
}

void OverworldMap::CopyTile8bpp16From8(int xP, int yP, int tileID,
                                       int* destbmpPtr, int* sourcebmpPtr) {
  auto gfx16Data = (byte*)destbmpPtr;

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
  byte* gfx16Data = (byte*)mapblockset16;
  byte* gfx8Data = currentOWgfx16Ptr;

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
                            TileInfo tile, byte* gfx16Pointer,
                            byte* gfx8Pointer)  // map,current
{
  int mx = x;
  int my = y;
  byte r = 0;

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

  gfx16Pointer[index + r ^ 1] = (byte)((pixel & 0x0F) + tile.palette_ * 16);
  gfx16Pointer[index + r] = (byte)(((pixel >> 4) & 0x0F) + tile.palette_ * 16);
}

void OverworldMap::CopyTileToMap(int x, int y, int xx, int yy, int offset,
                                 TileInfo tile, byte* gfx16Pointer,
                                 byte* gfx8Pointer)  // map,current
{
  int mx = x;
  int my = y;
  byte r = 0;

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

  gfx16Pointer[index + r ^ 1] = (byte)((pixel & 0x0F) + tile.palette_ * 16);
  gfx16Pointer[index + r] = (byte)(((pixel >> 4) & 0x0F) + tile.palette_ * 16);
}

/*
void OverworldMap::LoadPalette() {
  int previousPalId = 0;
  int previousSprPalId = 0;
  if (index > 0) {
    previousPalId =
        rom_.GetRawData()[Constants::overworldMapPalette + parent - 1];
    previousSprPalId =
        rom_.GetRawData()[Constants::overworldSpritePalette + parent - 1];
  }

  if (palette >= 0xA3) {
    palette = 0xA3;
  }

  byte pal0 = 0;

  byte pal1 = rom_.GetRawData()[Constants::overworldMapPaletteGroup +
                                (palette * 4)];  // aux1
  byte pal2 = rom_.GetRawData()[Constants::overworldMapPaletteGroup +
                                (palette * 4) + 1];  // aux2
  byte pal3 = rom_.GetRawData()[Constants::overworldMapPaletteGroup +
                                (palette * 4) + 2];  // animated

  byte pal4 = rom_.GetRawData()[Constants::overworldSpritePaletteGroup +
                                (sprpalette[ow_.gameState] * 2)];  // spr3
  byte pal5 = rom_.GetRawData()[Constants::overworldSpritePaletteGroup +
                                (sprpalette[ow_.gameState] * 2) + 1];  // spr4

  ImVec4 aux1, aux2, main, animated, hud, spr, spr2;
  ImVec4 bgr = Palettes.overworld_GrassPalettes[0];

  if (pal1 == 255) {
    pal1 = rom_.GetRawData()[Constants::overworldMapPaletteGroup +
                             (previousPalId * 4)];
  }
  if (pal1 != 255) {
    if (pal1 >= 20) {
      pal1 = 19;
    }

    aux1 = Palettes.overworld_AuxPalettes[pal1];
  } else {
    aux1 = Palettes.overworld_AuxPalettes[0];
  }

  if (pal2 == 255) {
    pal2 = rom_.GetRawData()[Constants::overworldMapPaletteGroup +
                             (previousPalId * 4) + 1];
  }
  if (pal2 != 255) {
    if (pal2 >= 20) {
      pal2 = 19;
    }

    aux2 = Palettes.overworld_AuxPalettes[pal2];
  } else {
    aux2 = Palettes.overworld_AuxPalettes[0];
  }

  if (pal3 == 255) {
    pal3 = rom_.GetRawData()[Constants::overworldMapPaletteGroup +
                             (previousPalId * 4) + 2];
  }

  if (parent < 0x40) {
    // default LW Palette
    pal0 = 0;
    bgr = Palettes.overworld_GrassPalettes[0];
    // hardcoded LW DM palettes if we are on one of those maps (might change
    // it to read game code)
    if ((parent >= 0x03 && parent <= 0x07)) {
      pal0 = 2;
    } else if (parent >= 0x0B && parent <= 0x0E) {
      pal0 = 2;
    }
  } else if (parent >= 0x40 && parent < 0x80) {
    bgr = Palettes.overworld_GrassPalettes[1];
    // default DW Palette
    pal0 = 1;
    // hardcoded DW DM palettes if we are on one of those maps (might change
    // it to read game code)
    if (parent >= 0x43 && parent <= 0x47) {
      pal0 = 3;
    } else if (parent >= 0x4B && parent <= 0x4E) {
      pal0 = 3;
    }
  }

  if (parent == 0x88) {
    pal0 = 4;
  }
  /*else if (parent >= 128) //special area like Zora's domain, etc...
  {
      bgr = Palettes.overworld_GrassPalettes[2];
      pal0 = 4;
  }

  if (pal0 != 255) {
    main = Palettes.overworld_MainPalettes[pal0];
  } else {
    main = Palettes.overworld_MainPalettes[0];
  }

  if (pal3 >= 14) {
    pal3 = 13;
  }
  animated = Palettes.overworld_AnimatedPalettes[(pal3)];

  hud = Palettes.HudPalettes[0];
  if (pal4 == 255) {
    pal4 = rom_.GetRawData()[Constants::overworldSpritePaletteGroup +
                             (previousSprPalId * 2)];  // spr3
  }
  if (pal4 == 255) {
    pal4 = 0;
  }
  if (pal4 >= 24) {
    pal4 = 23;
  }
  spr = Palettes.spritesAux3_Palettes[pal4];

  if (pal5 == 255) {
    pal5 = rom_.GetRawData()[Constants::overworldSpritePaletteGroup +
                             (previousSprPalId * 2) + 1];  // spr3
  }
  if (pal5 == 255) {
    pal5 = 0;
  }
  if (pal5 >= 24) {
    pal5 = 23;
  }
  spr2 = Palettes.spritesAux3_Palettes[pal5];

  SetColorsPalette(parent, main, animated, aux1, aux2, hud, bgr, spr, spr2);
}


void OverworldMap::SetColorsPalette(int index, ImVec4 main, ImVec4 animated,
                                    ImVec4 aux1, ImVec4 aux2, ImVec4 hud,
                                    Color bgrcolor, ImVec4 spr, ImVec4 spr2) {
  // Palettes infos, color 0 of a palette is always transparent (the arrays
  // contains 7 colors width wide) there is 16 color per line so 16*Y

  // Left side of the palette - Main, Animated
  ImVec4 currentPalette = new Color[256];
  // Main Palette, Location 0,2 : 35 colors [7x5]
  int k = 0;
  for (int y = 2; y < 7; y++) {
    for (int x = 1; x < 8; x++) {
      currentPalette[x + (16 * y)] = main[k];
      k++;
    }
  }

  // Animated Palette, Location 0,7 : 7colors
  for (int x = 1; x < 8; x++) {
    currentPalette[(16 * 7) + (x)] = animated[(x - 1)];
  }

  // Right side of the palette - Aux1, Aux2

  // Aux1 Palette, Location 8,2 : 21 colors [7x3]
  k = 0;
  for (int y = 2; y < 5; y++) {
    for (int x = 9; x < 16; x++) {
      currentPalette[x + (16 * y)] = aux1[k];
      k++;
    }
  }

  // Aux2 Palette, Location 8,5 : 21 colors [7x3]
  k = 0;
  for (int y = 5; y < 8; y++) {
    for (int x = 9; x < 16; x++) {
      currentPalette[x + (16 * y)] = aux2[k];
      k++;
    }
  }

  // Hud Palette, Location 0,0 : 32 colors [16x2]
  k = 0;
  for (int i = 0; i < 32; i++) {
    currentPalette[i] = hud[i];
  }

  // Hardcoded grass color (that might change to become invisible instead)
  for (int i = 0; i < 8; i++) {
    currentPalette[(i * 16)] = bgrcolor;
    currentPalette[(i * 16) + 8] = bgrcolor;
  }

  // Sprite Palettes
  k = 0;
  for (int y = 8; y < 9; y++) {
    for (int x = 1; x < 8; x++) {
      currentPalette[x + (16 * y)] = Palettes.spritesAux1_Palettes[1][k];
      k++;
    }
  }

  // Sprite Palettes
  k = 0;
  for (int y = 8; y < 9; y++) {
    for (int x = 9; x < 16; x++) {
      currentPalette[x + (16 * y)] = Palettes.spritesAux3_Palettes[0][k];
      k++;
    }
  }

  // Sprite Palettes
  k = 0;
  for (int y = 9; y < 13; y++) {
    for (int x = 1; x < 16; x++) {
      currentPalette[x + (16 * y)] = Palettes.globalSprite_Palettes[0][k];
      k++;
    }
  }

  // Sprite Palettes
  k = 0;
  for (int y = 13; y < 14; y++) {
    for (int x = 1; x < 8; x++) {
      currentPalette[x + (16 * y)] = spr[k];
      k++;
    }
  }

  // Sprite Palettes
  k = 0;
  for (int y = 14; y < 15; y++) {
    for (int x = 1; x < 8; x++) {
      currentPalette[x + (16 * y)] = spr2[k];
      k++;
    }
  }

  // Sprite Palettes
  k = 0;
  for (int y = 15; y < 16; y++) {
    for (int x = 1; x < 16; x++) {
      currentPalette[x + (16 * y)] = Palettes.armors_Palettes[0][k];
      k++;
    }
  }

  try {
    ColorPalette pal = GFX.editort16Bitmap.Palette;
    for (int i = 0; i < 256; i++) {
      pal.Entries[i] = currentPalette[i];
      pal.Entries[(i / 16) * 16] = Color.Transparent;
    }

    GFX.mapgfx16Bitmap.Palette = pal;
    GFX.mapblockset16Bitmap.Palette = pal;

    for (int i = 0; i < 256; i++)
    {
        if (index == 3)
        {

        }
        else if (index == 4)
        {
            pal.Entries[(i / 16) * 16] = Color.Transparent;
        }
    }
    gfxBitmap.Palette = pal;
  } catch (Exception e) {
    // TODO: Add exception message.
  }
}
*/

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
        (byte)(rom_.GetRawData()[Constants::sprite_blockset_pointer +
                                 (sprgfx[gameState] * 4) + i] +
               115);
  }

  // Main Blocksets
  for (int i = 0; i < 8; i++) {
    staticgfx[i] = rom_.GetRawData()[Constants::overworldgfxGroups2 +
                                     (indexWorld * 8) + i];
  }

  if (rom_.GetRawData()[Constants::overworldgfxGroups + (gfx * 4)] != 0) {
    staticgfx[3] = rom_.GetRawData()[Constants::overworldgfxGroups + (gfx * 4)];
  }
  if (rom_.GetRawData()[Constants::overworldgfxGroups + (gfx * 4) + 1] != 0) {
    staticgfx[4] =
        rom_.GetRawData()[Constants::overworldgfxGroups + (gfx * 4) + 1];
  }
  if (rom_.GetRawData()[Constants::overworldgfxGroups + (gfx * 4) + 2] != 0) {
    staticgfx[5] =
        rom_.GetRawData()[Constants::overworldgfxGroups + (gfx * 4) + 2];
  }
  if (rom_.GetRawData()[Constants::overworldgfxGroups + (gfx * 4) + 3] != 0) {
    staticgfx[6] =
        rom_.GetRawData()[Constants::overworldgfxGroups + (gfx * 4) + 3];
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

  byte* currentmapgfx8Data = new byte[(128 * 512) / 2];
  // (byte*)GFX.currentOWgfx16Ptr.ToPointer();  // loaded gfx for the current
  //                                            // map (empty at this point)
  byte* allgfxData = new byte[(128 * 7136) / 2];
  // (byte*)GFX.allgfx16Ptr
  //     .ToPointer();  // all gfx of the game pack of 2048 bytes (4bpp)

  for (int i = 0; i < 16; i++) {
    for (int j = 0; j < 2048; j++) {
      byte mapByte = allgfxData[j + (staticgfx[i] * 2048)];
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

}  // namespace Data
}  // namespace Application
}  // namespace yaze