#ifndef YAZE_APP_EDITOR_VRAM_CONTEXT_H
#define YAZE_APP_EDITOR_VRAM_CONTEXT_H

#include <imgui/imgui.h>

#include <cmath>
#include <vector>

#include "app/core/editor.h"
#include "app/editor/modules/palette_editor.h"
#include "app/gfx/bitmap.h"
#include "app/gfx/snes_palette.h"
#include "app/gfx/snes_tile.h"
#include "app/gui/canvas.h"
#include "app/gui/icons.h"
#include "app/gui/pipeline.h"
#include "app/rom.h"

namespace yaze {
namespace app {
namespace editor {

class GfxContext {
 public:
  absl::Status Update();

 protected:
  // Palettesets for the tile16 individual tiles
  static std::unordered_map<uint8_t, gfx::Paletteset> palettesets_;
};

}  // namespace editor
}  // namespace app
}  // namespace yaze

#endif  // YAZE_APP_EDITOR_VRAM_CONTEXT_H