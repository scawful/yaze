#include "overworld_map.h"

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

OverworldMap::OverworldMap(ROM& rom, const std::vector<gfx::Tile16>& tiles16,
                           int index_)
    : parent_(index_), index_(index_), rom_(rom), tiles16_(tiles16) {
  if (index_ != 0x80 && index_ <= 150 &&
      rom_.data()[core::overworldMapSize + (index_ & 0x3F)] != 0) {
    large_map_ = true;
  }

  if (index_ < 64) {
    sprite_graphics_[0] = rom_.data()[core::overworldSpriteset + parent_];
    sprite_graphics_[1] =
        rom_.data()[core::overworldSpriteset + parent_ + 64];
    sprite_graphics_[2] =
        rom_.data()[core::overworldSpriteset + parent_ + 128];
    gfx_ = rom_.data()[core::mapGfx + parent_];
    palette_ = rom_.data()[core::overworldMapPalette + parent_];
    sprite_palette_[0] =
        rom_.data()[core::overworldSpritePalette + parent_];
    sprite_palette_[1] =
        rom_.data()[core::overworldSpritePalette + parent_ + 64];
    sprite_palette_[2] =
        rom_.data()[core::overworldSpritePalette + parent_ + 128];
    musics[0] = rom_.data()[core::overworldMusicBegining + parent_];
    musics[1] = rom_.data()[core::overworldMusicZelda + parent_];
    musics[2] = rom_.data()[core::overworldMusicMasterSword + parent_];
    musics[3] = rom_.data()[core::overworldMusicAgahim + parent_];
  } else if (index_ < 128) {
    sprite_graphics_[0] =
        rom_.data()[core::overworldSpriteset + parent_ + 128];
    sprite_graphics_[1] =
        rom_.data()[core::overworldSpriteset + parent_ + 128];
    sprite_graphics_[2] =
        rom_.data()[core::overworldSpriteset + parent_ + 128];
    gfx_ = rom_.data()[core::mapGfx + parent_];
    palette_ = rom_.data()[core::overworldMapPalette + parent_];
    sprite_palette_[0] =
        rom_.data()[core::overworldSpritePalette + parent_ + 128];
    sprite_palette_[1] =
        rom_.data()[core::overworldSpritePalette + parent_ + 128];
    sprite_palette_[2] =
        rom_.data()[core::overworldSpritePalette + parent_ + 128];

    musics[0] = rom_.data()[core::overworldMusicDW + (parent_ - 64)];
  } else {
    if (index_ == 0x94) {
      parent_ = 128;
    } else if (index_ == 0x95) {
      parent_ = 03;
    } else if (index_ == 0x96)  // pyramid bg use 0x5B map
    {
      parent_ = 0x5B;
    } else if (index_ == 0x97)  // pyramid bg use 0x5B map
    {
      parent_ = 0x00;
    } else if (index_ == 156) {
      parent_ = 67;
    } else if (index_ == 157) {
      parent_ = 0;
    } else if (index_ == 158) {
      parent_ = 0;
    } else if (index_ == 159) {
      parent_ = 44;
    } else if (index_ == 136) {
      parent_ = 136;
    }

    message_id_ = rom_.data()[core::overworldMessages + parent_];

    sprite_graphics_[0] =
        rom_.data()[core::overworldSpriteset + parent_ + 128];
    sprite_graphics_[1] =
        rom_.data()[core::overworldSpriteset + parent_ + 128];
    sprite_graphics_[2] =
        rom_.data()[core::overworldSpriteset + parent_ + 128];
    sprite_palette_[0] =
        rom_.data()[core::overworldSpritePalette + parent_ + 128];
    sprite_palette_[1] =
        rom_.data()[core::overworldSpritePalette + parent_ + 128];
    sprite_palette_[2] =
        rom_.data()[core::overworldSpritePalette + parent_ + 128];

    palette_ = rom_.data()[core::overworldSpecialPALGroup + parent_ - 128];
    if (index_ >= 0x80 && index_ <= 0x8A && index_ != 0x88) {
      gfx_ = rom_.data()[core::overworldSpecialGFXGroup + (parent_ - 128)];
      palette_ = rom_.data()[core::overworldSpecialPALGroup + 1];
    } else if (index_ == 0x88) {
      gfx_ = 81;
      palette_ = 0;
    } else  // pyramid bg use 0x5B map
    {
      gfx_ = rom_.data()[core::mapGfx + parent_];
      palette_ = rom_.data()[core::overworldMapPalette + parent_];
    }
  }
}

void OverworldMap::BuildMap(int count, int game_state, uchar* map_parent,
                            OWMapTiles& map_tiles) {
  if (large_map_) {
    this->parent_ = map_parent[index_];

    if (parent_ != index_ && !initialized_) {
      if (index_ >= 0x80 && index_ <= 0x8A && index_ != 0x88) {
        gfx_ = rom_.data()[core::overworldSpecialGFXGroup +
                           (parent_ - 128)];
        palette_ = rom_.data()[core::overworldSpecialPALGroup + 1];
      } else if (index_ == 0x88) {
        gfx_ = 81;
        palette_ = 0;
      } else {
        gfx_ = rom_.data()[core::mapGfx + parent_];
        palette_ = rom_.data()[core::overworldMapPalette + parent_];
      }

      initialized_ = true;
    }
  }

  BuildTileset(game_state);
  BuildTiles16Gfx(count);  // build on GFX.mapgfx16Ptr

  int world = 0;
  if (index_ < 64) {
    tiles_used_ = map_tiles.light_world;
  } else if (index_ < 128 && index_ >= 64) {
    tiles_used_ = map_tiles.dark_world;
    world = 1;
  } else {
    tiles_used_ = map_tiles.special_world;
    world = 2;
  }

  int superY = ((index_ - (world * 64)) / 8);
  int superX = index_ - (world * 64) - (superY * 8);
  for (int y = 0; y < 32; y++) {
    for (int x = 0; x < 32; x++) {
      CopyTile8bpp16((x * 16), (y * 16),
                     tiles_used_[x + (superX * 32)][y + (superY * 32)], gfxPtr,
                     mapblockset16_);
    }
  }
}

void OverworldMap::CopyTile8bpp16(int x, int y, int tile, uchar* destbmpPtr,
                                  uchar* sourcebmpPtr) {
  int sourcePtrPos = ((tile - ((tile / 8) * 8)) * 16) +
                     ((tile / 8) * 2048);  // (sourceX * 16) + (sourceY * 128)
  auto sourcePtr = sourcebmpPtr;

  int destPtrPos = (x + (y * 512));
  auto destPtr = destbmpPtr;

  for (int ystrip = 0; ystrip < 16; ystrip++) {
    for (int xstrip = 0; xstrip < 16; xstrip++) {
      destPtr[destPtrPos + xstrip + (ystrip * 512)] =
          sourcePtr[sourcePtrPos + xstrip + (ystrip * 128)];
    }
  }
}

void OverworldMap::CopyTile8bpp16From8(int xP, int yP, int tileID,
                                       uchar* destbmpPtr, uchar* sourcebmpPtr) {
  auto gfx16Data = destbmpPtr;
  // TODO: PSEUDO VRAM
  auto gfx8Data = rom_.GetVRAM().GetGraphicsData();

  int offsets[] = {0, 8, 4096, 4104};

  auto tiles = tiles16_[tileID];

  for (auto tile = 0; tile < 4; tile++) {
    gfx::TileInfo info = tiles.tiles_info[tile];
    int offset = offsets[tile];

    for (auto y = 0; y < 8; y++) {
      for (auto x = 0; x < 4; x++) {
        CopyTileToMap(x, y, xP, yP, offset, info, gfx16Data, gfx8Data);
      }
    }
  }
}

void OverworldMap::BuildTiles16Gfx(int count) {
  auto gfx16Data = mapblockset16_;
  auto gfx8Data = rom_.GetVRAM().GetGraphicsData();

  int offsets[] = {0, 8, 1024, 1032};
  auto yy = 0;
  auto xx = 0;

  for (auto i = 0; i < count; i++)  // number of tiles16 3748?
  {
    // 8x8 tile draw
    // gfx8 = 4bpp so everyting is /2F
    auto tiles = tiles16_[i];

    for (auto tile = 0; tile < 4; tile++) {
      gfx::TileInfo info = tiles16_[i].tiles_info[tile];
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

// map,current
void OverworldMap::CopyTile(int x, int y, int xx, int yy, int offset,
                            gfx::TileInfo tile, uchar* gfx16Pointer,
                            uchar* gfx8Pointer) {
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

// map,current
void OverworldMap::CopyTileToMap(int x, int y, int xx, int yy, int offset,
                                 gfx::TileInfo tile, uchar* gfx16Pointer,
                                 uchar* gfx8Pointer) {
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
  if (parent_ < 0x40) {
    indexWorld = 0x20;
  } else if (parent_ >= 0x40 && parent_ < 0x80) {
    indexWorld = 0x21;
  } else if (parent_ == 0x88) {
    indexWorld = 36;
  }

  // Sprites Blocksets
  staticgfx[8] = 115 + 0;
  staticgfx[9] = 115 + 1;
  staticgfx[10] = 115 + 6;
  staticgfx[11] = 115 + 7;
  for (int i = 0; i < 4; i++) {
    staticgfx[12 + i] =
        (uchar)(rom_.data()[core::sprite_blockset_pointer +
                            (sprite_graphics_[gameState] * 4) + i] +
                115);
  }

  // Main Blocksets
  for (int i = 0; i < 8; i++) {
    staticgfx[i] =
        rom_.data()[core::overworldgfxGroups2 + (indexWorld * 8) + i];
  }

  if (rom_.data()[core::overworldgfxGroups + (gfx_ * 4)] != 0) {
    staticgfx[3] = rom_.data()[core::overworldgfxGroups + (gfx_ * 4)];
  }
  if (rom_.data()[core::overworldgfxGroups + (gfx_ * 4) + 1] != 0) {
    staticgfx[4] = rom_.data()[core::overworldgfxGroups + (gfx_ * 4) + 1];
  }
  if (rom_.data()[core::overworldgfxGroups + (gfx_ * 4) + 2] != 0) {
    staticgfx[5] = rom_.data()[core::overworldgfxGroups + (gfx_ * 4) + 2];
  }
  if (rom_.data()[core::overworldgfxGroups + (gfx_ * 4) + 3] != 0) {
    staticgfx[6] = rom_.data()[core::overworldgfxGroups + (gfx_ * 4) + 3];
  }

  // Hardcoded overworld GFX Values, for death mountain
  if ((parent_ >= 0x03 && parent_ <= 0x07) ||
      (parent_ >= 0x0B && parent_ <= 0x0E)) {
    staticgfx[7] = 89;
  } else if ((parent_ >= 0x43 && parent_ <= 0x47) ||
             (parent_ >= 0x4B && parent_ <= 0x4E)) {
    staticgfx[7] = 89;
  } else {
    staticgfx[7] = 91;
  }

  // TODO: PSEUDO VRAM DATA HERE
  uchar* currentmapgfx8Data = rom_.GetVRAM().GetGraphicsData();
  // TODO: PUT GRAPHICS DATA HERE
  uchar const* allgfxData = rom_.GetMasterGraphicsBin();

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