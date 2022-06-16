#ifndef YAZE_APPLICATION_GRAPHICS_PALETTE_H
#define YAZE_APPLICATION_GRAPHICS_PALETTE_H

#include <SDL2/SDL.h>
#include <imgui/imgui.h>
#include <palette.h>
#include <tile.h>

#include <cstdint>
#include <iostream>
#include <memory>
#include <vector>

namespace yaze {
namespace application {
namespace Graphics {

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
  explicit SNESPalette(std::vector<ImVec4>);

  char* encode();
  SDL_Palette* GetSDL_Palette();

  int size_ = 0;
  std::vector<SNESColor> colors;
  std::vector<std::shared_ptr<SDL_Palette>> sdl_palettes_;
  std::vector<SDL_Color*> colors_arrays_;
};

}  // namespace Graphics
}  // namespace application
}  // namespace yaze

#endif  // YAZE_APPLICATION_GRAPHICS_PALETTE_H