#ifndef YAZE_APP_GFX_SNES_COLOR_H_
#define YAZE_APP_GFX_SNES_COLOR_H_

#include <yaze.h>

#include <cstdint>
#include <vector>

#include "imgui/imgui.h"

namespace yaze {
namespace gfx {

constexpr int NumberOfColors = 3143;

snes_color ConvertSnesToRgb(uint16_t snes_color);
uint16_t ConvertRgbToSnes(const snes_color& color);
uint16_t ConvertRgbToSnes(const ImVec4& color);

std::vector<snes_color> Extract(const char* data, unsigned int offset,
                                unsigned int palette_size);

std::vector<char> Convert(const std::vector<snes_color>& palette);

constexpr uint8_t kColorByteMax = 255;
constexpr float kColorByteMaxF = 255.f;

/**
 * @brief SNES Color container
 *
 * Used for displaying the color to the screen and writing
 * the color to the Rom file in the correct format.
 *
 * SNES colors may be represented in one of three formats:
 *  - Color data from the rom in a snes_color struct
 *  - Color data for displaying to the UI via ImVec4
 */
class SnesColor {
 public:
  constexpr SnesColor()
      : rgb_({0.f, 0.f, 0.f, 0.f}), snes_(0), rom_color_({0, 0, 0}) {}

  explicit SnesColor(const ImVec4 val) : rgb_(val) {
    snes_color color;
    color.red = static_cast<uint16_t>(val.x * kColorByteMax);
    color.green = static_cast<uint16_t>(val.y * kColorByteMax);
    color.blue = static_cast<uint16_t>(val.z * kColorByteMax);
    snes_ = ConvertRgbToSnes(color);
  }

  explicit SnesColor(const uint16_t val) : snes_(val) {
    snes_color color = ConvertSnesToRgb(val);
    rgb_ = ImVec4(color.red, color.green, color.blue, 0.f);
  }

  explicit SnesColor(const snes_color val)
      : rgb_(val.red, val.green, val.blue, kColorByteMaxF),
        snes_(ConvertRgbToSnes(val)),
        rom_color_(val) {}

  SnesColor(uint8_t r, uint8_t g, uint8_t b) {
    rgb_ = ImVec4(r, g, b, kColorByteMaxF);
    snes_color color;
    color.red = r;
    color.green = g;
    color.blue = b;
    snes_ = ConvertRgbToSnes(color);
    rom_color_ = color;
  }

  void set_rgb(const ImVec4 val);
  void set_snes(uint16_t val);

  constexpr ImVec4 rgb() const { return rgb_; }
  constexpr snes_color rom_color() const { return rom_color_; }
  constexpr uint16_t snes() const { return snes_; }
  constexpr bool is_modified() const { return modified; }
  constexpr bool is_transparent() const { return transparent; }
  constexpr void set_transparent(bool t) { transparent = t; }
  constexpr void set_modified(bool m) { modified = m; }

 private:
  ImVec4 rgb_;
  uint16_t snes_;
  snes_color rom_color_;
  bool modified = false;
  bool transparent = false;
};

SnesColor ReadColorFromRom(int offset, const uint8_t* rom);

SnesColor GetCgxColor(uint16_t color);
std::vector<SnesColor> GetColFileData(uint8_t* data);

}  // namespace gfx
}  // namespace yaze

#endif  // YAZE_APP_GFX_SNES_COLOR_H_
