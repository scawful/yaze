#ifndef YAZE_GUI_COLOR_H
#define YAZE_GUI_COLOR_H

#include <imgui/imgui.h>

#include <cmath>
#include <string>

#include "app/gfx/bitmap.h"
#include "app/gfx/snes_palette.h"

namespace yaze {
namespace app {
namespace gui {

using gfx::SNESColor;

// A utility function to convert an SNESColor object to an ImVec4 with
// normalized color values
ImVec4 ConvertSNESColorToImVec4(const SNESColor& color);

// The wrapper function for ImGui::ColorButton that takes a SNESColor reference
IMGUI_API bool SNESColorButton(absl::string_view id, SNESColor& color,
                               ImGuiColorEditFlags flags = 0,
                               const ImVec2& size_arg = ImVec2(0, 0));

void DisplayPalette(app::gfx::SNESPalette& palette, bool loaded);

}  // namespace gui
}  // namespace app
}  // namespace yaze

#endif