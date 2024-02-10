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
#include "absl/status/statusor.h"
#include "app/core/constants.h"
#include "app/gfx/snes_color.h"

namespace yaze {
namespace app {
namespace gfx {

struct snes_palette {
  uint id;            /**< ID of the palette. */
  uint size;          /**< Size of the palette. */
  snes_color* colors; /**< Pointer to the colors in the palette. */
};
using snes_palette = struct snes_palette;

uint32_t GetPaletteAddress(const std::string& group_name, size_t palette_index,
                           size_t color_index);

class SnesPalette {
 public:
  template <typename T>
  explicit SnesPalette(const std::vector<T>& data) {
    for (const auto& item : data) {
      colors.push_back(SnesColor(item));
    }
    size_ = data.size();
  }

  SnesPalette() = default;

  explicit SnesPalette(uint8_t mSize);
  explicit SnesPalette(char* snesPal);
  explicit SnesPalette(const unsigned char* snes_pal);
  explicit SnesPalette(const std::vector<ImVec4>&);
  explicit SnesPalette(const std::vector<snes_color>&);
  explicit SnesPalette(const std::vector<SnesColor>&);

  SDL_Palette* GetSDL_Palette();

  void Create(const std::vector<SnesColor>& cols) {
    for (const auto& each : cols) {
      colors.push_back(each);
    }
    size_ = cols.size();
  }

  void AddColor(SnesColor color) {
    colors.push_back(color);
    size_++;
  }

  void AddColor(snes_color color) {
    colors.emplace_back(color);
    size_++;
  }

  auto GetColor(int i) const {
    if (i > size_) {
      std::cout << "SNESPalette: Index out of bounds" << std::endl;
      return colors[0];
    }
    return colors[i];
  }

  auto mutable_color(int i) { return &colors[i]; }

  void Clear() {
    colors.clear();
    size_ = 0;
  }

  auto size() const { return colors.size(); }

  SnesColor& operator[](int i) {
    if (i > size_) {
      std::cout << "SNESPalette: Index out of bounds" << std::endl;
      return colors[0];
    }
    return colors[i];
  }

  void operator()(int i, const SnesColor& color) {
    if (i >= size_) {
      throw std::out_of_range("SNESPalette: Index out of bounds");
    }
    colors[i] = color;
  }

  void operator()(int i, const ImVec4& color) {
    if (i >= size_) {
      throw std::out_of_range("SNESPalette: Index out of bounds");
    }
    colors[i].set_rgb(color);
    colors[i].set_modified(true);
  }

  SnesPalette sub_palette(int start, int end) const {
    SnesPalette pal;
    for (int i = start; i < end; i++) {
      pal.AddColor(colors[i]);
    }
    return pal;
  }

 private:
  int size_ = 0;                 /**< The size of the palette. */
  std::vector<SnesColor> colors; /**< The colors in the palette. */
};

SnesPalette ReadPaletteFromRom(int offset, int num_colors, const uint8_t* rom);

std::array<float, 4> ToFloatArray(const SnesColor& color);

struct PaletteGroup {
  PaletteGroup() = default;

  explicit PaletteGroup(uint8_t mSize);

  auto mutable_palette(int i) { return &palettes[i]; }

  absl::Status AddPalette(SnesPalette pal) {
    palettes.emplace_back(pal);
    size_ = palettes.size();
    return absl::OkStatus();
  }

  absl::Status AddColor(SnesColor color) {
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

  SnesPalette operator[](int i) {
    if (i > size_) {
      std::cout << "PaletteGroup: Index out of bounds" << std::endl;
      return palettes[0];
    }
    return palettes[i];
  }

  const SnesPalette& operator[](int i) const {
    if (i > size_) {
      std::cout << "PaletteGroup: Index out of bounds" << std::endl;
      return palettes[0];
    }
    return palettes[i];
  }

  absl::Status operator()(int i, const SnesColor& color) {
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
  std::vector<SnesPalette> palettes;
};

absl::StatusOr<PaletteGroup> CreatePaletteGroupFromColFile(
    std::vector<SnesColor>& colors);

absl::StatusOr<PaletteGroup> CreatePaletteGroupFromLargePalette(
    SnesPalette& palette);

struct Paletteset {
  Paletteset() = default;
  Paletteset(gfx::SnesPalette main, gfx::SnesPalette animated,
             gfx::SnesPalette aux1, gfx::SnesPalette aux2,
             gfx::SnesColor background, gfx::SnesPalette hud,
             gfx::SnesPalette spr, gfx::SnesPalette spr2, gfx::SnesPalette comp)
      : main(main),
        animated(animated),
        aux1(aux1),
        aux2(aux2),
        background(background),
        hud(hud),
        spr(spr),
        spr2(spr2),
        composite(comp) {}
  gfx::SnesPalette main;
  gfx::SnesPalette animated;
  gfx::SnesPalette aux1;
  gfx::SnesPalette aux2;
  gfx::SnesColor background;
  gfx::SnesPalette hud;
  gfx::SnesPalette spr;
  gfx::SnesPalette spr2;
  gfx::SnesPalette composite;
};

}  // namespace gfx
}  // namespace app
}  // namespace yaze

#endif  // YAZE_APP_GFX_PALETTE_H