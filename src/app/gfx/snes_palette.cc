#include "snes_palette.h"

#include <SDL2/SDL.h>
#include <imgui/imgui.h>
#include <palette.h>
#include <tile.h>

#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <memory>
#include <vector>

namespace yaze {
namespace app {
namespace gfx {

SNESColor::SNESColor() : rgb(ImVec4(0.f, 0.f, 0.f, 0.f)) {}

SNESColor::SNESColor(ImVec4 val) : rgb(val) {
  m_color col;
  col.red = (uchar)val.x;
  col.blue = (uchar)val.y;
  col.green = (uchar)val.z;
  snes = convertcolor_rgb_to_snes(col);
}

void SNESColor::setRgb(ImVec4 val) {
  rgb = val;
  m_color col;
  col.red = val.x;
  col.blue = val.y;
  col.green = val.z;
  snes = convertcolor_rgb_to_snes(col);
}

void SNESColor::setSNES(uint16_t val) {
  snes = val;
  m_color col = convertcolor_snes_to_rgb(val);
  rgb = ImVec4(col.red, col.green, col.blue, 1.f);
}

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
    m_color mColor = convertcolor_snes_to_rgb(col.snes);
    col.rgb = ImVec4(mColor.red, mColor.green, mColor.blue, 1.f);
    colors.push_back(col);
  }
}

SNESPalette::SNESPalette(const unsigned char* snes_pal) : size_(sizeof(snes_pal) / 2) {
  assert((sizeof(snes_pal) % 4 == 0) && (sizeof(snes_pal) <= 32));
  for (unsigned i = 0; i < sizeof(snes_pal); i += 2) {
    SNESColor col;
    col.snes = snes_pal[i + 1] << (uint16_t)8;
    col.snes = col.snes | snes_pal[i];
    m_color mColor = convertcolor_snes_to_rgb(col.snes);
    col.rgb = ImVec4(mColor.red, mColor.green, mColor.blue, 1.f);
    colors.push_back(col);
  }
}

SNESPalette::SNESPalette(const std::vector<ImVec4> & cols) {
  for (const auto& each : cols) {
    SNESColor scol;
    scol.setRgb(each);
    colors.push_back(scol);
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
  std::cout << "Converting SNESPalette to SDL_Palette" << std::endl;
  auto sdl_palette = std::make_shared<SDL_Palette>();
  sdl_palette->ncolors = size_;

  auto color = std::vector<SDL_Color>(size_);
  for (int i = 0; i < size_; i++) {
    color[i].r = (uint8_t)colors[i].rgb.x * 100;
    color[i].g = (uint8_t)colors[i].rgb.y * 100;
    color[i].b = (uint8_t)colors[i].rgb.z * 100;
    std::cout << "Color " << i << " added (R:" << color[i].r
              << " G:" << color[i].g << " B:" << color[i].b << ")"
              << std::endl;
  }
  sdl_palette->colors = color.data();

  // store the pointers to free them later
  sdl_palettes_.push_back(sdl_palette);
  colors_.push_back(color);

  return sdl_palette.get();
}

}  // namespace gfx
}  // namespace app
}  // namespace yaze