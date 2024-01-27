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
#include "app/zelda3/overworld.h"

namespace yaze {
namespace app {
namespace zelda3 {

namespace {

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

void SetColorsPalette(ROM& rom, int index, gfx::SNESPalette& current,
                      gfx::SNESPalette main, gfx::SNESPalette animated,
                      gfx::SNESPalette aux1, gfx::SNESPalette aux2,
                      gfx::SNESPalette hud, gfx::SNESColor bgrcolor,
                      gfx::SNESPalette spr, gfx::SNESPalette spr2) {
  // Palettes infos, color 0 of a palette is always transparent (the arrays
  // contains 7 colors width wide) There is 16 color per line so 16*Y

  // Left side of the palette - Main, Animated
  std::vector<gfx::SNESColor> new_palette(256);

  // Main Palette, Location 0,2 : 35 colors [7x5]
  int k = 0;
  for (int y = 2; y < 7; y++) {
    for (int x = 1; x < 8; x++) {
      new_palette[x + (16 * y)] = main[k];
      k++;
    }
  }

  // Animated Palette, Location 0,7 : 7colors
  for (int x = 1; x < 8; x++) {
    new_palette[(16 * 7) + (x)] = animated[(x - 1)];
  }

  // Right side of the palette - Aux1, Aux2

  // Aux1 Palette, Location 8,2 : 21 colors [7x3]
  k = 0;
  for (int y = 2; y < 5; y++) {
    for (int x = 9; x < 16; x++) {
      new_palette[x + (16 * y)] = aux1[k];
      k++;
    }
  }

  // Aux2 Palette, Location 8,5 : 21 colors [7x3]
  k = 0;
  for (int y = 5; y < 8; y++) {
    for (int x = 9; x < 16; x++) {
      new_palette[x + (16 * y)] = aux2[k];
      k++;
    }
  }

  // Hud Palette, Location 0,0 : 32 colors [16x2]
  for (int i = 0; i < 32; i++) {
    new_palette[i] = hud[i];
  }

  // Hardcoded grass color (that might change to become invisible instead)
  for (int i = 0; i < 8; i++) {
    new_palette[(i * 16)] = bgrcolor;
    new_palette[(i * 16) + 8] = bgrcolor;
  }

  // Sprite Palettes
  k = 0;
  for (int y = 8; y < 9; y++) {
    for (int x = 1; x < 8; x++) {
      new_palette[x + (16 * y)] = rom.palette_group("sprites_aux1")[1][k];
      k++;
    }
  }

  // Sprite Palettes
  k = 0;
  for (int y = 8; y < 9; y++) {
    for (int x = 9; x < 16; x++) {
      new_palette[x + (16 * y)] = rom.palette_group("sprites_aux3")[0][k];
      k++;
    }
  }

  // Sprite Palettes
  k = 0;
  for (int y = 9; y < 13; y++) {
    for (int x = 1; x < 16; x++) {
      new_palette[x + (16 * y)] = rom.palette_group("global_sprites")[0][k];
      k++;
    }
  }

  // Sprite Palettes
  k = 0;
  for (int y = 13; y < 14; y++) {
    for (int x = 1; x < 8; x++) {
      new_palette[x + (16 * y)] = spr[k];
      k++;
    }
  }

  // Sprite Palettes
  k = 0;
  for (int y = 14; y < 15; y++) {
    for (int x = 1; x < 8; x++) {
      new_palette[x + (16 * y)] = spr2[k];
      k++;
    }
  }

  // Sprite Palettes
  k = 0;
  for (int y = 15; y < 16; y++) {
    for (int x = 1; x < 16; x++) {
      new_palette[x + (16 * y)] = rom.palette_group("armors")[0][k];
      k++;
    }
  }

  current.Create(new_palette);
  for (int i = 0; i < 256; i++) {
    current[(i / 16) * 16].SetTransparent(true);
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
  game_state_ = game_state;
  world_ = world;
  if (large_map_) {
    parent_ = map_parent[index_];
    if (parent_ != index_ && !initialized_) {
      if (index_ >= 0x80 && index_ <= 0x8A && index_ != 0x88) {
        area_graphics_ = rom_[overworldSpecialGFXGroup + (parent_ - 0x80)];
        area_palette_ = rom_[overworldSpecialPALGroup + 1];
      } else if (index_ == 0x88) {
        area_graphics_ = 0x51;
        area_palette_ = 0x00;
      } else {
        area_graphics_ = rom_[mapGfx + parent_];
        area_palette_ = rom_[overworldMapPalette + parent_];
      }

      initialized_ = true;
    }
  }

  LoadAreaGraphics();
  RETURN_IF_ERROR(BuildTileset())
  RETURN_IF_ERROR(BuildTiles16Gfx(count))
  LoadPalette();
  RETURN_IF_ERROR(BuildBitmap(world_blockset))
  built_ = true;
  return absl::OkStatus();
}

void OverworldMap::LoadAreaInfo() {
  if (index_ != 0x80 && index_ <= 150 &&
      rom_[overworldMapSize + (index_ & 0x3F)] != 0) {
    large_map_ = true;
  }
  if (index_ < 64) {
    area_graphics_ = rom_[mapGfx + parent_];
    area_palette_ = rom_[overworldMapPalette + parent_];

    area_music_[0] = rom_[overworldMusicBegining + parent_];
    area_music_[1] = rom_[overworldMusicZelda + parent_];
    area_music_[2] = rom_[overworldMusicMasterSword + parent_];
    area_music_[3] = rom_[overworldMusicAgahim + parent_];

    sprite_graphics_[0] = rom_[overworldSpriteset + parent_];
    sprite_graphics_[1] = rom_[overworldSpriteset + parent_ + 0x40];
    sprite_graphics_[2] = rom_[overworldSpriteset + parent_ + 0x80];

    sprite_palette_[0] = rom_[overworldSpritePalette + parent_];
    sprite_palette_[1] = rom_[overworldSpritePalette + parent_ + 0x40];
    sprite_palette_[2] = rom_[overworldSpritePalette + parent_ + 0x80];
  } else if (index_ < 0x80) {
    area_graphics_ = rom_[mapGfx + parent_];
    area_palette_ = rom_[overworldMapPalette + parent_];
    area_music_[0] = rom_[overworldMusicDW + (parent_ - 64)];

    sprite_graphics_[0] = rom_[overworldSpriteset + parent_ + 0x80];
    sprite_graphics_[1] = rom_[overworldSpriteset + parent_ + 0x80];
    sprite_graphics_[2] = rom_[overworldSpriteset + parent_ + 0x80];

    sprite_palette_[0] = rom_[overworldSpritePalette + parent_ + 0x80];
    sprite_palette_[1] = rom_[overworldSpritePalette + parent_ + 0x80];
    sprite_palette_[2] = rom_[overworldSpritePalette + parent_ + 0x80];
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

    area_palette_ = rom_[overworldSpecialPALGroup + parent_ - 0x80];
    if (index_ >= 0x80 && index_ <= 0x8A && index_ != 0x88) {
      area_graphics_ = rom_[overworldSpecialGFXGroup + (parent_ - 0x80)];
      area_palette_ = rom_[overworldSpecialPALGroup + 1];
    } else if (index_ == 0x88) {
      area_graphics_ = 0x51;
      area_palette_ = 0x00;
    } else {
      // pyramid bg use 0x5B map
      area_graphics_ = rom_[mapGfx + parent_];
      area_palette_ = rom_[overworldMapPalette + parent_];
    }

    message_id_ = rom_[overworldMessages + parent_];

    sprite_graphics_[0] = rom_[overworldSpriteset + parent_ + 0x80];
    sprite_graphics_[1] = rom_[overworldSpriteset + parent_ + 0x80];
    sprite_graphics_[2] = rom_[overworldSpriteset + parent_ + 0x80];

    sprite_palette_[0] = rom_[overworldSpritePalette + parent_ + 0x80];
    sprite_palette_[1] = rom_[overworldSpritePalette + parent_ + 0x80];
    sprite_palette_[2] = rom_[overworldSpritePalette + parent_ + 0x80];
  }
}

// ============================================================================

void OverworldMap::LoadWorldIndex() {
  if (parent_ < 0x40) {
    world_index_ = 0x20;
  } else if (parent_ >= 0x40 && parent_ < 0x80) {
    world_index_ = 0x21;
  } else if (parent_ == 0x88) {
    world_index_ = 0x24;
  }
}

void OverworldMap::LoadSpritesBlocksets() {
  int static_graphics_base = 0x73;
  static_graphics_[8] = static_graphics_base + 0x00;
  static_graphics_[9] = static_graphics_base + 0x01;
  static_graphics_[10] = static_graphics_base + 0x06;
  static_graphics_[11] = static_graphics_base + 0x07;

  for (int i = 0; i < 4; i++) {
    static_graphics_[12 + i] =
        (rom_[rom_.version_constants().kSpriteBlocksetPointer +
              (sprite_graphics_[game_state_] * 4) + i] +
         static_graphics_base);
  }
}

void OverworldMap::LoadMainBlocksets() {
  for (int i = 0; i < 8; i++) {
    static_graphics_[i] = rom_[rom_.version_constants().kOverworldGfxGroups2 +
                               (world_index_ * 8) + i];
  }
}

// For animating water tiles on the overworld map.
// We want to swap out static_graphics_[07] with the next sheet
// Usually it is 5A, so we make it 5B instead.
// There is a middle frame which contains tiles from the bottom half
// of the 5A sheet, so this will need some special manipulation to make work
// during the BuildBitmap step (or a new one specifically for animating).
void OverworldMap::DrawAnimatedTiles() {
  std::cout << "static_graphics_[6] = "
            << core::UppercaseHexByte(static_graphics_[6]) << std::endl;
  std::cout << "static_graphics_[7] = "
            << core::UppercaseHexByte(static_graphics_[7]) << std::endl;
  std::cout << "static_graphics_[8] = "
            << core::UppercaseHexByte(static_graphics_[8]) << std::endl;
  if (static_graphics_[7] == 0x5B) {
    static_graphics_[7] = 0x5A;
  } else {
    if (static_graphics_[7] == 0x59) {
      static_graphics_[7] = 0x58;
    }
    static_graphics_[7] = 0x5B;
  }
  //   if (static_graphics_[7] == 0x5A) {
  //   static_graphics_[7] = 0x5B;
  // } else {
  //   if (static_graphics_[7] == 0x58) {
  //     static_graphics_[7] = 0x59;
  //   }
  //   static_graphics_[7] = 0x5A;
  // }
}

void OverworldMap::LoadAreaGraphicsBlocksets() {
  for (int i = 0; i < 4; i++) {
    uchar value = rom_[rom_.version_constants().kOverworldGfxGroups1 +
                       (area_graphics_ * 4) + i];
    if (value != 0) {
      static_graphics_[3 + i] = value;
    }
  }
}

void OverworldMap::LoadDeathMountainGFX() {
  static_graphics_[7] = (((parent_ >= 0x03 && parent_ <= 0x07) ||
                          (parent_ >= 0x0B && parent_ <= 0x0E)) ||
                         ((parent_ >= 0x43 && parent_ <= 0x47) ||
                          (parent_ >= 0x4B && parent_ <= 0x4E)))
                            ? 0x59
                            : 0x5B;
}

void OverworldMap::LoadAreaGraphics() {
  LoadWorldIndex();
  LoadSpritesBlocksets();
  LoadMainBlocksets();
  LoadAreaGraphicsBlocksets();
  LoadDeathMountainGFX();
}

// New helper function to get a palette from the ROM.
gfx::SNESPalette OverworldMap::GetPalette(const std::string& group, int index,
                                          int previousIndex, int limit) {
  if (index == 255) {
    index = rom_[rom_.version_constants().overworldMapPaletteGroup +
                 (previousIndex * 4)];
  }
  if (index != 255) {
    if (index >= limit) {
      index = limit - 1;
    }
    return rom_.palette_group(group)[index];
  } else {
    return rom_.palette_group(group)[0];
  }
}

void OverworldMap::LoadPalette() {
  int previousPalId = index_ > 0 ? rom_[overworldMapPalette + parent_ - 1] : 0;
  int previousSprPalId =
      index_ > 0 ? rom_[overworldSpritePalette + parent_ - 1] : 0;

  area_palette_ = std::min((int)area_palette_, 0xA3);

  uchar pal0 = 0;
  uchar pal1 = rom_[rom_.version_constants().overworldMapPaletteGroup +
                    (area_palette_ * 4)];
  uchar pal2 = rom_[rom_.version_constants().overworldMapPaletteGroup +
                    (area_palette_ * 4) + 1];
  uchar pal3 = rom_[rom_.version_constants().overworldMapPaletteGroup +
                    (area_palette_ * 4) + 2];
  uchar pal4 =
      rom_[overworldSpritePaletteGroup + (sprite_palette_[game_state_] * 2)];
  uchar pal5 = rom_[overworldSpritePaletteGroup +
                    (sprite_palette_[game_state_] * 2) + 1];

  gfx::SNESColor bgr = rom_.palette_group("grass")[0].GetColor(0);

  gfx::SNESPalette aux1 = GetPalette("ow_aux", pal1, previousPalId, 20);
  gfx::SNESPalette aux2 = GetPalette("ow_aux", pal2, previousPalId, 20);

  // Additional handling of `pal3` and `parent_`
  if (pal3 == 255) {
    pal3 = rom_[rom_.version_constants().overworldMapPaletteGroup +
                (previousPalId * 4) + 2];
  }
  if (parent_ < 0x40) {
    pal0 = parent_ == 0x03 || parent_ == 0x05 || parent_ == 0x07 ? 2 : 0;
    bgr = rom_.palette_group("grass")[0].GetColor(0);
  } else if (parent_ >= 0x40 && parent_ < 0x80) {
    pal0 = parent_ == 0x43 || parent_ == 0x45 || parent_ == 0x47 ? 3 : 1;
    bgr = rom_.palette_group("grass")[0].GetColor(1);
  } else if (parent_ >= 128 && parent_ < kNumOverworldMaps) {
    pal0 = 0;
    bgr = rom_.palette_group("grass")[0].GetColor(2);
  }
  if (parent_ == 0x88) {
    pal0 = 4;
  }
  gfx::SNESPalette main = GetPalette("ow_main", pal0, previousPalId, 255);
  gfx::SNESPalette animated =
      GetPalette("ow_animated", std::min((int)pal3, 13), previousPalId, 14);
  gfx::SNESPalette hud = rom_.palette_group("hud")[0];

  gfx::SNESPalette spr = GetPalette("sprites_aux3", pal4, previousSprPalId, 24);
  gfx::SNESPalette spr2 =
      GetPalette("sprites_aux3", pal5, previousSprPalId, 24);

  SetColorsPalette(rom_, parent_, current_palette_, main, animated, aux1, aux2,
                   hud, bgr, spr, spr2);
}

// New helper function to process graphics buffer.
void OverworldMap::ProcessGraphicsBuffer(int index, int static_graphics_offset,
                                         int size) {
  for (int i = 0; i < size; i++) {
    auto byte = all_gfx_[i + (static_graphics_offset * size)];
    switch (index) {
      case 0:
      case 3:
      case 4:
      case 5:
        byte += 0x88;
        break;
    }
    current_gfx_[(index * size) + i] = byte;
  }
}

absl::Status OverworldMap::BuildTileset() {
  all_gfx_ = rom_.graphics_buffer();
  current_gfx_.resize(0x10000, 0x00);

  for (int i = 0; i < 0x10; i++) {
    ProcessGraphicsBuffer(i, static_graphics_[i], 0x1000);
  }

  return absl::OkStatus();
}

absl::Status OverworldMap::BuildTiles16Gfx(int count) {
  if (current_blockset_.size() != 0) {
    current_blockset_.clear();
  }
  current_blockset_.reserve(0x100000);
  for (int i = 0; i < 0x100000; i++) {
    current_blockset_.push_back(0x00);
  }
  const int offsets[] = {0x00, 0x08, 0x400, 0x408};
  auto yy = 0;
  auto xx = 0;

  for (auto i = 0; i < count; i++) {
    for (auto tile = 0; tile < 0x04; tile++) {
      gfx::TileInfo info = tiles16_[i].tiles_info[tile];
      int offset = offsets[tile];
      for (auto y = 0; y < 0x08; ++y) {
        for (auto x = 0; x < 0x08; ++x) {
          int mx = x;
          int my = y;

          if (info.horizontal_mirror_ != 0) {
            mx = 0x07 - x;
          }

          if (info.vertical_mirror_ != 0) {
            my = 0x07 - y;
          }

          int xpos = ((info.id_ % 0x10) * 0x08);
          int ypos = (((info.id_ / 0x10)) * 0x400);
          int source = ypos + xpos + (x + (y * 0x80));

          auto destination = xx + yy + offset + (mx + (my * 0x80));
          current_blockset_[destination] =
              (current_gfx_[source] & 0x0F) + (info.palette_ * 0x10);
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
  if (bitmap_data_.size() != 0) {
    bitmap_data_.clear();
  }
  bitmap_data_.reserve(0x40000);
  for (int i = 0; i < 0x40000; i++) {
    bitmap_data_.push_back(0x00);
  }

  int superY = ((index_ - (world_ * 0x40)) / 0x08);
  int superX = index_ - (world_ * 0x40) - (superY * 0x08);

  for (int y = 0; y < 0x20; y++) {
    for (int x = 0; x < 0x20; x++) {
      auto xt = x + (superX * 0x20);
      auto yt = y + (superY * 0x20);
      CopyTile8bpp16((x * 0x10), (y * 0x10), world_blockset[xt][yt],
                     bitmap_data_, current_blockset_);
    }
  }
  return absl::OkStatus();
}

}  // namespace zelda3
}  // namespace app
}  // namespace yaze