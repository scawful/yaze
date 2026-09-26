#ifndef YAZE_APP_EDITOR_GRAPHICS_SHEET_PNG_TRANSFER_H
#define YAZE_APP_EDITOR_GRAPHICS_SHEET_PNG_TRANSFER_H

#include <cstdint>
#include <string>
#include <vector>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "zelda3/gfx_sheet_png.h"

namespace yaze {
class Rom;

namespace editor {

class UndoManager;
class GraphicsEditorState;

/**
 * @file sheet_png_transfer.h
 * @brief PNG export and import for the Graphics editor.
 *
 * These work on 128x32 sheets with one index 0-7 per pixel, including
 * unsaved pixel edits. An import writes the session's GraphicsSheetStore
 * (which refreshes the Arena copy) and marks the sheet modified; the ROM is
 * written by GraphicsEditor::Save, which re-encodes the sheet in place or
 * relocates it with rollback.
 */

/// Indexed PNG of a 3bpp sheet, laid out like `z3ed gfx-export` (16x16
/// blocks, 8 per row). Refuses 2bpp sheets and pixels above index 7.
absl::StatusOr<std::vector<uint8_t>> ExportSheetPixelsPng(
    uint16_t sheet_id, const std::vector<uint8_t>& indexed_pixels,
    const zelda3::SheetPalette& palette);

/// One sheet's pixels after an import, and what changed.
struct SheetPngImportPreview {
  uint16_t sheet_id = 0;
  std::vector<uint8_t> indexed_pixels;  // 128x32, the sheet after import
  std::vector<int> changed_blocks;      // 16x16 blocks 0-15
  std::vector<int> changed_tiles;       // 8x8 tiles 0-63
};

/// Places a PNG over a sheet from block 0 without changing anything. The PNG
/// must be a multiple of 16 pixels on each side and fit in the sheet;
/// indexed PNGs must use indices 0-7, RGB PNGs exactly the palette's colors.
/// Offending pixels are listed in the error, never clamped.
absl::StatusOr<SheetPngImportPreview> PreviewSheetPngImport(
    uint16_t sheet_id, const std::vector<uint8_t>& indexed_pixels,
    const std::vector<uint8_t>& png_bytes, const zelda3::SheetPalette& palette);

/// Splits a 128x256 room background PNG (`z3ed gfx-room-export` layout) into
/// the room's sheets, read from the ROM. Returns only sheets that change.
absl::StatusOr<std::vector<SheetPngImportPreview>> PreviewRoomPngImport(
    const Rom& rom, const zelda3::RoomBackgroundSet& set,
    const std::vector<uint8_t>& png_bytes, const zelda3::SheetPalette& palette);

/// Writes a previewed import through the session's sheet store, marks the
/// sheet modified and pushes one undoable step. Refuses when this ROM's
/// graphics are not loaded or another open ROM's sheets are on screen.
absl::Status ApplySheetPngImport(const SheetPngImportPreview& preview,
                                 GraphicsEditorState& state,
                                 UndoManager* undo_manager);

/// "Sheet 0x20: blocks 0, 3 (5 tiles)" for the preview list.
std::string DescribeSheetPngImport(const SheetPngImportPreview& preview);

}  // namespace editor
}  // namespace yaze

#endif  // YAZE_APP_EDITOR_GRAPHICS_SHEET_PNG_TRANSFER_H
