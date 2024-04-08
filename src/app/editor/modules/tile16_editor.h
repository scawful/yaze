#ifndef YAZE_APP_EDITOR_TILE16EDITOR_H
#define YAZE_APP_EDITOR_TILE16EDITOR_H

#include <imgui/imgui.h>

#include <cmath>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "app/core/editor.h"
#include "app/editor/context/gfx_context.h"
#include "app/editor/modules/palette_editor.h"
#include "app/gfx/bitmap.h"
#include "app/gfx/snes_palette.h"
#include "app/gfx/snes_tile.h"
#include "app/gui/canvas.h"
#include "app/gui/icons.h"
#include "app/gui/pipeline.h"
#include "app/rom.h"
#include "app/zelda3/overworld.h"

namespace yaze {
namespace app {
namespace editor {

class Tile16Editor : public GfxContext, public SharedROM {
 public:
  absl::Status Update();
  absl::Status DrawMenu();

  absl::Status DrawTile16Editor();
  absl::Status UpdateTile16Transfer();
  absl::Status UpdateBlockset();

  absl::Status DrawToCurrentTile16(ImVec2 pos);

  absl::Status UpdateTile16Edit();

  absl::Status DrawTileEditControls();

  absl::Status UpdateTransferTileCanvas();

  void InitBlockset(const gfx::Bitmap& tile16_blockset_bmp,
                    gfx::Bitmap current_gfx_bmp,
                    const std::vector<gfx::Bitmap>& tile16_individual,
                    uint8_t all_tiles_types[0x200]) {
    all_tiles_types_ = all_tiles_types;
    tile16_blockset_bmp_ = tile16_blockset_bmp;
    tile16_individual_ = tile16_individual;
    current_gfx_bmp_ = current_gfx_bmp;
    tile8_gfx_data_ = current_gfx_bmp_.vector();
  }

  absl::Status LoadTile8();

  absl::Status set_tile16(int id) {
    current_tile16_ = id;
    current_tile16_bmp_ = tile16_individual_[id];
    ASSIGN_OR_RETURN(auto ow_main_pal_group, rom()->palette_group("ow_main"));
    RETURN_IF_ERROR(
        current_tile16_bmp_.ApplyPalette(ow_main_pal_group[current_palette_]));
    rom()->RenderBitmap(&current_tile16_bmp_);
    return absl::OkStatus();
  }

 private:
  bool map_blockset_loaded_ = false;
  bool transfer_started_ = false;
  bool transfer_blockset_loaded_ = false;

  int current_tile16_ = 0;
  int current_tile8_ = 0;
  uint8_t current_palette_ = 0;

  core::NotifyValue<uint32_t> notify_tile16;
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

  uint8_t* all_tiles_types_;

  // Tile16 blockset for selecting the tile to edit
  gui::Canvas blockset_canvas_{ImVec2(0x100, 0x4000),
                               gui::CanvasGridSize::k32x32};
  gfx::Bitmap tile16_blockset_bmp_;

  // Canvas for editing the selected tile
  gui::Canvas tile16_edit_canvas_{ImVec2(0x40, 0x40),
                                  gui::CanvasGridSize::k64x64};
  gfx::Bitmap current_tile16_bmp_;
  gfx::Bitmap current_tile8_bmp_;

  // Tile8 canvas to get the tile to drawing in the tile16_edit_canvas_
  gui::Canvas tile8_source_canvas_{
      ImVec2(core::kTilesheetWidth * 4, core::kTilesheetHeight * 0x10 * 4),
      gui::CanvasGridSize::k32x32};
  gfx::Bitmap current_gfx_bmp_;

  gui::Canvas transfer_canvas_;
  gfx::Bitmap transfer_blockset_bmp_;
  gfx::Bitmap transfer_current_bmp_;

  std::vector<Bytes> tile16_individual_data_;
  std::vector<gfx::Bitmap> tile16_individual_;

  std::vector<gfx::Bitmap> current_gfx_individual_;

  std::vector<uint8_t> current_tile16_data_;

  std::vector<uint8_t> tile8_gfx_data_;

  PaletteEditor palette_editor_;

  gfx::SnesPalette palette_;
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