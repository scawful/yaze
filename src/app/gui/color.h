#ifndef YAZE_GUI_COLOR_H
#define YAZE_GUI_COLOR_H

#include "imgui/imgui.h"

#include <cmath>
#include <string>

#include "absl/status/status.h"
#include "app/gfx/bitmap.h"
#include "app/gfx/snes_palette.h"

namespace yaze {
namespace app {
namespace gui {

using gfx::SnesColor;

// A utility function to convert an SnesColor object to an ImVec4 with
// normalized color values
ImVec4 ConvertSNESColorToImVec4(const SnesColor& color);

// The wrapper function for ImGui::ColorButton that takes a SnesColor reference
IMGUI_API bool SnesColorButton(absl::string_view id, SnesColor& color,
                               ImGuiColorEditFlags flags = 0,
                               const ImVec2& size_arg = ImVec2(0, 0));

IMGUI_API bool SnesColorEdit4(absl::string_view label, SnesColor* color,
                              ImGuiColorEditFlags flags = 0);

absl::Status DisplayPalette(app::gfx::SnesPalette& palette, bool loaded);

void SelectablePalettePipeline(uint64_t& palette_id, bool& refresh_graphics,
                               gfx::SnesPalette& palette);

}  // namespace gui
}  // namespace app
}  // namespace yaze

#endif
