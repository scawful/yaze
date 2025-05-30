#include "overworld_map.h"

#include <cstddef>
#include <cstdint>
#include <unordered_map>
#include <vector>

#include "app/core/features.h"
#include "app/gfx/snes_tile.h"
#include "app/rom.h"
#include "app/zelda3/overworld/overworld.h"
#include "util/log.h"

namespace yaze {
namespace zelda3 {

OverworldMap::OverworldMap(int index, Rom *rom)
    : index_(index), parent_(index), rom_(rom) {
  LoadAreaInfo();

  if (core::FeatureFlags::get().overworld.kLoadCustomOverworld) {
    // If the custom overworld ASM has NOT already been applied, manually set
    // the vanilla values.
    uint8_t asm_version = (*rom_)[OverworldCustomASMHasBeenApplied];
    if (asm_version == 0x00) {
      LoadCustomOverworldData();
    } else {
      SetupCustomTileset(asm_version);
    }
  }
}

absl::Status OverworldMap::BuildMap(int count, int game_state, int world,
                                    std::vector<gfx::Tile16> &tiles16,
                                    OverworldBlockset &world_blockset) {
  game_state_ = game_state;
  world_ = world;
  if (large_map_) {
    if (parent_ != index_ && !initialized_) {
      if (index_ >= kSpecialWorldMapIdStart && index_ <= 0x8A &&
          index_ != 0x88) {
        area_graphics_ = (*rom_)[kOverworldSpecialGfxGroup +
                                 (parent_ - kSpecialWorldMapIdStart)];
        area_palette_ = (*rom_)[kOverworldSpecialPalGroup + 1];
      } else if (index_ == 0x88) {
        area_graphics_ = 0x51;
        area_palette_ = 0x00;
      } else {
        area_graphics_ = (*rom_)[kAreaGfxIdPtr + parent_];
        area_palette_ = (*rom_)[kOverworldMapPaletteIds + parent_];
      }

      initialized_ = true;
    }
  }

  LoadAreaGraphics();
  RETURN_IF_ERROR(BuildTileset())
  RETURN_IF_ERROR(BuildTiles16Gfx(tiles16, count))
  RETURN_IF_ERROR(LoadPalette());
  RETURN_IF_ERROR(BuildBitmap(world_blockset))
  built_ = true;
  return absl::OkStatus();
}

void OverworldMap::LoadAreaInfo() {
  if (index_ != kSpecialWorldMapIdStart) {
    if (index_ <= 128)
      large_map_ = ((*rom_)[kOverworldMapSize + (index_ & 0x3F)] != 0);
    else {
      large_map_ =
          index_ == 129 || index_ == 130 || index_ == 137 || index_ == 138;
    }
  }

  auto message_id = rom_->ReadWord(kOverworldMessageIds + (parent_ * 2));
  if (message_id.ok()) {
    message_id_ = message_id.value();
  } else {
    message_id_ = 0;
    util::logf("Error reading message id for map %d", parent_);
  }

  if (index_ < kDarkWorldMapIdStart) {
    area_graphics_ = (*rom_)[kAreaGfxIdPtr + parent_];
    area_palette_ = (*rom_)[kOverworldMapPaletteIds + parent_];

    area_music_[0] = (*rom_)[kOverworldMusicBeginning + parent_];
    area_music_[1] = (*rom_)[kOverworldMusicZelda + parent_];
    area_music_[2] = (*rom_)[kOverworldMusicMasterSword + parent_];
    area_music_[3] = (*rom_)[kOverworldMusicAgahnim + parent_];

    sprite_graphics_[0] = (*rom_)[kOverworldSpriteset + parent_];
    sprite_graphics_[1] =
        (*rom_)[kOverworldSpriteset + parent_ + kDarkWorldMapIdStart];
    sprite_graphics_[2] =
        (*rom_)[kOverworldSpriteset + parent_ + kSpecialWorldMapIdStart];

    sprite_palette_[0] = (*rom_)[kOverworldSpritePaletteIds + parent_];
    sprite_palette_[1] =
        (*rom_)[kOverworldSpritePaletteIds + parent_ + kDarkWorldMapIdStart];
    sprite_palette_[2] =
        (*rom_)[kOverworldSpritePaletteIds + parent_ + kSpecialWorldMapIdStart];
  } else if (index_ < kSpecialWorldMapIdStart) {
    area_graphics_ = (*rom_)[kAreaGfxIdPtr + parent_];
    area_palette_ = (*rom_)[kOverworldMapPaletteIds + parent_];
    area_music_[0] = (*rom_)[kOverworldMusicDarkWorld + (parent_ - 64)];

    sprite_graphics_[0] =
        (*rom_)[kOverworldSpriteset + parent_ + kSpecialWorldMapIdStart];
    sprite_graphics_[1] =
        (*rom_)[kOverworldSpriteset + parent_ + kSpecialWorldMapIdStart];
    sprite_graphics_[2] =
        (*rom_)[kOverworldSpriteset + parent_ + kSpecialWorldMapIdStart];

    sprite_palette_[0] =
        (*rom_)[kOverworldSpritePaletteIds + parent_ + kSpecialWorldMapIdStart];
    sprite_palette_[1] =
        (*rom_)[kOverworldSpritePaletteIds + parent_ + kSpecialWorldMapIdStart];
    sprite_palette_[2] =
        (*rom_)[kOverworldSpritePaletteIds + parent_ + kSpecialWorldMapIdStart];
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
    } else if (index_ == 129 || index_ == 130 || index_ == 137 ||
               index_ == 138) {
      parent_ = 129;
    }

    area_palette_ =
        (*rom_)[kOverworldSpecialPalGroup + parent_ - kSpecialWorldMapIdStart];
    if ((index_ >= kSpecialWorldMapIdStart && index_ <= 0x8A &&
         index_ != 0x88) ||
        index_ == 0x94) {
      area_graphics_ = (*rom_)[kOverworldSpecialGfxGroup +
                               (parent_ - kSpecialWorldMapIdStart)];
      area_palette_ = (*rom_)[kOverworldSpecialPalGroup + 1];
    } else if (index_ == 0x88) {
      area_graphics_ = 0x51;
      area_palette_ = 0x00;
    } else {
      // pyramid bg use 0x5B map
      area_graphics_ = (*rom_)[kAreaGfxIdPtr + parent_];
      area_palette_ = (*rom_)[kOverworldMapPaletteIds + parent_];
    }

    sprite_graphics_[0] =
        (*rom_)[kOverworldSpriteset + parent_ + kSpecialWorldMapIdStart];
    sprite_graphics_[1] =
        (*rom_)[kOverworldSpriteset + parent_ + kSpecialWorldMapIdStart];
    sprite_graphics_[2] =
        (*rom_)[kOverworldSpriteset + parent_ + kSpecialWorldMapIdStart];

    sprite_palette_[0] =
        (*rom_)[kOverworldSpritePaletteIds + parent_ + kSpecialWorldMapIdStart];
    sprite_palette_[1] =
        (*rom_)[kOverworldSpritePaletteIds + parent_ + kSpecialWorldMapIdStart];
    sprite_palette_[2] =
        (*rom_)[kOverworldSpritePaletteIds + parent_ + kSpecialWorldMapIdStart];
  }
}

void OverworldMap::LoadCustomOverworldData() {
  // Set the main palette values.
  if (index_ < kDarkWorldMapIdStart) {
    area_palette_ = 0;
  } else if (index_ >= kDarkWorldMapIdStart &&
             index_ < kSpecialWorldMapIdStart) {
    area_palette_ = 1;
  } else if (index_ >= kSpecialWorldMapIdStart && index_ < 0xA0) {
    area_palette_ = 0;
  }

  if (index_ == 0x03 || index_ == 0x05 || index_ == 0x07) {
    area_palette_ = 2;
  } else if (index_ == 0x43 || index_ == 0x45 || index_ == 0x47) {
    area_palette_ = 3;
  } else if (index_ == 0x88) {
    area_palette_ = 4;
  }

  // Set the mosaic values.
  mosaic_ = index_ == 0x00 || index_ == kDarkWorldMapIdStart ||
            index_ == kSpecialWorldMapIdStart || index_ == 0x81 ||
            index_ == 0x88;

  int index_world = 0x20;

  if (parent_ >= kDarkWorldMapIdStart &&
      parent_ < kSpecialWorldMapIdStart)  // DW
  {
    index_world = 0x21;
  } else if (parent_ == 0x88)  // Triforce room
  {
    index_world = 0x24;
  }

  const auto overworld_gfx_groups2 =
      rom_->version_constants().kOverworldGfxGroups2;

  // Main Blocksets
  for (int i = 0; i < 8; i++) {
    custom_gfx_ids_[i] =
        (uint8_t)(*rom_)[overworld_gfx_groups2 + (index_world * 8) + i];
  }

  const auto overworldgfxGroups =
      rom_->version_constants().kOverworldGfxGroups1;
  // Replace the variable tiles with the variable ones.
  uint8_t temp = (*rom_)[overworldgfxGroups + (area_graphics_ * 4)];
  if (temp != 0) {
    custom_gfx_ids_[3] = temp;
  } else {
    custom_gfx_ids_[3] = 0xFF;
  }

  temp = (*rom_)[overworldgfxGroups + (area_graphics_ * 4) + 1];
  if (temp != 0) {
    custom_gfx_ids_[4] = temp;
  } else {
    custom_gfx_ids_[4] = 0xFF;
  }

  temp = (*rom_)[overworldgfxGroups + (area_graphics_ * 4) + 2];
  if (temp != 0) {
    custom_gfx_ids_[5] = temp;
  } else {
    custom_gfx_ids_[5] = 0xFF;
  }

  temp = (*rom_)[overworldgfxGroups + (area_graphics_ * 4) + 3];
  if (temp != 0) {
    custom_gfx_ids_[6] = temp;
  } else {
    custom_gfx_ids_[6] = 0xFF;
  }

  // Set the animated GFX values.
  if (index_ == 0x03 || index_ == 0x05 || index_ == 0x07 || index_ == 0x43 ||
      index_ == 0x45 || index_ == 0x47) {
    animated_gfx_ = 0x59;
  } else {
    animated_gfx_ = 0x5B;
  }

  // Set the subscreen overlay values.
  subscreen_overlay_ = 0x00FF;

  if (index_ == 0x00 ||
      index_ ==
          kDarkWorldMapIdStart)  // Add fog 2 to the lost woods and skull woods.
  {
    subscreen_overlay_ = 0x009D;
  } else if (index_ == 0x03 || index_ == 0x05 ||
             index_ == 0x07)  // Add the sky BG to LW death mountain.
  {
    subscreen_overlay_ = 0x0095;
  } else if (index_ == 0x43 || index_ == 0x45 ||
             index_ == 0x47)  // Add the lava to DW death mountain.
  {
    subscreen_overlay_ = 0x009C;
  } else if (index_ == 0x5B)  // TODO: Might need this one too "index == 0x1B"
                              // but for now I don't think so.
  {
    subscreen_overlay_ = 0x0096;
  } else if (index_ == 0x80)  // Add fog 1 to the master sword area.
  {
    subscreen_overlay_ = 0x0097;
  } else if (index_ ==
             0x88)  // Add the triforce room curtains to the triforce room.
  {
    subscreen_overlay_ = 0x0093;
  }

  // Set the main palette values.
  if (index_ < 0x40)  // LW
  {
    area_palette_ = 0;
  } else if (index_ >= 0x40 && index_ < 0x80)  // DW
  {
    area_palette_ = 1;
  } else if (index_ >= 0x80 && index_ < 0xA0)  // SW
  {
    area_palette_ = 0;
  }

  if (index_ == 0x03 || index_ == 0x05 || index_ == 0x07)  // LW Death Mountain
  {
    area_palette_ = 2;
  } else if (index_ == 0x43 || index_ == 0x45 ||
             index_ == 0x47)  // DW Death Mountain
  {
    area_palette_ = 3;
  } else if (index_ == 0x88)  // Triforce room
  {
    area_palette_ = 4;
  }

  // Set the mosaic values.
  switch (index_) {
    case 0x00:  // Leaving Skull Woods / Lost Woods
    case 0x40:
      mosaic_expanded_ = {false, true, false, true};
      break;
    case 0x02:  // Going into Skull woods / Lost Woods west
    case 0x0A:
    case 0x42:
    case 0x4A:
      mosaic_expanded_ = {false, false, true, false};
      break;
    case 0x0F:  // Going into Zora's Domain North
    case 0x10:  // Going into Skull Woods / Lost Woods North
    case 0x11:
    case 0x50:
    case 0x51:
      mosaic_expanded_ = {true, false, false, false};
      break;
    case 0x80:  // Leaving Zora's Domain, the Master Sword area, and the
                // Triforce area
    case 0x81:
    case 0x88:
      mosaic_expanded_ = {false, true, false, false};
      break;
  }
}

void OverworldMap::SetupCustomTileset(uint8_t asm_version) {
  area_palette_ = (*rom_)[OverworldCustomMainPaletteArray + index_];
  mosaic_ = (*rom_)[OverworldCustomMosaicArray + index_] != 0x00;

  uint8_t mosaicByte = (*rom_)[OverworldCustomMosaicArray + index_];
  mosaic_expanded_ = {(mosaicByte & 0x08) != 0x00, (mosaicByte & 0x04) != 0x00,
                      (mosaicByte & 0x02) != 0x00, (mosaicByte & 0x01) != 0x00};

  // This is just to load the GFX groups for ROMs that have an older version
  // of the Overworld ASM already applied.
  if (asm_version >= 0x01 && asm_version != 0xFF) {
    for (int i = 0; i < 8; i++) {
      custom_gfx_ids_[i] =
          (*rom_)[OverworldCustomTileGFXGroupArray + (index_ * 8) + i];
    }

    animated_gfx_ = (*rom_)[OverworldCustomAnimatedGFXArray + index_];
  } else {
    int index_world = 0x20;

    if (parent_ >= kDarkWorldMapIdStart &&
        parent_ < kSpecialWorldMapIdStart)  // DW
    {
      index_world = 0x21;
    } else if (parent_ == 0x88)  // Triforce room
    {
      index_world = 0x24;
    }

    // Main Blocksets
    for (int i = 0; i < 8; i++) {
      custom_gfx_ids_[i] =
          (uint8_t)(*rom_)[rom_->version_constants().kOverworldGfxGroups2 +
                           (index_world * 8) + i];
    }

    const auto overworldgfxGroups =
        rom_->version_constants().kOverworldGfxGroups1;

    // Replace the variable tiles with the variable ones.
    // If the variable is 00 set it to 0xFF which is the new "don't load
    // anything" value.
    uint8_t temp = (*rom_)[overworldgfxGroups + (area_graphics_ * 4)];
    if (temp != 0x00) {
      custom_gfx_ids_[3] = temp;
    } else {
      custom_gfx_ids_[3] = 0xFF;
    }

    temp = (*rom_)[overworldgfxGroups + (area_graphics_ * 4) + 1];
    if (temp != 0x00) {
      custom_gfx_ids_[4] = temp;
    } else {
      custom_gfx_ids_[4] = 0xFF;
    }

    temp = (*rom_)[overworldgfxGroups + (area_graphics_ * 4) + 2];
    if (temp != 0x00) {
      custom_gfx_ids_[5] = temp;
    } else {
      custom_gfx_ids_[5] = 0xFF;
    }

    temp = (*rom_)[overworldgfxGroups + (area_graphics_ * 4) + 3];
    if (temp != 0x00) {
      custom_gfx_ids_[6] = temp;
    } else {
      custom_gfx_ids_[6] = 0xFF;
    }

    // Set the animated GFX values.
    if (index_ == 0x03 || index_ == 0x05 || index_ == 0x07 || index_ == 0x43 ||
        index_ == 0x45 || index_ == 0x47) {
      animated_gfx_ = 0x59;
    } else {
      animated_gfx_ = 0x5B;
    }
  }

  subscreen_overlay_ =
      (*rom_)[OverworldCustomSubscreenOverlayArray + (index_ * 2)];
}

void OverworldMap::LoadMainBlocksetId() {
  if (parent_ < kDarkWorldMapIdStart) {
    main_gfx_id_ = 0x20;
  } else if (parent_ >= kDarkWorldMapIdStart &&
             parent_ < kSpecialWorldMapIdStart) {
    main_gfx_id_ = 0x21;
  } else if (parent_ == 0x88) {
    main_gfx_id_ = 0x24;
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
        ((*rom_)[rom_->version_constants().kSpriteBlocksetPointer +
                 (sprite_graphics_[game_state_] * 4) + i] +
         static_graphics_base);
  }
}

void OverworldMap::LoadMainBlocksets() {
  for (int i = 0; i < 8; i++) {
    static_graphics_[i] =
        (*rom_)[rom_->version_constants().kOverworldGfxGroups2 +
                (main_gfx_id_ * 8) + i];
  }
}

// For animating water tiles on the overworld map.
// We want to swap out static_graphics_[07] with the next sheet
// Usually it is 5A, so we make it 5B instead.
// There is a middle frame which contains tiles from the bottom half
// of the 5A sheet, so this will need some special manipulation to make work
// during the BuildBitmap step (or a new one specifically for animating).
void OverworldMap::DrawAnimatedTiles() {
  if (static_graphics_[7] == 0x5B) {
    static_graphics_[7] = 0x5A;
  } else {
    if (static_graphics_[7] == 0x59) {
      static_graphics_[7] = 0x58;
    }
    static_graphics_[7] = 0x5B;
  }
}

void OverworldMap::LoadAreaGraphicsBlocksets() {
  for (int i = 0; i < 4; i++) {
    uint8_t value = (*rom_)[rom_->version_constants().kOverworldGfxGroups1 +
                            (area_graphics_ * 4) + i];
    if (value != 0) {
      static_graphics_[3 + i] = value;
    }
  }
}

// TODO: Change the conditions for death mountain gfx
// JaredBrian: This is how ZS did it, but in 3.0.4 I changed it to just check
// for 03, 05, 07, and the DW ones as that's how it would appear in-game if
// you were to make area 03 not a large area anymore for example, so you might
// want to do the same.
void OverworldMap::LoadDeathMountainGFX() {
  static_graphics_[7] = (((parent_ >= 0x03 && parent_ <= 0x07) ||
                          (parent_ >= 0x0B && parent_ <= 0x0E)) ||
                         ((parent_ >= 0x43 && parent_ <= 0x47) ||
                          (parent_ >= 0x4B && parent_ <= 0x4E)))
                            ? 0x59
                            : 0x5B;
}

void OverworldMap::LoadAreaGraphics() {
  LoadMainBlocksetId();
  LoadSpritesBlocksets();
  LoadMainBlocksets();
  LoadAreaGraphicsBlocksets();
  LoadDeathMountainGFX();
}

namespace palette_internal {

absl::Status SetColorsPalette(Rom &rom, int index, gfx::SnesPalette &current,
                              gfx::SnesPalette main, gfx::SnesPalette animated,
                              gfx::SnesPalette aux1, gfx::SnesPalette aux2,
                              gfx::SnesPalette hud, gfx::SnesColor bgrcolor,
                              gfx::SnesPalette spr, gfx::SnesPalette spr2) {
  // Palettes infos, color 0 of a palette is always transparent (the arrays
  // contains 7 colors width wide) There is 16 color per line so 16*Y

  // Left side of the palette - Main, Animated
  std::array<gfx::SnesColor, 256> new_palette = {};

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
      new_palette[x + (16 * y)] = rom.palette_group().sprites_aux1[1][k];
      k++;
    }
  }

  // Sprite Palettes
  k = 0;
  for (int y = 8; y < 9; y++) {
    for (int x = 9; x < 16; x++) {
      new_palette[x + (16 * y)] = rom.palette_group().sprites_aux3[0][k];
      k++;
    }
  }

  // Sprite Palettes
  k = 0;
  for (int y = 9; y < 13; y++) {
    for (int x = 1; x < 16; x++) {
      new_palette[x + (16 * y)] = rom.palette_group().global_sprites[0][k];
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
      new_palette[x + (16 * y)] = rom.palette_group().armors[0][k];
      k++;
    }
  }

  for (int i = 0; i < 256; i++) {
    current[i] = new_palette[i];
    current[(i / 16) * 16].set_transparent(true);
  }

  current.set_size(256);
  return absl::OkStatus();
}
}  // namespace palette_internal

absl::StatusOr<gfx::SnesPalette> OverworldMap::GetPalette(
    const gfx::PaletteGroup &palette_group, int index, int previous_index,
    int limit) {
  if (index == 255) {
    index = (*rom_)[rom_->version_constants().kOverworldMapPaletteGroup +
                    (previous_index * 4)];
  }
  if (index >= limit) {
    index = limit - 1;
  }
  return palette_group[index];
}

absl::Status OverworldMap::LoadPalette() {
  int previous_pal_id =
      index_ > 0 ? (*rom_)[kOverworldMapPaletteIds + parent_ - 1] : 0;
  int previous_spr_pal_id =
      index_ > 0 ? (*rom_)[kOverworldSpritePaletteIds + parent_ - 1] : 0;

  area_palette_ = std::min((int)area_palette_, 0xA3);

  uint8_t pal0 = 0;
  uint8_t pal1 = (*rom_)[rom_->version_constants().kOverworldMapPaletteGroup +
                         (area_palette_ * 4)];
  uint8_t pal2 = (*rom_)[rom_->version_constants().kOverworldMapPaletteGroup +
                         (area_palette_ * 4) + 1];
  uint8_t pal3 = (*rom_)[rom_->version_constants().kOverworldMapPaletteGroup +
                         (area_palette_ * 4) + 2];
  uint8_t pal4 = (*rom_)[kOverworldSpritePaletteGroup +
                         (sprite_palette_[game_state_] * 2)];
  uint8_t pal5 = (*rom_)[kOverworldSpritePaletteGroup +
                         (sprite_palette_[game_state_] * 2) + 1];

  auto grass_pal_group = rom_->palette_group().grass;
  auto bgr = grass_pal_group[0][0];

  auto ow_aux_pal_group = rom_->palette_group().overworld_aux;
  ASSIGN_OR_RETURN(gfx::SnesPalette aux1,
                   GetPalette(ow_aux_pal_group, pal1, previous_pal_id, 20));
  ASSIGN_OR_RETURN(gfx::SnesPalette aux2,
                   GetPalette(ow_aux_pal_group, pal2, previous_pal_id, 20));

  // Additional handling of `pal3` and `parent_`
  if (pal3 == 255) {
    pal3 = (*rom_)[rom_->version_constants().kOverworldMapPaletteGroup +
                   (previous_pal_id * 4) + 2];
  }

  if (parent_ < kDarkWorldMapIdStart) {
    pal0 = parent_ == 0x03 || parent_ == 0x05 || parent_ == 0x07 ? 2 : 0;
    bgr = grass_pal_group[0][0];
  } else if (parent_ >= kDarkWorldMapIdStart &&
             parent_ < kSpecialWorldMapIdStart) {
    pal0 = parent_ == 0x43 || parent_ == 0x45 || parent_ == 0x47 ? 3 : 1;
    bgr = grass_pal_group[0][1];
  } else if (parent_ >= 128 && parent_ < kNumOverworldMaps) {
    pal0 = 0;
    bgr = grass_pal_group[0][2];
  }

  if (parent_ == 0x88) {
    pal0 = 4;
  }

  auto ow_main_pal_group = rom_->palette_group().overworld_main;
  ASSIGN_OR_RETURN(gfx::SnesPalette main,
                   GetPalette(ow_main_pal_group, pal0, previous_pal_id, 255));
  auto ow_animated_pal_group = rom_->palette_group().overworld_animated;
  ASSIGN_OR_RETURN(gfx::SnesPalette animated,
                   GetPalette(ow_animated_pal_group, std::min((int)pal3, 13),
                              previous_pal_id, 14));

  auto hud_pal_group = rom_->palette_group().hud;
  gfx::SnesPalette hud = hud_pal_group[0];

  ASSIGN_OR_RETURN(gfx::SnesPalette spr,
                   GetPalette(rom_->palette_group().sprites_aux3, pal4,
                              previous_spr_pal_id, 24));
  ASSIGN_OR_RETURN(gfx::SnesPalette spr2,
                   GetPalette(rom_->palette_group().sprites_aux3, pal5,
                              previous_spr_pal_id, 24));

  RETURN_IF_ERROR(palette_internal::SetColorsPalette(
      *rom_, parent_, current_palette_, main, animated, aux1, aux2, hud, bgr,
      spr, spr2));

  if (palettesets_.count(area_palette_) == 0) {
    palettesets_[area_palette_] = gfx::Paletteset{
        main, animated, aux1, aux2, bgr, hud, spr, spr2, current_palette_};
  }

  return absl::OkStatus();
}

void OverworldMap::ProcessGraphicsBuffer(int index, int static_graphics_offset,
                                         int size, uint8_t *all_gfx) {
  for (int i = 0; i < size; i++) {
    auto byte = all_gfx[i + (static_graphics_offset * size)];
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
  if (current_gfx_.size() == 0) current_gfx_.resize(0x10000, 0x00);
  for (int i = 0; i < 0x10; i++) {
    ProcessGraphicsBuffer(i, static_graphics_[i], 0x1000,
                          rom_->graphics_buffer().data());
  }
  return absl::OkStatus();
}

absl::Status OverworldMap::BuildTiles16Gfx(std::vector<gfx::Tile16> &tiles16,
                                           int count) {
  if (current_blockset_.size() == 0) current_blockset_.resize(0x100000, 0x00);

  const int offsets[] = {0x00, 0x08, 0x400, 0x408};
  auto yy = 0;
  auto xx = 0;

  for (auto i = 0; i < count; i++) {
    for (auto tile = 0; tile < 0x04; tile++) {
      gfx::TileInfo info = tiles16[i].tiles_info[tile];
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

absl::Status OverworldMap::BuildBitmap(OverworldBlockset &world_blockset) {
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
      gfx::CopyTile8bpp16((x * 0x10), (y * 0x10), world_blockset[xt][yt],
                          bitmap_data_, current_blockset_);
    }
  }
  return absl::OkStatus();
}

}  // namespace zelda3
}  // namespace yaze
