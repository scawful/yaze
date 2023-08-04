#ifndef YAZE_APP_GFX_PALETTE_H
#define YAZE_APP_GFX_PALETTE_H

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

struct snes_color {
  uchar red;
  uchar blue;
  uchar green;
};
using snes_color = struct snes_color;

struct snes_palette {
  uint id;
  uint size;
  snes_color* colors;
};
using snes_palette = struct snes_palette;

ushort ConvertRGBtoSNES(const snes_color color);
snes_color ConvertSNEStoRGB(const ushort snes_color);
snes_palette* Extract(const char* data, const unsigned int offset,
                      const unsigned int palette_size);
char* Convert(const snes_palette pal);

struct SNESColor {
  SNESColor();
  explicit SNESColor(ImVec4);
  explicit SNESColor(snes_color);

  void setRgb(ImVec4);
  void setSNES(snes_color);
  void setSNES(uint16_t);
  void setTransparent(bool t) { transparent = t; }

  auto RGB() const {
    return ImVec4(rgb.x / 255, rgb.y / 255, rgb.z / 255, rgb.w);
  }

  float* ToFloatArray() {
    static std::vector<float> colorArray(4);
    colorArray[0] = rgb.x / 255.0f;
    colorArray[1] = rgb.y / 255.0f;
    colorArray[2] = rgb.z / 255.0f;
    colorArray[3] = rgb.w;
    return colorArray.data();
  }

  bool modified = false;
  bool transparent = false;
  uint16_t snes = 0;
  ImVec4 rgb;
};

SNESColor GetCgxColor(short color);

std::vector<SNESColor> GetColFileData(uchar* data);

class SNESPalette {
 public:
  SNESPalette() = default;
  explicit SNESPalette(uint8_t mSize);
  explicit SNESPalette(char* snesPal);
  explicit SNESPalette(const unsigned char* snes_pal);
  explicit SNESPalette(const std::vector<ImVec4>&);
  explicit SNESPalette(const std::vector<snes_color>&);
  explicit SNESPalette(const std::vector<SNESColor>&);

  void Create(const std::vector<SNESColor>&);
  void AddColor(SNESColor color) {
    colors.push_back(color);
    size_++;
  }
  auto GetColor(int i) const { return colors[i]; }
  void Clear() {
    colors.clear();
    size_ = 0;
  }

  SNESColor operator[](int i) {
    if (i > size_) {
      std::cout << "SNESPalette: Index out of bounds" << std::endl;
      return colors[0];
    }
    return colors[i];
  }

  void operator()(int i, const SNESColor& color) {
    if (i >= size_) {
      std::cout << "SNESPalette: Index out of bounds" << std::endl;
      return;
    }
    colors[i] = color;
  }

  void operator()(int i, const ImVec4& color) {
    if (i >= size_) {
      std::cout << "SNESPalette: Index out of bounds" << std::endl;
      return;
    }
    colors[i].rgb.x = color.x;
    colors[i].rgb.y = color.y;
    colors[i].rgb.z = color.z;
    colors[i].rgb.w = color.w;
    colors[i].modified = true;
  }

  char* encode();
  SDL_Palette* GetSDL_Palette();

  int size_ = 0;
  auto size() const { return colors.size(); }
  std::vector<SNESColor> colors;
};

struct PaletteGroup {
  PaletteGroup() = default;
  explicit PaletteGroup(uint8_t mSize);
  void AddPalette(SNESPalette pal) {
    palettes.emplace_back(pal);
    size_ = palettes.size();
  }
  void AddColor(SNESColor color) {
    if (size_ == 0) {
      palettes.emplace_back();
    }
    palettes[0].AddColor(color);
  }
  void Clear() {
    palettes.clear();
    size_ = 0;
  }
  SNESPalette operator[](int i) {
    if (i > size_) {
      std::cout << "PaletteGroup: Index out of bounds" << std::endl;
      return palettes[0];
    }
    return palettes[i];
  }
  int size_ = 0;
  auto size() const { return palettes.size(); }
  std::vector<SNESPalette> palettes;
};

PaletteGroup CreatePaletteGroupFromColFile(std::vector<SNESColor>& colors);

}  // namespace gfx
}  // namespace app
}  // namespace yaze

#endif  // YAZE_APP_GFX_PALETTE_H