#ifndef YAZE_APP_EDITOR_TILE16EDITOR_H
#define YAZE_APP_EDITOR_TILE16EDITOR_H

#include <imgui/imgui.h>

#include <cmath>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "app/core/editor.h"
#include "app/core/pipeline.h"
#include "app/editor/palette_editor.h"
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

class Tile16Editor : public SharedROM {
 public:
  absl::Status Update();

  absl::Status InitBlockset(gfx::Bitmap tile16_blockset_bmp);

 private:
  bool map_blockset_loaded_ = false;
  bool transfer_started_ = false;
  bool transfer_blockset_loaded_ = false;

  // Canvas dimensions
  int canvas_width;
  int canvas_height;

  // Texture ID for the canvas
  int texture_id;

  // Various options for the Tile16 Editor
  bool x_flip;
  bool y_flip;
  bool priority_tile;
  int tile_size;

  gui::Canvas blockset_canvas_;
  gfx::Bitmap tile16_blockset_bmp_;

  gui::Canvas transfer_canvas_;
  gfx::Bitmap transfer_blockset_bmp_;
  gfx::Bitmap transfer_current_bmp_;

  PaletteEditor palette_editor_;

  gfx::SNESPalette palette_;
  zelda3::Overworld transfer_overworld_;
  std::vector<Bytes> tile16_individual_data_;
  std::vector<gfx::Bitmap> tile16_individual_;
  gfx::BitmapTable graphics_bin_;

  ROM transfer_rom_;
  absl::Status transfer_status_;
};

}  // namespace editor
}  // namespace app
}  // namespace yaze
#endif  // YAZE_APP_EDITOR_TILE16EDITOR_H