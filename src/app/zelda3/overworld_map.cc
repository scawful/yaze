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

OverworldMap::OverworldMap(int index, ROM& rom,
                           const std::vector<gfx::Tile16>& tiles16)
    : parent_(index_), index(index_), rom_(rom), tiles16_(tiles16) {
  if (index_ != 0x80 && index_ <= 150 &&
      rom_.data()[core::overworldMapSize + (index_ & 0x3F)] != 0) {
    large_map_ = true;
  }
  LoadAreaInfo();
}

void Overworld::LoadAreaInfo() {
  auto z3data = rom_.data();

  if (index_ < 64) {
    area_graphics_ = z3data[core::mapGfx + parent_];
    area_palette_ = z3data[core::overworldMapPalette + parent_];

    area_music_[0] = z3data[core::overworldMusicBegining + parent_];
    area_music_[1] = z3data[core::overworldMusicZelda + parent_];
    area_music_[2] = z3data[core::overworldMusicMasterSword + parent_];
    area_music_[3] = z3data[core::overworldMusicAgahim + parent_];

    sprite_graphics_[0] = z3data[core::overworldSpriteset + parent_];
    sprite_graphics_[1] = z3data[core::overworldSpriteset + parent_ + 64];
    sprite_graphics_[2] = z3data[core::overworldSpriteset + parent_ + 128];

    sprite_palette_[0] = z3data[core::overworldSpritePalette + parent_];
    sprite_palette_[1] = z3data[core::overworldSpritePalette + parent_ + 64];
    sprite_palette_[2] = z3data[core::overworldSpritePalette + parent_ + 128];
  } else if (index_ < 128) {
    area_graphics_ = z3data[core::mapGfx + parent_];
    area_palette_ = z3data[core::overworldMapPalette + parent_];
    area_music_[0] = z3data[core::overworldMusicDW + (parent_ - 64)];

    sprite_graphics_[0] = z3data[core::overworldSpriteset + parent_ + 128];
    sprite_graphics_[1] = z3data[core::overworldSpriteset + parent_ + 128];
    sprite_graphics_[2] = z3data[core::overworldSpriteset + parent_ + 128];

    sprite_palette_[0] = z3data[core::overworldSpritePalette + parent_ + 128];
    sprite_palette_[1] = z3data[core::overworldSpritePalette + parent_ + 128];
    sprite_palette_[2] = z3data[core::overworldSpritePalette + parent_ + 128];
  } else {
    if (index_ == 0x94) {
      parent_ = 128;
    } else if (index_ == 0x95) {
      parent_ = 03;
    } else if (index_ == 0x96) {
      parent_ = 0x5B;  // pyramid bg use 0x5B map
    } else if (index_ == 0x97) {
      parent_ = 0x00;  // pyramid bg use 0x5B map
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

    area_palette_ = z3data[core::overworldSpecialPALGroup + parent_ - 128];
    if (index_ >= 0x80 && index_ <= 0x8A && index_ != 0x88) {
      area_graphics_ = z3data[core::overworldSpecialGFXGroup + (parent_ - 128)];
      area_palette_ = z3data[core::overworldSpecialPALGroup + 1];
    } else if (index_ == 0x88) {
      area_graphics_ = 81;
      area_palette_ = 0;
    } else  // pyramid bg use 0x5B map
    {
      area_graphics_ = z3data[core::mapGfx + parent_];
      area_palette_ = z3data[core::overworldMapPalette + parent_];
    }

    message_id_ = z3data[core::overworldMessages + parent_];

    sprite_graphics_[0] = z3data[core::overworldSpriteset + parent_ + 128];
    sprite_graphics_[1] = z3data[core::overworldSpriteset + parent_ + 128];
    sprite_graphics_[2] = z3data[core::overworldSpriteset + parent_ + 128];

    sprite_palette_[0] = z3data[core::overworldSpritePalette + parent_ + 128];
    sprite_palette_[1] = z3data[core::overworldSpritePalette + parent_ + 128];
    sprite_palette_[2] = z3data[core::overworldSpritePalette + parent_ + 128];
  }
}

void OverworldMap::BuildMap(int count, int game_state, uchar* map_parent,
                            OWMapTiles& map_tiles) {
  if (large_map_) {
    parent_ = map_parent[index_];
    if (parent_ != index_ && !initialized_) {
      if (index_ >= 0x80 && index_ <= 0x8A && index_ != 0x88) {
        area_graphics_ =
            rom_.data()[core::overworldSpecialGFXGroup + (parent_ - 128)];
        area_palette_ = rom_.data()[core::overworldSpecialPALGroup + 1];
      } else if (index_ == 0x88) {
        area_graphics_ = 81;
        area_palette_ = 0;
      } else {
        area_graphics_ = rom_.data()[core::mapGfx + parent_];
        area_palette_ = rom_.data()[core::overworldMapPalette + parent_];
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
                     tiles_used_[x + (superX * 32)][y + (superY * 32)], gfxPtr);
    }
  }
}

void OverworldMap::BuildTileset(int gameState) {
  int index_world = 0x20;
  if (parent_ < 0x40) {
    index_world = 0x20;
  } else if (parent_ >= 0x40 && parent_ < 0x80) {
    index_world = 0x21;
  } else if (parent_ == 0x88) {
    index_world = 36;
  }

  // Sprites Blocksets
  static_graphics_[8] = 115 + 0;
  static_graphics_[9] = 115 + 1;
  static_graphics_[10] = 115 + 6;
  static_graphics_[11] = 115 + 7;
  for (int i = 0; i < 4; i++) {
    static_graphics_[12 + i] =
        (uchar)(rom_.data()[core::sprite_blockset_pointer +
                            (sprite_graphics_[gameState] * 4) + i] +
                115);
  }

  // Main Blocksets
  for (int i = 0; i < 8; i++) {
    static_graphics_[i] =
        rom_.data()[core::overworldgfxGroups2 + (index_world * 8) + i];
  }

  if (rom_.data()[core::overworldgfxGroups + (area_graphics_ * 4)] != 0) {
    static_graphics_[3] =
        rom_.data()[core::overworldgfxGroups + (area_graphics_ * 4)];
  }
  if (rom_.data()[core::overworldgfxGroups + (area_graphics_ * 4) + 1] != 0) {
    static_graphics_[4] =
        rom_.data()[core::overworldgfxGroups + (area_graphics_ * 4) + 1];
  }
  if (rom_.data()[core::overworldgfxGroups + (area_graphics_ * 4) + 2] != 0) {
    static_graphics_[5] =
        rom_.data()[core::overworldgfxGroups + (area_graphics_ * 4) + 2];
  }
  if (rom_.data()[core::overworldgfxGroups + (area_graphics_ * 4) + 3] != 0) {
    static_graphics_[6] =
        rom_.data()[core::overworldgfxGroups + (area_graphics_ * 4) + 3];
  }

  // Hardcoded overworld GFX Values, for death mountain
  if ((parent_ >= 0x03 && parent_ <= 0x07) ||
      (parent_ >= 0x0B && parent_ <= 0x0E)) {
    static_graphics_[7] = 89;
  } else if ((parent_ >= 0x43 && parent_ <= 0x47) ||
             (parent_ >= 0x4B && parent_ <= 0x4E)) {
    static_graphics_[7] = 89;
  } else {
    static_graphics_[7] = 91;
  }

  uchar* current_map_gfx_tile8_data = rom_.GetVRAM().GetGraphicsData();
  uchar const* all_gfx_data = rom_.GetMasterGraphicsBin();

  for (int i = 0; i < 16; i++) {
    for (int j = 0; j < 2048; j++) {
      uchar mapByte = all_gfx_data[j + (static_graphics_[i] * 2048)];
      switch (i) {
        case 0:
        case 3:
        case 4:
        case 5:
          mapByte += 0x88;
          break;
      }
      // Upload used gfx data
      current_map_gfx_tile8_data[(i * 2048) + j] = mapByte;
    }
  }
}

void OverworldMap::BuildTiles16Gfx(int count) {
  auto gfx_tile16_data = tile16_blockset_bmp_.GetData();
  auto gfx_tile8_data = rom_.GetVRAM().GetGraphicsData();

  int offsets[] = {0, 8, 1024, 1032};
  auto yy = 0;
  auto xx = 0;

  // number of tiles16 3748?
  for (auto i = 0; i < count; i++) {
    // 8x8 tile draw
    // gfx8 = 4bpp so everyting is /2F
    auto tiles = tiles16_[i];

    for (auto tile = 0; tile < 4; tile++) {
      gfx::TileInfo info = tiles16_[i].tiles_info[tile];
      int offset = offsets[tile];

      for (auto y = 0; y < 8; y++) {
        for (auto x = 0; x < 4; x++) {
          CopyTile(x, y, xx, yy, offset, info, gfx_tile16_data, gfx_tile8_data);
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

void OverworldMap::CopyTile8bpp16(int x, int y, int tile, uchar* destbmpPtr) {
  // (sourceX * 16) + (sourceY * 128)
  int source_ptr_pos = ((tile - ((tile / 8) * 8)) * 16) + ((tile / 8) * 2048);
  auto source_ptr = tile16_blockset_bmp_.GetData();

  int dest_ptr_pos = (x + (y * 512));
  auto dest_ptr = destbmpPtr;

  for (int ystrip = 0; ystrip < 16; ystrip++) {
    for (int xstrip = 0; xstrip < 16; xstrip++) {
      dest_ptr[dest_ptr_pos + xstrip + (ystrip * 512)] =
          source_ptr[source_ptr_pos + xstrip + (ystrip * 128)];
    }
  }
}

void OverworldMap::CopyTile8bpp16From8(int xP, int yP, int tileID,
                                       uchar* destbmpPtr) {
  auto gfx_tile16_data = destbmpPtr;
  auto gfx_tile8_data = rom_.GetVRAM().GetGraphicsData();

  auto tiles = tiles16_[tileID];

  for (auto tile = 0; tile < 4; tile++) {
    gfx::TileInfo info = tiles.tiles_info[tile];
    int offset = kTileOffsets[tile];

    for (auto y = 0; y < 8; y++) {
      for (auto x = 0; x < 4; x++) {
        CopyTileToMap(x, y, xP, yP, offset, info, gfx_tile16_data,
                      gfx_tile8_data);
      }
    }
  }
}

}  // namespace zelda3
}  // namespace app
}  // namespace yaze