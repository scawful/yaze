#include "zelda3/gfx_sheet_storage.h"

#include <gtest/gtest.h>

#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <random>
#include <vector>

#include "absl/status/status.h"
#include "rom/rom.h"
#include "unique_temp_path.h"
#include "unit/zelda3/gfx_sheet_test_rom.h"

namespace yaze::test {
namespace {

using zelda3::GfxSheetPlacement;
using zelda3::GfxSheetWritePolicy;

Rom LoadTestRom(const std::vector<uint8_t>& bytes) {
  Rom rom;
  EXPECT_TRUE(rom.LoadFromData(bytes).ok());
  return rom;
}

GfxSheetWritePolicy PolicyWithFreeSpace() {
  GfxSheetWritePolicy policy;
  policy.allocation_regions.push_back(
      {GfxSheetTestRom::kFreeStart, GfxSheetTestRom::kFreeEnd});
  return policy;
}

// Noise does not compress, so the new stream is larger than the old one.
std::vector<uint8_t> IncompressibleSheet(uint32_t seed) {
  std::mt19937 rng(seed);
  std::vector<uint8_t> data(zelda3::kGfxSheet3bppBytes);
  for (auto& byte : data) {
    byte = static_cast<uint8_t>(rng());
  }
  return data;
}

void ExpectOtherSheetsUnchanged(
    const Rom& rom, const std::map<uint16_t, std::vector<uint8_t>>& before,
    std::initializer_list<uint16_t> changed) {
  const auto after = ReadAllGfxSheets(rom);
  ASSERT_EQ(after.size(), before.size());
  for (const auto& [sheet, data] : before) {
    if (std::find(changed.begin(), changed.end(), sheet) != changed.end()) {
      continue;
    }
    EXPECT_EQ(after.at(sheet), data) << "sheet " << sheet;
  }
}

TEST(GfxSheetStorageTest, ClassifiesSheetKinds) {
  using zelda3::GfxSheetStorageKind;
  EXPECT_EQ(zelda3::GetGfxSheetStorageKind(0),
            GfxSheetStorageKind::kCompressed3bpp);
  EXPECT_EQ(zelda3::GetGfxSheetStorageKind(112),
            GfxSheetStorageKind::kCompressed3bpp);
  EXPECT_EQ(zelda3::GetGfxSheetStorageKind(113),
            GfxSheetStorageKind::kCompressed2bpp);
  EXPECT_EQ(zelda3::GetGfxSheetStorageKind(115), GfxSheetStorageKind::kRaw3bpp);
  EXPECT_EQ(zelda3::GetGfxSheetStorageKind(126), GfxSheetStorageKind::kRaw3bpp);
  EXPECT_EQ(zelda3::GetGfxSheetStorageKind(127),
            GfxSheetStorageKind::kCompressed3bpp);
  EXPECT_EQ(zelda3::GetGfxSheetStorageKind(217),
            GfxSheetStorageKind::kCompressed3bpp);
  EXPECT_EQ(zelda3::GetGfxSheetStorageKind(218),
            GfxSheetStorageKind::kCompressed2bpp);
}

TEST(GfxSheetStorageTest, ReadsEveryTestSheet) {
  const auto fixture = BuildGfxSheetTestRom();
  const Rom rom = LoadTestRom(fixture.bytes);
  const auto sheets = ReadAllGfxSheets(rom);
  ASSERT_EQ(sheets.size(), zelda3::kGfxSheetCount);
  for (const auto& [sheet, data] : sheets) {
    EXPECT_EQ(data, fixture.sheets.at(sheet)) << "sheet " << sheet;
  }
}

TEST(GfxSheetStorageTest, SmallerSheetIsWrittenInPlace) {
  const auto fixture = BuildGfxSheetTestRom();
  Rom rom = LoadTestRom(fixture.bytes);
  const auto before = ReadAllGfxSheets(rom);
  auto old_extent = zelda3::ReadGfxSheetExtent(rom, 0x20);
  ASSERT_TRUE(old_extent.ok());

  std::vector<uint8_t> edited(zelda3::kGfxSheet3bppBytes, 0x00);
  edited[0x40] = 0x7E;
  auto result = zelda3::WriteGfxSheet(rom, 0x20, edited, PolicyWithFreeSpace());
  ASSERT_TRUE(result.ok()) << result.status();

  EXPECT_EQ(result->placement, GfxSheetPlacement::kInPlace);
  EXPECT_EQ(result->new_pc, old_extent->pc);
  EXPECT_LE(result->new_stored_size, old_extent->stored_size);
  auto readback = zelda3::ReadGfxSheetData(rom, 0x20);
  ASSERT_TRUE(readback.ok());
  EXPECT_EQ(*readback, edited);
  ExpectOtherSheetsUnchanged(rom, before, {0x20});
  // Nothing past the old stream changed.
  for (uint32_t pc = old_extent->pc + old_extent->stored_size;
       pc < fixture.bytes.size(); ++pc) {
    ASSERT_EQ(rom.vector()[pc], fixture.bytes[pc]) << "pc " << pc;
  }
}

TEST(GfxSheetStorageTest, GrownSheetRelocatesAndUpdatesAllPointerBytes) {
  const auto fixture = BuildGfxSheetTestRom();
  Rom rom = LoadTestRom(fixture.bytes);
  const auto before = ReadAllGfxSheets(rom);
  const zelda3::GfxSheetPointerTables tables;

  for (uint16_t sheet : {0x20, 0x21}) {  // SlowROM, then FastROM pointer
    SCOPED_TRACE(sheet);
    auto old_extent = zelda3::ReadGfxSheetExtent(rom, sheet);
    ASSERT_TRUE(old_extent.ok());
    const uint8_t old_bank = rom.vector()[tables.bank + sheet];
    const auto edited = IncompressibleSheet(sheet);

    auto result =
        zelda3::WriteGfxSheet(rom, sheet, edited, PolicyWithFreeSpace());
    ASSERT_TRUE(result.ok()) << result.status();

    EXPECT_EQ(result->placement, GfxSheetPlacement::kRelocated);
    EXPECT_GT(result->new_stored_size, old_extent->stored_size);
    EXPECT_GE(result->new_pc, GfxSheetTestRom::kFreeStart);
    EXPECT_LE(result->new_pc + result->new_stored_size,
              GfxSheetTestRom::kFreeEnd);

    const uint8_t bank = rom.vector()[tables.bank + sheet];
    EXPECT_EQ(bank & 0x80, old_bank & 0x80);
    EXPECT_EQ(bank & 0x7F, result->new_pc >> 15);
    EXPECT_EQ(rom.vector()[tables.high + sheet],
              (0x80 | ((result->new_pc >> 8) & 0x7F)));
    EXPECT_EQ(rom.vector()[tables.low + sheet], result->new_pc & 0xFF);

    auto readback = zelda3::ReadGfxSheetData(rom, sheet);
    ASSERT_TRUE(readback.ok());
    EXPECT_EQ(*readback, edited);
  }

  // The second relocation did not land on the first one.
  auto first = zelda3::ReadGfxSheetExtent(rom, 0x20);
  auto second = zelda3::ReadGfxSheetExtent(rom, 0x21);
  ASSERT_TRUE(first.ok());
  ASSERT_TRUE(second.ok());
  EXPECT_GE(second->pc, first->pc + first->stored_size);
  ExpectOtherSheetsUnchanged(rom, before, {0x20, 0x21});
}

TEST(GfxSheetStorageTest, RelocatedSheetCanBeRewrittenInPlace) {
  const auto fixture = BuildGfxSheetTestRom();
  Rom rom = LoadTestRom(fixture.bytes);
  auto grown = zelda3::WriteGfxSheet(rom, 0x30, IncompressibleSheet(1),
                                     PolicyWithFreeSpace());
  ASSERT_TRUE(grown.ok()) << grown.status();
  ASSERT_EQ(grown->placement, GfxSheetPlacement::kRelocated);

  // Blank tail: the new stream is shorter than the relocated one.
  auto smaller = IncompressibleSheet(2);
  std::fill(smaller.end() - 0x100, smaller.end(), 0x00);
  auto again = zelda3::WriteGfxSheet(rom, 0x30, smaller, PolicyWithFreeSpace());
  ASSERT_TRUE(again.ok()) << again.status();
  EXPECT_EQ(again->placement, GfxSheetPlacement::kInPlace);
  EXPECT_EQ(again->new_pc, grown->new_pc);
}

TEST(GfxSheetStorageTest, RefusalsLeaveTheRomUntouched) {
  const auto fixture = BuildGfxSheetTestRom();

  struct Case {
    const char* name;
    uint16_t sheet;
    std::vector<uint8_t> data;
    GfxSheetWritePolicy policy;
    absl::StatusCode code;
  };
  GfxSheetWritePolicy reserved = PolicyWithFreeSpace();
  reserved.reserved_sheets.insert(0x7B);
  GfxSheetWritePolicy tiny;
  tiny.allocation_regions.push_back(
      {GfxSheetTestRom::kFreeStart, GfxSheetTestRom::kFreeStart + 0x40});
  GfxSheetWritePolicy protected_all = PolicyWithFreeSpace();
  protected_all.check_write = [](uint32_t, uint32_t) {
    return absl::FailedPreconditionError("protected");
  };
  GfxSheetWritePolicy past_end;
  past_end.allocation_regions.push_back(
      {GfxSheetTestRom::kFreeStart, GfxSheetTestRom::kRomSize + 0x100});

  const std::vector<Case> cases = {
      {"no free space registered", 0x20, IncompressibleSheet(3),
       GfxSheetWritePolicy{}, absl::StatusCode::kFailedPrecondition},
      {"free space too small", 0x20, IncompressibleSheet(3), tiny,
       absl::StatusCode::kResourceExhausted},
      {"reserved sheet", 0x7B,
       std::vector<uint8_t>(zelda3::kGfxSheet3bppBytes, 0), reserved,
       absl::StatusCode::kFailedPrecondition},
      {"2bpp sheet", 113, std::vector<uint8_t>(zelda3::kGfxSheet3bppBytes, 0),
       PolicyWithFreeSpace(), absl::StatusCode::kUnimplemented},
      {"wrong size", 0x20, std::vector<uint8_t>(0x800, 0),
       PolicyWithFreeSpace(), absl::StatusCode::kInvalidArgument},
      {"protected in place", 0x20,
       std::vector<uint8_t>(zelda3::kGfxSheet3bppBytes, 0), protected_all,
       absl::StatusCode::kFailedPrecondition},
      {"protected raw", 115,
       std::vector<uint8_t>(zelda3::kGfxSheet3bppBytes, 0), protected_all,
       absl::StatusCode::kFailedPrecondition},
      {"region past ROM end", 0x20, IncompressibleSheet(3), past_end,
       absl::StatusCode::kInvalidArgument},
  };
  for (const auto& test_case : cases) {
    SCOPED_TRACE(test_case.name);
    Rom rom = LoadTestRom(fixture.bytes);
    auto result = zelda3::WriteGfxSheet(rom, test_case.sheet, test_case.data,
                                        test_case.policy);
    EXPECT_EQ(result.status().code(), test_case.code) << result.status();
    EXPECT_EQ(rom.vector(), fixture.bytes);
  }
}

TEST(GfxSheetStorageTest, ReservedBlocksMustKeepTheirPixels) {
  const auto fixture = BuildGfxSheetTestRom();
  Rom rom = LoadTestRom(fixture.bytes);
  GfxSheetWritePolicy policy = PolicyWithFreeSpace();
  policy.reserved_blocks[0x20] = {3};

  // Changing a pixel outside block 3 is allowed.
  auto edited = fixture.sheets.at(0x20);
  const int free_tile = zelda3::GfxSheetBlockTiles(0)[0];
  edited[free_tile * 24] ^= 0xFF;
  ASSERT_TRUE(zelda3::WriteGfxSheet(rom, 0x20, edited, policy).ok());

  // Changing one byte of block 3's bottom-right tile is refused.
  const int reserved_tile = zelda3::GfxSheetBlockTiles(3)[3];
  edited[reserved_tile * 24 + 5] ^= 0x01;
  const auto before = rom.vector();
  auto refused = zelda3::WriteGfxSheet(rom, 0x20, edited, policy);
  EXPECT_EQ(refused.status().code(), absl::StatusCode::kFailedPrecondition);
  EXPECT_NE(std::string(refused.status().message()).find("block 3"),
            std::string::npos);
  EXPECT_EQ(rom.vector(), before);
}

TEST(GfxSheetStorageTest, RawSheetWritesExactlyItsOwnBytes) {
  const auto fixture = BuildGfxSheetTestRom();
  Rom rom = LoadTestRom(fixture.bytes);
  auto extent = zelda3::ReadGfxSheetExtent(rom, 116);
  ASSERT_TRUE(extent.ok());
  const auto edited = IncompressibleSheet(116);

  auto result = zelda3::WriteGfxSheet(rom, 116, edited, GfxSheetWritePolicy{});
  ASSERT_TRUE(result.ok()) << result.status();
  EXPECT_EQ(result->placement, GfxSheetPlacement::kInPlace);
  EXPECT_EQ(result->new_stored_size, zelda3::kGfxSheet3bppBytes);

  for (uint32_t pc = 0; pc < fixture.bytes.size(); ++pc) {
    const bool inside =
        pc >= extent->pc && pc < extent->pc + zelda3::kGfxSheet3bppBytes;
    const uint8_t expected =
        inside ? edited[pc - extent->pc] : fixture.bytes[pc];
    ASSERT_EQ(rom.vector()[pc], expected) << "pc " << pc;
  }
}

TEST(GfxSheetStorageTest, AliasedSheetMovesInsteadOfChangingItsTwin) {
  const auto fixture = BuildGfxSheetTestRom(/*alias_2bpp=*/false);
  auto bytes = fixture.bytes;
  // Point 3bpp sheet 0x41 at sheet 0x40's data.
  const zelda3::GfxSheetPointerTables tables;
  for (uint32_t table : {tables.bank, tables.high, tables.low}) {
    bytes[table + 0x41] = bytes[table + 0x40];
  }
  Rom rom = LoadTestRom(bytes);
  const auto before = ReadAllGfxSheets(rom);

  std::vector<uint8_t> edited(zelda3::kGfxSheet3bppBytes, 0x00);
  auto result = zelda3::WriteGfxSheet(rom, 0x41, edited, PolicyWithFreeSpace());
  ASSERT_TRUE(result.ok()) << result.status();
  EXPECT_EQ(result->placement, GfxSheetPlacement::kRelocated);
  EXPECT_EQ(*zelda3::ReadGfxSheetData(rom, 0x40), before.at(0x40));
  EXPECT_EQ(*zelda3::ReadGfxSheetData(rom, 0x41), edited);
}

TEST(GfxSheetStorageTest, VerifiedWriteSurvivesSaveAndReopen) {
  const auto fixture = BuildGfxSheetTestRom();
  Rom rom = LoadTestRom(fixture.bytes);
  const auto edited = IncompressibleSheet(0x55);
  ASSERT_TRUE(
      zelda3::WriteGfxSheet(rom, 0x55, edited, PolicyWithFreeSpace()).ok());

  const auto path = UniqueTempPath("gfx_sheet_reopen", ".sfc");
  {
    std::ofstream out(path, std::ios::binary);
    out.write(reinterpret_cast<const char*>(rom.vector().data()),
              static_cast<std::streamsize>(rom.vector().size()));
  }
  Rom reopened;
  ASSERT_TRUE(reopened.LoadFromFile(path.string()).ok());
  std::filesystem::remove(path);

  auto readback = zelda3::ReadGfxSheetData(reopened, 0x55);
  ASSERT_TRUE(readback.ok());
  EXPECT_EQ(*readback, edited);
}

// On a copy of the Oracle RC ROM: grow a real sheet past its stored size,
// relocate it into test-declared space past the end of the image, save,
// reopen, and prove every other sheet decodes to exactly what it did before.
TEST(GfxSheetStorageRomTest, OracleSheetGrowsRelocatesAndReopens) {
  const char* env = std::getenv("YAZE_TEST_ROM_OOS");
  if (env == nullptr || !std::filesystem::exists(env)) {
    GTEST_SKIP() << "Set YAZE_TEST_ROM_OOS to an Oracle ROM";
  }
  std::ifstream file(env, std::ios::binary);
  std::vector<uint8_t> bytes((std::istreambuf_iterator<char>(file)),
                             std::istreambuf_iterator<char>());
  const size_t original_size = bytes.size();
  // Test-only space, so the check never depends on which ROM bytes the
  // hack happens to leave unused.
  const uint32_t free_begin =
      static_cast<uint32_t>((original_size + 0x7FFF) & ~0x7FFFu);
  bytes.resize(free_begin + 0x8000, 0xFF);

  Rom rom;
  ASSERT_TRUE(rom.LoadFromData(bytes).ok());
  const auto before = ReadAllGfxSheets(rom);
  ASSERT_EQ(before.size(), zelda3::kGfxSheetCount);

  constexpr uint16_t kSheet = 0x20;
  auto old_extent = zelda3::ReadGfxSheetExtent(rom, kSheet);
  ASSERT_TRUE(old_extent.ok());
  GfxSheetWritePolicy policy;
  policy.allocation_regions.push_back({free_begin, free_begin + 0x8000});
  policy.reserved_sheets = {0x7B, 0x7C};
  const auto edited = IncompressibleSheet(2026);

  auto result = zelda3::WriteGfxSheet(rom, kSheet, edited, policy);
  ASSERT_TRUE(result.ok()) << result.status();
  EXPECT_EQ(result->placement, GfxSheetPlacement::kRelocated);
  EXPECT_GT(result->new_stored_size, old_extent->stored_size);
  EXPECT_EQ(result->new_pc, free_begin);
  // The original image is untouched except for the three pointer bytes.
  const zelda3::GfxSheetPointerTables tables;
  for (size_t pc = 0; pc < original_size; ++pc) {
    if (pc == tables.bank + kSheet || pc == tables.high + kSheet ||
        pc == tables.low + kSheet) {
      continue;
    }
    ASSERT_EQ(rom.vector()[pc], bytes[pc]) << "pc " << pc;
  }

  EXPECT_EQ(zelda3::WriteGfxSheet(rom, 0x7B, edited, policy).status().code(),
            absl::StatusCode::kFailedPrecondition);

  const auto path = UniqueTempPath("oracle_gfx_sheet_reopen", ".sfc");
  {
    std::ofstream out(path, std::ios::binary);
    out.write(reinterpret_cast<const char*>(rom.vector().data()),
              static_cast<std::streamsize>(rom.vector().size()));
  }
  Rom reopened;
  ASSERT_TRUE(reopened.LoadFromFile(path.string()).ok());
  std::filesystem::remove(path);
  EXPECT_EQ(*zelda3::ReadGfxSheetData(reopened, kSheet), edited);
  ExpectOtherSheetsUnchanged(reopened, before, {kSheet});
}

}  // namespace
}  // namespace yaze::test
