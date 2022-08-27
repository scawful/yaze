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
                           std::vector<gfx::Tile16>& tiles16)
    : parent_(index), index_(index), rom_(rom), tiles16_(tiles16) {
  LoadAreaInfo();
  current_overworld_map_.resize(512 * 512);
}

absl::Status OverworldMap::BuildMap(int count, int game_state, int world,
                                    uchar* map_parent,
                                    OWBlockset& world_blockset) {
  world_ = world;
  if (large_map_) {
    parent_ = map_parent[index_];
    if (parent_ != index_ && !initialized_) {
      if (index_ >= 0x80 && index_ <= 0x8A && index_ != 0x88) {
        area_graphics_ =
            rom_[core::overworldSpecialGFXGroup + (parent_ - 0x80)];
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

  int world_index = 0x20;
  if (parent_ < 0x40) {
    world_index = 0x20;
  } else if (parent_ >= 0x40 && parent_ < 0x80) {
    world_index = 0x21;
  } else if (parent_ == 0x88) {
    world_index = 36;
  }

  LoadAreaGraphics(game_state, world_index);
  RETURN_IF_ERROR(BuildTileset())
  RETURN_IF_ERROR(BuildTiles16Gfx(count))

  int superY = ((index_ - (world * 64)) / 8);
  int superX = index_ - (world * 64) - (superY * 8);
  for (int y = 0; y < 32; y++) {
    for (int x = 0; x < 32; x++) {
      auto xt = x + (superX * 32);
      auto yt = y + (superY * 32);
      CopyTile8bpp16((x * 16), (y * 16), world_blockset[xt][yt]);
    }
  }

  bitmap_.Create(512, 512, 8, current_overworld_map_.data());
  bitmap_.CreateTexture(rom_.GetRenderer());
  built_ = true;
  return absl::OkStatus();
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
    sprite_graphics_[2] = rom_[core::overworldSpriteset + parent_ + 0x80];

    sprite_palette_[0] = rom_[core::overworldSpritePalette + parent_];
    sprite_palette_[1] = rom_[core::overworldSpritePalette + parent_ + 64];
    sprite_palette_[2] = rom_[core::overworldSpritePalette + parent_ + 0x80];
  } else if (index_ < 0x80) {
    area_graphics_ = rom_[core::mapGfx + parent_];
    area_palette_ = rom_[core::overworldMapPalette + parent_];
    area_music_[0] = rom_[core::overworldMusicDW + (parent_ - 64)];

    sprite_graphics_[0] = rom_[core::overworldSpriteset + parent_ + 0x80];
    sprite_graphics_[1] = rom_[core::overworldSpriteset + parent_ + 0x80];
    sprite_graphics_[2] = rom_[core::overworldSpriteset + parent_ + 0x80];

    sprite_palette_[0] = rom_[core::overworldSpritePalette + parent_ + 0x80];
    sprite_palette_[1] = rom_[core::overworldSpritePalette + parent_ + 0x80];
    sprite_palette_[2] = rom_[core::overworldSpritePalette + parent_ + 0x80];
  } else {
    if (index_ == 0x94) {
      parent_ = 0x80;
    } else if (index_ == 0x95) {
      parent_ = 0x03;
    } else if (index_ == 0x96) {
      parent_ = 0x5B;  // pyramid bg use 0x5B map
    } else if (index_ == 0x97) {
      parent_ = 0x00;  // pyramid bg use 0x5B map
    } else if (index_ == 0x9C) {
      parent_ = 0x43;
    } else if (index_ == 0x9D) {
      parent_ = 0x00;
    } else if (index_ == 0x9E) {
      parent_ = 0x00;
    } else if (index_ == 0x9F) {
      parent_ = 0x2C;
    } else if (index_ == 0x88) {
      parent_ = 0x88;
    }

    area_palette_ = rom_[core::overworldSpecialPALGroup + parent_ - 0x80];
    if (index_ >= 0x80 && index_ <= 0x8A && index_ != 0x88) {
      area_graphics_ = rom_[core::overworldSpecialGFXGroup + (parent_ - 0x80)];
      area_palette_ = rom_[core::overworldSpecialPALGroup + 1];
    } else if (index_ == 0x88) {
      area_graphics_ = 0x51;
      area_palette_ = 0x00;
    } else {
      // pyramid bg use 0x5B map
      area_graphics_ = rom_[core::mapGfx + parent_];
      area_palette_ = rom_[core::overworldMapPalette + parent_];
    }

    message_id_ = rom_[core::overworldMessages + parent_];

    sprite_graphics_[0] = rom_[core::overworldSpriteset + parent_ + 0x80];
    sprite_graphics_[1] = rom_[core::overworldSpriteset + parent_ + 0x80];
    sprite_graphics_[2] = rom_[core::overworldSpriteset + parent_ + 0x80];

    sprite_palette_[0] = rom_[core::overworldSpritePalette + parent_ + 0x80];
    sprite_palette_[1] = rom_[core::overworldSpritePalette + parent_ + 0x80];
    sprite_palette_[2] = rom_[core::overworldSpritePalette + parent_ + 0x80];
  }
}

void OverworldMap::LoadAreaGraphics(int game_state, int world_index) {
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
        rom_[core::overworldgfxGroups2 + (world_index * 8) + i];
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
}

absl::Status OverworldMap::BuildTileset() {
  auto all_gfx_data = rom_.GetGraphicsBin();
  for (int i = 0; i < 16; i++) {
    current_graphics_sheet_set[i] = all_gfx_data[static_graphics_[i]];
  }
  current_gfx_.reserve(16 * 2048);
  for (int i = 0; i < 16; ++i) {
    for (int j = 0; j < 2048; ++j) {
      current_gfx_.emplace_back(current_graphics_sheet_set[i].GetByte(j));
    }
  }
  return absl::OkStatus();
}

absl::Status OverworldMap::BuildTiles16Gfx(int count) {
  int offsets[] = {0, 8, 1024, 1032};
  auto yy = 0;
  auto xx = 0;

  // number of tiles16 3748?
  for (auto i = 0; i < count; i++) {
    // 8x8 tile draw, gfx8 = 4bpp so everyting is /2F
    for (auto tile = 0; tile < 4; tile++) {
      gfx::TileInfo info;
      switch (tile) {
        case 0:
          info = tiles16_[i].tile0_;
          break;
        case 1:
          info = tiles16_[i].tile1_;
          break;
        case 2:
          info = tiles16_[i].tile2_;
          break;
        case 3:
          info = tiles16_[i].tile3_;
          break;
      }
      int offset = offsets[tile];

      for (auto y = 0; y < 8; y++) {
        for (auto x = 0; x < 4; x++) {
          CopyTile(x, y, xx, yy, offset, info);
        }
      }
    }

    xx += 16;
    if (xx >= 0x80) {
      yy += 2048;
      xx = 0;
    }
  }

  return absl::OkStatus();
}

// map,current
void OverworldMap::CopyTile(int x, int y, int xx, int yy, int offset,
                            gfx::TileInfo tile) {
  auto map_gfx_data = current_overworld_map_.data();
  auto current_gfx_data = current_gfx_.data();
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
  auto index = xx + yy + offset + (mx * 2) + (my * 0x80);
  auto pixel = current_gfx_data[tx + (y * 64) + x];

  auto p1 = index + r ^ 1;
  map_gfx_data[p1] = (uchar)((pixel & 0x0F) + tile.palette_ * 16);
  auto p2 = index + r;
  map_gfx_data[p2] = (uchar)(((pixel >> 4) & 0x0F) + tile.palette_ * 16);
}

void OverworldMap::CopyTile8bpp16(int x, int y, int tile) {
  // (sourceX * 16) + (sourceY * 0x80)
  int source_ptr_pos = ((tile - ((tile / 8) * 8)) * 16) + ((tile / 8) * 2048);
  auto source_ptr = current_gfx_.data();

  int dest_ptr_pos = (x + (y * 512));
  auto dest_ptr = current_overworld_map_.data();

  for (int ystrip = 0; ystrip < 16; ystrip++) {
    for (int xstrip = 0; xstrip < 16; xstrip++) {
      dest_ptr[dest_ptr_pos + xstrip + (ystrip * 512)] =
          source_ptr[source_ptr_pos + xstrip + (ystrip * 0x80)];
    }
  }
}

}  // namespace zelda3
}  // namespace app
}  // namespace yaze