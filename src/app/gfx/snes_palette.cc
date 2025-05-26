#include "snes_palette.h"

#include <SDL.h>

#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <vector>

#include "absl/container/flat_hash_map.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "app/gfx/snes_color.h"
#include "imgui/imgui.h"
#include "util/macro.h"

namespace yaze::gfx {

SnesPalette::SnesPalette(char *data) {
  assert((sizeof(data) % 4 == 0) && (sizeof(data) <= 32));
  for (unsigned i = 0; i < sizeof(data); i += 2) {
    SnesColor col;
    col.set_snes(static_cast<uint8_t>(data[i + 1]) << 8);
    col.set_snes(col.snes() | static_cast<uint8_t>(data[i]));
    snes_color mColor = ConvertSnesToRgb(col.snes());
    col.set_rgb(ImVec4(mColor.red, mColor.green, mColor.blue, 1.f));
    colors_[size_++] = col;
  }
}

SnesPalette::SnesPalette(const unsigned char *snes_pal) {
  assert((sizeof(snes_pal) % 4 == 0) && (sizeof(snes_pal) <= 32));
  for (unsigned i = 0; i < sizeof(snes_pal); i += 2) {
    SnesColor col;
    col.set_snes(snes_pal[i + 1] << (uint16_t)8);
    col.set_snes(col.snes() | snes_pal[i]);
    snes_color mColor = ConvertSnesToRgb(col.snes());
    col.set_rgb(ImVec4(mColor.red, mColor.green, mColor.blue, 1.f));
    colors_[size_++] = col;
  }
}

SnesPalette::SnesPalette(const char *data, size_t length) : size_(0) {
  for (size_t i = 0; i < length && size_ < kMaxColors; i += 2) {
    uint16_t color = (static_cast<uint8_t>(data[i + 1]) << 8) |
                     static_cast<uint8_t>(data[i]);
    colors_[size_++] = SnesColor(color);
  }
}

SnesPalette::SnesPalette(const std::vector<uint16_t> &colors) : size_(0) {
  for (const auto &color : colors) {
    if (size_ < kMaxColors) {
      colors_[size_++] = SnesColor(color);
    }
  }
}

SnesPalette::SnesPalette(const std::vector<SnesColor> &colors) : size_(0) {
  for (const auto &color : colors) {
    if (size_ < kMaxColors) {
      colors_[size_++] = color;
    }
  }
}

SnesPalette::SnesPalette(const std::vector<ImVec4> &colors) : size_(0) {
  for (const auto &color : colors) {
    if (size_ < kMaxColors) {
      colors_[size_++] = SnesColor(color);
    }
  }
}

/**
 * @namespace yaze::gfx::palette_group_internal
 * @brief Internal functions for loading palettes by group.
 */
namespace palette_group_internal {
absl::Status LoadOverworldMainPalettes(const std::vector<uint8_t> &rom_data,
                                       gfx::PaletteGroupMap &palette_groups) {
  auto data = rom_data.data();
  for (int i = 0; i < 6; i++) {
    palette_groups.overworld_main.AddPalette(
        gfx::ReadPaletteFromRom(kOverworldPaletteMain + (i * (35 * 2)),
                                /*num_colors=*/35, data));
  }
  return absl::OkStatus();
}

absl::Status LoadOverworldAuxiliaryPalettes(
    const std::vector<uint8_t> &rom_data,
    gfx::PaletteGroupMap &palette_groups) {
  auto data = rom_data.data();
  for (int i = 0; i < 20; i++) {
    palette_groups.overworld_aux.AddPalette(
        gfx::ReadPaletteFromRom(kOverworldPaletteAux + (i * (21 * 2)),
                                /*num_colors=*/21, data));
  }
  return absl::OkStatus();
}

absl::Status LoadOverworldAnimatedPalettes(
    const std::vector<uint8_t> &rom_data,
    gfx::PaletteGroupMap &palette_groups) {
  auto data = rom_data.data();
  for (int i = 0; i < 14; i++) {
    palette_groups.overworld_animated.AddPalette(gfx::ReadPaletteFromRom(
        kOverworldPaletteAnimated + (i * (7 * 2)), /*num_colors=*/7, data));
  }
  return absl::OkStatus();
}

absl::Status LoadHUDPalettes(const std::vector<uint8_t> &rom_data,
                             gfx::PaletteGroupMap &palette_groups) {
  auto data = rom_data.data();
  for (int i = 0; i < 2; i++) {
    palette_groups.hud.AddPalette(gfx::ReadPaletteFromRom(
        kHudPalettes + (i * 64), /*num_colors=*/32, data));
  }
  return absl::OkStatus();
}

absl::Status LoadGlobalSpritePalettes(const std::vector<uint8_t> &rom_data,
                                      gfx::PaletteGroupMap &palette_groups) {
  auto data = rom_data.data();
  palette_groups.global_sprites.AddPalette(
      gfx::ReadPaletteFromRom(kGlobalSpritesLW, /*num_colors=*/60, data));
  palette_groups.global_sprites.AddPalette(gfx::ReadPaletteFromRom(
      kGlobalSpritePalettesDW, /*num_colors=*/60, data));
  return absl::OkStatus();
}

absl::Status LoadArmorPalettes(const std::vector<uint8_t> &rom_data,
                               gfx::PaletteGroupMap &palette_groups) {
  auto data = rom_data.data();
  for (int i = 0; i < 5; i++) {
    palette_groups.armors.AddPalette(gfx::ReadPaletteFromRom(
        kArmorPalettes + (i * 30), /*num_colors=*/15, data));
  }
  return absl::OkStatus();
}

absl::Status LoadSwordPalettes(const std::vector<uint8_t> &rom_data,
                               gfx::PaletteGroupMap &palette_groups) {
  auto data = rom_data.data();
  for (int i = 0; i < 4; i++) {
    palette_groups.swords.AddPalette(gfx::ReadPaletteFromRom(
        kSwordPalettes + (i * 6), /*num_colors=*/3, data));
  }
  return absl::OkStatus();
}

absl::Status LoadShieldPalettes(const std::vector<uint8_t> &rom_data,
                                gfx::PaletteGroupMap &palette_groups) {
  auto data = rom_data.data();
  for (int i = 0; i < 3; i++) {
    palette_groups.shields.AddPalette(gfx::ReadPaletteFromRom(
        kShieldPalettes + (i * 8), /*num_colors=*/4, data));
  }
  return absl::OkStatus();
}

absl::Status LoadSpriteAux1Palettes(const std::vector<uint8_t> &rom_data,
                                    gfx::PaletteGroupMap &palette_groups) {
  auto data = rom_data.data();
  for (int i = 0; i < 12; i++) {
    palette_groups.sprites_aux1.AddPalette(gfx::ReadPaletteFromRom(
        kSpritesPalettesAux1 + (i * 14), /*num_colors=*/7, data));
  }
  return absl::OkStatus();
}

absl::Status LoadSpriteAux2Palettes(const std::vector<uint8_t> &rom_data,
                                    gfx::PaletteGroupMap &palette_groups) {
  auto data = rom_data.data();
  for (int i = 0; i < 11; i++) {
    palette_groups.sprites_aux2.AddPalette(gfx::ReadPaletteFromRom(
        kSpritesPalettesAux2 + (i * 14), /*num_colors=*/7, data));
  }
  return absl::OkStatus();
}

absl::Status LoadSpriteAux3Palettes(const std::vector<uint8_t> &rom_data,
                                    gfx::PaletteGroupMap &palette_groups) {
  auto data = rom_data.data();
  for (int i = 0; i < 24; i++) {
    palette_groups.sprites_aux3.AddPalette(gfx::ReadPaletteFromRom(
        kSpritesPalettesAux3 + (i * 14), /*num_colors=*/7, data));
  }
  return absl::OkStatus();
}

absl::Status LoadDungeonMainPalettes(const std::vector<uint8_t> &rom_data,
                                     gfx::PaletteGroupMap &palette_groups) {
  auto data = rom_data.data();
  for (int i = 0; i < 20; i++) {
    palette_groups.dungeon_main.AddPalette(gfx::ReadPaletteFromRom(
        kDungeonMainPalettes + (i * 180), /*num_colors=*/90, data));
  }
  return absl::OkStatus();
}

absl::Status LoadGrassColors(const std::vector<uint8_t> &rom_data,
                             gfx::PaletteGroupMap &palette_groups) {
  palette_groups.grass.AddColor(
      gfx::ReadColorFromRom(kHardcodedGrassLW, rom_data.data()));
  palette_groups.grass.AddColor(
      gfx::ReadColorFromRom(kHardcodedGrassDW, rom_data.data()));
  palette_groups.grass.AddColor(
      gfx::ReadColorFromRom(kHardcodedGrassSpecial, rom_data.data()));
  return absl::OkStatus();
}

absl::Status Load3DObjectPalettes(const std::vector<uint8_t> &rom_data,
                                  gfx::PaletteGroupMap &palette_groups) {
  auto data = rom_data.data();
  palette_groups.object_3d.AddPalette(
      gfx::ReadPaletteFromRom(kTriforcePalette, 8, data));
  palette_groups.object_3d.AddPalette(
      gfx::ReadPaletteFromRom(kCrystalPalette, 8, data));
  return absl::OkStatus();
}

absl::Status LoadOverworldMiniMapPalettes(
    const std::vector<uint8_t> &rom_data,
    gfx::PaletteGroupMap &palette_groups) {
  auto data = rom_data.data();
  for (int i = 0; i < 2; i++) {
    palette_groups.overworld_mini_map.AddPalette(gfx::ReadPaletteFromRom(
        kOverworldMiniMapPalettes + (i * 256), /*num_colors=*/128, data));
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

uint32_t GetPaletteAddress(const std::string &group_name, size_t palette_index,
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

SnesPalette ReadPaletteFromRom(int offset, int num_colors, const uint8_t *rom) {
  int color_offset = 0;
  std::vector<gfx::SnesColor> colors(num_colors);

  while (color_offset < num_colors) {
    short color = (uint16_t)((rom[offset + 1]) << 8) | rom[offset];
    snes_color new_color;
    new_color.red = (color & 0x1F) * 8;
    new_color.green = ((color >> 5) & 0x1F) * 8;
    new_color.blue = ((color >> 10) & 0x1F) * 8;
    colors[color_offset].set_snes(ConvertRgbToSnes(new_color));
    if (color_offset == 0) {
      colors[color_offset].set_transparent(true);
    }
    color_offset++;
    offset += 2;
  }

  return gfx::SnesPalette(colors);
}

std::array<float, 4> ToFloatArray(const SnesColor &color) {
  std::array<float, 4> colorArray;
  colorArray[0] = color.rgb().x / 255.0f;
  colorArray[1] = color.rgb().y / 255.0f;
  colorArray[2] = color.rgb().z / 255.0f;
  colorArray[3] = color.rgb().w;
  return colorArray;
}

absl::StatusOr<PaletteGroup> CreatePaletteGroupFromColFile(
    std::vector<SnesColor> &palette_rows) {
  PaletteGroup palette_group;
  for (int i = 0; i < palette_rows.size(); i += 8) {
    SnesPalette palette;
    for (int j = 0; j < 8; j++) {
      palette.AddColor(palette_rows[i + j]);
    }
    palette_group.AddPalette(palette);
  }
  return palette_group;
}

absl::StatusOr<PaletteGroup> CreatePaletteGroupFromLargePalette(
    SnesPalette &palette, int num_colors) {
  PaletteGroup palette_group;
  for (int i = 0; i < palette.size(); i += num_colors) {
    SnesPalette new_palette;
    if (i + num_colors < palette.size()) {
      for (int j = 0; j < num_colors; j++) {
        new_palette.AddColor(palette[i + j]);
      }
    }
    palette_group.AddPalette(new_palette);
  }
  return palette_group;
}

using namespace palette_group_internal;

// TODO: Refactor LoadAllPalettes to use group names, move to zelda3 namespace
absl::Status LoadAllPalettes(const std::vector<uint8_t> &rom_data,
                             PaletteGroupMap &groups) {
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

std::unordered_map<uint8_t, gfx::Paletteset> GfxContext::palettesets_;

}  // namespace yaze::gfx
