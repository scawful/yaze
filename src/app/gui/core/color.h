#ifndef YAZE_GUI_COLOR_H
#define YAZE_GUI_COLOR_H

#include "absl/strings/str_format.h"
#include <string>

#include "absl/status/status.h"
#include "app/gfx/types/snes_palette.h"
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
IMGUI_API bool InlinePaletteSelector(gfx::SnesPalette &palette, 
                                     int num_colors = 8,
                                     int* selected_index = nullptr);

/**
 * @brief Full inline palette editor with color picker and copy options
 * @param palette Palette to edit
 * @param title Display title
 * @param flags ImGui color edit flags
 * @return Status of the operation
 */
IMGUI_API absl::Status InlinePaletteEditor(gfx::SnesPalette &palette,
                                           const std::string &title = "",
                                           ImGuiColorEditFlags flags = 0);

/**
 * @brief Popup palette editor - same as inline but in a popup
 * @param popup_id ID for the popup window
 * @param palette Palette to edit
 * @param flags ImGui color edit flags
 * @return True if palette was modified
 */
IMGUI_API bool PopupPaletteEditor(const char* popup_id,
                                  gfx::SnesPalette &palette,
                                  ImGuiColorEditFlags flags = 0);

// Legacy functions (kept for compatibility, will be deprecated)
IMGUI_API bool DisplayPalette(gfx::SnesPalette &palette, bool loaded);

IMGUI_API absl::Status DisplayEditablePalette(gfx::SnesPalette &palette,
                                              const std::string &title = "",
                                              bool show_color_picker = false,
                                              int colors_per_row = 8,
                                              ImGuiColorEditFlags flags = 0);

void SelectablePalettePipeline(uint64_t &palette_id, bool &refresh_graphics,
                               gfx::SnesPalette &palette);

// Palette color button with selection and modification indicators
IMGUI_API bool PaletteColorButton(const char* id, const gfx::SnesColor& color,
                                  bool is_selected, bool is_modified,
                                  const ImVec2& size = ImVec2(28, 28),
                                  ImGuiColorEditFlags flags = 0);

}  // namespace gui
}  // namespace yaze

#endif
