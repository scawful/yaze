#include "app/editor/context/gfx_context.h"

#include <imgui/imgui.h>

#include <cmath>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "app/core/editor.h"
#include "app/gui/pipeline.h"
#include "app/editor/modules/palette_editor.h"
#include "app/gfx/bitmap.h"
#include "app/gfx/snes_palette.h"
#include "app/gfx/snes_tile.h"
#include "app/gui/canvas.h"
#include "app/gui/icons.h"
#include "app/rom.h"
#include "app/zelda3/overworld.h"

namespace yaze {
namespace app {
namespace editor {

absl::Status GfxContext::Update() { return absl::OkStatus(); }

gfx::Bitmap GfxContext::current_ow_gfx_bmp_;
gfx::SNESPalette GfxContext::current_ow_palette_;
gfx::Bitmap GfxContext::tile16_blockset_bmp_;
gfx::Bitmap GfxContext::tile8_blockset_bmp_;
std::vector<gfx::Bitmap> GfxContext::tile16_individual_bmp_;
std::vector<gfx::Bitmap> GfxContext::tile8_individual_bmp_;

}  // namespace editor
}  // namespace app
}  // namespace yaze