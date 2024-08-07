#include "snes_palette.h"

#include <SDL.h>

#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <memory>
#include <vector>

#include "absl/container/flat_hash_map.h"  // for flat_hash_map
#include "absl/status/status.h"            // for Status
#include "absl/status/statusor.h"
#include "app/core/constants.h"
#include "app/gfx/snes_color.h"
#include "imgui/imgui.h"

namespace yaze {
namespace app {
namespace gfx {

/**
 * @namespace yaze::app::gfx::palette_group_internal
 * @brief Internal functions for loading palettes by group.
 */
namespace palette_group_internal {
absl::Status LoadOverworldMainPalettes(const Bytes& rom_data,
                                       gfx::PaletteGroupMap& palette_groups) {
  auto data = rom_data.data();
  for (int i = 0; i < 6; i++) {
    RETURN_IF_ERROR(palette_groups.overworld_main.AddPalette(
        gfx::ReadPaletteFromRom(kOverworldPaletteMain + (i * (35 * 2)),
                                /*num_colors*/ 35, data)))
  }
  return absl::OkStatus();
}

absl::Status LoadOverworldAuxiliaryPalettes(
    const Bytes& rom_data, gfx::PaletteGroupMap& palette_groups) {
  auto data = rom_data.data();
  for (int i = 0; i < 20; i++) {
    RETURN_IF_ERROR(palette_groups.overworld_aux.AddPalette(
        gfx::ReadPaletteFromRom(kOverworldPaletteAux + (i * (21 * 2)),
                                /*num_colors*/ 21, data)))
  }
  return absl::OkStatus();
}

absl::Status LoadOverworldAnimatedPalettes(
    const Bytes& rom_data, gfx::PaletteGroupMap& palette_groups) {
  auto data = rom_data.data();
  for (int i = 0; i < 14; i++) {
    RETURN_IF_ERROR(
        palette_groups.overworld_animated.AddPalette(gfx::ReadPaletteFromRom(
            kOverworldPaletteAnimated + (i * (7 * 2)), 7, data)))
  }
  return absl::OkStatus();
}

absl::Status LoadHUDPalettes(const Bytes& rom_data,
                             gfx::PaletteGroupMap& palette_groups) {
  auto data = rom_data.data();
  for (int i = 0; i < 2; i++) {
    RETURN_IF_ERROR(palette_groups.hud.AddPalette(
        gfx::ReadPaletteFromRom(kHudPalettes + (i * 64), 32, data)))
  }
  return absl::OkStatus();
}

absl::Status LoadGlobalSpritePalettes(const Bytes& rom_data,
                                      gfx::PaletteGroupMap& palette_groups) {
  auto data = rom_data.data();
  RETURN_IF_ERROR(palette_groups.global_sprites.AddPalette(
      gfx::ReadPaletteFromRom(kGlobalSpritesLW, 60, data)))
  RETURN_IF_ERROR(palette_groups.global_sprites.AddPalette(
      gfx::ReadPaletteFromRom(globalSpritePalettesDW, 60, data)))
  return absl::OkStatus();
}

absl::Status LoadArmorPalettes(const Bytes& rom_data,
                               gfx::PaletteGroupMap& palette_groups) {
  auto data = rom_data.data();
  for (int i = 0; i < 5; i++) {
    RETURN_IF_ERROR(palette_groups.armors.AddPalette(
        gfx::ReadPaletteFromRom(kArmorPalettes + (i * 30), 15, data)))
  }
  return absl::OkStatus();
}

absl::Status LoadSwordPalettes(const Bytes& rom_data,
                               gfx::PaletteGroupMap& palette_groups) {
  auto data = rom_data.data();
  for (int i = 0; i < 4; i++) {
    RETURN_IF_ERROR(palette_groups.swords.AddPalette(
        gfx::ReadPaletteFromRom(kSwordPalettes + (i * 6), 3, data)))
  }
  return absl::OkStatus();
}

absl::Status LoadShieldPalettes(const Bytes& rom_data,
                                gfx::PaletteGroupMap& palette_groups) {
  auto data = rom_data.data();
  for (int i = 0; i < 3; i++) {
    RETURN_IF_ERROR(palette_groups.shields.AddPalette(
        gfx::ReadPaletteFromRom(kShieldPalettes + (i * 8), 4, data)))
  }
  return absl::OkStatus();
}

absl::Status LoadSpriteAux1Palettes(const Bytes& rom_data,
                                    gfx::PaletteGroupMap& palette_groups) {
  auto data = rom_data.data();
  for (int i = 0; i < 12; i++) {
    RETURN_IF_ERROR(palette_groups.sprites_aux1.AddPalette(
        gfx::ReadPaletteFromRom(kSpritesPalettesAux1 + (i * 14), 7, data)))
  }
  return absl::OkStatus();
}

absl::Status LoadSpriteAux2Palettes(const Bytes& rom_data,
                                    gfx::PaletteGroupMap& palette_groups) {
  auto data = rom_data.data();
  for (int i = 0; i < 11; i++) {
    RETURN_IF_ERROR(palette_groups.sprites_aux2.AddPalette(
        gfx::ReadPaletteFromRom(kSpritesPalettesAux2 + (i * 14), 7, data)))
  }
  return absl::OkStatus();
}

absl::Status LoadSpriteAux3Palettes(const Bytes& rom_data,
                                    gfx::PaletteGroupMap& palette_groups) {
  auto data = rom_data.data();
  for (int i = 0; i < 24; i++) {
    RETURN_IF_ERROR(palette_groups.sprites_aux3.AddPalette(
        gfx::ReadPaletteFromRom(kSpritesPalettesAux3 + (i * 14), 7, data)))
  }
  return absl::OkStatus();
}

absl::Status LoadDungeonMainPalettes(const Bytes& rom_data,
                                     gfx::PaletteGroupMap& palette_groups) {
  auto data = rom_data.data();
  for (int i = 0; i < 20; i++) {
    RETURN_IF_ERROR(palette_groups.dungeon_main.AddPalette(
        gfx::ReadPaletteFromRom(kDungeonMainPalettes + (i * 180), 90, data)))
  }
  return absl::OkStatus();
}

absl::Status LoadGrassColors(const Bytes& rom_data,
                             gfx::PaletteGroupMap& palette_groups) {
  RETURN_IF_ERROR(palette_groups.grass.AddColor(
      gfx::ReadColorFromRom(kHardcodedGrassLW, rom_data.data())))
  RETURN_IF_ERROR(palette_groups.grass.AddColor(
      gfx::ReadColorFromRom(hardcodedGrassDW, rom_data.data())))
  RETURN_IF_ERROR(palette_groups.grass.AddColor(
      gfx::ReadColorFromRom(hardcodedGrassSpecial, rom_data.data())))
  return absl::OkStatus();
}

absl::Status Load3DObjectPalettes(const Bytes& rom_data,
                                  gfx::PaletteGroupMap& palette_groups) {
  auto data = rom_data.data();
  RETURN_IF_ERROR(palette_groups.object_3d.AddPalette(
      gfx::ReadPaletteFromRom(kTriforcePalette, 8, data)))
  RETURN_IF_ERROR(palette_groups.object_3d.AddPalette(
      gfx::ReadPaletteFromRom(crystalPalette, 8, data)))
  return absl::OkStatus();
}

absl::Status LoadOverworldMiniMapPalettes(
    const Bytes& rom_data, gfx::PaletteGroupMap& palette_groups) {
  auto data = rom_data.data();
  for (int i = 0; i < 2; i++) {
    RETURN_IF_ERROR(
        palette_groups.overworld_mini_map.AddPalette(gfx::ReadPaletteFromRom(
            kOverworldMiniMapPalettes + (i * 256), 128, data)))
  }
  return absl::OkStatus();
}
}  // namespace palette_group_internal

const absl::flat_hash_map<std::string, uint32_t> kPaletteGroupAddressMap = {
    {"ow_main", kOverworldPaletteMain},
    {"ow_aux", kOverworldPaletteAux},
    {"ow_animated", kOverworldPaletteAnimated},
    {"hud", kHudPalettes},
    {"global_sprites", kGlobalSpritesLW},
    {"armors", kArmorPalettes},
    {"swords", kSwordPalettes},
    {"shields", kShieldPalettes},
    {"sprites_aux1", kSpritesPalettesAux1},
    {"sprites_aux2", kSpritesPalettesAux2},
    {"sprites_aux3", kSpritesPalettesAux3},
    {"dungeon_main", kDungeonMainPalettes},
    {"grass", kHardcodedGrassLW},
    {"3d_object", kTriforcePalette},
    {"ow_mini_map", kOverworldMiniMapPalettes},
};

const absl::flat_hash_map<std::string, uint32_t> kPaletteGroupColorCounts = {
    {"ow_main", 35},     {"ow_aux", 21},         {"ow_animated", 7},
    {"hud", 32},         {"global_sprites", 60}, {"armors", 15},
    {"swords", 3},       {"shields", 4},         {"sprites_aux1", 7},
    {"sprites_aux2", 7}, {"sprites_aux3", 7},    {"dungeon_main", 90},
    {"grass", 1},        {"3d_object", 8},       {"ow_mini_map", 128},
};

uint32_t GetPaletteAddress(const std::string& group_name, size_t palette_index,
                           size_t color_index) {
  // Retrieve the base address for the palette group
  uint32_t base_address = kPaletteGroupAddressMap.at(group_name);

  // Retrieve the number of colors for each palette in the group
  uint32_t colors_per_palette = kPaletteGroupColorCounts.at(group_name);

  // Calculate the address for thes specified color in the ROM
  uint32_t address = base_address + (palette_index * colors_per_palette * 2) +
                     (color_index * 2);

  return address;
}

// ============================================================================

SnesPalette::SnesPalette(uint8_t mSize) : size_(mSize) {
  for (unsigned int i = 0; i < mSize; i++) {
    SnesColor col;
    colors.push_back(col);
  }
  size_ = mSize;
}

SnesPalette::SnesPalette(char* data) : size_(sizeof(data) / 2) {
  assert((sizeof(data) % 4 == 0) && (sizeof(data) <= 32));
  for (unsigned i = 0; i < sizeof(data); i += 2) {
    SnesColor col;
    col.set_snes(static_cast<uchar>(data[i + 1]) << 8);
    col.set_snes(col.snes() | static_cast<uchar>(data[i]));
    snes_color mColor = ConvertSNEStoRGB(col.snes());
    col.set_rgb(ImVec4(mColor.red, mColor.green, mColor.blue, 1.f));
    colors.push_back(col);
  }
  size_ = sizeof(data) / 2;
}

SnesPalette::SnesPalette(const unsigned char* snes_pal)
    : size_(sizeof(snes_pal) / 2) {
  assert((sizeof(snes_pal) % 4 == 0) && (sizeof(snes_pal) <= 32));
  for (unsigned i = 0; i < sizeof(snes_pal); i += 2) {
    SnesColor col;
    col.set_snes(snes_pal[i + 1] << (uint16_t)8);
    col.set_snes(col.snes() | snes_pal[i]);
    snes_color mColor = ConvertSNEStoRGB(col.snes());
    col.set_rgb(ImVec4(mColor.red, mColor.green, mColor.blue, 1.f));
    colors.push_back(col);
  }
  size_ = sizeof(snes_pal) / 2;
}

SnesPalette::SnesPalette(const std::vector<ImVec4>& cols) {
  for (const auto& each : cols) {
    SnesColor scol;
    scol.set_rgb(each);
    colors.push_back(scol);
  }
  size_ = cols.size();
}

SnesPalette::SnesPalette(const std::vector<snes_color>& cols) {
  for (const auto& each : cols) {
    SnesColor scol;
    scol.set_snes(ConvertRGBtoSNES(each));
    colors.push_back(scol);
  }
  size_ = cols.size();
}

SnesPalette::SnesPalette(const std::vector<SnesColor>& cols) {
  for (const auto& each : cols) {
    colors.push_back(each);
  }
  size_ = cols.size();
}

SDL_Palette* SnesPalette::GetSDL_Palette() {
  auto sdl_palette = std::make_shared<SDL_Palette>();
  sdl_palette->ncolors = size_;

  auto color = std::vector<SDL_Color>(size_);
  for (int i = 0; i < size_; i++) {
    color[i].r = (uint8_t)colors[i].rgb().x * 100;
    color[i].g = (uint8_t)colors[i].rgb().y * 100;
    color[i].b = (uint8_t)colors[i].rgb().z * 100;
    color[i].a = 0;
    std::cout << "Color " << i << " added (R:" << color[i].r
              << " G:" << color[i].g << " B:" << color[i].b << ")" << std::endl;
  }
  sdl_palette->colors = color.data();
  return sdl_palette.get();
}

SnesPalette ReadPaletteFromRom(int offset, int num_colors, const uchar* rom) {
  int color_offset = 0;
  std::vector<gfx::SnesColor> colors(num_colors);

  while (color_offset < num_colors) {
    short color = (ushort)((rom[offset + 1]) << 8) | rom[offset];
    snes_color new_color;
    new_color.red = (color & 0x1F) * 8;
    new_color.green = ((color >> 5) & 0x1F) * 8;
    new_color.blue = ((color >> 10) & 0x1F) * 8;
    colors[color_offset].set_snes(ConvertRGBtoSNES(new_color));
    if (color_offset == 0) {
      colors[color_offset].set_transparent(true);
    }
    color_offset++;
    offset += 2;
  }

  return gfx::SnesPalette(colors);
}

std::array<float, 4> ToFloatArray(const SnesColor& color) {
  std::array<float, 4> colorArray;
  colorArray[0] = color.rgb().x / 255.0f;
  colorArray[1] = color.rgb().y / 255.0f;
  colorArray[2] = color.rgb().z / 255.0f;
  colorArray[3] = color.rgb().w;
  return colorArray;
}

PaletteGroup::PaletteGroup(uint8_t mSize) : size_(mSize) {}

absl::StatusOr<PaletteGroup> CreatePaletteGroupFromColFile(
    std::vector<SnesColor>& palette_rows) {
  PaletteGroup toret;

  for (int i = 0; i < palette_rows.size(); i += 8) {
    SnesPalette palette;
    for (int j = 0; j < 8; j++) {
      palette.AddColor(palette_rows[i + j].rom_color());
    }
    RETURN_IF_ERROR(toret.AddPalette(palette));
  }
  return toret;
}

// Take a SNESPalette with N many colors and divide it into palettes of 8 colors
absl::StatusOr<PaletteGroup> CreatePaletteGroupFromLargePalette(
    SnesPalette& palette) {
  PaletteGroup toret;

  for (int i = 0; i < palette.size(); i += 8) {
    SnesPalette new_palette;
    if (i + 8 < palette.size()) {
      // new_palette.AddColor(SnesColor(ImVec4(0,0,0,0)));
      for (int j = 0; j < 8; j++) {
        new_palette.AddColor(palette[i + j]);
      }
    }

    RETURN_IF_ERROR(toret.AddPalette(new_palette));
  }
  return toret;
}

using namespace palette_group_internal;

absl::Status LoadAllPalettes(const Bytes& rom_data, PaletteGroupMap& groups) {
  RETURN_IF_ERROR(LoadOverworldMainPalettes(rom_data, groups))
  RETURN_IF_ERROR(LoadOverworldAuxiliaryPalettes(rom_data, groups))
  RETURN_IF_ERROR(LoadOverworldAnimatedPalettes(rom_data, groups))
  RETURN_IF_ERROR(LoadHUDPalettes(rom_data, groups))
  RETURN_IF_ERROR(LoadGlobalSpritePalettes(rom_data, groups))
  RETURN_IF_ERROR(LoadArmorPalettes(rom_data, groups))
  RETURN_IF_ERROR(LoadSwordPalettes(rom_data, groups))
  RETURN_IF_ERROR(LoadShieldPalettes(rom_data, groups))
  RETURN_IF_ERROR(LoadSpriteAux1Palettes(rom_data, groups))
  RETURN_IF_ERROR(LoadSpriteAux2Palettes(rom_data, groups))
  RETURN_IF_ERROR(LoadSpriteAux3Palettes(rom_data, groups))
  RETURN_IF_ERROR(LoadDungeonMainPalettes(rom_data, groups))
  RETURN_IF_ERROR(LoadGrassColors(rom_data, groups))
  RETURN_IF_ERROR(Load3DObjectPalettes(rom_data, groups))
  RETURN_IF_ERROR(LoadOverworldMiniMapPalettes(rom_data, groups))
  return absl::OkStatus();
}

}  // namespace gfx
}  // namespace app
}  // namespace yaze
