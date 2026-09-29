#ifndef YAZE_APP_ZELDA3_SCREEN_MENU_TILEMAP_H
#define YAZE_APP_ZELDA3_SCREEN_MENU_TILEMAP_H

#include <array>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <string>
#include <vector>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "app/gfx/types/snes_color.h"
#include "app/gfx/types/snes_tile.h"

namespace yaze::zelda3 {

// Core (no-ImGui) model for a 2bpp SNES BG tilemap used by Oracle of
// Secrets' custom menu system (BG3). Files are little-endian arrays of
// 16-bit "vhopppcc cccccccc" words -- see gfx::TileInfo / WordToTileInfo /
// TileInfoToWord in app/gfx/types/snes_tile.h, which this class reuses
// directly for the per-cell codec.
//
// Full maps are 2048 bytes (32x32 words). Some Oracle files only cover the
// rows they actually use: quest_icons.tilemap is 128 bytes (32x2 words),
// hud.tilemap is 384 bytes (32x6 words). The map is always 32 words wide;
// row count = size / 64 bytes. Any other size is rejected by
// ValidateSize()/LoadFromBytes() with a descriptive error, and Save()
// never changes the file's byte size.
class MenuTilemapDocument {
 public:
  static constexpr int kCols = 32;
  static constexpr int kBytesPerRow = kCols * 2;  // 64
  static constexpr int kMaxRows = 32;
  static constexpr int kMaxBytes = kBytesPerRow * kMaxRows;  // 2048

  // A rectangular selection expressed in cell (word) coordinates.
  struct Rect {
    int row = 0;
    int col = 0;
    int rows = 0;
    int cols = 0;
  };

  // A rectangular block of cells captured for copy/paste, independent of
  // where it came from.
  struct Clipboard {
    int rows = 0;
    int cols = 0;
    std::vector<gfx::TileInfo> cells;  // row-major, rows*cols entries
  };

  // Renders one 2bpp pixel (0-3) of `tile_id` at local (local_x, local_y)
  // in [0,8). Implementations should return 0 for tiles outside whatever
  // CHR source they're backed by rather than throwing/crashing.
  using ChrPixelFn =
      std::function<uint8_t(int tile_id, int local_x, int local_y)>;

  MenuTilemapDocument() = default;

  // Returns OkStatus() iff a buffer of this size would be an accepted
  // menu tilemap (32-word-wide, whole number of rows, 64 <= size <= 2048).
  static absl::Status ValidateSize(size_t size);

  // Loads from an in-memory buffer. `source_path` is remembered as the
  // document's path (used by Save()/Revert()/ExternalChangeDetected());
  // pass "" to load without an associated file (e.g. for tests).
  absl::Status LoadFromBytes(std::vector<uint8_t> bytes,
                             std::string source_path = "");
  absl::Status LoadFromFile(const std::string& path);

  // Writes the current bytes to disk via a temp-file + rename so a reader
  // never observes a partially-written file. Before the *first* successful
  // save performed by this document instance, the pristine (pre-edit)
  // bytes captured at load time are written to "<dest>.bak" (only once;
  // later saves do not touch the backup). Fails (no file is touched) if
  // the encoded size would differ from `bytes_.size()` at load time, or if
  // no path is set (use SaveAs instead).
  absl::Status Save();
  absl::Status SaveAs(const std::string& path);

  // Reloads bytes_ from path_, discarding in-memory edits. Clears the
  // dirty flag; does not touch the backup-written flag.
  absl::Status Revert();

  // Compares the file at path_ on disk against the content this document
  // last loaded from or saved to it (never the unsaved in-memory edits).
  // Returns true iff the bytes differ. Content-based on purpose: size and
  // mtime cannot see a same-size rewrite inside one timestamp tick, and a
  // touch that leaves the bytes alone is not a change. Returns an error if
  // path_ is empty or the file is missing/unreadable.
  absl::StatusOr<bool> ExternalChangeDetected() const;

  // "Keep my edits" after an external change: adopts the file's current
  // on-disk content as the baseline (so ExternalChangeDetected() stops
  // reporting it and the next save's .bak captures what it overwrites)
  // while keeping the in-memory bytes. dirty() becomes "differs from the
  // file on disk".
  absl::Status AcknowledgeExternalChange();

  int rows() const { return rows_; }
  int cols() const { return kCols; }
  const std::string& path() const { return path_; }
  bool dirty() const { return dirty_; }
  bool loaded() const { return rows_ > 0; }
  bool has_backup() const { return backup_written_; }

  // Replaces the in-memory bytes (same size only) for undo/redo. Unlike
  // LoadFromBytes() it keeps the file baseline, backup bookkeeping and path,
  // and recomputes dirty() as "differs from the file as last loaded/saved".
  absl::Status RestoreBytes(std::vector<uint8_t> bytes);
  const std::string& backup_path() const { return backup_path_; }
  const std::vector<uint8_t>& raw_bytes() const { return bytes_; }

  bool InBounds(int row, int col) const {
    return row >= 0 && row < rows_ && col >= 0 && col < kCols;
  }

  // Out-of-bounds reads return a zeroed TileInfo (tile 0, palette 0, no
  // flags); out-of-bounds writes are a no-op (Set* return false).
  gfx::TileInfo GetCell(int row, int col) const;
  uint16_t GetCellWord(int row, int col) const;
  bool SetCell(int row, int col, gfx::TileInfo info);
  bool SetCellWord(int row, int col, uint16_t word);

  // All rect operations clip `rect` to the document bounds first, so
  // callers may pass selections that hang off the edge. Returns the
  // number of cells actually touched.
  Clipboard CopyRect(Rect rect) const;
  int PasteRect(int row, int col, const Clipboard& clip);
  int FillRect(Rect rect, gfx::TileInfo info);
  int EraseRect(Rect rect, uint16_t erase_word = 0x0000);

  // Renders to an "indexed" image: width_px = kCols*8 (256), height_px =
  // rows()*8. Each pixel value is `subpalette*4 + color` in [0,31], where
  // `color` (0-3) comes from `chr` and `subpalette` (0-7) plus the cell's
  // H/V flip are applied from the encoded TileInfo. Color 0 within a
  // subpalette is left as index `subpalette*4` -- see
  // ComposeMenuTilemapRgba() for how that becomes transparent.
  std::vector<uint8_t> RenderIndexed(const ChrPixelFn& chr) const;
  int render_width() const { return kCols * 8; }
  int render_height() const { return rows_ * 8; }

 private:
  void ClipRect(Rect& rect) const;

  std::vector<uint8_t> bytes_;
  // The file's content as last read by Load*() or written by Save*(): the
  // baseline for dirty() after RestoreBytes(), the source of the one-time
  // ".bak", and what ExternalChangeDetected() compares the disk against.
  std::vector<uint8_t> pristine_bytes_;
  int rows_ = 0;
  std::string path_;
  bool dirty_ = false;
  bool backup_written_ = false;
  std::string backup_path_;
};

// Composites an indexed image (as produced by RenderIndexed) into RGBA8.
// `subpalette_colors[p*4 + c]` is the color for subpalette p, color c;
// c == 0 in every subpalette is always transparent (alpha 0), matching
// the SNES BG convention the menu tilemap uses (see
// docs/public/usage/screen-editor.md for the Oracle-specific +1 CGRAM
// mapping that produces this table). Output is width*height*4 bytes.
std::vector<uint8_t> ComposeMenuTilemapRgba(
    const std::vector<uint8_t>& indexed, int width, int height,
    const std::array<gfx::SnesColor, 32>& subpalette_colors);

// Builds a ChrPixelFn over an 8bpp-indexed tilesheet buffer laid out the
// way gfx::SnesTo8bppSheet() produces it: chunks of 128 tiles (16 cols x
// 8 rows of 8x8 tiles, 8192 bytes/chunk), tile_id / 128 selects the
// chunk. This matches both zelda3::Load2BppGraphics()'s concatenated
// 7-sheet buffer and a raw .bin chr file decoded the same way (see
// ResolveMenuChrFromFile() in menu_tilemap_sources.h). The returned
// function borrows `sheet_8bpp_indexed` by reference and is only valid
// while that vector is alive.
MenuTilemapDocument::ChrPixelFn MakeChrPixelFn(
    const std::vector<uint8_t>& sheet_8bpp_indexed);

}  // namespace yaze::zelda3

#endif  // YAZE_APP_ZELDA3_SCREEN_MENU_TILEMAP_H
