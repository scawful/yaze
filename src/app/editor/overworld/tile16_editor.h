#ifndef YAZE_APP_EDITOR_TILE16EDITOR_H
#define YAZE_APP_EDITOR_TILE16EDITOR_H

#include <algorithm>
#include <array>
#include <chrono>
#include <functional>
#include <map>
#include <optional>
#include <string>
#include <vector>

#include "absl/status/status.h"
#include "app/editor/core/undo_manager.h"
#include "app/editor/overworld/tile16_undo_actions.h"
#include "app/editor/palette/palette_editor.h"
#include "app/gfx/core/bitmap.h"
#include "app/gfx/types/snes_palette.h"
#include "app/gfx/types/snes_tile.h"
#include "app/gui/canvas/canvas.h"
#include "app/gui/core/input.h"
#include "app/gui/widgets/tile_selector_widget.h"
#include "rom/rom.h"
#include "imgui/imgui.h"
#include "util/log.h"
#include "util/notify.h"

namespace yaze {
namespace zelda3 {
struct GameData;
}  // namespace zelda3

namespace editor {

// ============================================================================
// Tile16 Editor Constants
// ============================================================================

constexpr int kTile16Size = 16;                    // 16x16 pixel tile
constexpr int kTile8Size = 8;                      // 8x8 pixel sub-tile
constexpr int kTilesheetEditorWidth = 0x100;       // 256 pixels wide
constexpr int kTilesheetEditorHeight = 0x4000;     // 16384 pixels tall
constexpr int kTile16CanvasSize = 0x20;            // 32 pixels
constexpr int kTile8CanvasHeight = 0x175;          // 373 pixels
constexpr int kNumScratchSlots = 4;                // 4 scratch space slots
constexpr int kNumPalettes = 8;                    // 8 palette buttons (0-7)
constexpr int kTile8PixelCount = 64;               // 8x8 = 64 pixels
constexpr int kTile16PixelCount = 256;             // 16x16 = 256 pixels

struct Tile16ClipboardData {
  gfx::Tile16 tile_data;
  gfx::Bitmap bitmap;
  bool has_data = false;
};

struct Tile16ScratchData {
  gfx::Tile16 tile_data;
  gfx::Bitmap bitmap;
  bool has_data = false;
};

// ============================================================================
// Tile16 Editor
// ============================================================================
//
// ARCHITECTURE OVERVIEW:
// ----------------------
// The Tile16Editor provides a popup window for editing individual 16x16 tiles
// used in the overworld tileset. Each Tile16 is composed of four 8x8 sub-tiles
// (Tile8) arranged in a 2x2 grid.
//
// EDITING WORKFLOW:
// -----------------
// 1. Select a Tile16 from the blockset canvas (left panel)
// 2. Edit by clicking on the tile8 source canvas to select sub-tiles
// 3. Place selected tile8s into the four quadrants of the Tile16
// 4. Changes are held as "pending" until explicitly committed or discarded
// 5. Commit saves to ROM; Discard reverts to original
//
// PENDING CHANGES SYSTEM:
// -----------------------
// To prevent accidental ROM modifications, all edits are staged:
//   - pending_tile16_changes_: Maps tile ID -> modified Tile16 data
//   - pending_tile16_bitmaps_: Maps tile ID -> preview bitmap
//   - has_pending_changes(): Returns true if any tiles are modified
//   - CommitAllChanges(): Writes all pending changes to ROM
//   - DiscardAllChanges(): Reverts all pending changes
//
// PALETTE COORDINATION:
// ---------------------
// The overworld uses a 256-color palette organized as 16 rows of 16 colors.
// Different graphics sheets map to different palette regions:
//
//   Sheet Index | Palette Region | Purpose
//   ------------|----------------|------------------------
//   0, 3, 4     | AUX1 (row 10+) | Main blockset graphics
//   1, 2        | MAIN (row 2+)  | Main area graphics
//   5, 6        | AUX2 (row 10+) | Secondary blockset
//   7           | ANIMATED       | Animated tiles
//
// Key palette methods:
//   - GetPaletteSlotForSheet(): Get base palette slot for a sheet
//   - GetActualPaletteSlot(): Combine palette button + sheet to get final slot
//   - GetActualPaletteSlotForCurrentTile16(): Get slot for current editing tile
//   - ApplyPaletteToCurrentTile16Bitmap(): Apply correct colors to preview
//
// INTEGRATION WITH OVERWORLD:
// ---------------------------
// The Tile16Editor communicates with OverworldEditor via:
//   - set_palette(): Called when overworld area changes (updates colors)
//   - on_changes_committed_: Callback invoked after CommitAllChanges()
//   - The callback triggers RefreshTile16Blockset() and RefreshOverworldMap()
//
// See README.md in this directory for complete documentation.
// ============================================================================

/**
 * @brief Popup window to edit Tile16 data
 *
 * Provides visual editing of 16x16 tiles composed of four 8x8 sub-tiles.
 * Uses a pending changes system to prevent accidental ROM modifications.
 *
 * @see README.md for architecture overview and workflow documentation
 */
class Tile16Editor : public gfx::GfxContext {
 public:
  Tile16Editor(Rom* rom, gfx::Tilemap* tile16_blockset)
      : rom_(rom), tile16_blockset_(tile16_blockset) {}
  absl::Status Initialize(const gfx::Bitmap& tile16_blockset_bmp,
                          const gfx::Bitmap& current_gfx_bmp,
                          std::array<uint8_t, 0x200>& all_tiles_types);

  absl::Status Update();

  /**
   * @brief Update the editor content without MenuBar (for EditorPanel usage)
   *
   * This is the panel-friendly version that doesn't require ImGuiWindowFlags_MenuBar.
   * Menu items are available through the context menu instead.
   */
  absl::Status UpdateAsPanel();

  /**
   * @brief Draw context menu with editor actions
   *
   * Contains the same actions as the MenuBar but in context menu form.
   * Call this when right-clicking or from a menu button.
   */
  void DrawContextMenu();

  void DrawTile16Editor();
  absl::Status UpdateBlockset();

  // Scratch space for tile16 layouts
  void DrawScratchSpace();
  absl::Status SaveLayoutToScratch(int slot);
  absl::Status LoadLayoutFromScratch(int slot);

  absl::Status DrawToCurrentTile16(ImVec2 pos,
                                   const gfx::Bitmap* source_tile = nullptr);

  absl::Status UpdateTile16Edit();

  absl::Status LoadTile8();

  absl::Status SetCurrentTile(int id);

  // Request a tile switch - shows confirmation dialog if current tile has pending changes
  void RequestTileSwitch(int target_tile_id);

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

  // ===========================================================================
  // Integration with Overworld System
  // ===========================================================================
  // These methods handle the connection between tile editing and ROM data.
  // The workflow is: Edit -> Pending -> Commit -> ROM
  
  /// @brief Write current tile16 data directly to ROM (bypasses pending system)
  absl::Status SaveTile16ToROM();
  
  /// @brief Update the overworld tilemap to reflect tile changes
  absl::Status UpdateOverworldTilemap();
  
  /// @brief Commit pending changes to the blockset atlas
  absl::Status CommitChangesToBlockset();
  
  /// @brief Full commit workflow: ROM + blockset + notify parent
  absl::Status CommitChangesToOverworld();
  
  /// @brief Discard current tile's changes (single tile)
  absl::Status DiscardChanges();

  // ===========================================================================
  // Pending Changes System
  // ===========================================================================
  // All tile edits are staged in memory before being written to ROM.
  // This prevents accidental modifications and allows preview before commit.
  //
  // Usage:
  //   1. Edit tiles normally (changes go to pending_tile16_changes_)
  //   2. Check has_pending_changes() to show save/discard UI
  //   3. User clicks Save -> CommitAllChanges()
  //   4. User clicks Discard -> DiscardAllChanges()
  //
  // The on_changes_committed_ callback notifies OverworldEditor to refresh.
  
  /// @brief Check if any tiles have uncommitted changes
  bool has_pending_changes() const { return !pending_tile16_changes_.empty(); }
  
  /// @brief Get count of tiles with pending changes
  int pending_changes_count() const {
    return static_cast<int>(pending_tile16_changes_.size());
  }
  
  /// @brief Check if a specific tile has pending changes
  bool is_tile_modified(int tile_id) const {
    return pending_tile16_changes_.find(tile_id) != pending_tile16_changes_.end();
  }
  
  /// @brief Get preview bitmap for a pending tile (nullptr if not modified)
  const gfx::Bitmap* GetPendingTileBitmap(int tile_id) const {
    auto it = pending_tile16_bitmaps_.find(tile_id);
    return it != pending_tile16_bitmaps_.end() ? &it->second : nullptr;
  }
  
  /// @brief Write all pending changes to ROM and notify parent
  absl::Status CommitAllChanges();
  
  /// @brief Discard all pending changes (revert to ROM state)
  void DiscardAllChanges();
  
  /// @brief Discard only the current tile's pending changes
  void DiscardCurrentTileChanges();
  
  /// @brief Mark the current tile as having pending changes
  void MarkCurrentTileModified();

  // ===========================================================================
  // Palette Coordination System
  // ===========================================================================
  // The overworld uses a 256-color palette organized as 16 rows of 16 colors.
  // Different graphics sheets map to different palette regions based on how
  // the SNES PPU organizes tile graphics.
  //
  // Palette Structure (256 colors = 16 rows × 16 colors):
  //   Row 0:     Transparent/system colors
  //   Row 1:     HUD colors (0x10-0x1F)
  //   Rows 2-6:  MAIN/BG palettes for main graphics (sheets 1-2)
  //   Rows 7:    ANIMATED palette (sheet 7)
  //   Rows 10+:  AUX palettes for blockset graphics (sheets 0, 3-6)
  //
  // The palette button (0-7) selects which of the 8 available sub-palettes
  // to use, and the sheet index determines the base offset.
  
  /// @brief Update palette for a specific tile8
  absl::Status UpdateTile8Palette(int tile8_id);
  
  /// @brief Refresh all tile8 palettes after a palette change
  absl::Status RefreshAllPalettes();
  
  /// @brief Draw palette settings UI
  void DrawPaletteSettings();

  /// @brief Get base palette slot for a graphics sheet
  /// @param sheet_index Graphics sheet index (0-7)
  /// @return Base palette offset (e.g., 10 for AUX, 2 for MAIN)
  int GetPaletteSlotForSheet(int sheet_index) const;

  /// @brief Calculate actual palette slot from button + sheet
  /// @param palette_button User-selected palette (0-7)
  /// @param sheet_index Graphics sheet the tile8 belongs to
  /// @return Final palette slot index in 256-color palette
  /// 
  /// This is the core palette mapping function. It combines:
  ///   - palette_button: Which of 8 sub-palettes user selected
  ///   - sheet_index: Which graphics sheet contains the tile8
  /// To produce the actual 256-color palette index.
  int GetActualPaletteSlot(int palette_button, int sheet_index) const;

  /// @brief Get palette base row for a graphics sheet
  /// @param sheet_index Graphics sheet index (0-7)
  /// @return Base row index in the 16-row palette structure
  int GetPaletteBaseForSheet(int sheet_index) const;

  /// @brief Determine which graphics sheet contains a tile8
  /// @param tile8_id Tile8 ID from the graphics buffer
  /// @return Sheet index (0-7) based on tile position
  int GetSheetIndexForTile8(int tile8_id) const;
  
  /// @brief Get the palette slot for the current tile being edited
  /// @return Palette slot based on current_tile8_ and current_palette_
  int GetActualPaletteSlotForCurrentTile16() const;

  /// @brief Create a remapped palette for viewing with user-selected palette
  /// @param source Full 256-color palette
  /// @param target_row User-selected palette row (0-7 maps to the sheet base)
  /// @return Remapped 256-color palette where all pixels map to target row
  gfx::SnesPalette CreateRemappedPaletteForViewing(
      const gfx::SnesPalette& source, int target_row) const;

  /// @brief Get the encoded palette row for a pixel value
  /// @param pixel_value Raw pixel value from the graphics buffer
  /// @return Palette row (0-15) that this pixel would use
  int GetEncodedPaletteRow(uint8_t pixel_value) const;

  // ROM data access and modification
  absl::Status UpdateROMTile16Data();
  absl::Status RefreshTile16Blockset();
  gfx::Tile16* GetCurrentTile16Data();
  absl::Status RegenerateTile16BitmapFromROM();
  absl::Status UpdateBlocksetBitmap();
  absl::Status PickTile8FromTile16(const ImVec2& position);

  // Manual tile8 input controls
  void DrawManualTile8Inputs();

  void SetRom(Rom* rom) { rom_ = rom; }
  Rom* rom() const { return rom_; }
  void SetGameData(zelda3::GameData* game_data) { game_data_ = game_data; }
  zelda3::GameData* game_data() const { return game_data_; }

  // Set the palette from overworld to ensure color consistency
  void set_palette(const gfx::SnesPalette& palette) {
    palette_ = palette;

    // Store the complete 256-color overworld palette
    if (palette.size() >= 256) {
      overworld_palette_ = palette;
      util::logf(
          "Tile16 editor received complete overworld palette with %zu colors",
          palette.size());
    } else {
      util::logf("Warning: Received incomplete palette with %zu colors",
                 palette.size());
      overworld_palette_ = palette;
    }

    // CRITICAL FIX: Load tile8 graphics now that we have the proper palette
    if (rom_ && current_gfx_bmp_.is_active()) {
      auto status = LoadTile8();
      if (!status.ok()) {
        util::logf("Failed to load tile8 graphics with new palette: %s",
                   status.message().data());
      } else {
        util::logf(
            "Successfully loaded tile8 graphics with complete overworld "
            "palette");
      }
    }

    util::logf("Tile16 editor palette coordination complete");
  }

  // Callback for when changes are committed to notify parent editor
  void set_on_changes_committed(std::function<absl::Status()> callback) {
    on_changes_committed_ = callback;
  }

  // Accessors for testing and external use
  int current_palette() const { return current_palette_; }
  void set_current_palette(int palette) {
    current_palette_ = static_cast<uint8_t>(std::clamp(palette, 0, 7));
  }
  int current_tile16() const { return current_tile16_; }
  int current_tile8() const { return current_tile8_; }

  // Diagnostic function to analyze tile8 source data format
  void AnalyzeTile8SourceData() const;

 private:
  Rom* rom_ = nullptr;
  zelda3::GameData* game_data_ = nullptr;
  bool map_blockset_loaded_ = false;
  bool x_flip = false;
  bool y_flip = false;
  bool priority_tile = false;

  int tile_size;
  int current_tile16_ = 0;
  int current_tile8_ = 0;
  uint8_t current_palette_ = 0;

  // Clipboard for Tile16 graphics and metadata
  Tile16ClipboardData clipboard_tile16_;

  // Scratch space for Tile16 graphics and metadata (4 slots)
  std::array<Tile16ScratchData, 4> scratch_space_;

  // Layout scratch space for tile16 arrangements (4 slots of 8x8 grids)
  struct LayoutScratch {
    std::array<std::array<int, 8>, 8> tile_layout;  // 8x8 grid of tile16 IDs
    bool in_use = false;
    std::string name = "Empty";
  };
  std::array<LayoutScratch, 4> layout_scratch_;

  // Undo/Redo system (unified UndoManager framework)
  UndoManager undo_manager_;
  std::optional<Tile16Snapshot> pending_undo_before_;

  /// @brief Finalize any pending undo snapshot by capturing current state
  /// as "after" and pushing a Tile16EditAction to undo_manager_.
  void FinalizePendingUndo();

  /// @brief Restore editor state from a Tile16Snapshot (used by undo actions).
  void RestoreFromSnapshot(const Tile16Snapshot& snapshot);

  // Live preview system
  bool live_preview_enabled_ = true;
  gfx::Bitmap preview_tile16_;
  bool preview_dirty_ = false;
  gfx::Bitmap tile8_preview_bmp_;  // Persistent preview to keep arena commands valid

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
  uint8_t palette_normalization_mask_ =
      0xFF;  // Default 8-bit mask (preserve full palette index)
  bool auto_normalize_pixels_ =
      false;  // Disabled by default to preserve palette offsets

  // Performance tracking
  std::chrono::steady_clock::time_point last_edit_time_;
  bool batch_mode_ = false;

  // Pending changes system for batch preview/commit workflow
  std::map<int, gfx::Tile16> pending_tile16_changes_;
  std::map<int, gfx::Bitmap> pending_tile16_bitmaps_;
  bool show_unsaved_changes_dialog_ = false;
  int pending_tile_switch_target_ = -1;  // Target tile for pending switch

  // Navigation controls for expanded tile support
  int jump_to_tile_id_ = 0;       // Input field for jump to tile ID
  bool scroll_to_current_ = false; // Flag to scroll to current tile
  int current_page_ = 0;          // Current page (64 tiles per page)
  static constexpr int kTilesPerPage = 64;  // 8x8 tiles per page
  static constexpr int kTilesPerRow = 8;    // Tiles per row in grid

  util::NotifyValue<uint32_t> notify_tile16;
  util::NotifyValue<uint8_t> notify_palette;

  std::array<uint8_t, 0x200> all_tiles_types_;

  // Tile16 blockset for selecting the tile to edit
  gui::Canvas blockset_canvas_{
      "blocksetCanvas", ImVec2(kTilesheetEditorWidth, kTilesheetEditorHeight),
      gui::CanvasGridSize::k32x32};
  gui::TileSelectorWidget blockset_selector_{"Tile16BlocksetSelector"};
  gfx::Bitmap tile16_blockset_bmp_;

  // Canvas for editing the selected tile - optimized for 2x2 grid of 8x8 tiles
  // (16x16 total)
  gui::Canvas tile16_edit_canvas_{
      "Tile16EditCanvas",
      ImVec2(64, 64),  // Fixed 64x64 display size (16x16 pixels at 4x scale)
      gui::CanvasGridSize::k8x8, 8.0F};  // 8x8 grid with 4x scale for clarity
  gfx::Bitmap current_tile16_bmp_;

  // Tile8 canvas to get the tile to drawing in the tile16_edit_canvas_
  gui::Canvas tile8_source_canvas_{
      "Tile8SourceCanvas",
      ImVec2(gfx::kTilesheetWidth * 8, gfx::kTilesheetHeight * 0x10 * 8),
      gui::CanvasGridSize::k32x32};
  gfx::Bitmap current_gfx_bmp_;

  gui::Table tile_edit_table_{"##TileEditTable", 3, ImGuiTableFlags_Borders,
                              ImVec2(0, 0)};

  gfx::Tilemap* tile16_blockset_ = nullptr;
  std::vector<gfx::Bitmap> current_gfx_individual_;

  PaletteEditor palette_editor_;
  gfx::SnesPalette palette_;
  gfx::SnesPalette overworld_palette_;  // Complete 256-color overworld palette

  absl::Status status_;

  // Callback to notify parent editor when changes are committed
  std::function<absl::Status()> on_changes_committed_;

  // Instance variable to store current tile16 data for proper persistence
  gfx::Tile16 current_tile16_data_;

  // Apply the active palette (overworld area if available) to the current
  // tile16 bitmap using sheet-aware offsets.
  void ApplyPaletteToCurrentTile16Bitmap();

  // Handle keyboard shortcuts (shared between Update and UpdateAsPanel)
  void HandleKeyboardShortcuts();

  // Copy current tile16 bitmap pixels into the blockset atlas at the given
  // tile position. Consolidates the repeated 16x16 copy loops.
  void CopyTile16ToAtlas(int tile_id);
};

}  // namespace editor
}  // namespace yaze

#endif  // YAZE_APP_EDITOR_TILE16EDITOR_H
