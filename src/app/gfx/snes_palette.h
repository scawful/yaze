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

#include "absl/base/casts.h"
#include "app/core/constants.h"

namespace yaze {
namespace app {
namespace gfx {

struct snes_color {
  uint16_t red;
  uint16_t blue;
  uint16_t green;
};
using snes_color = struct snes_color;

struct snes_palette {
  uint id;
  uint size;
  snes_color* colors;
};
using snes_palette = struct snes_palette;

uint16_t ConvertRGBtoSNES(const snes_color& color);
snes_color ConvertSNEStoRGB(uint16_t snes_color);
std::vector<snes_color> Extract(const char* data, unsigned int offset,
                                unsigned int palette_size);
std::vector<char> Convert(const std::vector<snes_color>& palette);

struct SNESColor {
  SNESColor() : rgb(0.f, 0.f, 0.f, 0.f), snes(0) {}

  explicit SNESColor(const ImVec4 val) : rgb(val) {
    snes_color color;
    color.red = val.x / 255;
    color.green = val.y / 255;
    color.blue = val.z / 255;
    snes = ConvertRGBtoSNES(color);
  }

  explicit SNESColor(const snes_color val)
      : rgb(val.red, val.green, val.blue, 255.f),
        snes(ConvertRGBtoSNES(val)),
        rom_color(val) {}

  ImVec4 GetRGB() const { return rgb; }
  void SetRGB(const ImVec4 val) {
    rgb.x = val.x / 255;
    rgb.y = val.y / 255;
    rgb.z = val.z / 255;
    snes_color color;
    color.red = val.x;
    color.green = val.y;
    color.blue = val.z;
    rom_color = color;
    snes = ConvertRGBtoSNES(color);
    modified = true;
  }

  // Used for indexed Bitmaps from CGX files
  snes_color GetRomRGB() const { return rom_color; }

  uint16_t GetSNES() const { return snes; }

  void SetSNES(uint16_t val) {
    snes = val;
    snes_color col = ConvertSNEStoRGB(val);
    rgb = ImVec4(col.red, col.green, col.blue, 0.f);
    modified = true;
  }

  bool isModified() const { return modified; }
  bool isTransparent() const { return transparent; }
  void setTransparent(bool t) { transparent = t; }
  void setModified(bool m) { modified = m; }

 private:
  ImVec4 rgb;
  uint16_t snes;
  snes_color rom_color;
  bool modified = false;
  bool transparent = false;
};

SNESColor GetCgxColor(uint16_t color);

std::vector<SNESColor> GetColFileData(uchar* data);

class SNESPalette {
 public:
  template <typename T>
  explicit SNESPalette(const std::vector<T>& data) {
    for (const auto& item : data) {
      colors.push_back(SNESColor(item));
    }
  }

  SNESPalette() = default;
  explicit SNESPalette(uint8_t mSize);
  explicit SNESPalette(char* snesPal);
  explicit SNESPalette(const unsigned char* snes_pal);
  explicit SNESPalette(const std::vector<ImVec4>&);
  explicit SNESPalette(const std::vector<snes_color>&);
  explicit SNESPalette(const std::vector<SNESColor>&);

  char* encode();
  SDL_Palette* GetSDL_Palette();

  void Create(const std::vector<SNESColor>& cols) {
    for (const auto each : cols) {
      colors.push_back(each);
    }
    size_ = cols.size();
  }

  void AddColor(SNESColor color) {
    colors.push_back(color);
    size_++;
  }

  void AddColor(snes_color color) {
    colors.emplace_back(color);
    size_++;
  }

  auto GetColor(int i) const {
    if (i > size_) {
      throw std::out_of_range("SNESPalette: Index out of bounds");
    }
    return colors[i];
  }

  void Clear() {
    colors.clear();
    size_ = 0;
  }

  auto size() const { return colors.size(); }

  SNESColor operator[](int i) {
    if (i > size_) {
      throw std::out_of_range("SNESPalette: Index out of bounds");
    }
    return colors[i];
  }

  void operator()(int i, const SNESColor& color) {
    if (i >= size_) {
      throw std::out_of_range("SNESPalette: Index out of bounds");
    }
    colors[i] = color;
  }

  void operator()(int i, const ImVec4& color) {
    if (i >= size_) {
      throw std::out_of_range("SNESPalette: Index out of bounds");
    }
    colors[i].SetRGB(color);
    colors[i].setModified(true);
  }

 private:
  int size_ = 0;
  std::vector<SNESColor> colors;
};

std::array<float, 4> ToFloatArray(const SNESColor& color);

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

  auto size() const { return palettes.size(); }

  SNESPalette operator[](int i) {
    if (i > size_) {
      std::cout << "PaletteGroup: Index out of bounds" << std::endl;
      return palettes[0];
    }
    return palettes[i];
  }

 private:
  int size_ = 0;
  std::vector<SNESPalette> palettes;
};

PaletteGroup CreatePaletteGroupFromColFile(std::vector<SNESColor>& colors);

}  // namespace gfx
}  // namespace app
}  // namespace yaze

#endif  // YAZE_APP_GFX_PALETTE_H