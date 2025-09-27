#include "overworld_map.h"

#include <cstddef>
#include <cstdint>
#include <unordered_map>
#include <vector>

#include "app/core/features.h"
#include "app/gfx/snes_color.h"
#include "app/gfx/snes_tile.h"
#include "app/rom.h"
#include "app/zelda3/overworld/overworld.h"

namespace yaze {
namespace zelda3 {

OverworldMap::OverworldMap(int index, Rom *rom)
    : index_(index), parent_(index), rom_(rom) {
  LoadAreaInfo();

  // Use ASM version byte as source of truth
  uint8_t asm_version = (*rom_)[OverworldCustomASMHasBeenApplied];
  
  if (asm_version != 0xFF) {
    // Custom overworld ASM is applied - use custom logic
    if (asm_version == 0x00) {
      // Special case: version 0 means flag-enabled vanilla mode
      LoadCustomOverworldData();
    } else {
      // Custom overworld ASM applied - set up custom tileset
      SetupCustomTileset(asm_version);
    }
  } else if (core::FeatureFlags::get().overworld.kLoadCustomOverworld) {
    // Pure vanilla ROM but flag enabled - set up custom data manually
    LoadCustomOverworldData();
  }
  // For pure vanilla ROMs, LoadAreaInfo already handles everything
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
  RETURN_IF_ERROR(LoadOverlay());
  RETURN_IF_ERROR(BuildBitmap(world_blockset))
  built_ = true;
  return absl::OkStatus();
}

void OverworldMap::LoadAreaInfo() {
  uint8_t asm_version = (*rom_)[OverworldCustomASMHasBeenApplied];

  // Load message ID and area size based on ASM version
  if (asm_version < 3 || asm_version == 0xFF) {
    // v2 and vanilla: use original message table
    message_id_ = (*rom_)[kOverworldMessageIds + (parent_ * 2)] |
                  ((*rom_)[kOverworldMessageIds + (parent_ * 2) + 1] << 8);

    // Load area size for v2/vanilla
    if (index_ < 0x80) {
      // For LW and DW, check the screen size byte
      uint8_t size_byte = (*rom_)[kOverworldScreenSize + (index_ & 0x3F)];
      switch (size_byte) {
        case 0:
          area_size_ = AreaSizeEnum::LargeArea;
          break;
        case 1:
        default:
          area_size_ = AreaSizeEnum::SmallArea;
          break;
        case 2:
          area_size_ = AreaSizeEnum::WideArea;
          break;
        case 3:
          area_size_ = AreaSizeEnum::TallArea;
          break;
      }
    } else {
      // For SW, use hardcoded values for v2 compatibility
      area_size_ =
          (index_ == 0x81 || index_ == 0x82 || index_ == 0x89 || index_ == 0x8A)
              ? AreaSizeEnum::LargeArea
              : AreaSizeEnum::SmallArea;
    }
  } else {
    // v3: use expanded message table and area size table
    message_id_ =
        (*rom_)[kOverworldMessagesExpanded + (parent_ * 2)] |
        ((*rom_)[kOverworldMessagesExpanded + (parent_ * 2) + 1] << 8);
    area_size_ =
        static_cast<AreaSizeEnum>((*rom_)[kOverworldScreenSize + index_]);
  }

  // Update large_map_ based on area size
  large_map_ = (area_size_ == AreaSizeEnum::LargeArea);

  // Load area-specific data based on index range
  if (index_ < kDarkWorldMapIdStart) {
    // Light World (LW) areas
    sprite_graphics_[0] = (*rom_)[kOverworldSpriteset + parent_];
    sprite_graphics_[1] =
        (*rom_)[kOverworldSpriteset + parent_ + kDarkWorldMapIdStart];
    sprite_graphics_[2] =
        (*rom_)[kOverworldSpriteset + parent_ + kSpecialWorldMapIdStart];

    area_graphics_ = (*rom_)[kAreaGfxIdPtr + parent_];
    area_palette_ = (*rom_)[kOverworldPalettesScreenToSetNew + parent_];

    sprite_palette_[0] = (*rom_)[kOverworldSpritePaletteIds + parent_];
    sprite_palette_[1] =
        (*rom_)[kOverworldSpritePaletteIds + parent_ + kDarkWorldMapIdStart];
    sprite_palette_[2] =
        (*rom_)[kOverworldSpritePaletteIds + parent_ + kSpecialWorldMapIdStart];

    area_music_[0] = (*rom_)[kOverworldMusicBeginning + parent_];
    area_music_[1] = (*rom_)[kOverworldMusicZelda + parent_];
    area_music_[2] = (*rom_)[kOverworldMusicMasterSword + parent_];
    area_music_[3] = (*rom_)[kOverworldMusicAgahnim + parent_];

    // For v2/vanilla, use original palette table
    if (asm_version < 3 || asm_version == 0xFF) {
      area_palette_ = (*rom_)[kOverworldMapPaletteIds + parent_];
    }
  } else if (index_ < kSpecialWorldMapIdStart) {
    // Dark World (DW) areas
    sprite_graphics_[0] =
        (*rom_)[kOverworldSpriteset + parent_ + kSpecialWorldMapIdStart];
    sprite_graphics_[1] =
        (*rom_)[kOverworldSpriteset + parent_ + kSpecialWorldMapIdStart];
    sprite_graphics_[2] =
        (*rom_)[kOverworldSpriteset + parent_ + kSpecialWorldMapIdStart];

    area_graphics_ = (*rom_)[kAreaGfxIdPtr + parent_];
    area_palette_ = (*rom_)[kOverworldPalettesScreenToSetNew + parent_];

    sprite_palette_[0] =
        (*rom_)[kOverworldSpritePaletteIds + parent_ + kSpecialWorldMapIdStart];
    sprite_palette_[1] =
        (*rom_)[kOverworldSpritePaletteIds + parent_ + kSpecialWorldMapIdStart];
    sprite_palette_[2] =
        (*rom_)[kOverworldSpritePaletteIds + parent_ + kSpecialWorldMapIdStart];

    area_music_[0] =
        (*rom_)[kOverworldMusicDarkWorld + (parent_ - kDarkWorldMapIdStart)];

    // For v2/vanilla, use original palette table
    if (asm_version < 3 || asm_version == 0xFF) {
      area_palette_ = (*rom_)[kOverworldMapPaletteIds + parent_];
    }
  } else {
    // Special World (SW) areas
    // Message ID already loaded above based on ASM version

    // For v3, use expanded sprite tables
    if (asm_version >= 3 && asm_version != 0xFF) {
      sprite_graphics_[0] =
          (*rom_)[kOverworldSpecialSpriteGfxGroupExpandedTemp + parent_ -
                  kSpecialWorldMapIdStart];
      sprite_graphics_[1] =
          (*rom_)[kOverworldSpecialSpriteGfxGroupExpandedTemp + parent_ -
                  kSpecialWorldMapIdStart];
      sprite_graphics_[2] =
          (*rom_)[kOverworldSpecialSpriteGfxGroupExpandedTemp + parent_ -
                  kSpecialWorldMapIdStart];

      sprite_palette_[0] = (*rom_)[kOverworldSpecialSpritePaletteExpandedTemp +
                                   parent_ - kSpecialWorldMapIdStart];
      sprite_palette_[1] = (*rom_)[kOverworldSpecialSpritePaletteExpandedTemp +
                                   parent_ - kSpecialWorldMapIdStart];
      sprite_palette_[2] = (*rom_)[kOverworldSpecialSpritePaletteExpandedTemp +
                                   parent_ - kSpecialWorldMapIdStart];
    } else {
      // For v2/vanilla, use original sprite tables
      sprite_graphics_[0] = (*rom_)[kOverworldSpecialGfxGroup + parent_ -
                                    kSpecialWorldMapIdStart];
      sprite_graphics_[1] = (*rom_)[kOverworldSpecialGfxGroup + parent_ -
                                    kSpecialWorldMapIdStart];
      sprite_graphics_[2] = (*rom_)[kOverworldSpecialGfxGroup + parent_ -
                                    kSpecialWorldMapIdStart];

      sprite_palette_[0] = (*rom_)[kOverworldSpecialPalGroup + parent_ -
                                   kSpecialWorldMapIdStart];
      sprite_palette_[1] = (*rom_)[kOverworldSpecialPalGroup + parent_ -
                                   kSpecialWorldMapIdStart];
      sprite_palette_[2] = (*rom_)[kOverworldSpecialPalGroup + parent_ -
                                   kSpecialWorldMapIdStart];
    }

    area_graphics_ = (*rom_)[kAreaGfxIdPtr + parent_];
    area_palette_ = (*rom_)[kOverworldPalettesScreenToSetNew + parent_];

    // For v2/vanilla, use original palette table and handle special cases
    if (asm_version < 3 || asm_version == 0xFF) {
      area_palette_ = (*rom_)[kOverworldMapPaletteIds + parent_];

      // Handle special world area cases
      if (index_ == 0x88 || index_ == 0x93) {
        area_graphics_ = 0x51;
        area_palette_ = 0x00;
      } else if (index_ == 0x80) {
        area_graphics_ = (*rom_)[kOverworldSpecialGfxGroup +
                                 (parent_ - kSpecialWorldMapIdStart)];
        area_palette_ = (*rom_)[kOverworldSpecialPalGroup + 1];
      } else if (index_ == 0x81 || index_ == 0x82 || index_ == 0x89 ||
                 index_ == 0x8A) {
        // Zora's Domain areas - use special sprite graphics
        sprite_graphics_[0] = 0x0E;
        sprite_graphics_[1] = 0x0E;
        sprite_graphics_[2] = 0x0E;

        area_graphics_ = (*rom_)[kOverworldSpecialGfxGroup +
                                 (parent_ - kSpecialWorldMapIdStart)];
        area_palette_ = (*rom_)[kOverworldSpecialPalGroup + 1];
      } else if (index_ == 0x94) {
        // Make this the same GFX as the true master sword area
        area_graphics_ = (*rom_)[kOverworldSpecialGfxGroup +
                                 (0x80 - kSpecialWorldMapIdStart)];
        area_palette_ = (*rom_)[kOverworldSpecialPalGroup + 1];
      } else if (index_ == 0x95) {
        // Make this the same GFX as the LW death mountain areas
        area_graphics_ = (*rom_)[kAreaGfxIdPtr + 0x03];
        area_palette_ = (*rom_)[kOverworldMapPaletteIds + 0x03];
      } else if (index_ == 0x96) {
        // Make this the same GFX as the pyramid areas
        area_graphics_ = (*rom_)[kAreaGfxIdPtr + 0x5B];
        area_palette_ = (*rom_)[kOverworldMapPaletteIds + 0x5B];
      } else if (index_ == 0x9C) {
        // Make this the same GFX as the DW death mountain areas
        area_graphics_ = (*rom_)[kAreaGfxIdPtr + 0x43];
        area_palette_ = (*rom_)[kOverworldMapPaletteIds + 0x43];
      } else {
        // Default case
        area_graphics_ = (*rom_)[kAreaGfxIdPtr + 0x00];
        area_palette_ = (*rom_)[kOverworldMapPaletteIds + 0x00];
      }
    }
  }
}

void OverworldMap::LoadCustomOverworldData() {
  // Set the main palette values based on ZScream logic
  if (index_ < 0x40 || index_ == 0x95) {  // LW
    main_palette_ = 0;
  } else if ((index_ >= 0x40 && index_ < 0x80) || index_ == 0x96) {  // DW
    main_palette_ = 1;
  } else if (index_ >= 0x80 && index_ < 0xA0) {  // SW
    main_palette_ = 0;
  }

  if (index_ == 0x03 || index_ == 0x05 ||
      index_ == 0x07) {  // LW Death Mountain
    main_palette_ = 2;
  } else if (index_ == 0x43 || index_ == 0x45 ||
             index_ == 0x47) {  // DW Death Mountain
    main_palette_ = 3;
  } else if (index_ == 0x88 || index_ == 0x93) {  // Triforce room
    main_palette_ = 4;
  }

  // Set the mosaic values based on ZScream logic
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
    default:
      mosaic_expanded_ = {false, false, false, false};
      break;
  }

  // Set up world index for GFX groups
  int index_world = 0x20;
  if (parent_ >= kDarkWorldMapIdStart &&
      parent_ < kSpecialWorldMapIdStart) {  // DW
    index_world = 0x21;
  } else if (parent_ == 0x88 || parent_ == 0x93) {  // Triforce room
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

  // Replace the variable tiles with the variable ones
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

  // Set the animated GFX values
  if (index_ == 0x03 || index_ == 0x05 || index_ == 0x07 || index_ == 0x43 ||
      index_ == 0x45 || index_ == 0x47 || index_ == 0x95) {
    animated_gfx_ = 0x59;
  } else {
    animated_gfx_ = 0x5B;
  }

  // Set the subscreen overlay values
  subscreen_overlay_ = 0x00FF;

  if (index_ == 0x00 || index_ == 0x01 || index_ == 0x08 || index_ == 0x09 ||
      index_ == 0x40 || index_ == 0x41 || index_ == 0x48 ||
      index_ == 0x49) {  // Add fog 2 to the lost woods and skull woods
    subscreen_overlay_ = 0x009D;
  } else if (index_ == 0x03 || index_ == 0x04 || index_ == 0x0B ||
             index_ == 0x0C || index_ == 0x05 || index_ == 0x06 ||
             index_ == 0x0D || index_ == 0x0E ||
             index_ == 0x07) {  // Add the sky BG to LW death mountain
    subscreen_overlay_ = 0x0095;
  } else if (index_ == 0x43 || index_ == 0x44 || index_ == 0x4B ||
             index_ == 0x4C || index_ == 0x45 || index_ == 0x46 ||
             index_ == 0x4D || index_ == 0x4E ||
             index_ == 0x47) {  // Add the lava to DW death mountain
    subscreen_overlay_ = 0x009C;
  } else if (index_ == 0x5B || index_ == 0x5C || index_ == 0x63 ||
             index_ == 0x64) {  // TODO: Might need this one too "index == 0x1B"
                                // but for now I don't think so
    subscreen_overlay_ = 0x0096;
  } else if (index_ == 0x80) {  // Add fog 1 to the master sword area
    subscreen_overlay_ = 0x0097;
  } else if (index_ ==
             0x88) {  // Add the triforce room curtains to the triforce room
    subscreen_overlay_ = 0x0093;
  }
}

void OverworldMap::SetupCustomTileset(uint8_t asm_version) {
  // Load custom palette and mosaic settings
  main_palette_ = (*rom_)[OverworldCustomMainPaletteArray + index_];
  mosaic_ = (*rom_)[OverworldCustomMosaicArray + index_] != 0x00;

  uint8_t mosaicByte = (*rom_)[OverworldCustomMosaicArray + index_];
  mosaic_expanded_ = {(mosaicByte & 0x08) != 0x00, (mosaicByte & 0x04) != 0x00,
                      (mosaicByte & 0x02) != 0x00, (mosaicByte & 0x01) != 0x00};

  // Load area size for v3
  if (asm_version >= 3 && asm_version != 0xFF) {
    uint8_t size_byte = (*rom_)[kOverworldScreenSize + index_];
    area_size_ = static_cast<AreaSizeEnum>(size_byte);
    large_map_ = (area_size_ == AreaSizeEnum::LargeArea);
  }

  // Load custom GFX groups based on ASM version
  if (asm_version >= 0x01 && asm_version != 0xFF) {
    // Load from custom GFX group array
    for (int i = 0; i < 8; i++) {
      custom_gfx_ids_[i] =
          (*rom_)[OverworldCustomTileGFXGroupArray + (index_ * 8) + i];
    }
    animated_gfx_ = (*rom_)[OverworldCustomAnimatedGFXArray + index_];
  } else {
    // Fallback to vanilla logic for ROMs without custom ASM
    int index_world = 0x20;
    if (parent_ >= kDarkWorldMapIdStart &&
        parent_ < kSpecialWorldMapIdStart) {  // DW
      index_world = 0x21;
    } else if (parent_ == 0x88 || parent_ == 0x93) {  // Triforce room
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

    // Replace the variable tiles with the variable ones
    // If the variable is 00 set it to 0xFF which is the new "don't load
    // anything" value
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

    // Set the animated GFX values
    if (index_ == 0x03 || index_ == 0x05 || index_ == 0x07 || index_ == 0x43 ||
        index_ == 0x45 || index_ == 0x47) {
      animated_gfx_ = 0x59;
    } else {
      animated_gfx_ = 0x5B;
    }
  }

  // Load subscreen overlay
  subscreen_overlay_ =
      (*rom_)[OverworldCustomSubscreenOverlayArray + (index_ * 2)];
}

void OverworldMap::LoadMainBlocksetId() {
  if (parent_ < kDarkWorldMapIdStart) {
    main_gfx_id_ = 0x20;
  } else if (parent_ >= kDarkWorldMapIdStart &&
             parent_ < kSpecialWorldMapIdStart) {
    main_gfx_id_ = 0x21;
  } else if (parent_ >= kSpecialWorldMapIdStart) {
    // Special world maps - use appropriate graphics ID based on the specific map
    if (parent_ == 0x88) {
      main_gfx_id_ = 0x24;
    } else {
      // Default special world graphics ID
      main_gfx_id_ = 0x20;
    }
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
  uint8_t asm_version = (*rom_)[OverworldCustomASMHasBeenApplied];
  
  int previous_pal_id = 0;
  int previous_spr_pal_id = 0;
  
  if (index_ > 0) {
    // Load previous palette ID based on ASM version
    if (asm_version < 3 || asm_version == 0xFF) {
      previous_pal_id = (*rom_)[kOverworldMapPaletteIds + parent_ - 1];
    } else {
      // v3 uses expanded palette table
      previous_pal_id = (*rom_)[kOverworldPalettesScreenToSetNew + parent_ - 1];
    }
    
    previous_spr_pal_id = (*rom_)[kOverworldSpritePaletteIds + parent_ - 1];
  }

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

  // Handle 0xFF palette references (use previous palette)
  if (pal1 == 0xFF) {
    pal1 = (*rom_)[rom_->version_constants().kOverworldMapPaletteGroup +
                   (previous_pal_id * 4)];
  }
  
  if (pal2 == 0xFF) {
    pal2 = (*rom_)[rom_->version_constants().kOverworldMapPaletteGroup +
                   (previous_pal_id * 4) + 1];
  }
  
  if (pal3 == 0xFF) {
    pal3 = (*rom_)[rom_->version_constants().kOverworldMapPaletteGroup +
                   (previous_pal_id * 4) + 2];
  }

  auto ow_aux_pal_group = rom_->palette_group().overworld_aux;
  ASSIGN_OR_RETURN(gfx::SnesPalette aux1,
                   GetPalette(ow_aux_pal_group, pal1, previous_pal_id, 20));
  ASSIGN_OR_RETURN(gfx::SnesPalette aux2,
                   GetPalette(ow_aux_pal_group, pal2, previous_pal_id, 20));

  // Set background color based on world type and area-specific settings
  bool use_area_specific_bg = (*rom_)[OverworldCustomAreaSpecificBGEnabled] != 0x00;
  if (use_area_specific_bg) {
    // Use area-specific background color from custom array
    area_specific_bg_color_ = (*rom_)[OverworldCustomAreaSpecificBGPalette + (parent_ * 2)] |
                              ((*rom_)[OverworldCustomAreaSpecificBGPalette + (parent_ * 2) + 1] << 8);
    // Convert 15-bit SNES color to palette color
    bgr = gfx::SnesColor(area_specific_bg_color_);
  } else {
    // Use default world-based background colors
    if (parent_ < kDarkWorldMapIdStart) {
      bgr = grass_pal_group[0][0];  // LW
    } else if (parent_ >= kDarkWorldMapIdStart &&
               parent_ < kSpecialWorldMapIdStart) {
      bgr = grass_pal_group[0][1];  // DW
    } else if (parent_ >= 128 && parent_ < kNumOverworldMaps) {
      bgr = grass_pal_group[0][2];  // SW
    }
  }

  // Use main palette from the overworld map data (matches ZScream logic)
  pal0 = main_palette_;

  auto ow_main_pal_group = rom_->palette_group().overworld_main;
  ASSIGN_OR_RETURN(gfx::SnesPalette main,
                   GetPalette(ow_main_pal_group, pal0, previous_pal_id, 255));
  auto ow_animated_pal_group = rom_->palette_group().overworld_animated;
  ASSIGN_OR_RETURN(gfx::SnesPalette animated,
                   GetPalette(ow_animated_pal_group, std::min((int)pal3, 13),
                              previous_pal_id, 14));

  auto hud_pal_group = rom_->palette_group().hud;
  gfx::SnesPalette hud = hud_pal_group[0];

  // Handle 0xFF sprite palette references (use previous sprite palette)
  if (pal4 == 0xFF) {
    pal4 = (*rom_)[kOverworldSpritePaletteGroup + (previous_spr_pal_id * 2)];
  }
  
  if (pal4 == 0xFF) {
    pal4 = 0;  // Fallback to 0 if still 0xFF
  }
  
  if (pal5 == 0xFF) {
    pal5 = (*rom_)[kOverworldSpritePaletteGroup + (previous_spr_pal_id * 2) + 1];
  }
  
  if (pal5 == 0xFF) {
    pal5 = 0;  // Fallback to 0 if still 0xFF
  }

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

absl::Status OverworldMap::LoadOverlay() {
  uint8_t asm_version = (*rom_)[OverworldCustomASMHasBeenApplied];
  
  // Load overlays based on ROM version
  if (asm_version == 0xFF) {
    // Vanilla ROM - load overlay from overlay pointers
    return LoadVanillaOverlayData();
  } else {
    // Custom overworld ROM - use overlay from custom data
    overlay_id_ = subscreen_overlay_;
    has_overlay_ = (overlay_id_ != 0x00FF);
    overlay_data_.clear();
    return absl::OkStatus();
  }
}

absl::Status OverworldMap::LoadVanillaOverlayData() {
  
  // Load vanilla overlay for this map (interactive overlays for revealing holes/changing elements)
  int address = (kOverlayPointersBank << 16) +
                ((*rom_)[kOverlayPointers + (index_ * 2) + 1] << 8) +
                (*rom_)[kOverlayPointers + (index_ * 2)];
  
  // Convert SNES address to PC address
  address = ((address & 0x7F0000) >> 1) | (address & 0x7FFF);
  
  // Check if custom overlay code is present
  if ((*rom_)[kOverlayData1] == 0x6B) {
    // Use custom overlay data pointer
    address = ((*rom_)[kOverlayData2 + 2 + (index_ * 3)] << 16) +
              ((*rom_)[kOverlayData2 + 1 + (index_ * 3)] << 8) +
              (*rom_)[kOverlayData2 + (index_ * 3)];
    address = ((address & 0x7F0000) >> 1) | (address & 0x7FFF);
  }
  
  // Validate address
  if (address >= rom_->size()) {
    has_overlay_ = false;
    overlay_id_ = 0;
    overlay_data_.clear();
    return absl::OkStatus();
  }
  
  // Parse overlay data (interactive overlays)
  overlay_data_.clear();
  uint8_t b = (*rom_)[address];
  
  // Parse overlay commands until we hit END (0x60)
  while (b != 0x60 && address < rom_->size()) {
    overlay_data_.push_back(b);
    
    // Handle different overlay commands
    switch (b) {
      case 0xA9:  // LDA #$
        if (address + 2 < rom_->size()) {
          overlay_data_.push_back((*rom_)[address + 1]);
          overlay_data_.push_back((*rom_)[address + 2]);
          address += 3;
        } else {
          address++;
        }
        break;
      case 0xA2:  // LDX #$
        if (address + 2 < rom_->size()) {
          overlay_data_.push_back((*rom_)[address + 1]);
          overlay_data_.push_back((*rom_)[address + 2]);
          address += 3;
        } else {
          address++;
        }
        break;
      case 0x8D:  // STA $xxxx
        if (address + 3 < rom_->size()) {
          overlay_data_.push_back((*rom_)[address + 1]);
          overlay_data_.push_back((*rom_)[address + 2]);
          overlay_data_.push_back((*rom_)[address + 3]);
          address += 4;
        } else {
          address++;
        }
        break;
      case 0x9D:  // STA $xxxx,x
        if (address + 3 < rom_->size()) {
          overlay_data_.push_back((*rom_)[address + 1]);
          overlay_data_.push_back((*rom_)[address + 2]);
          overlay_data_.push_back((*rom_)[address + 3]);
          address += 4;
        } else {
          address++;
        }
        break;
      case 0x8F:  // STA $xxxxxx
        if (address + 4 < rom_->size()) {
          overlay_data_.push_back((*rom_)[address + 1]);
          overlay_data_.push_back((*rom_)[address + 2]);
          overlay_data_.push_back((*rom_)[address + 3]);
          overlay_data_.push_back((*rom_)[address + 4]);
          address += 5;
        } else {
          address++;
        }
        break;
      case 0x1A:  // INC A
        address++;
        break;
      case 0x4C:  // JMP
        if (address + 3 < rom_->size()) {
          overlay_data_.push_back((*rom_)[address + 1]);
          overlay_data_.push_back((*rom_)[address + 2]);
          overlay_data_.push_back((*rom_)[address + 3]);
          address += 4;
        } else {
          address++;
        }
        break;
      default:
        address++;
        break;
    }
    
    if (address < rom_->size()) {
      b = (*rom_)[address];
    } else {
      break;
    }
  }
  
  // Add the END command if we found it
  if (b == 0x60) {
    overlay_data_.push_back(0x60);
  }
  
  // Set overlay ID based on map index (simplified)
  overlay_id_ = index_;
  has_overlay_ = !overlay_data_.empty();
  
  return absl::OkStatus();
}

void OverworldMap::ProcessGraphicsBuffer(int index, int static_graphics_offset,
                                         int size, uint8_t *all_gfx) {
  // Ensure we don't go out of bounds
  int max_offset = static_graphics_offset * size + size;
  if (max_offset > rom_->graphics_buffer().size()) {
    // Fill with zeros if out of bounds
    for (int i = 0; i < size; i++) {
      current_gfx_[(index * size) + i] = 0x00;
    }
    return;
  }
  
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
  
  // Process the 8 main graphics sheets (slots 0-7)
  for (int i = 0; i < 8; i++) {
    if (static_graphics_[i] != 0) {
      ProcessGraphicsBuffer(i, static_graphics_[i], 0x1000,
                            rom_->graphics_buffer().data());
    }
  }
  
  // Process sprite graphics (slots 8-15)
  for (int i = 8; i < 16; i++) {
    if (static_graphics_[i] != 0) {
      ProcessGraphicsBuffer(i, static_graphics_[i], 0x1000,
                            rom_->graphics_buffer().data());
    }
  }
  
  // Process animated graphics if available (slot 16)
  if (static_graphics_[16] != 0) {
    ProcessGraphicsBuffer(7, static_graphics_[16], 0x1000,
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
