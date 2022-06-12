#ifndef YAZE_APPLICATION_GRAPHICS_PALETTE_H
#define YAZE_APPLICATION_GRAPHICS_PALETTE_H

#include <imgui/imgui.h>
#include <palette.h>
#include <tile.h>

#include <cstdint>
#include <iostream>
#include <vector>

namespace yaze {
namespace Application {
namespace Graphics {

struct SNESColor {
  SNESColor();
  uint16_t snes;
  ImVec4 rgb;
  void setRgb(ImVec4);
  void setSNES(uint16_t);
  uint8_t approxSNES();
  ImVec4 approxRGB();
};

class SNESPalette {
 public:
  SNESPalette();
  SNESPalette(uint8_t mSize);
  SNESPalette(char* snesPal);
  SNESPalette(std::vector<ImVec4>);

  char* encode();
  uint8_t size;
  std::vector<SNESColor> colors;
};

}  // namespace Graphics
}  // namespace Application
}  // namespace yaze

#endif  // YAZE_APPLICATION_GRAPHICS_PALETTE_H