#ifndef YAZE_APP_GFX_SNES_COLOR_H_
#define YAZE_APP_GFX_SNES_COLOR_H_

#include <yaze.h>

#include <cstdint>
#include <vector>

#include "imgui/imgui.h"

namespace yaze {
namespace gfx {

constexpr int NumberOfColors = 3143;

// ============================================================================
// SNES Color Conversion Functions
// ============================================================================
//
// Color Format Guide:
// - SNES Color (uint16_t): 15-bit BGR format (0bbbbbgggggrrrrr)
// - snes_color struct: RGB values in 0-255 range
// - ImVec4: RGBA values in 0.0-1.0 range (standard for ImGui)
// - SDL_Color: RGBA values in 0-255 range
//
// Conversion paths:
// 1. SNES (uint16_t) <-> snes_color (0-255) <-> ImVec4 (0.0-1.0)
// 2. Use these functions to convert between formats explicitly
// ============================================================================

/**
 * @brief Convert SNES 15-bit color to RGB (0-255 range)
 * @param snes_color SNES color in 15-bit BGR format (0bbbbbgggggrrrrr)
 * @return snes_color struct with RGB values in 0-255 range
 */
snes_color ConvertSnesToRgb(uint16_t snes_color);

/**
 * @brief Convert RGB (0-255) to SNES 15-bit color
 * @param color snes_color struct with RGB values in 0-255 range
 * @return SNES color in 15-bit BGR format
 */
uint16_t ConvertRgbToSnes(const snes_color& color);

/**
 * @brief Convert ImVec4 (0.0-1.0) to SNES 15-bit color
 * @param color ImVec4 with RGB values in 0.0-1.0 range
 * @return SNES color in 15-bit BGR format
 */
uint16_t ConvertRgbToSnes(const ImVec4& color);

/**
 * @brief Convert snes_color (0-255) to ImVec4 (0.0-1.0)
 * @param color snes_color struct with RGB values in 0-255 range
 * @return ImVec4 with RGBA values in 0.0-1.0 range
 */
inline ImVec4 SnesColorToImVec4(const snes_color& color) {
  return ImVec4(color.red / 255.0f, color.green / 255.0f, color.blue / 255.0f,
                1.0f);
}

/**
 * @brief Convert ImVec4 (0.0-1.0) to snes_color (0-255)
 * @param color ImVec4 with RGB values in 0.0-1.0 range
 * @return snes_color struct with RGB values in 0-255 range
 */
inline snes_color ImVec4ToSnesColor(const ImVec4& color) {
  snes_color result;
  result.red = static_cast<uint16_t>(color.x * 255.0f);
  result.green = static_cast<uint16_t>(color.y * 255.0f);
  result.blue = static_cast<uint16_t>(color.z * 255.0f);
  return result;
}

/**
 * @brief Convert SNES 15-bit color directly to ImVec4 (0.0-1.0)
 * @param color_value SNES color in 15-bit BGR format
 * @return ImVec4 with RGBA values in 0.0-1.0 range
 */
inline ImVec4 SnesTo8bppColor(uint16_t color_value) {
  snes_color rgb = ConvertSnesToRgb(color_value);
  return SnesColorToImVec4(rgb);
}

std::vector<snes_color> Extract(const char* data, unsigned int offset,
                                unsigned int palette_size);

std::vector<char> Convert(const std::vector<snes_color>& palette);

constexpr uint8_t kColorByteMax = 255;
constexpr float kColorByteMaxF = 255.f;

/**
 * @brief SNES Color container
 *
 * Manages SNES colors in multiple formats for editing and display.
 *
 * IMPORTANT: Internal storage format
 * - rgb_: ImVec4 storing RGB values in 0-255 range (NOT standard 0-1!)
 *   This is unconventional but done for performance reasons
 * - snes_: SNES 15-bit BGR format (0bbbbbgggggrrrrr)
 * - rom_color_: snes_color struct with 0-255 RGB values
 *
 * When getting RGB for display:
 * - Use rgb() to get raw values (0-255 in ImVec4 - unusual!)
 * - Convert to standard ImVec4 (0-1) using: ImVec4(rgb.x/255, rgb.y/255,
 * rgb.z/255, 1.0)
 * - Or use the helper: ConvertSnesColorToImVec4() in color.cc
 */
class SnesColor {
 public:
  constexpr SnesColor()
      : rgb_({0.f, 0.f, 0.f, 255.f}), snes_(0), rom_color_({0, 0, 0}) {}

  /**
   * @brief Construct from ImVec4 (0.0-1.0 range)
   * @param val ImVec4 with RGB in standard 0.0-1.0 range
   */
  explicit SnesColor(const ImVec4 val) {
    // Convert from ImGui's 0-1 range to internal 0-255 storage
    rgb_.x = val.x * kColorByteMax;
    rgb_.y = val.y * kColorByteMax;
    rgb_.z = val.z * kColorByteMax;
    rgb_.w = kColorByteMaxF;  // Alpha always 255

    snes_color color;
    color.red = static_cast<uint16_t>(rgb_.x);
    color.green = static_cast<uint16_t>(rgb_.y);
    color.blue = static_cast<uint16_t>(rgb_.z);
    rom_color_ = color;
    snes_ = ConvertRgbToSnes(color);
  }

  /**
   * @brief Construct from SNES 15-bit color
   * @param val SNES color in 15-bit BGR format
   */
  explicit SnesColor(const uint16_t val) : snes_(val) {
    snes_color color = ConvertSnesToRgb(val);  // Returns 0-255 RGB
    // Store 0-255 values in ImVec4 (unconventional but internal)
    rgb_ = ImVec4(color.red, color.green, color.blue, kColorByteMaxF);
    rom_color_ = color;
  }

  /**
   * @brief Construct from snes_color struct (0-255 range)
   * @param val snes_color with RGB in 0-255 range
   */
  explicit SnesColor(const snes_color val)
      : rgb_(val.red, val.green, val.blue, kColorByteMaxF),
        snes_(ConvertRgbToSnes(val)),
        rom_color_(val) {}

  /**
   * @brief Construct from RGB byte values (0-255)
   */
  SnesColor(uint8_t r, uint8_t g, uint8_t b) {
    rgb_ = ImVec4(r, g, b, kColorByteMaxF);
    snes_color color;
    color.red = r;
    color.green = g;
    color.blue = b;
    snes_ = ConvertRgbToSnes(color);
    rom_color_ = color;
  }

  /**
   * @brief Set color from ImVec4 (0.0-1.0 range)
   * @param val ImVec4 with RGB in standard 0.0-1.0 range
   */
  void set_rgb(const ImVec4 val);

  /**
   * @brief Set color from SNES 15-bit format
   * @param val SNES color in 15-bit BGR format
   */
  void set_snes(uint16_t val);

  /**
   * @brief Get RGB values (WARNING: stored as 0-255 in ImVec4)
   * @return ImVec4 with RGB in 0-255 range (unconventional!)
   */
  constexpr ImVec4 rgb() const { return rgb_; }

  /**
   * @brief Get snes_color struct (0-255 RGB)
   */
  constexpr snes_color rom_color() const { return rom_color_; }

  /**
   * @brief Get SNES 15-bit color
   */
  constexpr uint16_t snes() const { return snes_; }

  constexpr bool is_modified() const { return modified; }
  constexpr bool is_transparent() const { return transparent; }
  constexpr void set_transparent(bool t) { transparent = t; }
  constexpr void set_modified(bool m) { modified = m; }

 private:
  ImVec4 rgb_;            // Stores 0-255 values (unconventional!)
  uint16_t snes_;         // 15-bit SNES format
  snes_color rom_color_;  // 0-255 RGB struct
  bool modified = false;
  bool transparent = false;
};

SnesColor ReadColorFromRom(int offset, const uint8_t* rom);

SnesColor GetCgxColor(uint16_t color);
std::vector<SnesColor> GetColFileData(uint8_t* data);

}  // namespace gfx
}  // namespace yaze

#endif  // YAZE_APP_GFX_SNES_COLOR_H_
