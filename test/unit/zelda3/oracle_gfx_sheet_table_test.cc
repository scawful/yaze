#include <gtest/gtest.h>

#include <algorithm>
#include <cstdint>
#include <memory>
#include <utility>
#include <vector>

#include "app/gfx/types/snes_tile.h"
#include "app/gfx/util/compression.h"
#include "rom/rom.h"
#include "rom/snes.h"
#include "unit/zelda3/gfx_sheet_test_rom.h"
#include "zelda3/game_data.h"

namespace yaze::test {
namespace {

constexpr size_t kOracleRomSize = 0x200000;
constexpr uint32_t kHelperSnes = 0x23FC00;
constexpr uint32_t kBankSnes = 0x23FD00;
constexpr uint32_t kHighSnes = 0x23FE00;
constexpr uint32_t kLowSnes = 0x23FF00;
constexpr uint32_t kEmptyStreamPc = 0x11FBF0;
constexpr uint32_t kNewSheetDataPc = 0x108000;
constexpr uint16_t kReplacedSheet = 0x10;
constexpr uint16_t kNewSheet = 0xE8;
constexpr uint16_t kLastSheet = 0xFE;

void PutLong(std::vector<uint8_t>& rom, size_t pc, uint32_t value) {
  rom[pc] = value & 0xFF;
  rom[pc + 1] = (value >> 8) & 0xFF;
  rom[pc + 2] = (value >> 16) & 0xFF;
}

void SetOracleRow(std::vector<uint8_t>& rom, uint16_t row, uint32_t pc) {
  const uint32_t snes = PcToSnes(pc);
  rom[SnesToPc(kBankSnes) + row] = (snes >> 16) & 0xFF;
  rom[SnesToPc(kHighSnes) + row] = (snes >> 8) & 0xFF;
  rom[SnesToPc(kLowSnes) + row] = snes & 0xFF;
}

uint32_t StoreCompressed(std::vector<uint8_t>& rom, uint32_t pc,
                         const std::vector<uint8_t>& contents) {
  int size = 0;
  auto stored = gfx::HyruleMagicCompress(
      contents.data(), static_cast<int>(contents.size()), &size, /*flag=*/0);
  std::copy_n(stored.begin(), size, rom.begin() + pc);
  return pc + static_cast<uint32_t>(size);
}

std::vector<uint8_t> Pixels(const std::vector<uint8_t>& contents) {
  auto pixels = gfx::SnesTo8bppSheet(contents, 3);
  pixels.resize(4096, 0);
  return pixels;
}

struct OracleRom {
  std::vector<uint8_t> bytes;
  std::vector<uint8_t> replaced;
  std::vector<uint8_t> added;
  std::vector<uint8_t> last;
};

// The vanilla-layout fixture, grown to 2 MiB, with Core/gfx_sheet_table.asm
// installed: the $00:E78F reader JSLs to GfxSheet_GetPtr_BgCA, rows $00-$DE
// copy the vanilla table except kReplacedSheet, and rows $DF-$FF point at an
// empty stream except kNewSheet and kLastSheet.
OracleRom BuildOracleRom() {
  OracleRom out;
  out.bytes = BuildGfxSheetTestRom().bytes;
  out.bytes.resize(kOracleRomSize, 0x00);
  auto& rom = out.bytes;

  const uint32_t site = SnesToPc(zelda3::kOracleSheetReaderSnes);
  rom[site] = 0x22;
  PutLong(rom, site + 1, kHelperSnes);
  const uint8_t helper[] = {
      0x08, 0xE2, 0x30, 0xDA, 0xC2, 0x10, 0xBB, 0xBF, 0, 0, 0,    0x85, 0xC8,
      0xBF, 0,    0,    0,    0x85, 0xC9, 0xBF, 0,    0, 0, 0x85, 0xCA};
  const uint32_t helper_pc = SnesToPc(kHelperSnes);
  std::copy(std::begin(helper), std::end(helper), rom.begin() + helper_pc);
  PutLong(rom, helper_pc + 8, kLowSnes);
  PutLong(rom, helper_pc + 14, kHighSnes);
  PutLong(rom, helper_pc + 20, kBankSnes);

  const zelda3::GfxSheetPointerTables vanilla;
  for (uint16_t row = 0; row < zelda3::kLegacyNumGfxSheets; ++row) {
    rom[SnesToPc(kBankSnes) + row] = rom[vanilla.bank + row];
    rom[SnesToPc(kHighSnes) + row] = rom[vanilla.high + row];
    rom[SnesToPc(kLowSnes) + row] = rom[vanilla.low + row];
  }
  const uint8_t empty[] = {0xE7, 0xFF, 0x00, 0xE5, 0xFF, 0x00, 0xFF};
  std::copy(std::begin(empty), std::end(empty), rom.begin() + kEmptyStreamPc);
  for (uint16_t row = zelda3::kLegacyNumGfxSheets; row < zelda3::kMaxGfxSheets;
       ++row) {
    SetOracleRow(rom, row, kEmptyStreamPc);
  }

  out.replaced = MakeTestSheetContents(0x1F0, zelda3::kGfxSheet3bppBytes);
  out.added = MakeTestSheetContents(0x2E8, zelda3::kGfxSheet3bppBytes);
  out.last = MakeTestSheetContents(0x2FE, zelda3::kGfxSheet3bppBytes);
  uint32_t cursor = kNewSheetDataPc;
  for (auto [row, contents] :
       {std::pair{kReplacedSheet, &out.replaced},
        std::pair{kNewSheet, &out.added}, std::pair{kLastSheet, &out.last}}) {
    SetOracleRow(rom, row, cursor);
    cursor = StoreCompressed(rom, cursor, *contents);
  }
  return out;
}

std::vector<uint8_t> BufferSheet(const zelda3::GameData& data, uint16_t sheet) {
  const auto span = data.sheet_store.Sheet(sheet);
  return std::vector<uint8_t>(span.begin(), span.end());
}

TEST(OracleGfxSheetTableTest, LoadsAllRowsFromTheBank23Table) {
  const auto fixture = BuildOracleRom();
  Rom rom;
  ASSERT_TRUE(rom.LoadFromData(fixture.bytes).ok());

  const zelda3::GfxSheetPointerTables vanilla;
  const auto table = zelda3::ResolveGfxSheetPointerTable(
      rom, vanilla.bank, vanilla.high, vanilla.low);
  EXPECT_TRUE(table.oracle_expanded);
  EXPECT_EQ(table.count, zelda3::kNumGfxSheets);  // $FF is the sentinel
  EXPECT_EQ(table.bank_pc, SnesToPc(kBankSnes));
  EXPECT_EQ(table.high_pc, SnesToPc(kHighSnes));
  EXPECT_EQ(table.low_pc, SnesToPc(kLowSnes));

  auto data = std::make_unique<zelda3::GameData>();
  ASSERT_TRUE(zelda3::LoadGraphics(rom, *data).ok());
  EXPECT_TRUE(data->gfx_sheet_table.oracle_expanded);
  EXPECT_EQ(data->graphics_buffer.size(), zelda3::kNumGfxSheets * 4096u);
  EXPECT_EQ(BufferSheet(*data, kReplacedSheet), Pixels(fixture.replaced));
  EXPECT_EQ(BufferSheet(*data, kNewSheet), Pixels(fixture.added));
  EXPECT_EQ(BufferSheet(*data, kLastSheet), Pixels(fixture.last));
  EXPECT_EQ(BufferSheet(*data, 0xE7), std::vector<uint8_t>(4096, 0));
  EXPECT_NE(data->sheet_store.Revision(kNewSheet), 0u);
}

TEST(OracleGfxSheetTableTest, VanillaRomKeeps223Sheets) {
  const auto fixture = BuildGfxSheetTestRom();
  Rom rom;
  ASSERT_TRUE(rom.LoadFromData(fixture.bytes).ok());

  const zelda3::GfxSheetPointerTables vanilla;
  const auto table = zelda3::ResolveGfxSheetPointerTable(
      rom, vanilla.bank, vanilla.high, vanilla.low);
  EXPECT_FALSE(table.oracle_expanded);
  EXPECT_EQ(table.count, zelda3::kLegacyNumGfxSheets);
  EXPECT_EQ(table.bank_pc, vanilla.bank);

  auto data = std::make_unique<zelda3::GameData>();
  ASSERT_TRUE(zelda3::LoadGraphics(rom, *data).ok());
  EXPECT_FALSE(data->gfx_sheet_table.oracle_expanded);
  EXPECT_EQ(data->graphics_buffer.size(), zelda3::kLegacyNumGfxSheets * 4096u);
  EXPECT_EQ(BufferSheet(*data, kReplacedSheet),
            Pixels(fixture.sheets.at(kReplacedSheet)));
  EXPECT_FALSE(data->sheet_store.HasSheet(zelda3::kLegacyNumGfxSheets));
}

}  // namespace
}  // namespace yaze::test
