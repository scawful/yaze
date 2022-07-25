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
    : parent_(index), index_(index), rom_(rom), tiles16_(tiles16) {
  LoadAreaInfo();
  bitmap_.Create(512, 512, 8, 512 * 512);
}

void OverworldMap::LoadAreaInfo() {
  if (index_ != 0x80 && index_ <= 150 &&
      rom_[core::overworldMapSize + (index_ & 0x3F)] != 0) {
    large_map_ = true;
  }
  if (index_ < 64) {
    area_graphics_ = rom_[core::mapGfx + parent_];
    area_palette_ = rom_[core::overworldMapPalette + parent_];

    area_music_[0] = rom_[core::overworldMusicBegining + parent_];
    area_music_[1] = rom_[core::overworldMusicZelda + parent_];
    area_music_[2] = rom_[core::overworldMusicMasterSword + parent_];
    area_music_[3] = rom_[core::overworldMusicAgahim + parent_];

    sprite_graphics_[0] = rom_[core::overworldSpriteset + parent_];
    sprite_graphics_[1] = rom_[core::overworldSpriteset + parent_ + 64];
    sprite_graphics_[2] = rom_[core::overworldSpriteset + parent_ + 128];

    sprite_palette_[0] = rom_[core::overworldSpritePalette + parent_];
    sprite_palette_[1] = rom_[core::overworldSpritePalette + parent_ + 64];
    sprite_palette_[2] = rom_[core::overworldSpritePalette + parent_ + 128];
  } else if (index_ < 128) {
    area_graphics_ = rom_[core::mapGfx + parent_];
    area_palette_ = rom_[core::overworldMapPalette + parent_];
    area_music_[0] = rom_[core::overworldMusicDW + (parent_ - 64)];

    sprite_graphics_[0] = rom_[core::overworldSpriteset + parent_ + 128];
    sprite_graphics_[1] = rom_[core::overworldSpriteset + parent_ + 128];
    sprite_graphics_[2] = rom_[core::overworldSpriteset + parent_ + 128];

    sprite_palette_[0] = rom_[core::overworldSpritePalette + parent_ + 128];
    sprite_palette_[1] = rom_[core::overworldSpritePalette + parent_ + 128];
    sprite_palette_[2] = rom_[core::overworldSpritePalette + parent_ + 128];
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

    area_palette_ = rom_[core::overworldSpecialPALGroup + parent_ - 128];
    if (index_ >= 0x80 && index_ <= 0x8A && index_ != 0x88) {
      area_graphics_ = rom_[core::overworldSpecialGFXGroup + (parent_ - 128)];
      area_palette_ = rom_[core::overworldSpecialPALGroup + 1];
    } else if (index_ == 0x88) {
      area_graphics_ = 81;
      area_palette_ = 0;
    } else {
      // pyramid bg use 0x5B map
      area_graphics_ = rom_[core::mapGfx + parent_];
      area_palette_ = rom_[core::overworldMapPalette + parent_];
    }

    message_id_ = rom_[core::overworldMessages + parent_];

    sprite_graphics_[0] = rom_[core::overworldSpriteset + parent_ + 128];
    sprite_graphics_[1] = rom_[core::overworldSpriteset + parent_ + 128];
    sprite_graphics_[2] = rom_[core::overworldSpriteset + parent_ + 128];

    sprite_palette_[0] = rom_[core::overworldSpritePalette + parent_ + 128];
    sprite_palette_[1] = rom_[core::overworldSpritePalette + parent_ + 128];
    sprite_palette_[2] = rom_[core::overworldSpritePalette + parent_ + 128];
  }
}

void OverworldMap::BuildMap(int count, int game_state, uchar* map_parent,
                            uchar* ow_blockset, OWMapTiles& map_tiles) {
  if (large_map_) {
    parent_ = map_parent[index_];
    if (parent_ != index_ && !initialized_) {
      if (index_ >= 0x80 && index_ <= 0x8A && index_ != 0x88) {
        area_graphics_ = rom_[core::overworldSpecialGFXGroup + (parent_ - 128)];
        area_palette_ = rom_[core::overworldSpecialPALGroup + 1];
      } else if (index_ == 0x88) {
        area_graphics_ = 81;
        area_palette_ = 0;
      } else {
        area_graphics_ = rom_[core::mapGfx + parent_];
        area_palette_ = rom_[core::overworldMapPalette + parent_];
      }

      initialized_ = true;
    }
  }

  BuildTileset(game_state);
  BuildTiles16Gfx(count, ow_blockset);  // build on GFX.mapgfx16Ptr

  // int world = 0;
  // if (index_ < 64) {
  //   map_tiles_ = map_tiles.light_world;
  // } else if (index_ < 128 && index_ >= 64) {
  //   map_tiles_ = map_tiles.dark_world;
  //   world = 1;
  // } else {
  //   map_tiles_ = map_tiles.special_world;
  //   world = 2;
  // }

  // int superY = ((index_ - (world * 64)) / 8);
  // int superX = index_ - (world * 64) - (superY * 8);
  // for (int y = 0; y < 32; y++) {
  //   for (int x = 0; x < 32; x++) {
  //     auto xt = x + (superX * 32);
  //     auto yt = y + (superY * 32);
  //     CopyTile8bpp16((x * 16), (y * 16), map_tiles_[xt][yt], ow_blockset);
  //   }
  // }
}

absl::Status OverworldMap::BuildMapV2(int count, int game_state,
                                      uchar* map_parent) {
  if (large_map_) {
    parent_ = map_parent[index_];
    if (parent_ != index_ && !initialized_) {
      if (index_ >= 0x80 && index_ <= 0x8A && index_ != 0x88) {
        area_graphics_ = rom_[core::overworldSpecialGFXGroup + (parent_ - 128)];
        area_palette_ = rom_[core::overworldSpecialPALGroup + 1];
      } else if (index_ == 0x88) {
        area_graphics_ = 81;
        area_palette_ = 0;
      } else {
        area_graphics_ = rom_[core::mapGfx + parent_];
        area_palette_ = rom_[core::overworldMapPalette + parent_];
      }

      initialized_ = true;
    }
  }

  auto tileset_status = BuildTileset(game_state);
  if (!tileset_status.ok()) {
    return tileset_status;
  }

  // if (index_ < 64) {
  //   world_ = 0;
  // } else if (index_ < 128 && index_ >= 64) {
  //   world_ = 1;
  // } else {
  //   world_ = 2;
  // }

  return absl::OkStatus();
}

absl::Status OverworldMap::BuildTileset(int game_state) {
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
    static_graphics_[12 + i] = (rom_[core::kSpriteBlocksetPointer +
                                     (sprite_graphics_[game_state] * 4) + i] +
                                115);
  }

  // Main Blocksets
  for (int i = 0; i < 8; i++) {
    static_graphics_[i] =
        rom_[core::overworldgfxGroups2 + (index_world * 8) + i];
  }

  if (rom_[core::overworldgfxGroups + (area_graphics_ * 4)] != 0) {
    static_graphics_[3] = rom_[core::overworldgfxGroups + (area_graphics_ * 4)];
  }
  if (rom_[core::overworldgfxGroups + (area_graphics_ * 4) + 1] != 0) {
    static_graphics_[4] =
        rom_[core::overworldgfxGroups + (area_graphics_ * 4) + 1];
  }
  if (rom_[core::overworldgfxGroups + (area_graphics_ * 4) + 2] != 0) {
    static_graphics_[5] =
        rom_[core::overworldgfxGroups + (area_graphics_ * 4) + 2];
  }
  if (rom_[core::overworldgfxGroups + (area_graphics_ * 4) + 3] != 0) {
    static_graphics_[6] =
        rom_[core::overworldgfxGroups + (area_graphics_ * 4) + 3];
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

  auto all_gfx_data = rom_.GetGraphicsBin();
  for (int i = 0; i < 16; i++) {
    current_graphics_sheet_set[i] = all_gfx_data[static_graphics_[i]];
  }
  return absl::OkStatus();
}

void OverworldMap::BuildTiles16Gfx(int count, uchar* ow_blockset) {
  auto gfx_tile16_data = ow_blockset;
  auto gfx_tile8_data = nullptr;  // rom_.GetMasterGraphicsBin();

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

absl::Status OverworldMap::BuildTiles16GfxV2(int count) {
  auto gfx_tile8_data = nullptr;  // rom_.GetMasterGraphicsBin();

  int offsets[] = {0, 8, 1024, 1032};
  auto yy = 0;
  auto xx = 0;

  // number of tiles16 3748?
  for (auto i = 0; i < count; i++) {
    // 8x8 tile draw, gfx8 = 4bpp so everyting is /2F
    auto tiles = tiles16_[i];

    for (auto tile = 0; tile < 4; tile++) {
      gfx::TileInfo info = tiles16_[i].tiles_info[tile];
      int offset = offsets[tile];

      // for (auto y = 0; y < 8; y++) {
      //   for (auto x = 0; x < 4; x++) {
      //     CopyTile(x, y, xx, yy, offset, info, gfx_tile16_data,
      //     gfx_tile8_data);
      //   }
      // }
    }

    xx += 16;
    if (xx >= 128) {
      yy += 2048;
      xx = 0;
    }
  }

  return absl::OkStatus();
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

void OverworldMap::CopyTile8bpp16(int x, int y, int tile, uchar* ow_blockset) {
  // (sourceX * 16) + (sourceY * 128)
  int source_ptr_pos = ((tile - ((tile / 8) * 8)) * 16) + ((tile / 8) * 2048);
  auto source_ptr = ow_blockset;

  int dest_ptr_pos = (x + (y * 512));
  auto dest_ptr = bitmap_.GetData();

  for (int ystrip = 0; ystrip < 16; ystrip++) {
    for (int xstrip = 0; xstrip < 16; xstrip++) {
      dest_ptr[dest_ptr_pos + xstrip + (ystrip * 512)] =
          source_ptr[source_ptr_pos + xstrip + (ystrip * 128)];
    }
  }
}

// EXPERIMENTAL ----------------------------------------------------------------

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

}  // namespace zelda3
}  // namespace app
}  // namespace yaze