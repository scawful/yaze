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
  uint16_t snes = 0;
  ImVec4 rgb;
  void setRgb(ImVec4);
  void setRgb(snes_color);
  void setSNES(uint16_t);
};

class SNESPalette {
 public:
  SNESPalette() = default;
  explicit SNESPalette(uint8_t mSize);
  explicit SNESPalette(char* snesPal);
  explicit SNESPalette(const unsigned char* snes_pal);
  explicit SNESPalette(const std::vector<ImVec4>&);
  explicit SNESPalette(const std::vector<snes_color>&);

  char* encode();
  SDL_Palette* GetSDL_Palette();

  int size_ = 0;
  std::vector<SNESColor> colors;
};

}  // namespace gfx
}  // namespace app
}  // namespace yaze

#endif  // YAZE_APP_GFX_PALETTE_H