#ifndef YAZE_APP_GFX_SNES_COLOR_H_
#define YAZE_APP_GFX_SNES_COLOR_H_

#include <imgui/imgui.h>

#include <cstdint>
#include <vector>

namespace yaze {
namespace app {
namespace gfx {

/**
 * @brief Primitive of 16-bit RGB SNES color.
 */
struct snes_color {
  uint16_t red;   /**< Red component of the color. */
  uint16_t blue;  /**< Blue component of the color. */
  uint16_t green; /**< Green component of the color. */
};
typedef struct snes_color snes_color;

snes_color ConvertSNEStoRGB(uint16_t snes_color);
uint16_t ConvertRGBtoSNES(const snes_color& color);
uint16_t ConvertRGBtoSNES(const ImVec4& color);

std::vector<snes_color> Extract(const char* data, unsigned int offset,
                                unsigned int palette_size);

std::vector<char> Convert(const std::vector<snes_color>& palette);

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
  SnesColor() : rgb_(0.f, 0.f, 0.f, 0.f), snes_(0) {}
  explicit SnesColor(const ImVec4 val) : rgb_(val) {
    snes_color color;
    color.red = val.x / 255;
    color.green = val.y / 255;
    color.blue = val.z / 255;
    snes_ = ConvertRGBtoSNES(color);
  }
  explicit SnesColor(const uint16_t val) : snes_(val) {
    snes_color color = ConvertSNEStoRGB(val);
    rgb_ = ImVec4(color.red, color.green, color.blue, 0.f);
  }
  explicit SnesColor(const snes_color val)
      : rgb_(val.red, val.green, val.blue, 255.f),
        snes_(ConvertRGBtoSNES(val)),
        rom_color_(val) {}

  SnesColor(uint8_t r, uint8_t g, uint8_t b) {
    rgb_ = ImVec4(r, g, b, 255.f);
    snes_color color;
    color.red = r;
    color.green = g;
    color.blue = b;
    snes_ = ConvertRGBtoSNES(color);
    rom_color_ = color;
  }

  ImVec4 rgb() const { return rgb_; }

  void set_rgb(const ImVec4 val) {
    rgb_.x = val.x / 255;
    rgb_.y = val.y / 255;
    rgb_.z = val.z / 255;
    snes_color color;
    color.red = val.x;
    color.green = val.y;
    color.blue = val.z;
    rom_color_ = color;
    snes_ = ConvertRGBtoSNES(color);
    modified = true;
  }

  void set_snes(uint16_t val) {
    snes_ = val;
    snes_color col = ConvertSNEStoRGB(val);
    rgb_ = ImVec4(col.red, col.green, col.blue, 0.f);
    modified = true;
  }

  snes_color rom_color() const { return rom_color_; }
  uint16_t snes() const { return snes_; }
  bool is_modified() const { return modified; }
  bool is_transparent() const { return transparent; }
  void set_transparent(bool t) { transparent = t; }
  void set_modified(bool m) { modified = m; }

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
}  // namespace app
}  // namespace yaze

#endif  // YAZE_APP_GFX_SNES_COLOR_H_
