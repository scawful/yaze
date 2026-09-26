#ifndef YAZE_ZELDA3_GFX_SHEET_STORAGE_H
#define YAZE_ZELDA3_GFX_SHEET_STORAGE_H

#include <array>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <map>
#include <set>
#include <string>
#include <vector>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "rom/rom.h"

namespace yaze::zelda3 {

/**
 * @file gfx_sheet_storage.h
 * @brief Locates graphics sheets in the ROM and writes edited sheets back.
 *
 * Sheet N is addressed by three byte tables (bank, high, low) at PC 0x4F80,
 * 0x505F and 0x513E. Sheets 115-126 are raw 3bpp (0x600 bytes). Sheets
 * 113-114 and 218+ are LC-LZ2 compressed 2bpp. Every other sheet is LC-LZ2
 * compressed 3bpp that must decode to exactly 0x600 bytes: the game
 * decompresses sprite sheets to $7F:4000 and $7F:4600, so a longer stream
 * overwrites the neighbouring sheet in WRAM.
 */

constexpr size_t kGfxSheetCount = 223;
constexpr size_t kGfxSheet3bppBytes = 0x600;
constexpr size_t kGfxSheet2bppBytes = 0x800;

enum class GfxSheetStorageKind {
  kCompressed3bpp,
  kRaw3bpp,
  kCompressed2bpp,
};

GfxSheetStorageKind GetGfxSheetStorageKind(uint16_t sheet_id);
const char* GfxSheetStorageKindName(GfxSheetStorageKind kind);

/// PC offsets of the bank, high and low byte pointer tables.
struct GfxSheetPointerTables {
  uint32_t bank = 0x4F80;
  uint32_t high = 0x505F;
  uint32_t low = 0x513E;
};

/// Half-open PC range [begin, end).
struct GfxSheetPcRange {
  uint32_t begin = 0;
  uint32_t end = 0;
};

/// Where one sheet's bytes live and how many bytes they occupy.
struct GfxSheetExtent {
  uint16_t sheet_id = 0;
  GfxSheetStorageKind kind = GfxSheetStorageKind::kCompressed3bpp;
  uint32_t pc = 0;
  // Bytes the sheet occupies at pc (compressed size including the 0xFF
  // terminator, or 0x600 for raw sheets).
  size_t stored_size = 0;
  // Bytes the sheet expands to.
  size_t decoded_size = 0;
};

/// Resolves a sheet's pointer. Does not read the sheet data.
absl::StatusOr<uint32_t> ReadGfxSheetPc(
    const Rom& rom, uint16_t sheet_id,
    const GfxSheetPointerTables& tables = {});

/// Resolves a sheet's pointer and measures its stored and decoded size.
/// Fails when a compressed stream does not decode.
absl::StatusOr<GfxSheetExtent> ReadGfxSheetExtent(
    const Rom& rom, uint16_t sheet_id,
    const GfxSheetPointerTables& tables = {});

/// Returns the sheet's decoded bytes (SNES planar data).
absl::StatusOr<std::vector<uint8_t>> ReadGfxSheetData(
    const Rom& rom, uint16_t sheet_id,
    const GfxSheetPointerTables& tables = {});

/// 16x16 blocks per sheet: 8 columns x 2 rows over the 128x32 sheet.
constexpr int kGfxSheetBlockCount = 16;

/// The four 8x8 tile indices of 16x16 block `block` (row-major, 0-15): top
/// left, top right, bottom left, bottom right. A sheet row holds 16 tiles.
std::array<int, 4> GfxSheetBlockTiles(int block);

/// True when all four 8x8 tiles of the block are zero in `data` (SNES planar
/// sheet bytes, 8 * bpp bytes per tile). Blocks past the data are not empty.
bool IsGfxSheetBlockEmpty(const std::vector<uint8_t>& data, int block,
                          int bpp = 3);

/// Rules a sheet write must follow.
struct GfxSheetWritePolicy {
  // Free space a sheet may move to when its new stream no longer fits.
  // Bytes in these ranges that no sheet pointer references are free.
  // Empty means relocation is refused.
  std::vector<GfxSheetPcRange> allocation_regions;
  // Sheets that must never be written.
  std::set<uint16_t> reserved_sheets;
  // 16x16 blocks (0-15) per sheet whose pixels must not change.
  std::map<uint16_t, std::vector<uint16_t>> reserved_blocks;
  // Returns an error when [begin, end) must not be written (for example a
  // hack-manifest protected region). Unset means no extra check.
  std::function<absl::Status(uint32_t begin, uint32_t end)> check_write;
};

enum class GfxSheetPlacement { kInPlace, kRelocated };

struct GfxSheetWriteResult {
  uint16_t sheet_id = 0;
  GfxSheetPlacement placement = GfxSheetPlacement::kInPlace;
  uint32_t old_pc = 0;
  uint32_t new_pc = 0;
  // 0 when the old stream could not be measured.
  size_t old_stored_size = 0;
  size_t new_stored_size = 0;
};

/**
 * @brief Writes one edited 3bpp sheet to the ROM buffer.
 *
 * @param snes_3bpp exactly 0x600 bytes of SNES planar sheet data.
 *
 * Raw sheets are written in place. Compressed sheets are encoded with
 * little-endian LZ addresses, verified by decoding the encoded bytes, then
 * written in place when they fit the old stream and no other sheet shares
 * those bytes. Otherwise they move to the first free fit in
 * policy.allocation_regions and all three pointer bytes are updated (keeping
 * the sheet's FastROM/SlowROM bank form). The written sheet is read back and
 * compared; on any failure after the first byte is written, every touched
 * byte is restored. 2bpp sheets and reserved sheets are refused.
 */
absl::StatusOr<GfxSheetWriteResult> WriteGfxSheet(
    Rom& rom, uint16_t sheet_id, const std::vector<uint8_t>& snes_3bpp,
    const GfxSheetWritePolicy& policy,
    const GfxSheetPointerTables& tables = {});

}  // namespace yaze::zelda3

#endif  // YAZE_ZELDA3_GFX_SHEET_STORAGE_H
