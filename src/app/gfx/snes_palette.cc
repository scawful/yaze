#include "snes_palette.h"

#include <SDL.h>
#include <imgui/imgui.h>

#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <memory>
#include <vector>

#include "app/core/constants.h"

namespace yaze {
namespace app {
namespace gfx {

ushort ConvertRGBtoSNES(const snes_color color) {
  uchar red = color.red / 8;
  uchar green = color.green / 8;
  uchar blue = color.blue / 8;
  return blue * 1024 + green * 32 + red;
}

snes_color ConvertSNEStoRGB(const ushort color) {
  snes_color toret;

  toret.red = ((color) % 32) * 8;
  toret.green = ((color / 32) % 32) * 8;
  toret.blue = ((color / 1024) % 32) * 8;

  toret.red = toret.red + toret.red / 32;
  toret.green = toret.green + toret.green / 32;
  toret.blue = toret.blue + toret.blue / 32;
  return toret;
}

snes_palette* Extract(const char* data, const unsigned int offset,
                      const unsigned int palette_size) {
  snes_palette* toret = nullptr;  // palette_create(palette_size, 0)
  unsigned colnum = 0;
  for (int i = 0; i < palette_size * 2; i += 2) {
    unsigned short snes_color;
    snes_color = ((uchar)data[offset + i + 1]) << 8;
    snes_color = snes_color | ((uchar)data[offset + i]);
    toret->colors[colnum] = ConvertSNEStoRGB(snes_color);
    colnum++;
  }
  return toret;
}

char* Convert(const snes_palette pal) {
  char* toret = (char*)malloc(pal.size * 2);
  for (unsigned int i = 0; i < pal.size; i++) {
    unsigned short snes_data = ConvertRGBtoSNES(pal.colors[i]);
    toret[i * 2] = snes_data & 0xFF;
    toret[i * 2 + 1] = snes_data >> 8;
  }
  return toret;
}

// ============================================================================

SNESColor::SNESColor() : rgb(ImVec4(0.f, 0.f, 0.f, 0.f)) {}

SNESColor::SNESColor(snes_color val) {
  rgb.x = val.red;
  rgb.y = val.blue;
  rgb.z = val.green;
}

SNESColor::SNESColor(ImVec4 val) : rgb(val) {
  snes_color col;
  col.red = (uchar)val.x;
  col.blue = (uchar)val.y;
  col.green = (uchar)val.z;
  snes = ConvertRGBtoSNES(col);
}

void SNESColor::setRgb(ImVec4 val) {
  rgb = val;
  snes_color col;
  col.red = val.x;
  col.blue = val.y;
  col.green = val.z;
  snes = ConvertRGBtoSNES(col);
}

void SNESColor::setSNES(snes_color val) {
  rgb = ImVec4(val.red, val.green, val.blue, 1.f);
}

void SNESColor::setSNES(uint16_t val) {
  snes = val;
  snes_color col = ConvertSNEStoRGB(val);
  rgb = ImVec4(col.red, col.green, col.blue, 1.f);
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
    col.snes = static_cast<uchar>(data[i + 1]) << 8;
    col.snes = col.snes | static_cast<uchar>(data[i]);
    snes_color mColor = ConvertSNEStoRGB(col.snes);
    col.rgb = ImVec4(mColor.red, mColor.green, mColor.blue, 1.f);
    colors.push_back(col);
  }
}

SNESPalette::SNESPalette(const unsigned char* snes_pal)
    : size_(sizeof(snes_pal) / 2) {
  assert((sizeof(snes_pal) % 4 == 0) && (sizeof(snes_pal) <= 32));
  for (unsigned i = 0; i < sizeof(snes_pal); i += 2) {
    SNESColor col;
    col.snes = snes_pal[i + 1] << (uint16_t)8;
    col.snes = col.snes | snes_pal[i];
    snes_color mColor = ConvertSNEStoRGB(col.snes);
    col.rgb = ImVec4(mColor.red, mColor.green, mColor.blue, 1.f);
    colors.push_back(col);
  }
}

SNESPalette::SNESPalette(const std::vector<ImVec4>& cols) {
  for (const auto& each : cols) {
    SNESColor scol;
    scol.setRgb(each);
    colors.push_back(scol);
  }
  size_ = cols.size();
}

SNESPalette::SNESPalette(const std::vector<snes_color>& cols) {
  for (const auto& each : cols) {
    SNESColor scol;
    scol.setSNES(each);
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

void SNESPalette::Create(const std::vector<SNESColor>& cols) {
  for (const auto each : cols) {
    colors.push_back(each);
  }
  size_ = cols.size();
}

char* SNESPalette::encode() {
  auto data = new char[size_ * 2];
  for (unsigned int i = 0; i < size_; i++) {
    std::cout << colors[i].snes << std::endl;
    data[i * 2] = (char)(colors[i].snes & 0xFF);
    data[i * 2 + 1] = (char)(colors[i].snes >> 8);
  }
  return data;
}

SDL_Palette* SNESPalette::GetSDL_Palette() {
  auto sdl_palette = std::make_shared<SDL_Palette>();
  sdl_palette->ncolors = size_;

  auto color = std::vector<SDL_Color>(size_);
  for (int i = 0; i < size_; i++) {
    color[i].r = (uint8_t)colors[i].rgb.x * 100;
    color[i].g = (uint8_t)colors[i].rgb.y * 100;
    color[i].b = (uint8_t)colors[i].rgb.z * 100;
    color[i].a = 0;
    std::cout << "Color " << i << " added (R:" << color[i].r
              << " G:" << color[i].g << " B:" << color[i].b << ")" << std::endl;
  }
  sdl_palette->colors = color.data();
  return sdl_palette.get();
}

PaletteGroup::PaletteGroup(uint8_t mSize) : size(mSize) {}

}  // namespace gfx
}  // namespace app
}  // namespace yaze