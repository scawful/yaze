#ifndef YAZE_APP_EDITOR_PALETTE_EDITOR_H
#define YAZE_APP_EDITOR_PALETTE_EDITOR_H

#include <imgui/imgui.h>

#include "absl/status/status.h"
#include "app/gfx/snes_palette.h"
#include "gui/canvas.h"
#include "gui/icons.h"

namespace yaze {
namespace app {
namespace editor {

static constexpr absl::string_view kPaletteCategoryNames[] = {
    "Sword",       "Shield",   "Clothes",  "World Colors",
    "Area Colors", "Enemies",  "Dungeons", "World Map",
    "Dungeon Map", "Triforce", "Crystal"};

class PaletteEditor {
 public:
  absl::Status Update();

 private:
};

}  // namespace editor
}  // namespace app
}  // namespace yaze

#endif