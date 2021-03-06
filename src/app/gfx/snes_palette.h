#ifndef YAZE_APP_GFX_PALETTE_H
#define YAZE_APP_GFX_PALETTE_H

#include <SDL2/SDL.h>
#include <imgui/imgui.h>
#include <palette.h>

#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <memory>
#include <vector>

namespace yaze {
namespace app {
namespace gfx {

struct SNESColor {
  SNESColor();
  explicit SNESColor(ImVec4);
  uint16_t snes = 0;
  ImVec4 rgb;
  void setRgb(ImVec4);
  void setSNES(uint16_t);
  uint8_t approxSNES();
  ImVec4 approxRGB();
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