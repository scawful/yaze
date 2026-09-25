#ifndef YAZE_TEST_UNIT_ZELDA3_GFX_SHEET_TEST_ROM_H
#define YAZE_TEST_UNIT_ZELDA3_GFX_SHEET_TEST_ROM_H

#include <gtest/gtest.h>

#include <cstdint>
#include <map>
#include <vector>

#include "app/gfx/util/compression.h"
#include "rom/rom.h"
#include "zelda3/gfx_sheet_storage.h"

namespace yaze::test {

// A 1 MiB LoROM image laid out like vanilla: 223 sheets packed from PC
// 0x80000 behind the pointer tables at 0x4F80/0x505F/0x513E. Even sheets use
// SlowROM pointers and odd sheets FastROM pointers. PC 0xE0000-0xF0000 is left
// as 0xFF filler for allocation tests.
struct GfxSheetTestRom {
  static constexpr uint32_t kSheetDataStart = 0x80000;
  static constexpr uint32_t kFreeStart = 0xE0000;
  static constexpr uint32_t kFreeEnd = 0xF0000;
  static constexpr size_t kRomSize = 0x100000;

  std::vector<uint8_t> bytes;
  std::map<uint16_t, std::vector<uint8_t>> sheets;  // decoded contents
};

// Deterministic, compressible sheet contents: blank rows, byte ramps and
// sheet-specific noise so every command type appears.
inline std::vector<uint8_t> MakeTestSheetContents(uint16_t sheet_id,
                                                  size_t size) {
  std::vector<uint8_t> data(size);
  uint32_t state = 0x9E3779B9u ^ (sheet_id * 0x85EBCA6Bu);
  for (size_t i = 0; i < size; ++i) {
    state = state * 1664525u + 1013904223u;
    switch ((i / 48) % 4) {
      case 0:
        data[i] = 0x00;
        break;
      case 1:
        data[i] = static_cast<uint8_t>(i + sheet_id);
        break;
      default:
        data[i] = static_cast<uint8_t>(state >> 24);
        break;
    }
  }
  return data;
}

inline void SetTestSheetPointer(std::vector<uint8_t>& rom, uint16_t sheet_id,
                                uint32_t pc, bool fast_rom) {
  uint8_t bank = static_cast<uint8_t>(pc >> 15);
  if (fast_rom) {
    bank |= 0x80;
  }
  const uint16_t addr = static_cast<uint16_t>(0x8000 | (pc & 0x7FFF));
  const zelda3::GfxSheetPointerTables tables;
  rom[tables.bank + sheet_id] = bank;
  rom[tables.high + sheet_id] = static_cast<uint8_t>(addr >> 8);
  rom[tables.low + sheet_id] = static_cast<uint8_t>(addr & 0xFF);
}

// When `alias_2bpp` is set, sheets 221 and 222 share 113 and 114's data, as
// in the vanilla ROM.
inline GfxSheetTestRom BuildGfxSheetTestRom(bool alias_2bpp = false) {
  GfxSheetTestRom out;
  out.bytes.assign(GfxSheetTestRom::kRomSize, 0x00);
  std::fill(out.bytes.begin() + GfxSheetTestRom::kFreeStart,
            out.bytes.begin() + GfxSheetTestRom::kFreeEnd, 0xFF);

  uint32_t cursor = GfxSheetTestRom::kSheetDataStart;
  for (uint16_t sheet = 0; sheet < zelda3::kGfxSheetCount; ++sheet) {
    const auto kind = zelda3::GetGfxSheetStorageKind(sheet);
    if (alias_2bpp && (sheet == 221 || sheet == 222)) {
      const uint16_t source = sheet == 221 ? 113 : 114;
      out.sheets[sheet] = out.sheets[source];
      const zelda3::GfxSheetPointerTables tables;
      out.bytes[tables.bank + sheet] = out.bytes[tables.bank + source];
      out.bytes[tables.high + sheet] = out.bytes[tables.high + source];
      out.bytes[tables.low + sheet] = out.bytes[tables.low + source];
      continue;
    }
    const size_t size = kind == zelda3::GfxSheetStorageKind::kCompressed2bpp
                            ? zelda3::kGfxSheet2bppBytes
                            : zelda3::kGfxSheet3bppBytes;
    auto contents = MakeTestSheetContents(sheet, size);
    std::vector<uint8_t> stored = contents;
    if (kind != zelda3::GfxSheetStorageKind::kRaw3bpp) {
      int encoded_size = 0;
      stored = gfx::HyruleMagicCompress(contents.data(),
                                        static_cast<int>(contents.size()),
                                        &encoded_size, /*flag=*/0);
      stored.resize(encoded_size);
    }
    std::copy(stored.begin(), stored.end(), out.bytes.begin() + cursor);
    SetTestSheetPointer(out.bytes, sheet, cursor, /*fast_rom=*/(sheet & 1));
    out.sheets[sheet] = std::move(contents);
    cursor += static_cast<uint32_t>(stored.size());
  }
  EXPECT_LT(cursor, GfxSheetTestRom::kFreeStart);
  return out;
}

// Decoded contents of every sheet, for "nothing else changed" checks.
inline std::map<uint16_t, std::vector<uint8_t>> ReadAllGfxSheets(
    const Rom& rom) {
  std::map<uint16_t, std::vector<uint8_t>> sheets;
  for (uint16_t sheet = 0; sheet < zelda3::kGfxSheetCount; ++sheet) {
    auto data = zelda3::ReadGfxSheetData(rom, sheet);
    if (data.ok()) {
      sheets[sheet] = std::move(*data);
    }
  }
  return sheets;
}

}  // namespace yaze::test

#endif  // YAZE_TEST_UNIT_ZELDA3_GFX_SHEET_TEST_ROM_H
