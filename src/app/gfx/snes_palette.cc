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
#include "app/core/constants.h"

namespace yaze {
namespace app {
namespace gfx {

// Define a hash map to hold the addresses of different palette groups
const absl::flat_hash_map<std::string, uint32_t> paletteGroupAddresses = {
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

// Define a hash map to hold the number of colors in each palette group
const absl::flat_hash_map<std::string, uint32_t> paletteGroupColorCounts = {
    {"ow_main", 35},     {"ow_aux", 21},         {"ow_animated", 7},
    {"hud", 32},         {"global_sprites", 60}, {"armors", 15},
    {"swords", 3},       {"shields", 4},         {"sprites_aux1", 7},
    {"sprites_aux2", 7}, {"sprites_aux3", 7},    {"dungeon_main", 90},
    {"grass", 1},        {"3d_object", 8},       {"ow_mini_map", 128},
};

constexpr uint16_t SNES_RED_MASK = 32;
constexpr uint16_t SNES_GREEN_MASK = 32;
constexpr uint16_t SNES_BLUE_MASK = 32;

constexpr uint16_t SNES_GREEN_SHIFT = 32;
constexpr uint16_t SNES_BLUE_SHIFT = 1024;

uint16_t ConvertRGBtoSNES(const snes_color& color) {
  uint16_t red = color.red / 8;
  uint16_t green = color.green / 8;
  uint16_t blue = color.blue / 8;
  return (blue * SNES_BLUE_SHIFT) + (green * SNES_GREEN_SHIFT) + red;
}

uint16_t ConvertRGBtoSNES(const ImVec4& color) {
  snes_color new_color;
  new_color.red = color.x * 255;
  new_color.green = color.y * 255;
  new_color.blue = color.z * 255;
  return ConvertRGBtoSNES(new_color);
}

snes_color ConvertSNEStoRGB(uint16_t color_snes) {
  snes_color result;

  result.red = (color_snes % SNES_RED_MASK) * 8;
  result.green = ((color_snes / SNES_GREEN_MASK) % SNES_GREEN_MASK) * 8;
  result.blue = ((color_snes / SNES_BLUE_SHIFT) % SNES_BLUE_MASK) * 8;

  result.red += result.red / SNES_RED_MASK;
  result.green += result.green / SNES_GREEN_MASK;
  result.blue += result.blue / SNES_BLUE_MASK;

  return result;
}

std::vector<snes_color> Extract(const char* data, unsigned int offset,
                                unsigned int palette_size) {
  std::vector<snes_color> palette(palette_size);
  for (unsigned int i = 0; i < palette_size * 2; i += 2) {
    uint16_t snes_color = (static_cast<uint8_t>(data[offset + i + 1]) << 8) |
                          static_cast<uint8_t>(data[offset + i]);
    palette[i / 2] = ConvertSNEStoRGB(snes_color);
  }
  return palette;
}

std::vector<char> Convert(const std::vector<snes_color>& palette) {
  std::vector<char> data(palette.size() * 2);
  for (unsigned int i = 0; i < palette.size(); i++) {
    uint16_t snes_data = ConvertRGBtoSNES(palette[i]);
    data[i * 2] = snes_data & 0xFF;
    data[i * 2 + 1] = snes_data >> 8;
  }
  return data;
}

SNESColor ReadColorFromROM(int offset, const uchar* rom) {
  short color = (ushort)((rom[offset + 1]) << 8) | rom[offset];
  snes_color new_color;
  new_color.red = (color & 0x1F) * 8;
  new_color.green = ((color >> 5) & 0x1F) * 8;
  new_color.blue = ((color >> 10) & 0x1F) * 8;
  SNESColor snes_color(new_color);
  return snes_color;
}

SNESColor GetCgxColor(uint16_t color) {
  ImVec4 rgb;
  rgb.x = (color & 0x1F) * 8;
  rgb.y = ((color & 0x3E0) >> 5) * 8;
  rgb.z = ((color & 0x7C00) >> 10) * 8;
  SNESColor toret;
  toret.SetRGB(rgb);
  return toret;
}

std::vector<SNESColor> GetColFileData(uchar* data) {
  std::vector<SNESColor> colors;
  colors.reserve(256);
  colors.resize(256);

  for (int i = 0; i < 512; i += 2) {
    colors[i / 2] = GetCgxColor((uint16_t)((data[i + 1] << 8) + data[i]));
  }

  return colors;
}

// ============================================================================

SNESPalette::SNESPalette(uint8_t mSize) : size_(mSize) {
  for (unsigned int i = 0; i < mSize; i++) {
    SNESColor col;
    colors.push_back(col);
  }
}

SNESPalette::SNESPalette(char* data) : size_(sizeof(data) / 2) {
  assert((sizeof(data) % 4 == 0) && (sizeof(data) <= 32));
  for (unsigned i = 0; i < sizeof(data); i += 2) {
    SNESColor col;
    col.SetSNES(static_cast<uchar>(data[i + 1]) << 8);
    col.SetSNES(col.GetSNES() | static_cast<uchar>(data[i]));
    snes_color mColor = ConvertSNEStoRGB(col.GetSNES());
    col.SetRGB(ImVec4(mColor.red, mColor.green, mColor.blue, 1.f));
    colors.push_back(col);
  }
}

SNESPalette::SNESPalette(const unsigned char* snes_pal)
    : size_(sizeof(snes_pal) / 2) {
  assert((sizeof(snes_pal) % 4 == 0) && (sizeof(snes_pal) <= 32));
  for (unsigned i = 0; i < sizeof(snes_pal); i += 2) {
    SNESColor col;
    col.SetSNES(snes_pal[i + 1] << (uint16_t)8);
    col.SetSNES(col.GetSNES() | snes_pal[i]);
    snes_color mColor = ConvertSNEStoRGB(col.GetSNES());
    col.SetRGB(ImVec4(mColor.red, mColor.green, mColor.blue, 1.f));
    colors.push_back(col);
  }
}

SNESPalette::SNESPalette(const std::vector<ImVec4>& cols) {
  for (const auto& each : cols) {
    SNESColor scol;
    scol.SetRGB(each);
    colors.push_back(scol);
  }
  size_ = cols.size();
}

SNESPalette::SNESPalette(const std::vector<snes_color>& cols) {
  for (const auto& each : cols) {
    SNESColor scol;
    scol.SetSNES(ConvertRGBtoSNES(each));
    colors.push_back(scol);
  }
  size_ = cols.size();
}

SNESPalette::SNESPalette(const std::vector<SNESColor>& cols) {
  for (const auto& each : cols) {
    colors.push_back(each);
  }
  size_ = cols.size();
}

SDL_Palette* SNESPalette::GetSDL_Palette() {
  auto sdl_palette = std::make_shared<SDL_Palette>();
  sdl_palette->ncolors = size_;

  auto color = std::vector<SDL_Color>(size_);
  for (int i = 0; i < size_; i++) {
    color[i].r = (uint8_t)colors[i].GetRGB().x * 100;
    color[i].g = (uint8_t)colors[i].GetRGB().y * 100;
    color[i].b = (uint8_t)colors[i].GetRGB().z * 100;
    color[i].a = 0;
    std::cout << "Color " << i << " added (R:" << color[i].r
              << " G:" << color[i].g << " B:" << color[i].b << ")" << std::endl;
  }
  sdl_palette->colors = color.data();
  return sdl_palette.get();
}

SNESPalette ReadPaletteFromROM(int offset, int num_colors, const uchar* rom) {
  int color_offset = 0;
  std::vector<gfx::SNESColor> colors(num_colors);

  while (color_offset < num_colors) {
    short color = (ushort)((rom[offset + 1]) << 8) | rom[offset];
    gfx::snes_color new_color;
    new_color.red = (color & 0x1F) * 8;
    new_color.green = ((color >> 5) & 0x1F) * 8;
    new_color.blue = ((color >> 10) & 0x1F) * 8;
    colors[color_offset].SetSNES(ConvertRGBtoSNES(new_color));
    if (color_offset == 0) {
      colors[color_offset].SetTransparent(true);
    }
    color_offset++;
    offset += 2;
  }

  gfx::SNESPalette palette(colors);
  return palette;
}

uint32_t GetPaletteAddress(const std::string& group_name, size_t palette_index,
                           size_t color_index) {
  // Retrieve the base address for the palette group
  uint32_t base_address = paletteGroupAddresses.at(group_name);

  // Retrieve the number of colors for each palette in the group
  uint32_t colors_per_palette = paletteGroupColorCounts.at(group_name);

  // Calculate the address for thes specified color in the ROM
  uint32_t address = base_address + (palette_index * colors_per_palette * 2) +
                     (color_index * 2);

  return address;
}

std::array<float, 4> ToFloatArray(const SNESColor& color) {
  std::array<float, 4> colorArray;
  colorArray[0] = color.GetRGB().x / 255.0f;
  colorArray[1] = color.GetRGB().y / 255.0f;
  colorArray[2] = color.GetRGB().z / 255.0f;
  colorArray[3] = color.GetRGB().w;
  return colorArray;
}

PaletteGroup::PaletteGroup(uint8_t mSize) : size_(mSize) {}

PaletteGroup CreatePaletteGroupFromColFile(
    std::vector<SNESColor>& palette_rows) {
  PaletteGroup toret;

  for (int i = 0; i < palette_rows.size(); i += 8) {
    SNESPalette palette;
    for (int j = 0; j < 8; j++) {
      palette.AddColor(palette_rows[i + j].GetRomRGB());
    }
    toret.AddPalette(palette);
  }
  return toret;
}

// Take a SNESPalette with N many colors and divide it into palettes of 8 colors
// each
PaletteGroup CreatePaletteGroupFromLargePalette(SNESPalette& palette) {
  PaletteGroup toret;

  std::cout << "Palette size is " << palette.size() << std::endl;

  for (int i = 0; i < palette.size(); i += 8) {
    SNESPalette new_palette;
    if (i + 8 < palette.size()) {
      for (int j = 0; j < 8; j++) {
        new_palette.AddColor(palette[i + j]);
      }
    }

    toret.AddPalette(new_palette);
  }
  return toret;
}

}  // namespace gfx
}  // namespace app
}  // namespace yaze