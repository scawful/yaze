#include "zelda3/gfx_sheet_storage.h"

#include <algorithm>
#include <utility>

#include "absl/strings/str_format.h"
#include "absl/strings/str_join.h"
#include "app/gfx/util/compression.h"
#include "rom/snes.h"
#include "util/log.h"
#include "util/macro.h"

namespace yaze::zelda3 {
namespace {

// Measuring an existing stream must not fail just because an older tool
// wrote an oversized sheet; the doctor reports that separately.
constexpr size_t kMeasureDecodeLimit = 0x8000;

size_t ExpectedDecodedSize(GfxSheetStorageKind kind) {
  return kind == GfxSheetStorageKind::kCompressed2bpp ? kGfxSheet2bppBytes
                                                      : kGfxSheet3bppBytes;
}

bool Overlaps(uint32_t a_begin, uint32_t a_end, uint32_t b_begin,
              uint32_t b_end) {
  return a_begin < b_end && b_begin < a_end;
}

// Records the original bytes of every range it writes and restores them on
// destruction unless Commit() is called, so a failed write-back leaves the
// ROM buffer exactly as it was.
class RomWriteJournal {
 public:
  explicit RomWriteJournal(Rom& rom) : rom_(rom) {}
  RomWriteJournal(const RomWriteJournal&) = delete;
  RomWriteJournal& operator=(const RomWriteJournal&) = delete;

  ~RomWriteJournal() {
    if (committed_) {
      return;
    }
    auto& bytes = rom_.mutable_vector();
    for (auto it = entries_.rbegin(); it != entries_.rend(); ++it) {
      std::copy(it->second.begin(), it->second.end(),
                bytes.begin() + it->first);
    }
  }

  absl::Status Write(uint32_t pc, const std::vector<uint8_t>& data) {
    if (static_cast<size_t>(pc) + data.size() > rom_.size()) {
      return absl::OutOfRangeError(absl::StrFormat(
          "Write of %zu bytes at 0x%06X is past the ROM end", data.size(), pc));
    }
    std::vector<uint8_t> original(rom_.vector().begin() + pc,
                                  rom_.vector().begin() + pc + data.size());
    RETURN_IF_ERROR(rom_.WriteVector(static_cast<int>(pc), data));
    entries_.emplace_back(pc, std::move(original));
    return absl::OkStatus();
  }

  void Commit() { committed_ = true; }

 private:
  Rom& rom_;
  std::vector<std::pair<uint32_t, std::vector<uint8_t>>> entries_;
  bool committed_ = false;
};

absl::Status CheckPolicyWrite(const GfxSheetWritePolicy& policy, uint32_t begin,
                              uint32_t end) {
  if (!policy.check_write) {
    return absl::OkStatus();
  }
  return policy.check_write(begin, end);
}

struct OtherSheetExtents {
  std::vector<GfxSheetExtent> extents;
  std::vector<uint16_t> unreadable;
};

OtherSheetExtents CollectOtherExtents(const Rom& rom, uint16_t skip_sheet,
                                      const GfxSheetPointerTables& tables) {
  OtherSheetExtents result;
  for (uint16_t sheet = 0; sheet < kGfxSheetCount; ++sheet) {
    if (sheet == skip_sheet) {
      continue;
    }
    auto extent = ReadGfxSheetExtent(rom, sheet, tables);
    if (extent.ok()) {
      result.extents.push_back(*extent);
    } else {
      result.unreadable.push_back(sheet);
    }
  }
  return result;
}

absl::StatusOr<uint32_t> FindFreeSpace(const Rom& rom,
                                       const GfxSheetWritePolicy& policy,
                                       const OtherSheetExtents& others,
                                       const GfxSheetPointerTables& tables,
                                       size_t size) {
  std::vector<GfxSheetPcRange> occupied;
  occupied.reserve(others.extents.size() + 3);
  for (const auto& extent : others.extents) {
    occupied.push_back(
        {extent.pc, static_cast<uint32_t>(extent.pc + extent.stored_size)});
  }
  for (uint32_t table : {tables.bank, tables.high, tables.low}) {
    occupied.push_back({table, static_cast<uint32_t>(table + kGfxSheetCount)});
  }
  std::sort(
      occupied.begin(), occupied.end(),
      [](const auto& lhs, const auto& rhs) { return lhs.begin < rhs.begin; });

  std::vector<GfxSheetPcRange> regions = policy.allocation_regions;
  std::sort(
      regions.begin(), regions.end(),
      [](const auto& lhs, const auto& rhs) { return lhs.begin < rhs.begin; });
  size_t largest_gap = 0;
  for (const auto& region : regions) {
    if (region.begin >= region.end || region.end > rom.size()) {
      return absl::InvalidArgumentError(absl::StrFormat(
          "Graphics allocation region [0x%06X, 0x%06X) is empty or past the "
          "ROM end (0x%06zX)",
          region.begin, region.end, rom.size()));
    }
    uint32_t cursor = region.begin;
    auto try_gap = [&](uint32_t gap_end) -> bool {
      if (gap_end > cursor) {
        largest_gap = std::max<size_t>(largest_gap, gap_end - cursor);
        return gap_end - cursor >= size;
      }
      return false;
    };
    for (const auto& used : occupied) {
      if (used.end <= cursor || used.begin >= region.end) {
        continue;
      }
      if (try_gap(std::min(used.begin, region.end))) {
        return cursor;
      }
      cursor = std::max(cursor, used.end);
      if (cursor >= region.end) {
        break;
      }
    }
    if (cursor < region.end && try_gap(region.end)) {
      return cursor;
    }
  }
  return absl::ResourceExhaustedError(absl::StrFormat(
      "No registered graphics free space fits %zu bytes (largest free gap: "
      "%zu bytes)",
      size, largest_gap));
}

absl::Status VerifyCompressedSheet(const Rom& rom, uint16_t sheet_id,
                                   uint32_t expected_pc,
                                   const std::vector<uint8_t>& expected,
                                   size_t expected_stored_size,
                                   const GfxSheetPointerTables& tables) {
  ASSIGN_OR_RETURN(const uint32_t pc, ReadGfxSheetPc(rom, sheet_id, tables));
  if (pc != expected_pc) {
    return absl::DataLossError(absl::StrFormat(
        "Sheet 0x%02X pointer reads back as 0x%06X, expected 0x%06X", sheet_id,
        pc, expected_pc));
  }
  auto decoded =
      gfx::lc_lz2::DecompressExact(rom.data(), rom.size(), pc, expected.size(),
                                   /*big_endian_copy=*/false);
  if (!decoded.ok()) {
    return absl::DataLossError(
        absl::StrFormat("Sheet 0x%02X does not decode after write: %s",
                        sheet_id, decoded.status().message()));
  }
  if (decoded->data != expected ||
      decoded->compressed_size != expected_stored_size) {
    return absl::DataLossError(absl::StrFormat(
        "Sheet 0x%02X read-back differs from the edited sheet", sheet_id));
  }
  return absl::OkStatus();
}

}  // namespace

std::array<int, 4> GfxSheetBlockTiles(int block) {
  const int top_left = (block / 8) * 32 + (block % 8) * 2;
  return {top_left, top_left + 1, top_left + 16, top_left + 17};
}

bool IsGfxSheetBlockEmpty(const std::vector<uint8_t>& data, int block,
                          int bpp) {
  const size_t tile_bytes = static_cast<size_t>(8 * bpp);
  for (int tile : GfxSheetBlockTiles(block)) {
    const size_t begin = static_cast<size_t>(tile) * tile_bytes;
    if (begin + tile_bytes > data.size()) {
      return false;
    }
    for (size_t i = begin; i < begin + tile_bytes; ++i) {
      if (data[i] != 0) {
        return false;
      }
    }
  }
  return true;
}

GfxSheetStorageKind GetGfxSheetStorageKind(uint16_t sheet_id) {
  if (sheet_id >= 115 && sheet_id <= 126) {
    return GfxSheetStorageKind::kRaw3bpp;
  }
  if (sheet_id == 113 || sheet_id == 114 || sheet_id >= 218) {
    return GfxSheetStorageKind::kCompressed2bpp;
  }
  return GfxSheetStorageKind::kCompressed3bpp;
}

const char* GfxSheetStorageKindName(GfxSheetStorageKind kind) {
  switch (kind) {
    case GfxSheetStorageKind::kCompressed3bpp:
      return "compressed 3bpp";
    case GfxSheetStorageKind::kRaw3bpp:
      return "raw 3bpp";
    case GfxSheetStorageKind::kCompressed2bpp:
      return "compressed 2bpp";
  }
  return "unknown";
}

absl::StatusOr<uint32_t> ReadGfxSheetPc(const Rom& rom, uint16_t sheet_id,
                                        const GfxSheetPointerTables& tables) {
  if (sheet_id >= kGfxSheetCount) {
    return absl::InvalidArgumentError(
        absl::StrFormat("Graphics sheet %d is out of range (0-%zu)", sheet_id,
                        kGfxSheetCount - 1));
  }
  for (uint32_t table : {tables.bank, tables.high, tables.low}) {
    if (static_cast<size_t>(table) + kGfxSheetCount > rom.size()) {
      return absl::OutOfRangeError(absl::StrFormat(
          "Graphics pointer table at 0x%06X is past the ROM end", table));
    }
  }
  const uint32_t snes = (rom.data()[tables.bank + sheet_id] << 16) |
                        (rom.data()[tables.high + sheet_id] << 8) |
                        rom.data()[tables.low + sheet_id];
  if ((snes & 0xFFFF) < 0x8000) {
    return absl::DataLossError(
        absl::StrFormat("Sheet 0x%02X pointer $%06X is not a LoROM ROM address",
                        sheet_id, snes));
  }
  const uint32_t pc = SnesToPc(snes);
  if (pc >= rom.size()) {
    return absl::OutOfRangeError(absl::StrFormat(
        "Sheet 0x%02X pointer $%06X (PC 0x%06X) is past the ROM end", sheet_id,
        snes, pc));
  }
  return pc;
}

absl::StatusOr<GfxSheetExtent> ReadGfxSheetExtent(
    const Rom& rom, uint16_t sheet_id, const GfxSheetPointerTables& tables) {
  GfxSheetExtent extent;
  extent.sheet_id = sheet_id;
  extent.kind = GetGfxSheetStorageKind(sheet_id);
  ASSIGN_OR_RETURN(extent.pc, ReadGfxSheetPc(rom, sheet_id, tables));

  if (extent.kind == GfxSheetStorageKind::kRaw3bpp) {
    if (static_cast<size_t>(extent.pc) + kGfxSheet3bppBytes > rom.size()) {
      return absl::OutOfRangeError(
          absl::StrFormat("Raw sheet 0x%02X at 0x%06X runs past the ROM end",
                          sheet_id, extent.pc));
    }
    extent.stored_size = kGfxSheet3bppBytes;
    extent.decoded_size = kGfxSheet3bppBytes;
    return extent;
  }

  ASSIGN_OR_RETURN(auto decoded,
                   gfx::lc_lz2::DecompressExact(rom.data(), rom.size(),
                                                extent.pc, kMeasureDecodeLimit,
                                                /*big_endian_copy=*/false));
  extent.stored_size = decoded.compressed_size;
  extent.decoded_size = decoded.data.size();
  return extent;
}

absl::StatusOr<std::vector<uint8_t>> ReadGfxSheetData(
    const Rom& rom, uint16_t sheet_id, const GfxSheetPointerTables& tables) {
  ASSIGN_OR_RETURN(const GfxSheetExtent extent,
                   ReadGfxSheetExtent(rom, sheet_id, tables));
  if (extent.kind == GfxSheetStorageKind::kRaw3bpp) {
    return std::vector<uint8_t>(
        rom.vector().begin() + extent.pc,
        rom.vector().begin() + extent.pc + kGfxSheet3bppBytes);
  }
  ASSIGN_OR_RETURN(auto decoded,
                   gfx::lc_lz2::DecompressExact(rom.data(), rom.size(),
                                                extent.pc, kMeasureDecodeLimit,
                                                /*big_endian_copy=*/false));
  return std::move(decoded.data);
}

absl::StatusOr<GfxSheetWriteResult> WriteGfxSheet(
    Rom& rom, uint16_t sheet_id, const std::vector<uint8_t>& snes_3bpp,
    const GfxSheetWritePolicy& policy, const GfxSheetPointerTables& tables) {
  if (!rom.is_loaded()) {
    return absl::FailedPreconditionError("ROM not loaded");
  }
  if (sheet_id >= kGfxSheetCount) {
    return absl::InvalidArgumentError(
        absl::StrFormat("Graphics sheet %d is out of range (0-%zu)", sheet_id,
                        kGfxSheetCount - 1));
  }
  if (policy.reserved_sheets.count(sheet_id) != 0) {
    return absl::FailedPreconditionError(absl::StrFormat(
        "Graphics sheet 0x%02X is reserved by the project and must not be "
        "written",
        sheet_id));
  }
  const GfxSheetStorageKind kind = GetGfxSheetStorageKind(sheet_id);
  if (kind == GfxSheetStorageKind::kCompressed2bpp) {
    return absl::UnimplementedError(absl::StrFormat(
        "Graphics sheet 0x%02X is 2bpp; 2bpp sheets are read-only", sheet_id));
  }
  if (snes_3bpp.size() != kGfxSheet3bppBytes) {
    return absl::InvalidArgumentError(absl::StrFormat(
        "Sheet 0x%02X data is %zu bytes; a 3bpp sheet is exactly 0x%zX",
        sheet_id, snes_3bpp.size(), kGfxSheet3bppBytes));
  }

  if (auto reserved = policy.reserved_blocks.find(sheet_id);
      reserved != policy.reserved_blocks.end() && !reserved->second.empty()) {
    auto current = ReadGfxSheetData(rom, sheet_id, tables);
    if (!current.ok() || current->size() < kGfxSheet3bppBytes) {
      return absl::FailedPreconditionError(absl::StrFormat(
          "Sheet 0x%02X has reserved 16x16 blocks but its current data "
          "cannot be read to protect them",
          sheet_id));
    }
    constexpr size_t kTileBytes = 24;
    for (uint16_t block : reserved->second) {
      for (int tile : GfxSheetBlockTiles(block)) {
        const size_t begin = static_cast<size_t>(tile) * kTileBytes;
        if (begin + kTileBytes > kGfxSheet3bppBytes ||
            !std::equal(snes_3bpp.begin() + begin,
                        snes_3bpp.begin() + begin + kTileBytes,
                        current->begin() + begin)) {
          return absl::FailedPreconditionError(absl::StrFormat(
              "Sheet 0x%02X 16x16 block %d is reserved by the project and "
              "must keep its pixels",
              sheet_id, block));
        }
      }
    }
  }

  ASSIGN_OR_RETURN(const uint32_t old_pc,
                   ReadGfxSheetPc(rom, sheet_id, tables));
  GfxSheetWriteResult result;
  result.sheet_id = sheet_id;
  result.old_pc = old_pc;
  RomWriteJournal journal(rom);

  if (kind == GfxSheetStorageKind::kRaw3bpp) {
    const uint32_t end = old_pc + static_cast<uint32_t>(kGfxSheet3bppBytes);
    RETURN_IF_ERROR(CheckPolicyWrite(policy, old_pc, end));
    RETURN_IF_ERROR(journal.Write(old_pc, snes_3bpp));
    ASSIGN_OR_RETURN(auto readback, ReadGfxSheetData(rom, sheet_id, tables));
    if (readback != snes_3bpp) {
      return absl::DataLossError(absl::StrFormat(
          "Raw sheet 0x%02X read-back differs from the edited sheet",
          sheet_id));
    }
    journal.Commit();
    result.new_pc = old_pc;
    result.old_stored_size = kGfxSheet3bppBytes;
    result.new_stored_size = kGfxSheet3bppBytes;
    return result;
  }

  // Graphics streams use little-endian LZ source addresses (flag 0); the
  // big-endian form is for overworld map data.
  int encoded_size = 0;
  std::vector<uint8_t> encoded = gfx::HyruleMagicCompress(
      snes_3bpp.data(), static_cast<int>(snes_3bpp.size()), &encoded_size,
      /*flag=*/0);
  encoded.resize(encoded_size);
  {
    auto check = gfx::lc_lz2::DecompressExact(encoded.data(), encoded.size(), 0,
                                              kGfxSheet3bppBytes,
                                              /*big_endian_copy=*/false);
    if (!check.ok() || check->data != snes_3bpp ||
        check->compressed_size != encoded.size()) {
      return absl::InternalError(absl::StrFormat(
          "Encoding sheet 0x%02X did not round-trip; nothing was written",
          sheet_id));
    }
  }
  result.new_stored_size = encoded.size();

  const OtherSheetExtents others = CollectOtherExtents(rom, sheet_id, tables);
  auto old_extent = ReadGfxSheetExtent(rom, sheet_id, tables);
  bool fits_in_place = false;
  if (old_extent.ok()) {
    result.old_stored_size = old_extent->stored_size;
    fits_in_place = encoded.size() <= old_extent->stored_size;
    const uint32_t old_end =
        old_pc + static_cast<uint32_t>(old_extent->stored_size);
    for (const auto& other : others.extents) {
      // Another sheet sharing these bytes would change with this write.
      if (Overlaps(old_pc, old_end, other.pc,
                   other.pc + static_cast<uint32_t>(other.stored_size))) {
        fits_in_place = false;
        break;
      }
    }
  }

  uint32_t target = old_pc;
  if (!fits_in_place) {
    if (policy.allocation_regions.empty()) {
      return absl::FailedPreconditionError(absl::StrFormat(
          "Sheet 0x%02X needs %zu bytes but its current stream holds %zu, "
          "and no graphics free space is registered "
          "(hack manifest graphics_sheet_regions.allocation_regions)",
          sheet_id, encoded.size(), result.old_stored_size));
    }
    if (!others.unreadable.empty()) {
      std::vector<std::string> ids;
      for (uint16_t id : others.unreadable) {
        ids.push_back(absl::StrFormat("0x%02X", id));
      }
      return absl::FailedPreconditionError(absl::StrFormat(
          "Cannot compute graphics free space: sheet(s) %s do not decode",
          absl::StrJoin(ids, ", ")));
    }
    ASSIGN_OR_RETURN(
        target, FindFreeSpace(rom, policy, others, tables, encoded.size()));
  }

  RETURN_IF_ERROR(CheckPolicyWrite(
      policy, target, target + static_cast<uint32_t>(encoded.size())));
  RETURN_IF_ERROR(journal.Write(target, encoded));

  if (target != old_pc) {
    const uint8_t old_bank = rom.data()[tables.bank + sheet_id];
    uint8_t bank = static_cast<uint8_t>(target >> 15);
    // Keep the sheet's FastROM form, and never point into the WRAM banks.
    if ((old_bank & 0x80) != 0 || bank >= 0x7E) {
      bank |= 0x80;
    }
    const uint16_t addr = static_cast<uint16_t>(0x8000 | (target & 0x7FFF));
    const std::pair<uint32_t, uint8_t> pointer_bytes[] = {
        {tables.bank + sheet_id, bank},
        {tables.high + sheet_id, static_cast<uint8_t>(addr >> 8)},
        {tables.low + sheet_id, static_cast<uint8_t>(addr & 0xFF)}};
    for (const auto& [pc, value] : pointer_bytes) {
      RETURN_IF_ERROR(CheckPolicyWrite(policy, pc, pc + 1));
      RETURN_IF_ERROR(journal.Write(pc, {value}));
    }
    result.placement = GfxSheetPlacement::kRelocated;
  }
  result.new_pc = target;

  RETURN_IF_ERROR(VerifyCompressedSheet(rom, sheet_id, target, snes_3bpp,
                                        encoded.size(), tables));
  journal.Commit();
  LOG_INFO("GfxSheetStorage", "Wrote sheet %02X: %zu bytes %s at 0x%06X",
           sheet_id, encoded.size(),
           result.placement == GfxSheetPlacement::kRelocated ? "relocated"
                                                             : "in place",
           target);
  return result;
}

}  // namespace yaze::zelda3
