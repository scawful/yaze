#ifndef YAZE_APP_EDITOR_GFX_GROUP_EDITOR_H
#define YAZE_APP_EDITOR_GFX_GROUP_EDITOR_H

#include <imgui/imgui.h>

#include <cmath>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "app/core/editor.h"
#include "app/core/pipeline.h"
#include "app/editor/resources/palette_editor.h"
#include "app/gfx/bitmap.h"
#include "app/gfx/snes_palette.h"
#include "app/gfx/snes_tile.h"
#include "app/gui/canvas.h"
#include "app/gui/icons.h"
#include "app/gui/widgets.h"
#include "app/rom.h"
#include "app/zelda3/overworld.h"

namespace yaze {
namespace app {
namespace editor {

class GfxGroupEditor : public SharedROM {
 public:
  absl::Status Update();

  void InitBlockset(gfx::Bitmap tile16_blockset);

 private:
  int preview_palette_id_ = 0;
  int last_sheet_id_ = 0;
  uint8_t selected_blockset_ = 0;
  uint8_t selected_roomset_ = 0;
  uint8_t selected_spriteset_ = 0;

  gui::Canvas blockset_canvas_;
  gui::Canvas roomset_canvas_;
  gui::Canvas spriteset_canvas_;

  gfx::SNESPalette palette_;
  gfx::PaletteGroup palette_group_;
  gfx::Bitmap tile16_blockset_bmp_;

  std::vector<Bytes> tile16_individual_data_;
  std::vector<gfx::Bitmap> tile16_individual_;

  gui::widgets::BitmapViewer gfx_group_viewer_;
  zelda3::Overworld overworld_;
};

}  // namespace editor
}  // namespace app
}  // namespace yaze
#endif  // YAZE_APP_EDITOR_GFX_GROUP_EDITOR_H