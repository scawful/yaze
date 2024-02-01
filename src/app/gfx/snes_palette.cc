#include "snes_palette.h"

#include <SDL.h>
#include <imgui/imgui.h>

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

namespace yaze {
namespace app {
namespace gfx {

const absl::flat_hash_map<std::string, uint32_t> kPaletteGroupAddressMap = {
    {"ow_main", core::overworldPaletteMain},
    {"ow_aux", core::overworldPaletteAuxialiary},
    {"ow_animated", core::overworldPaletteAnimated},
    {"hud", core::hudPalettes},
    {"global_sprites", core::globalSpritePalettesLW},
    {"armors", core::armorPalettes},
    {"swords", core::swordPalettes},
    {"shields", core::shieldPalettes},
    {"sprites_aux1", core::spritePalettesAux1},
    {"sprites_aux2", core::spritePalettesAux2},
    {"sprites_aux3", core::spritePalettesAux3},
    {"dungeon_main", core::dungeonMainPalettes},
    {"grass", core::hardcodedGrassLW},
    {"3d_object", core::triforcePalette},
    {"ow_mini_map", core::overworldMiniMapPalettes},
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
    gfx::snes_color new_color;
    new_color.red = (color & 0x1F) * 8;
    new_color.green = ((color >> 5) & 0x1F) * 8;
    new_color.blue = ((color >> 10) & 0x1F) * 8;
    colors[color_offset].set_snes(ConvertRGBtoSNES(new_color));
    if (color_offset == 0) {
      colors[color_offset].SetTransparent(true);
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
      for (int j = 0; j < 8; j++) {
        new_palette.AddColor(palette[i + j]);
      }
    }

    RETURN_IF_ERROR(toret.AddPalette(new_palette));
  }
  return toret;
}

}  // namespace gfx
}  // namespace app
}  // namespace yaze