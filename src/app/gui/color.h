#ifndef YAZE_GUI_COLOR_H
#define YAZE_GUI_COLOR_H

#include "absl/strings/str_format.h"
#include <string>

#include "absl/status/status.h"
#include "app/gfx/snes_palette.h"
#include "imgui/imgui.h"

namespace yaze {
namespace gui {

struct Color {
  float red;
  float green;
  float blue;
  float alpha;
};

inline ImVec4 ConvertColorToImVec4(const Color &color) {
  return ImVec4(color.red, color.green, color.blue, color.alpha);
}

inline std::string ColorToHexString(const Color &color) {
  return absl::StrFormat("%02X%02X%02X%02X", 
                         static_cast<int>(color.red * 255),
                         static_cast<int>(color.green * 255),
                         static_cast<int>(color.blue * 255),
                         static_cast<int>(color.alpha * 255));
}

// A utility function to convert an SnesColor object to an ImVec4 with
// normalized color values
ImVec4 ConvertSnesColorToImVec4(const gfx::SnesColor &color);

// A utility function to convert an ImVec4 to an SnesColor object
gfx::SnesColor ConvertImVec4ToSnesColor(const ImVec4 &color);

// The wrapper function for ImGui::ColorButton that takes a SnesColor reference
IMGUI_API bool SnesColorButton(absl::string_view id, gfx::SnesColor &color,
                               ImGuiColorEditFlags flags = 0,
                               const ImVec2 &size_arg = ImVec2(0, 0));

IMGUI_API bool SnesColorEdit4(absl::string_view label, gfx::SnesColor *color,
                              ImGuiColorEditFlags flags = 0);

IMGUI_API bool DisplayPalette(gfx::SnesPalette &palette, bool loaded);

IMGUI_API absl::Status DisplayEditablePalette(gfx::SnesPalette &palette,
                                              const std::string &title = "",
                                              bool show_color_picker = false,
                                              int colors_per_row = 8,
                                              ImGuiColorEditFlags flags = 0);

void SelectablePalettePipeline(uint64_t &palette_id, bool &refresh_graphics,
                               gfx::SnesPalette &palette);

}  // namespace gui
}  // namespace yaze

#endif
