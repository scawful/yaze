#include "app/editor/context/gfx_context.h"

#include <imgui/imgui.h>

#include <cmath>

#include "app/core/editor.h"
#include "app/gui/pipeline.h"
#include "app/editor/modules/palette_editor.h"
#include "app/gfx/bitmap.h"
#include "app/gfx/snes_palette.h"
#include "app/gfx/snes_tile.h"
#include "app/gui/canvas.h"
#include "app/gui/icons.h"
#include "app/rom.h"

namespace yaze {
namespace app {
namespace editor {


std::unordered_map<uint8_t, gfx::Paletteset> GfxContext::palettesets_;

}  // namespace editor
}  // namespace app
}  // namespace yaze