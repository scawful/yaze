#include "overworld_map.h"

#include <imgui/imgui.h>

#include <cstddef>
#include <cstdint>
#include <memory>
#include <unordered_map>
#include <vector>

#include "app/core/common.h"
#include "app/gfx/bitmap.h"
#include "app/gfx/snes_tile.h"
#include "app/rom.h"

namespace yaze {
namespace app {
namespace zelda3 {

namespace {

void CopyTile(int x, int y, int xx, int yy, int offset, gfx::TileInfo tile,
              Bytes& ow_blockset, Bytes& current_gfx) {
  int mx = x;
  int my = y;
  uchar r = 0x00;

  if (tile.horizontal_mirror_ != 0) {
    mx = 0x03 - x;
    r = 0x01;
  }

  if (tile.vertical_mirror_ != 0) {
    my = 0x07 - y;
  }

  int tx = ((tile.id_ / 0x10) * 0x200) +
           ((tile.id_ - ((tile.id_ / 0x10) * 0x10)) * 0x04);
  auto index = xx + yy + offset + (mx * 0x02) + (my * 0x80);
  auto pixel = current_gfx[tx + (y * 0x40) + x];

  auto p1 = index + r ^ 1;
  ow_blockset[p1] = (uchar)((pixel & 0x0F) + tile.palette_ * 0x10);
  auto p2 = index + r;
  ow_blockset[p2] = (uchar)(((pixel >> 0x04) & 0x0F) + tile.palette_ * 0x10);
}

void CopyTile8bpp8(int x, int y, int xx, int yy, int offset, gfx::TileInfo tile,
                   Bytes& ow_blockset, Bytes& current_gfx) {
  int mx = x;
  int my = y;
  uchar r = 0x00;

  if (tile.horizontal_mirror_ != 0) {
    mx = 0x07 - x;
    r = 0x01;
  }

  if (tile.vertical_mirror_ != 0) {
    my = 0x07 - y;
  }

  int tx = ((tile.id_ / 0x20) * 0x800) +
           ((tile.id_ - ((tile.id_ / 0x20) * 0x20)) * 0x08);
  auto pixel = current_gfx[tx + x + (y * 0x80)];
  auto index = xx + yy + offset + mx + (my * 0x80);

  ow_blockset[index] = (uchar)(pixel + (tile.palette_ * 0x10));
}

void CopyTile8bpp16(int x, int y, int tile, Bytes& bitmap, Bytes& blockset) {
  int src_pos =
      ((tile - ((tile / 0x08) * 0x08)) * 0x10) + ((tile / 0x08) * 2048);
  int dest_pos = (x + (y * 0x200));
  for (int yy = 0; yy < 0x10; yy++) {
    for (int xx = 0; xx < 0x10; xx++) {
      bitmap[dest_pos + xx + (yy * 0x200)] =
          blockset[src_pos + xx + (yy * 0x80)];
    }
  }
}

}  // namespace

OverworldMap::OverworldMap(int index, ROM& rom,
                           std::vector<gfx::Tile16>& tiles16)
    : parent_(index), index_(index), rom_(rom), tiles16_(tiles16) {
  LoadAreaInfo();
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
  RETURN_IF_ERROR(BuildBitmap(world_blockset))
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
  for (int i = 0; i < 16; i++) {
    auto sheet = rom_.GetGraphicsBin().at(static_graphics_[i]);
    current_graphics_sheet_set[i] = sheet;
  }

  all_gfx_ = rom_.GetGraphics8BPP();

  current_gfx_.reserve(65536);
  for (int i = 0; i < 65536; i++) {
    current_gfx_.push_back(0x00);
  }

  for (int i = 0; i < 16; i++) {
    for (int j = 0; j < 4096; j++) {
      current_gfx_[(i * 4096) + j] = all_gfx_[j + (static_graphics_[i] * 4096)];
    }
  }
  return absl::OkStatus();
}

absl::Status OverworldMap::BuildTiles16Gfx(int count) {
  current_blockset_.reserve(0x100000);
  for (int i = 0; i < 0x100000; i++) {
    current_blockset_.push_back(0x00);
  }

  int offsets[] = {0x00, 0x08, 0x400, 0x408};
  auto yy = 0;
  auto xx = 0;

  for (auto i = 0; i < count; i++) {
    for (auto tile = 0; tile < 0x04; tile++) {
      gfx::TileInfo info = tiles16_[i].tiles_info[tile];
      int offset = offsets[tile];
      for (auto y = 0; y < 0x08; y++) {
        for (auto x = 0; x < 0x08; x++) {
          CopyTile8bpp8(x, y, xx, yy, offset, info, current_blockset_,
                        current_gfx_);
        }
      }
    }

    xx += 0x10;
    if (xx >= 0x80) {
      yy += 0x800;
      xx = 0;
    }
  }

  return absl::OkStatus();
}

absl::Status OverworldMap::BuildBitmap(OWBlockset& world_blockset) {
  bitmap_data_.reserve(262144);
  for (int i = 0; i < 262144; i++) {
    bitmap_data_.push_back(0x00);
  }

  int superY = ((index_ - (world_ * 64)) / 8);
  int superX = index_ - (world_ * 64) - (superY * 8);

  for (int y = 0; y < 32; y++) {
    for (int x = 0; x < 32; x++) {
      auto xt = x + (superX * 32);
      auto yt = y + (superY * 32);
      CopyTile8bpp16((x * 16), (y * 16), world_blockset[xt][yt], bitmap_data_,
                     current_blockset_);
    }
  }
  return absl::OkStatus();
}

}  // namespace zelda3
}  // namespace app
}  // namespace yaze