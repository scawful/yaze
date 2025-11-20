
#include "app/gfx/types/snes_color.h"

#include <cstdint>
#include <vector>

#include "imgui/imgui.h"

namespace yaze {
namespace gfx {

constexpr uint16_t SNES_RED_MASK = 32;
constexpr uint16_t SNES_GREEN_MASK = 32;
constexpr uint16_t SNES_BLUE_MASK = 32;

constexpr uint16_t SNES_GREEN_SHIFT = 32;
constexpr uint16_t SNES_BLUE_SHIFT = 1024;

snes_color ConvertSnesToRgb(uint16_t color_snes) {
  snes_color result;

  result.red = (color_snes % SNES_RED_MASK) * 8;
  result.green = ((color_snes / SNES_GREEN_MASK) % SNES_GREEN_MASK) * 8;
  result.blue = ((color_snes / SNES_BLUE_SHIFT) % SNES_BLUE_MASK) * 8;

  result.red += result.red / SNES_RED_MASK;
  result.green += result.green / SNES_GREEN_MASK;
  result.blue += result.blue / SNES_BLUE_MASK;

  return result;
}

uint16_t ConvertRgbToSnes(const snes_color& color) {
  uint16_t red = color.red / 8;
  uint16_t green = color.green / 8;
  uint16_t blue = color.blue / 8;
  return (blue * SNES_BLUE_SHIFT) + (green * SNES_GREEN_SHIFT) + red;
}

uint16_t ConvertRgbToSnes(const ImVec4& color) {
  snes_color new_color;
  new_color.red = color.x * kColorByteMax;
  new_color.green = color.y * kColorByteMax;
  new_color.blue = color.z * kColorByteMax;
  return ConvertRgbToSnes(new_color);
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

std::vector<snes_color> Extract(const char* data, unsigned int offset,
                                unsigned int palette_size) {
  std::vector<snes_color> palette(palette_size);
  for (unsigned int i = 0; i < palette_size * 2; i += 2) {
    uint16_t snes_color = (static_cast<uint8_t>(data[offset + i + 1]) << 8) |
                          static_cast<uint8_t>(data[offset + i]);
    palette[i / 2] = ConvertSnesToRgb(snes_color);
  }
  return palette;
}

std::vector<char> Convert(const std::vector<snes_color>& palette) {
  std::vector<char> data(palette.size() * 2);
  for (unsigned int i = 0; i < palette.size(); i++) {
    uint16_t snes_data = ConvertRgbToSnes(palette[i]);
    data[i * 2] = snes_data & 0xFF;
    data[i * 2 + 1] = snes_data >> 8;
  }
  return data;
}

SnesColor GetCgxColor(uint16_t color) {
  ImVec4 rgb;
  rgb.x = (color & 0x1F) * 8;
  rgb.y = ((color & 0x3E0) >> 5) * 8;
  rgb.z = ((color & 0x7C00) >> 10) * 8;
  SnesColor toret;
  toret.set_rgb(rgb);
  return toret;
}

std::vector<SnesColor> GetColFileData(uint8_t* data) {
  std::vector<SnesColor> colors;
  colors.reserve(256);
  colors.resize(256);

  for (int i = 0; i < 512; i += 2) {
    colors[i / 2] = GetCgxColor((uint16_t)((data[i + 1] << 8) + data[i]));
  }

  return colors;
}

void SnesColor::set_rgb(const ImVec4 val) {
  // IMPORTANT: Input val is expected to be in standard ImVec4 range (0.0-1.0)
  // We convert to internal 0-255 storage format
  rgb_.x = val.x * kColorByteMax;
  rgb_.y = val.y * kColorByteMax;
  rgb_.z = val.z * kColorByteMax;
  rgb_.w = kColorByteMaxF;  // Alpha always 255

  // Update rom_color_ and snes_ representations
  snes_color color;
  color.red = static_cast<uint16_t>(rgb_.x);
  color.green = static_cast<uint16_t>(rgb_.y);
  color.blue = static_cast<uint16_t>(rgb_.z);

  rom_color_ = color;
  snes_ = ConvertRgbToSnes(color);
  modified = true;
}

void SnesColor::set_snes(uint16_t val) {
  // Store SNES 15-bit color
  snes_ = val;

  // Convert SNES to RGB (0-255)
  snes_color col = ConvertSnesToRgb(val);

  // Store 0-255 values in ImVec4 (unconventional but our internal format)
  rgb_ = ImVec4(static_cast<float>(col.red), static_cast<float>(col.green),
                static_cast<float>(col.blue), kColorByteMaxF);

  rom_color_ = col;
  modified = true;
}

}  // namespace gfx
}  // namespace yaze
