#ifndef YAZE_ZELDA3_GFX_SHEET_PNG_H
#define YAZE_ZELDA3_GFX_SHEET_PNG_H

#include <array>
#include <cstdint>
#include <utility>
#include <vector>

#include "absl/status/statusor.h"
#include "rom/rom.h"
#include "util/indexed_png.h"

namespace yaze::zelda3 {

struct GameData;

/**
 * @file gfx_sheet_png.h
 * @brief 3bpp graphics sheets to and from indexed PNGs.
 *
 * A sheet is 128x32 pixels: 16 blocks of 16x16 in an 8x2 grid, block b at
 * pixel (b % 8 * 16, b / 8 * 16). A block holds 8x8 tiles n, n+1, n+16, n+17
 * (GfxSheetBlockTiles), so copying a 16x16 pixel region keeps SNES tile
 * order. Exported PNGs lay out their blocks the same way, eight per row.
 */

using SheetColor = std::array<uint8_t, 3>;
/// Colors for 3bpp indices 0-7. Index 0 is exported as transparent.
using SheetPalette = std::array<SheetColor, 8>;

SheetPalette GrayscaleSheetPalette();

/// Background CGRAM row `row` (0-7) of dungeon room `room`: colors 0-7 of the
/// row, the colors a 3bpp tile with that tile palette shows in the room.
absl::StatusOr<SheetPalette> RoomBackgroundSheetPalette(Rom& rom,
                                                        GameData& data,
                                                        int room, int row);

/// Sprite palette `row` (OAM palette 0-7, CGRAM row 8 + row) of room `room`.
absl::StatusOr<SheetPalette> RoomSpriteSheetPalette(Rom& rom, GameData& data,
                                                    int room, int row);

/// Indexed PNG of `block_count` blocks from `first_block` of a 0x600-byte
/// SNES 3bpp sheet.
absl::StatusOr<std::vector<uint8_t>> ExportSheetBlocksPng(
    const std::vector<uint8_t>& snes_3bpp, const SheetPalette& palette,
    int first_block = 0, int block_count = 16);

struct SheetImportResult {
  std::vector<uint8_t> snes_3bpp;   // the edited sheet (0x600 bytes)
  std::vector<int> changed_blocks;  // 16x16 blocks 0-15
  std::vector<int> changed_tiles;   // 8x8 tiles 0-63
  std::vector<std::pair<int, int>> changed_ranges;  // [begin, end) in sheet
};

/**
 * @brief Places a PNG's 16x16 blocks over a sheet from `first_block`.
 *
 * PNG width and height must be multiples of 16; its blocks are read left to
 * right, top to bottom. Indexed PNGs must use indices 0-7. RGB/RGBA PNGs
 * must use exactly the colors in `palette` (alpha 0 or palette[0] is index
 * 0). Any other pixel fails the import and lists the first offenders; no
 * value is clamped.
 */
absl::StatusOr<SheetImportResult> ImportSheetBlocksPng(
    const std::vector<uint8_t>& snes_3bpp, const util::PngImage& png,
    int first_block, const SheetPalette& palette);

// ---------------------------------------------------------------------------
// Dungeon room background sets
// ---------------------------------------------------------------------------

/// The 8 background sheets a dungeon room loads, as Room::LoadRoomGraphics
/// picks them: the main blockset from the room's default entrance (the
/// header blockset byte when there is none), with slots 3-6 replaced by the
/// room blockset (header byte) entries that are non-zero.
struct RoomBackgroundSet {
  int room = 0;
  uint8_t header_blockset = 0;  // room blockset ($0AA2)
  uint8_t main_blockset = 0;    // main blockset ($0AA1)
  std::array<uint8_t, 8> sheets{};
  std::array<bool, 8> from_room_blockset{};
};

RoomBackgroundSet ResolveRoomBackgroundSet(const GameData& data, int room,
                                           uint8_t header_blockset);

/// All 296 rooms' background sets (header bytes read from the ROM).
std::vector<RoomBackgroundSet> ResolveAllRoomBackgroundSets(
    const Rom& rom, const GameData& data);

/// One 128x256 indexed PNG: the room's 8 background sheets stacked in slot
/// order (slot n at y = 32 * n). 2bpp slots are left blank.
absl::StatusOr<std::vector<uint8_t>> ExportRoomBackgroundPng(
    const Rom& rom, const RoomBackgroundSet& set, const SheetPalette& palette);

struct RoomSheetImport {
  uint16_t sheet = 0;
  std::vector<int> slots;  // slots showing this sheet
  SheetImportResult result;
};

/// Splits a 128x256 PNG back into the room's sheets and returns the sheets
/// whose pixels changed. A sheet shown in two slots must be edited the same
/// way in both; 2bpp slots must stay unchanged.
absl::StatusOr<std::vector<RoomSheetImport>> ImportRoomBackgroundPng(
    const Rom& rom, const RoomBackgroundSet& set, const util::PngImage& png,
    const SheetPalette& palette);

}  // namespace yaze::zelda3

#endif  // YAZE_ZELDA3_GFX_SHEET_PNG_H
