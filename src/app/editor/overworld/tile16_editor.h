#ifndef YAZE_APP_EDITOR_TILE16EDITOR_H
#define YAZE_APP_EDITOR_TILE16EDITOR_H

#include <array>
#include <chrono>
#include <functional>
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

// Constants for tile editing
constexpr int kTile16Size = 16;
constexpr int kTile8Size = 8;
constexpr int kTilesheetEditorWidth = 0x100;
constexpr int kTilesheetEditorHeight = 0x4000;
constexpr int kTile16CanvasSize = 0x20;
constexpr int kTile8CanvasHeight = 0x175;
constexpr int kNumScratchSlots = 4;
constexpr int kNumPalettes = 8;
constexpr int kTile8PixelCount = 64;
constexpr int kTile16PixelCount = 256;

/**
 * @brief Popup window to edit Tile16 data
 */
class Tile16Editor : public gfx::GfxContext {
 public:
  Tile16Editor(Rom* rom, gfx::Tilemap* tile16_blockset)
      : rom_(rom), tile16_blockset_(tile16_blockset) {}
  absl::Status Initialize(const gfx::Bitmap& tile16_blockset_bmp,
                          const gfx::Bitmap& current_gfx_bmp,
                          std::array<uint8_t, 0x200>& all_tiles_types);

  absl::Status Update();

  void DrawTile16Editor();
  absl::Status UpdateBlockset();

  // Scratch space for tile16 layouts
  void DrawScratchSpace();
  absl::Status SaveLayoutToScratch(int slot);
  absl::Status LoadLayoutFromScratch(int slot);

  absl::Status DrawToCurrentTile16(ImVec2 pos);

  absl::Status UpdateTile16Edit();

  absl::Status LoadTile8();

  absl::Status SetCurrentTile(int id);

  // New methods for clipboard and scratch space
  absl::Status CopyTile16ToClipboard(int tile_id);
  absl::Status PasteTile16FromClipboard();
  absl::Status SaveTile16ToScratchSpace(int slot);
  absl::Status LoadTile16FromScratchSpace(int slot);
  absl::Status ClearScratchSpace(int slot);

  // Advanced editing features
  absl::Status FlipTile16Horizontal();
  absl::Status FlipTile16Vertical();
  absl::Status RotateTile16();
  absl::Status FillTile16WithTile8(int tile8_id);
  absl::Status AutoTileTile16();
  absl::Status ClearTile16();

  // Palette management
  absl::Status CyclePalette(bool forward = true);
  absl::Status ApplyPaletteToAll(uint8_t palette_id);
  absl::Status PreviewPaletteChange(uint8_t palette_id);

  // Batch operations
  absl::Status ApplyToSelection(const std::function<void(int)>& operation);
  absl::Status BatchEdit(const std::vector<int>& tile_ids,
                         const std::function<void(int)>& operation);

  // History and undo system
  absl::Status Undo();
  absl::Status Redo();
  void SaveUndoState();

  // Live preview system
  void EnableLivePreview(bool enable) { live_preview_enabled_ = enable; }
  absl::Status UpdateLivePreview();

  // Validation and integrity checks
  absl::Status ValidateTile16Data();
  bool IsTile16Valid(int tile_id) const;

  // Integration with overworld system
  absl::Status SaveTile16ToROM();
  absl::Status UpdateOverworldTilemap();
  absl::Status CommitChangesToBlockset();
  absl::Status CommitChangesToOverworld();
  absl::Status DiscardChanges();

  // Helper methods for palette management
  absl::Status UpdateTile8Palette(int tile8_id);
  absl::Status RefreshAllPalettes();
  void DrawPaletteSettings();
  
  // ROM data access and modification
  absl::Status UpdateROMTile16Data();
  absl::Status RefreshTile16Blockset();
  gfx::Tile16* GetCurrentTile16Data();
  absl::Status RegenerateTile16BitmapFromROM();
  
  // Manual tile8 input controls
  void DrawManualTile8Inputs();

  void set_rom(Rom* rom) { rom_ = rom; }
  Rom* rom() const { return rom_; }
  
  // Callback for when changes are committed to notify parent editor
  void set_on_changes_committed(std::function<absl::Status()> callback) {
    on_changes_committed_ = callback;
  }

 private:
  Rom* rom_ = nullptr;
  bool map_blockset_loaded_ = false;
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

  // Layout scratch space for tile16 arrangements (4 slots of 8x8 grids)
  struct LayoutScratch {
    std::array<std::array<int, 8>, 8> tile_layout;  // 8x8 grid of tile16 IDs
    bool in_use = false;
    std::string name = "Empty";
  };
  std::array<LayoutScratch, 4> layout_scratch_;

  // Undo/Redo system
  struct UndoState {
    int tile_id;
    gfx::Bitmap tile_bitmap;
    gfx::Tile16 tile_data;
    uint8_t palette;
    bool x_flip, y_flip, priority;
  };
  std::vector<UndoState> undo_stack_;
  std::vector<UndoState> redo_stack_;
  static constexpr size_t kMaxUndoStates_ = 50;

  // Live preview system
  bool live_preview_enabled_ = true;
  gfx::Bitmap preview_tile16_;
  bool preview_dirty_ = false;

  // Selection system
  std::vector<int> selected_tiles_;
  int selection_start_tile_ = -1;
  bool multi_select_mode_ = false;

  // Advanced editing state
  bool auto_tile_mode_ = false;
  bool grid_snap_enabled_ = true;
  bool show_tile_info_ = true;
  bool show_palette_preview_ = true;

  // Palette management settings
  bool show_palette_settings_ = false;
  int current_palette_group_ = 0;  // 0=overworld_main, 1=aux1, 2=aux2, etc.
  uint8_t palette_normalization_mask_ = 0x0F;  // Default 4-bit mask
  bool auto_normalize_pixels_ = true;

  // Performance tracking
  std::chrono::steady_clock::time_point last_edit_time_;
  bool batch_mode_ = false;

  util::NotifyValue<uint32_t> notify_tile16;
  util::NotifyValue<uint8_t> notify_palette;

  std::array<uint8_t, 0x200> all_tiles_types_;

  // Tile16 blockset for selecting the tile to edit
  gui::Canvas blockset_canvas_{
      "blocksetCanvas", ImVec2(kTilesheetEditorWidth, kTilesheetEditorHeight),
      gui::CanvasGridSize::k32x32};
  gfx::Bitmap tile16_blockset_bmp_;

  // Canvas for editing the selected tile - optimized for 2x2 grid of 8x8 tiles (16x16 total)
  gui::Canvas tile16_edit_canvas_{"Tile16EditCanvas",
                                  ImVec2(64, 64),   // Fixed 64x64 display size (16x16 pixels at 4x scale)
                                  gui::CanvasGridSize::k8x8, 4.0F};   // 8x8 grid with 4x scale for clarity
  gfx::Bitmap current_tile16_bmp_;

  // Tile8 canvas to get the tile to drawing in the tile16_edit_canvas_
  gui::Canvas tile8_source_canvas_{
      "Tile8SourceCanvas",
      ImVec2(gfx::kTilesheetWidth * 4, gfx::kTilesheetHeight * 0x10 * 4),
      gui::CanvasGridSize::k32x32};
  gfx::Bitmap current_gfx_bmp_;

  gui::Table tile_edit_table_{"##TileEditTable", 3, ImGuiTableFlags_Borders};

  gfx::Tilemap* tile16_blockset_ = nullptr;
  std::vector<gfx::Bitmap> current_gfx_individual_;

  PaletteEditor palette_editor_;
  gfx::SnesPalette palette_;

  absl::Status status_;
  
  // Callback to notify parent editor when changes are committed
  std::function<absl::Status()> on_changes_committed_;
};

}  // namespace editor
}  // namespace yaze

#endif  // YAZE_APP_EDITOR_TILE16EDITOR_H
