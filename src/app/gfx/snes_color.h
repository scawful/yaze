#ifndef YAZE_APP_GFX_SNES_COLOR_H_
#define YAZE_APP_GFX_SNES_COLOR_H_

#include <imgui/imgui.h>

#include <cstdint>

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

// class SnesColor {
//  public:
//   SnesColor() : rgb(0.f, 0.f, 0.f, 0.f), snes(0) {}
//   SnesColor(const ImVec4 val) : rgb(val) {
//     snes_color color;
//     color.red = val.x / 255;
//     color.green = val.y / 255;
//     color.blue = val.z / 255;
//     snes = ConvertRGBtoSNES(color);
//   }
//   SnesColor(const snes_color internal_format) {
//     rgb.x = internal_format.red;
//     rgb.y = internal_format.green;
//     rgb.z = internal_format.blue;
//     snes = ConvertRGBtoSNES(internal_format);
//   }

//   ImVec4 rgb;    /**< The color in RGB format. */
//   uint16_t snes; /**< The color in SNES format. */
//   bool modified = false;
//   bool transparent = false;
// };

class SnesColor {
 public:
  SnesColor() : rgb(0.f, 0.f, 0.f, 0.f), snes(0) {}

  explicit SnesColor(const ImVec4 val) : rgb(val) {
    snes_color color;
    color.red = val.x / 255;
    color.green = val.y / 255;
    color.blue = val.z / 255;
    snes = ConvertRGBtoSNES(color);
  }

  explicit SnesColor(const snes_color val)
      : rgb(val.red, val.green, val.blue, 255.f),
        snes(ConvertRGBtoSNES(val)),
        rom_color(val) {}

  ImVec4 GetRGB() const { return rgb; }
  void SetRGB(const ImVec4 val) {
    rgb.x = val.x / 255;
    rgb.y = val.y / 255;
    rgb.z = val.z / 255;
    snes_color color;
    color.red = val.x;
    color.green = val.y;
    color.blue = val.z;
    rom_color = color;
    snes = ConvertRGBtoSNES(color);
    modified = true;
  }

  snes_color GetRomRGB() const { return rom_color; }

  uint16_t GetSNES() const { return snes; }
  void SetSNES(uint16_t val) {
    snes = val;
    snes_color col = ConvertSNEStoRGB(val);
    rgb = ImVec4(col.red, col.green, col.blue, 0.f);
    modified = true;
  }

  bool IsModified() const { return modified; }
  bool IsTransparent() const { return transparent; }
  void SetTransparent(bool t) { transparent = t; }
  void SetModified(bool m) { modified = m; }

 private:
  ImVec4 rgb;
  uint16_t snes;
  snes_color rom_color;
  bool modified = false;
  bool transparent = false;
};

SnesColor ReadColorFromRom(int offset, const uint8_t* rom);

}  // namespace gfx
}  // namespace app
}  // namespace yaze

#endif  // YAZE_APP_GFX_SNES_COLOR_H_
