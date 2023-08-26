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

void DisplayPalette(app::gfx::SNESPalette& palette, bool loaded);

}  // namespace gui
}  // namespace app
}  // namespace yaze

#endif