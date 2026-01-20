#ifndef YAZE_GUI_COLOR_H
#define YAZE_GUI_COLOR_H

#include <algorithm>
#include <cmath>
#include <string>

#include "absl/status/status.h"
#include "absl/strings/str_format.h"
#include "app/gfx/types/snes_palette.h"
#include "imgui/imgui.h"

namespace yaze {
namespace gui {

struct Color {
  float red = 0.0f;
  float green = 0.0f;
  float blue = 0.0f;
  float alpha = 1.0f;

  operator ImVec4() const { return ImVec4(red, green, blue, alpha); }

  // HSL conversion utilities for theme generation
  struct HSL {
    float h = 0.0f;  // 0-360
    float s = 0.0f;  // 0-1
    float l = 0.0f;  // 0-1
  };

  HSL ToHSL() const {
    float max_val = std::max({red, green, blue});
    float min_val = std::min({red, green, blue});
    float delta = max_val - min_val;

    HSL hsl;
    hsl.l = (max_val + min_val) / 2.0f;

    if (delta < 0.00001f) {
      hsl.h = 0.0f;
      hsl.s = 0.0f;
    } else {
      hsl.s = hsl.l > 0.5f ? delta / (2.0f - max_val - min_val)
                           : delta / (max_val + min_val);

      if (max_val == red) {
        hsl.h = 60.0f * fmodf((green - blue) / delta, 6.0f);
      } else if (max_val == green) {
        hsl.h = 60.0f * ((blue - red) / delta + 2.0f);
      } else {
        hsl.h = 60.0f * ((red - green) / delta + 4.0f);
      }
      if (hsl.h < 0.0f) {
        hsl.h += 360.0f;
      }
    }
    return hsl;
  }

  static Color FromHSL(float h, float s, float l, float a = 1.0f) {
    auto hue_to_rgb = [](float p, float q, float t) {
      if (t < 0.0f) t += 1.0f;
      if (t > 1.0f) t -= 1.0f;
      if (t < 1.0f / 6.0f) return p + (q - p) * 6.0f * t;
      if (t < 1.0f / 2.0f) return q;
      if (t < 2.0f / 3.0f) return p + (q - p) * (2.0f / 3.0f - t) * 6.0f;
      return p;
    };

    Color result;
    result.alpha = a;

    if (s < 0.00001f) {
      result.red = result.green = result.blue = l;
    } else {
      float h_norm = h / 360.0f;
      float q = l < 0.5f ? l * (1.0f + s) : l + s - l * s;
      float p = 2.0f * l - q;
      result.red = hue_to_rgb(p, q, h_norm + 1.0f / 3.0f);
      result.green = hue_to_rgb(p, q, h_norm);
      result.blue = hue_to_rgb(p, q, h_norm - 1.0f / 3.0f);
    }
    return result;
  }

  // Derive a new color by adjusting HSL components
  Color WithHue(float new_hue) const {
    HSL hsl = ToHSL();
    return FromHSL(new_hue, hsl.s, hsl.l, alpha);
  }

  Color WithSaturation(float new_sat) const {
    HSL hsl = ToHSL();
    return FromHSL(hsl.h, std::clamp(new_sat, 0.0f, 1.0f), hsl.l, alpha);
  }

  Color WithLightness(float new_light) const {
    HSL hsl = ToHSL();
    return FromHSL(hsl.h, hsl.s, std::clamp(new_light, 0.0f, 1.0f), alpha);
  }

  Color Lighter(float amount) const {
    HSL hsl = ToHSL();
    return FromHSL(hsl.h, hsl.s, std::clamp(hsl.l + amount, 0.0f, 1.0f), alpha);
  }

  Color Darker(float amount) const {
    HSL hsl = ToHSL();
    return FromHSL(hsl.h, hsl.s, std::clamp(hsl.l - amount, 0.0f, 1.0f), alpha);
  }

  Color Saturate(float amount) const {
    HSL hsl = ToHSL();
    return FromHSL(hsl.h, std::clamp(hsl.s + amount, 0.0f, 1.0f), hsl.l, alpha);
  }

  Color Desaturate(float amount) const {
    HSL hsl = ToHSL();
    return FromHSL(hsl.h, std::clamp(hsl.s - amount, 0.0f, 1.0f), hsl.l, alpha);
  }

  Color ShiftHue(float degrees) const {
    HSL hsl = ToHSL();
    float new_hue = fmodf(hsl.h + degrees, 360.0f);
    if (new_hue < 0.0f) new_hue += 360.0f;
    return FromHSL(new_hue, hsl.s, hsl.l, alpha);
  }

  Color WithAlpha(float new_alpha) const {
    return Color{red, green, blue, std::clamp(new_alpha, 0.0f, 1.0f)};
  }
};

inline ImVec4 ConvertColorToImVec4(const Color& color) {
  return ImVec4(color.red, color.green, color.blue, color.alpha);
}

inline std::string ColorToHexString(const Color& color) {
  return absl::StrFormat("%02X%02X%02X%02X", static_cast<int>(color.red * 255),
                         static_cast<int>(color.green * 255),
                         static_cast<int>(color.blue * 255),
                         static_cast<int>(color.alpha * 255));
}

// A utility function to convert an SnesColor object to an ImVec4 with
// normalized color values
ImVec4 ConvertSnesColorToImVec4(const gfx::SnesColor& color);

// A utility function to convert an ImVec4 to an SnesColor object
gfx::SnesColor ConvertImVec4ToSnesColor(const ImVec4& color);

// The wrapper function for ImGui::ColorButton that takes a SnesColor reference
IMGUI_API bool SnesColorButton(absl::string_view id, gfx::SnesColor& color,
                               ImGuiColorEditFlags flags = 0,
                               const ImVec2& size_arg = ImVec2(0, 0));

IMGUI_API bool SnesColorEdit4(absl::string_view label, gfx::SnesColor* color,
                              ImGuiColorEditFlags flags = 0);

// ============================================================================
// Palette Widget Functions
// ============================================================================

/**
 * @brief Small inline palette selector - just color buttons for selection
 * @param palette Palette to display
 * @param num_colors Number of colors to show (default 8)
 * @param selected_index Pointer to store selected color index (optional)
 * @return True if a color was selected
 */
IMGUI_API bool InlinePaletteSelector(gfx::SnesPalette& palette,
                                     int num_colors = 8,
                                     int* selected_index = nullptr);

/**
 * @brief Full inline palette editor with color picker and copy options
 * @param palette Palette to edit
 * @param title Display title
 * @param flags ImGui color edit flags
 * @return Status of the operation
 */
IMGUI_API absl::Status InlinePaletteEditor(gfx::SnesPalette& palette,
                                           const std::string& title = "",
                                           ImGuiColorEditFlags flags = 0);

/**
 * @brief Popup palette editor - same as inline but in a popup
 * @param popup_id ID for the popup window
 * @param palette Palette to edit
 * @param flags ImGui color edit flags
 * @return True if palette was modified
 */
IMGUI_API bool PopupPaletteEditor(const char* popup_id,
                                  gfx::SnesPalette& palette,
                                  ImGuiColorEditFlags flags = 0);

// Legacy functions (kept for compatibility, will be deprecated)
IMGUI_API bool DisplayPalette(gfx::SnesPalette& palette, bool loaded);

IMGUI_API absl::Status DisplayEditablePalette(gfx::SnesPalette& palette,
                                              const std::string& title = "",
                                              bool show_color_picker = false,
                                              int colors_per_row = 8,
                                              ImGuiColorEditFlags flags = 0);

void SelectablePalettePipeline(uint64_t& palette_id, bool& refresh_graphics,
                               gfx::SnesPalette& palette);

// Palette color button with selection and modification indicators
IMGUI_API bool PaletteColorButton(const char* id, const gfx::SnesColor& color,
                                  bool is_selected, bool is_modified,
                                  const ImVec2& size = ImVec2(28, 28),
                                  ImGuiColorEditFlags flags = 0);

}  // namespace gui
}  // namespace yaze

#endif
