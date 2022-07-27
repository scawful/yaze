#ifndef YAZE_APP_GFX_PALETTE_H
#define YAZE_APP_GFX_PALETTE_H

#include <SDL2/SDL.h>
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

typedef struct {
  uchar red;
  uchar blue;
  uchar green;
} snes_color;

typedef struct {
  uint id;
  uint size;
  snes_color* colors;
} snes_palette;

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
  void setSNES(uint16_t);
};

class SNESPalette {
 public:
  SNESPalette() = default;
  explicit SNESPalette(uint8_t mSize);
  explicit SNESPalette(char* snesPal);
  explicit SNESPalette(const unsigned char* snes_pal);
  explicit SNESPalette(const std::vector<ImVec4>&);

  char* encode();
  SDL_Palette* GetSDL_Palette();

  int size_ = 0;
  std::vector<SNESColor> colors;
  std::vector<SDL_Color*> colors_arrays_;
  std::vector<std::vector<SDL_Color>> colors_;
};

}  // namespace gfx
}  // namespace app
}  // namespace yaze

#endif  // YAZE_APP_GFX_PALETTE_H