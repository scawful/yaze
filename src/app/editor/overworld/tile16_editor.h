#ifndef YAZE_APP_EDITOR_TILE16EDITOR_H
#define YAZE_APP_EDITOR_TILE16EDITOR_H

#include <array>
#include <vector>

#include "absl/status/status.h"
#include "app/editor/graphics/palette_editor.h"
#include "app/gfx/bitmap.h"
#include "app/gfx/snes_palette.h"
#include "app/gfx/snes_tile.h"
#include "app/gui/canvas.h"
#include "app/gui/input.h"
#include "app/rom.h"
#include "app/zelda3/overworld/overworld.h"
#include "imgui/imgui.h"
#include "util/notify.h"

namespace yaze {
namespace editor {

/**
 * @brief Popup window to edit Tile16 data
 */
class Tile16Editor : public gfx::GfxContext {
 public:
  Tile16Editor(Rom *rom, gfx::Tilemap *tile16_blockset)
      : rom_(rom), tile16_blockset_(tile16_blockset) {}
  absl::Status Initialize(const gfx::Bitmap &tile16_blockset_bmp,
                          const gfx::Bitmap &current_gfx_bmp,
                          std::array<uint8_t, 0x200> &all_tiles_types);

  absl::Status Update();

  void DrawTile16Editor();
  absl::Status UpdateTile16Transfer();
  absl::Status UpdateBlockset();

  absl::Status DrawToCurrentTile16(ImVec2 pos);

  absl::Status UpdateTile16Edit();

  absl::Status UpdateTransferTileCanvas();

  absl::Status LoadTile8();

  absl::Status SetCurrentTile(int id);

  // New methods for clipboard and scratch space
  absl::Status CopyTile16ToClipboard(int tile_id);
  absl::Status PasteTile16FromClipboard();
  absl::Status SaveTile16ToScratchSpace(int slot);
  absl::Status LoadTile16FromScratchSpace(int slot);
  absl::Status ClearScratchSpace(int slot);

  void set_rom(Rom *rom) { rom_ = rom; }
  Rom *rom() const { return rom_; }

 private:
  Rom *rom_ = nullptr;
  bool map_blockset_loaded_ = false;
  bool transfer_started_ = false;
  bool transfer_blockset_loaded_ = false;
  bool x_flip = false;
  bool y_flip = false;
  bool priority_tile = false;

  int tile_size;
  int current_tile16_ = 0;
  int current_tile8_ = 0;
  uint8_t current_palette_ = 0;

  // Clipboard for Tile16 graphics
  gfx::Bitmap clipboard_tile16_;
  bool clipboard_has_data_ = false;

  // Scratch space for Tile16 graphics (4 slots)
  std::array<gfx::Bitmap, 4> scratch_space_;
  std::array<bool, 4> scratch_space_used_ = {false, false, false, false};

  util::NotifyValue<uint32_t> notify_tile16;
  util::NotifyValue<uint8_t> notify_palette;

  std::array<uint8_t, 0x200> all_tiles_types_;

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

  gui::Table tile_edit_table_{"##TileEditTable", 3, ImGuiTableFlags_Borders};

  gfx::Tilemap *tile16_blockset_ = nullptr;
  std::vector<gfx::Bitmap> current_gfx_individual_;

  PaletteEditor palette_editor_;
  gfx::SnesPalette palette_;

  absl::Status status_;

  Rom *transfer_rom_ = nullptr;
  zelda3::Overworld transfer_overworld_{transfer_rom_};
  std::array<gfx::Bitmap, kNumGfxSheets> transfer_gfx_;
  absl::Status transfer_status_;
};

}  // namespace editor
}  // namespace yaze

#endif  // YAZE_APP_EDITOR_TILE16EDITOR_H
