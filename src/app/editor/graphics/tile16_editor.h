#ifndef YAZE_APP_EDITOR_TILE16EDITOR_H
#define YAZE_APP_EDITOR_TILE16EDITOR_H

#include "absl/status/status.h"
#include "app/core/common.h"
#include "app/editor/graphics/palette_editor.h"
#include "app/editor/utils/gfx_context.h"
#include "app/gfx/bitmap.h"
#include "app/gfx/snes_palette.h"
#include "app/gfx/snes_tile.h"
#include "app/gui/canvas.h"
#include "app/rom.h"
#include "app/zelda3/overworld/overworld.h"
#include "imgui/imgui.h"

namespace yaze {
namespace app {
namespace editor {

/**
 * @brief Popup window to edit Tile16 data
 */
class Tile16Editor : public context::GfxContext, public SharedRom {
 public:
  absl::Status InitBlockset(const gfx::Bitmap& tile16_blockset_bmp,
                            const gfx::Bitmap& current_gfx_bmp,
                            const std::vector<gfx::Bitmap>& tile16_individual,
                            uint8_t all_tiles_types[0x200]);

  absl::Status Update();
  absl::Status DrawMenu();

  void DrawTile16Editor();
  absl::Status UpdateTile16Transfer();
  absl::Status UpdateBlockset();

  absl::Status DrawToCurrentTile16(ImVec2 pos);

  absl::Status UpdateTile16Edit();

  absl::Status DrawTileEditControls();

  absl::Status UpdateTransferTileCanvas();

  absl::Status LoadTile8();

  absl::Status SetCurrentTile(int id);

 private:
  bool map_blockset_loaded_ = false;
  bool transfer_started_ = false;
  bool transfer_blockset_loaded_ = false;

  int current_tile16_ = 0;
  int current_tile8_ = 0;
  uint8_t current_palette_ = 0;

  core::NotifyValue<uint32_t> notify_tile16;
  core::NotifyValue<uint8_t> notify_palette;

  // Various options for the Tile16 Editor
  bool x_flip;
  bool y_flip;
  bool priority_tile;
  int tile_size;

  uint8_t* all_tiles_types_;

  // Tile16 blockset for selecting the tile to edit
  gui::Canvas blockset_canvas_{"blocksetCanvas", ImVec2(0x100, 0x4000),
                               gui::CanvasGridSize::k32x32};
  gfx::Bitmap tile16_blockset_bmp_;

  // Canvas for editing the selected tile
  gui::Canvas tile16_edit_canvas_{"Tile16EditCanvas", ImVec2(0x40, 0x40),
                                  gui::CanvasGridSize::k64x64};
  gfx::Bitmap current_tile16_bmp_;

  // Tile8 canvas to get the tile to drawing in the tile16_edit_canvas_
  gui::Canvas tile8_source_canvas_{
      "Tile8SourceCanvas",
      ImVec2(gfx::kTilesheetWidth * 4, gfx::kTilesheetHeight * 0x10 * 4),
      gui::CanvasGridSize::k32x32};
  gfx::Bitmap current_gfx_bmp_;

  gui::Canvas transfer_canvas_;
  gfx::Bitmap transfer_blockset_bmp_;

  std::vector<gfx::Bitmap> tile16_individual_;
  std::vector<gfx::Bitmap> current_gfx_individual_;

  PaletteEditor palette_editor_;

  gfx::SnesPalette palette_;
  zelda3::overworld::Overworld transfer_overworld_;

  absl::Status status_;

  Rom transfer_rom_;
  absl::Status transfer_status_;
};

}  // namespace editor
}  // namespace app
}  // namespace yaze
#endif  // YAZE_APP_EDITOR_TILE16EDITOR_H
