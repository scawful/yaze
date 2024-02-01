#ifndef YAZE_APP_GFX_SNES_COLOR_H_
#define YAZE_APP_GFX_SNES_COLOR_H_

#include <imgui/imgui.h>

#include <cstdint>
#include <vector>

namespace yaze {
namespace app {
namespace gfx {

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

  explicit SnesColor(const snes_color val)
      : rgb_(val.red, val.green, val.blue, 255.f),
        snes_(ConvertRGBtoSNES(val)),
        rom_color_(val) {}

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

  snes_color rom_color() const { return rom_color_; }

  uint16_t snes() const { return snes_; }
  void set_snes(uint16_t val) {
    snes_ = val;
    snes_color col = ConvertSNEStoRGB(val);
    rgb_ = ImVec4(col.red, col.green, col.blue, 0.f);
    modified = true;
  }

  bool IsModified() const { return modified; }
  bool IsTransparent() const { return transparent; }
  void SetTransparent(bool t) { transparent = t; }
  void SetModified(bool m) { modified = m; }

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
