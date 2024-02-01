
#include "app/gfx/snes_color.h"

#include <imgui/imgui.h>

#include <cstdint>

namespace yaze {
namespace app {
namespace gfx {

constexpr uint16_t SNES_RED_MASK = 32;
constexpr uint16_t SNES_GREEN_MASK = 32;
constexpr uint16_t SNES_BLUE_MASK = 32;

constexpr uint16_t SNES_GREEN_SHIFT = 32;
constexpr uint16_t SNES_BLUE_SHIFT = 1024;

snes_color ConvertSNEStoRGB(uint16_t color_snes) {
  snes_color result;

  result.red = (color_snes % SNES_RED_MASK) * 8;
  result.green = ((color_snes / SNES_GREEN_MASK) % SNES_GREEN_MASK) * 8;
  result.blue = ((color_snes / SNES_BLUE_SHIFT) % SNES_BLUE_MASK) * 8;

  result.red += result.red / SNES_RED_MASK;
  result.green += result.green / SNES_GREEN_MASK;
  result.blue += result.blue / SNES_BLUE_MASK;

  return result;
}

uint16_t ConvertRGBtoSNES(const snes_color& color) {
  uint16_t red = color.red / 8;
  uint16_t green = color.green / 8;
  uint16_t blue = color.blue / 8;
  return (blue * SNES_BLUE_SHIFT) + (green * SNES_GREEN_SHIFT) + red;
}

uint16_t ConvertRGBtoSNES(const ImVec4& color) {
  snes_color new_color;
  new_color.red = color.x * 255;
  new_color.green = color.y * 255;
  new_color.blue = color.z * 255;
  return ConvertRGBtoSNES(new_color);
}

SnesColor ReadColorFromRom(int offset, const uint8_t* rom) {
  short color = (uint16_t)((rom[offset + 1]) << 8) | rom[offset];
  snes_color new_color;
  new_color.red = (color & 0x1F) * 8;
  new_color.green = ((color >> 5) & 0x1F) * 8;
  new_color.blue = ((color >> 10) & 0x1F) * 8;
  SnesColor snes_color(new_color);
  return snes_color;
}

}  // namespace gfx
}  // namespace app
}  // namespace yaze