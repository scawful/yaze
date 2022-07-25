#ifndef YAZE_APP_EDITOR_PALETTE_EDITOR_H
#define YAZE_APP_EDITOR_PALETTE_EDITOR_H

#include <imgui/imgui.h>

#include "app/gfx/snes_palette.h"
#include "gui/canvas.h"
#include "gui/icons.h"

namespace yaze {
namespace app {
namespace editor {

class PaletteEditor {
 public:
  void Update();

 private:
};

}  // namespace editor
}  // namespace app
}  // namespace yaze

#endif