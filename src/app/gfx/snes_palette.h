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

  auto RGB() {
    return ImVec4(rgb.x / 255, rgb.y / 255, rgb.z / 255, rgb.w);
  }

  bool transparent = false;
  uint16_t snes = 0;
  ImVec4 rgb;
};

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
  void AddColor(SNESColor color) { colors.push_back(color); }
  auto GetColor(int i) const { return colors[i]; }

  SNESColor operator[](int i) {
    if (i > size_) {
      std::cout << "SNESPalette: Index out of bounds" << std::endl;
      return colors[0];
    }
    return colors[i];
  }

  char* encode();
  SDL_Palette* GetSDL_Palette();

  int size_ = 0;
  std::vector<SNESColor> colors;
};

struct PaletteGroup {
  PaletteGroup() = default;
  explicit PaletteGroup(uint8_t mSize);
  void AddPalette(SNESPalette pal) {
    palettes.emplace_back(pal);
    size = palettes.size();
  }
  void AddColor(SNESColor color) {
    if (size == 0) {
      SNESPalette empty_pal;
      palettes.emplace_back(empty_pal);
    }
    palettes[0].AddColor(color);
  }
  SNESPalette operator[](int i) {
    if (i > size) {
      std::cout << "PaletteGroup: Index out of bounds" << std::endl;
      return palettes[0];
    }
    return palettes[i];
  }
  int size = 0;
  std::vector<SNESPalette> palettes;
};

}  // namespace gfx
}  // namespace app
}  // namespace yaze

#endif  // YAZE_APP_GFX_PALETTE_H