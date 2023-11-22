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
#include "absl/status/status.h"
#include "app/core/constants.h"

namespace yaze {
namespace app {
namespace gfx {

struct snes_color {
  uint16_t red;   /**< Red component of the color. */
  uint16_t blue;  /**< Blue component of the color. */
  uint16_t green; /**< Green component of the color. */
};
using snes_color = struct snes_color;

struct snes_palette {
  uint id;            /**< ID of the palette. */
  uint size;          /**< Size of the palette. */
  snes_color* colors; /**< Pointer to the colors in the palette. */
};
using snes_palette = struct snes_palette;

uint16_t ConvertRGBtoSNES(const snes_color& color);
snes_color ConvertSNEStoRGB(uint16_t snes_color);

/**
 * @brief Extracts a vector of SNES colors from a data buffer.
 *
 * @param data The data buffer to extract from.
 * @param offset The offset in the buffer to start extracting from.
 * @param palette_size The size of the palette to extract.
 * @return A vector of SNES colors extracted from the buffer.
 */
std::vector<snes_color> Extract(const char* data, unsigned int offset,
                                unsigned int palette_size);

/**
 * @brief Converts a vector of SNES colors to a vector of characters.
 *
 * @param palette The vector of SNES colors to convert.
 * @return A vector of characters representing the converted SNES colors.
 */
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

  snes_color GetRomRGB() const { return rom_color; }

  uint16_t GetSNES() const { return snes; }
  void SetSNES(uint16_t val) {
    snes = val;
    snes_color col = ConvertSNEStoRGB(val);
    rgb = ImVec4(col.red, col.green, col.blue, 0.f);
    modified = true;
  }

  bool IsModified() const { return modified; }
  bool IsTransparent() const { return transparent; }
  void SetTransparent(bool t) { transparent = t; }
  void SetModified(bool m) { modified = m; }

 private:
  ImVec4 rgb;
  uint16_t snes;
  snes_color rom_color;
  bool modified = false;
  bool transparent = false;
};

gfx::SNESColor ReadColorFromROM(int offset, const uchar* rom);

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

  SDL_Palette* GetSDL_Palette();

  void Create(const std::vector<SNESColor>& cols) {
    for (const auto& each : cols) {
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

  SNESColor& operator[](int i) {
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
    colors[i].SetModified(true);
  }

 private:
  int size_ = 0;                 /**< The size of the palette. */
  std::vector<SNESColor> colors; /**< The colors in the palette. */
};

SNESPalette ReadPaletteFromROM(int offset, int num_colors, const uchar* rom);
uint32_t GetPaletteAddress(const std::string& group_name, size_t palette_index,
                           size_t color_index);
std::array<float, 4> ToFloatArray(const SNESColor& color);

struct PaletteGroup {
  PaletteGroup() = default;

  explicit PaletteGroup(uint8_t mSize);

  absl::Status AddPalette(SNESPalette pal) {
    palettes.emplace_back(pal);
    size_ = palettes.size();
    return absl::OkStatus();
  }

  absl::Status AddColor(SNESColor color) {
    if (size_ == 0) {
      palettes.emplace_back();
    }
    palettes[0].AddColor(color);
    return absl::OkStatus();
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

  const SNESPalette& operator[](int i) const {
    if (i > size_) {
      std::cout << "PaletteGroup: Index out of bounds" << std::endl;
      return palettes[0];
    }
    return palettes[i];
  }

  absl::Status operator()(int i, const SNESColor& color) {
    if (i >= size_) {
      return absl::InvalidArgumentError("PaletteGroup: Index out of bounds");
    }
    palettes[i](0, color);
    return absl::OkStatus();
  }

  absl::Status operator()(int i, const ImVec4& color) {
    if (i >= size_) {
      return absl::InvalidArgumentError("PaletteGroup: Index out of bounds");
    }
    palettes[i](0, color);
    return absl::OkStatus();
  }

 private:
  int size_ = 0;
  std::vector<SNESPalette> palettes;
};

PaletteGroup CreatePaletteGroupFromColFile(std::vector<SNESColor>& colors);

PaletteGroup CreatePaletteGroupFromLargePalette(SNESPalette& palette);

}  // namespace gfx
}  // namespace app
}  // namespace yaze

#endif  // YAZE_APP_GFX_PALETTE_H