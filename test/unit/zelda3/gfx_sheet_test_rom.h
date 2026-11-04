#ifndef YAZE_TEST_UNIT_ZELDA3_GFX_SHEET_TEST_ROM_H
#define YAZE_TEST_UNIT_ZELDA3_GFX_SHEET_TEST_ROM_H

#include <gtest/gtest.h>

#include <algorithm>
#include <cstdint>
#include <map>
#include <string>
#include <utility>
#include <vector>

#include "app/gfx/util/compression.h"
#include "rom/rom.h"
#include "util/rom_hash.h"
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

inline void SetTestSheetPointer(
    std::vector<uint8_t>& rom, uint16_t sheet_id, uint32_t pc, bool fast_rom,
    const zelda3::GfxSheetPointerTables& tables = {}) {
  uint8_t bank = static_cast<uint8_t>(pc >> 15);
  if (fast_rom) {
    bank |= 0x80;
  }
  const uint16_t addr = static_cast<uint16_t>(0x8000 | (pc & 0x7FFF));
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

inline zelda3::GfxSheetPointerTables OracleGfxSheetTables() {
  return {0x11FD00, 0x11FE00, 0x11FF00};
}

// Oracle has 256 physical pointer rows, with usable IDs00-FE and sentinelFF.
// Keep the legacy tables distinct so accidentally reading them is observable.
inline GfxSheetTestRom BuildOracleGfxSheetTestRom() {
  auto out = BuildGfxSheetTestRom();
  out.bytes.resize(0x200000, 0);
  out.bytes[0x7FD9] = 1;  // US version, matching the configured table keys.
  const zelda3::GfxSheetPointerTables legacy;
  const auto tables = OracleGfxSheetTables();
  for (const auto& pair : {std::pair{legacy.bank, tables.bank},
                           std::pair{legacy.high, tables.high},
                           std::pair{legacy.low, tables.low}}) {
    std::copy_n(out.bytes.begin() + pair.first, zelda3::kGfxSheetCount,
                out.bytes.begin() + pair.second);
  }
  uint32_t cursor = 0x120000;
  for (uint16_t sheet = 0xDF; sheet <= 0xFE; ++sheet) {
    // A solid native3bpp color makes exact decoded pixels independent of the
    // loader's conversion helper, and differs from a missing-sheet placeholder.
    const uint8_t color = static_cast<uint8_t>(sheet & 7);
    std::vector<uint8_t> contents(0x600);
    for (size_t tile = 0; tile < 64; ++tile) {
      for (size_t y = 0; y < 8; ++y) {
        contents[tile * 24 + y * 2] = color & 1 ? 0xFF : 0;
        contents[tile * 24 + y * 2 + 1] = color & 2 ? 0xFF : 0;
        contents[tile * 24 + 16 + y] = color & 4 ? 0xFF : 0;
      }
    }
    int stored_size = 0;
    auto stored = gfx::HyruleMagicCompress(contents.data(),
                                           static_cast<int>(contents.size()),
                                           &stored_size, /*flag=*/0);
    stored.resize(stored_size);
    std::copy(stored.begin(), stored.end(), out.bytes.begin() + cursor);
    SetTestSheetPointer(out.bytes, sheet, cursor, /*fast_rom=*/(sheet & 1),
                        tables);
    // Give the sentinel a valid stream. It must still remain out of range.
    if (sheet == 0xDF) {
      SetTestSheetPointer(out.bytes, 0xFF, cursor, /*fast_rom=*/true, tables);
    }
    out.sheets[sheet] = std::move(contents);
    cursor += static_cast<uint32_t>(stored.size());
  }
  return out;
}

// The test declares the same complete identity/source contract a project supplies.
// This is fixture metadata, not another resolver or address-tuple inference.
inline std::string ExpandedGfxSheetTestManifest(const Rom& rom) {
  return R"json({"manifest_version":3,"graphics_sheet_regions":{"pointer_layout":{
    "bank_pc":"0x11FD00","high_pc":"0x11FE00","low_pc":"0x11FF00",
    "physical_table_rows":256,"usable_sheet_count":255,"default_sheet_id":255,
    "rom_sha1":")json" +
         util::ComputeSha1Hex(rom.data(), rom.size()) +
         R"json(","rom_role":"development",
    "source_path":"Data/graphics/source_sprite_sheets.json",
    "source_sha1":"0123456789abcdef0123456789abcdef01234567"}}})json";
}

enum class ExpandedDfStreamFailure { kMalformedCopy, kTruncated, kWrongSize };

// Keep the pointer layout valid while corrupting only the DF stream. Callers
// declare the identity AFTER this mutation so hash validation cannot mask it.
inline void CorruptExpandedDfStream(GfxSheetTestRom& fixture,
                                    ExpandedDfStreamFailure failure) {
  const auto tables = OracleGfxSheetTables();
  if (failure == ExpandedDfStreamFailure::kWrongSize) {
    // A valid 2bpp stream (800 bytes) cannot be accepted as DF's 600-byte 3bpp.
    for (uint32_t table : {tables.bank, tables.high, tables.low}) {
      fixture.bytes[table + 0xDF] = fixture.bytes[table + 0x71];
    }
    return;
  }
  const uint32_t pc = static_cast<uint32_t>(
      fixture.bytes.size() -
      (failure == ExpandedDfStreamFailure::kTruncated ? 1 : 4));
  SetTestSheetPointer(fixture.bytes, 0xDF, pc, false, tables);
  if (failure == ExpandedDfStreamFailure::kTruncated) {
    fixture.bytes.back() = 0x20;  // Byte fill missing operand and terminator.
  } else {
    fixture.bytes[pc] =
        0x80;  // Copy from output offset zero before any output.
    fixture.bytes[pc + 1] = 0;
    fixture.bytes[pc + 2] = 0;
    fixture.bytes[pc + 3] = 0xFF;
  }
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
