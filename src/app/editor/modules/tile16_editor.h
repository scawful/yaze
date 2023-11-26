#ifndef YAZE_APP_EDITOR_TILE16EDITOR_H
#define YAZE_APP_EDITOR_TILE16EDITOR_H

#include <imgui/imgui.h>

#include <cmath>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "app/core/editor.h"
#include "app/core/pipeline.h"
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

class Tile16Editor : public SharedROM {
 public:
  absl::Status Update();

  absl::Status UpdateBlockset();
  absl::Status UpdateTile16Edit();

  void DrawTileEditControls();

  absl::Status UpdateTransferTileCanvas();

  absl::Status InitBlockset(const gfx::Bitmap& tile16_blockset_bmp,
                            gfx::Bitmap current_gfx_bmp,
                            const std::vector<gfx::Bitmap>& tile16_individual);

  absl::Status LoadTile8();

 private:
  bool map_blockset_loaded_ = false;
  bool transfer_started_ = false;
  bool transfer_blockset_loaded_ = false;

  int current_tile16_ = 0;
  int current_tile8_ = 0;
  uint8_t current_palette_ = 0;

  core::NotifyValue<uint8_t> notify_tile16;
  core::NotifyValue<uint8_t> notify_palette;

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

  // Tile16 blockset for selecting the tile to edit
  gui::Canvas blockset_canvas_;
  gfx::Bitmap tile16_blockset_bmp_;

  // Canvas for editing the selected tile
  gui::Canvas tile16_edit_canvas_;
  gfx::Bitmap current_tile16_bmp_;
  gfx::Bitmap current_tile8_bmp_;

  // Tile8 canvas to get the tile to drawing in the tile16_edit_canvas_
  gui::Canvas tile8_source_canvas_;
  gfx::Bitmap current_gfx_bmp_;

  gui::Canvas transfer_canvas_;
  gfx::Bitmap transfer_blockset_bmp_;
  gfx::Bitmap transfer_current_bmp_;

  std::vector<Bytes> tile16_individual_data_;
  std::vector<gfx::Bitmap> tile16_individual_;

  std::vector<gfx::Bitmap> current_gfx_individual_;

  std::vector<uint8_t> current_tile16_data_;

  PaletteEditor palette_editor_;

  gfx::SNESPalette palette_;
  zelda3::Overworld transfer_overworld_;

  gfx::BitmapTable graphics_bin_;

  ROM transfer_rom_;
  absl::Status transfer_status_;

  core::TaskManager<std::function<void(int)>> task_manager_;
};

}  // namespace editor
}  // namespace app
}  // namespace yaze
#endif  // YAZE_APP_EDITOR_TILE16EDITOR_H