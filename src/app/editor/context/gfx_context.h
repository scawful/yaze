#ifndef YAZE_APP_EDITOR_VRAM_CONTEXT_H
#define YAZE_APP_EDITOR_VRAM_CONTEXT_H

#include <imgui/imgui.h>

#include <cmath>
#include <vector>

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

// Create a class which manages the current VRAM state of Link to the Past,
// including static members for the Bitmaps and Palettes as well as the current
// blockset as to update all of the overworld maps with the new blockset when it
// is changed. This class will also manage the current tile16 and tile8
// selection, as well as the current palette selection.

namespace yaze {
namespace app {
namespace editor {

class GfxContext {
 public:
  absl::Status Update();

 protected:
  static gfx::Bitmap current_ow_gfx_bmp_;

  static gfx::SNESPalette current_ow_palette_;

  static gfx::Bitmap tile16_blockset_bmp_;

  static gfx::Bitmap tile8_blockset_bmp_;

  // Bitmaps for the tile16 individual tiles
  static std::vector<gfx::Bitmap> tile16_individual_bmp_;

  // Bitmaps for the tile8 individual tiles
  static std::vector<gfx::Bitmap> tile8_individual_bmp_;
};

}  // namespace editor
}  // namespace app
}  // namespace yaze

#endif  // YAZE_APP_EDITOR_VRAM_CONTEXT_H