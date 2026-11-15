#include "zelda3/dungeon/dungeon_wall_private_sheet.h"

#include <algorithm>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

#include "app/gfx/types/snes_tile.h"
#include "app/gfx/util/compression.h"
#include "core/source_artifact_publisher.h"
#include "gtest/gtest.h"
#include "nlohmann/json.hpp"
#include "rom/rom.h"
#include "unique_temp_path.h"

namespace yaze::zelda3 {
namespace {

namespace fs = std::filesystem;
using Json = nlohmann::json;

constexpr uint32_t kWsp1Snes = 0x44E100;
constexpr uint32_t kArmoryStreamSnes = 0x458000;
constexpr uint32_t kPatrolSlot3Snes = 0x458800;
constexpr uint32_t kPatrolSlot6Snes = 0x459000;

size_t SnesToPc(uint32_t snes) {
  return ((snes >> 16) & 0x7F) * 0x8000 + (snes & 0x7FFF);
}

std::vector<uint8_t> CompressSheet(const std::vector<uint8_t>& sheet) {
  int size = 0;
  auto encoded = gfx::HyruleMagicCompress(
      sheet.data(), static_cast<int>(sheet.size()), &size, /*flag=*/0);
  encoded.resize(size);
  return encoded;
}

void WriteLe16(std::vector<uint8_t>& rom, size_t pc, uint16_t value) {
  rom[pc] = static_cast<uint8_t>(value & 0xFF);
  rom[pc + 1] = static_cast<uint8_t>((value >> 8) & 0xFF);
}

void WriteSnes24(std::vector<uint8_t>& rom, size_t pc, uint32_t snes) {
  rom[pc] = static_cast<uint8_t>(snes & 0xFF);
  rom[pc + 1] = static_cast<uint8_t>((snes >> 8) & 0xFF);
  rom[pc + 2] = static_cast<uint8_t>((snes >> 16) & 0xFF);
}

void WriteWsp1Record(std::vector<uint8_t>& rom, size_t record_pc, uint16_t room,
                     uint8_t kind, uint8_t request, uint8_t slot, uint8_t half,
                     uint32_t source, uint8_t profile) {
  WriteLe16(rom, record_pc, room);
  rom[record_pc + 2] = kind;
  rom[record_pc + 3] = request;
  rom[record_pc + 4] = slot;
  rom[record_pc + 5] = half;
  WriteSnes24(rom, record_pc + 6, source);
  WriteLe16(rom, record_pc + 9,
            static_cast<uint16_t>(kWallPrivateDecoded3bppBytes));
  rom[record_pc + 11] = profile;
}

// Synthetic ROM with room54 shared pair + room64 independent BG08/6F streams.
Rom MakeMultiBgWsp1Rom() {
  std::vector<uint8_t> bytes(0x230000, 0);
  const auto armory =
      CompressSheet(std::vector<uint8_t>(kWallPrivateDecoded3bppBytes, 0x21));
  const auto patrol3 =
      CompressSheet(std::vector<uint8_t>(kWallPrivateDecoded3bppBytes, 0x42));
  const auto patrol6 =
      CompressSheet(std::vector<uint8_t>(kWallPrivateDecoded3bppBytes, 0x63));
  const size_t armory_pc = SnesToPc(kArmoryStreamSnes);
  const size_t patrol3_pc = SnesToPc(kPatrolSlot3Snes);
  const size_t patrol6_pc = SnesToPc(kPatrolSlot6Snes);
  std::copy(armory.begin(), armory.end(), bytes.begin() + armory_pc);
  std::copy(patrol3.begin(), patrol3.end(), bytes.begin() + patrol3_pc);
  std::copy(patrol6.begin(), patrol6.end(), bytes.begin() + patrol6_pc);

  const size_t meta = SnesToPc(kWsp1Snes);
  bytes[meta + 0] = 'W';
  bytes[meta + 1] = 'S';
  bytes[meta + 2] = 'P';
  bytes[meta + 3] = '1';
  bytes[meta + 4] = 1;
  bytes[meta + 5] = 12;
  WriteLe16(bytes, meta + 6, 4);
  WriteWsp1Record(bytes, meta + 8, 0x54, 0, 0x08, 3, 1, kArmoryStreamSnes, 4);
  WriteWsp1Record(bytes, meta + 20, 0x54, 1, 0x36, 3, 0, kArmoryStreamSnes, 4);
  WriteWsp1Record(bytes, meta + 32, 0x64, 0, 0x08, 3, 1, kPatrolSlot3Snes, 5);
  WriteWsp1Record(bytes, meta + 44, 0x64, 0, 0x6F, 6, 0, kPatrolSlot6Snes, 5);

  Rom rom;
  EXPECT_TRUE(rom.LoadFromData(std::move(bytes)).ok());
  return rom;
}

std::vector<uint8_t> MakeSynthetic3bpp() {
  // One valid 3bpp sheet: 128 tiles * 24 bytes.
  return std::vector<uint8_t>(kWallPrivateDecoded3bppBytes, 0x11);
}

class WallPrivateSheetTest : public ::testing::Test {
 protected:
  void SetUp() override {
    directory_ = test::UniqueTempPath("wall_private_sheet");
    fs::create_directories(directory_);
    sheet_ = directory_ / "prison_armory.slot3.3bpp";
    kit_ = directory_ / "prison_armory.kit.json";
    const auto bytes = MakeSynthetic3bpp();
    {
      std::ofstream out(sheet_, std::ios::binary);
      out.write(reinterpret_cast<const char*>(bytes.data()), bytes.size());
    }
    digest_ = core::ComputeSourceArtifactSha256(
        std::string(reinterpret_cast<const char*>(bytes.data()), bytes.size()));
    WriteKit(digest_);
  }

  void WriteKit(const std::string& digest) {
    Json kit = {{"kit", "prison_armory"},
                {"profile", "prison_armory"},
                {"private_row", "0x19"},
                {"rooms", {"054"}},
                {"sheets",
                 {{{"slot", 3},
                   {"sheet", "0x08"},
                   {"copy_of", "0x08"},
                   {"source", "prison_armory.slot3.txt"},
                   {"decoded_sha256", digest},
                   {"private_pointer", true},
                   {"sprite_relative", "0x36"}}}}};
    std::ofstream(kit_) << kit.dump(1);
  }

  fs::path directory_;
  fs::path sheet_;
  fs::path kit_;
  std::string digest_;
};

TEST_F(WallPrivateSheetTest, PositiveContextAppliesPrivateOverrides) {
  auto binding = LoadWallPrivateSheetBinding(kit_, sheet_);
  ASSERT_TRUE(binding.ok()) << binding.status();
  WallPrivateSheetContext context{.room = 0x54,
                                  .profile = 4,
                                  .blockset = 0x19,
                                  .dungeon = 10,
                                  .indoor = true};
  auto resolved = ResolveWallPrivateSheet(*binding, context);
  ASSERT_TRUE(resolved.ok()) << resolved.status();
  EXPECT_TRUE(resolved->applied);
  EXPECT_EQ(resolved->sheet_overrides_8bpp.size(), 2u);
  EXPECT_EQ(resolved->sheet_overrides_8bpp.count(0x08), 1u);
  EXPECT_EQ(resolved->sheet_overrides_8bpp.count(0xA9), 1u);
  EXPECT_EQ(resolved->decoded_sha256, digest_);
}

TEST_F(WallPrivateSheetTest, NegativeWrongRoomRefuses) {
  auto binding = LoadWallPrivateSheetBinding(kit_, sheet_);
  ASSERT_TRUE(binding.ok());
  WallPrivateSheetContext context{.room = 0x55,
                                  .profile = 4,
                                  .blockset = 0x19,
                                  .dungeon = 10,
                                  .indoor = true};
  auto resolved = ResolveWallPrivateSheet(*binding, context);
  ASSERT_TRUE(resolved.ok());
  EXPECT_FALSE(resolved->applied);
  EXPECT_TRUE(resolved->sheet_overrides_8bpp.empty());
}

TEST_F(WallPrivateSheetTest, NegativeWrongProfileRefuses) {
  auto binding = LoadWallPrivateSheetBinding(kit_, sheet_);
  ASSERT_TRUE(binding.ok());
  WallPrivateSheetContext context{.room = 0x54,
                                  .profile = 3,
                                  .blockset = 0x19,
                                  .dungeon = 10,
                                  .indoor = true};
  auto resolved = ResolveWallPrivateSheet(*binding, context);
  ASSERT_TRUE(resolved.ok());
  EXPECT_FALSE(resolved->applied);
}

TEST_F(WallPrivateSheetTest, NegativeWrongBlocksetRefuses) {
  auto binding = LoadWallPrivateSheetBinding(kit_, sheet_);
  ASSERT_TRUE(binding.ok());
  WallPrivateSheetContext context{.room = 0x54,
                                  .profile = 4,
                                  .blockset = 0x01,
                                  .dungeon = 10,
                                  .indoor = true};
  auto resolved = ResolveWallPrivateSheet(*binding, context);
  ASSERT_TRUE(resolved.ok());
  EXPECT_FALSE(resolved->applied);
}

TEST_F(WallPrivateSheetTest, RecipeDryRunMatchesFurniturePins) {
  auto binding = LoadWallPrivateSheetBinding(kit_, sheet_);
  ASSERT_TRUE(binding.ok());
  Json contract = {{"bindings",
                    {{{"object_id", "0x32"},
                      {"subtype", "0x0D"},
                      {"file", "prison_armory_worktable.bin"},
                      {"sha256", std::string(64, 'a')},
                      {"position_tile8", {13, 14}}},
                     {{"object_id", "0x32"},
                      {"subtype", "0x0E"},
                      {"file", "prison_armory_drawers.bin"},
                      {"sha256", std::string(64, 'b')},
                      {"position_tile8", {6, 9}}}}}};
  ASSERT_TRUE(AttachWallPrivateSheetObjectBindings(&*binding, contract).ok());
  Json plan = {
      {"version", 1},
      {"room", 84},
      {"edits",
       {{{"op", "replace"},
         {"object",
          {{"id", 50}, {"x", 13}, {"y", 14}, {"size", 13}, {"list", 0}}}},
        {{"op", "append"},
         {"object",
          {{"id", 50}, {"x", 6}, {"y", 9}, {"size", 14}, {"list", 0}}}}}}};
  auto recipe = DryRunWallPrivateSheetRecipe(*binding, plan);
  ASSERT_TRUE(recipe.ok()) << recipe.status();
  EXPECT_TRUE(recipe->accepted);
  EXPECT_EQ(recipe->matched_assets.size(), 2u);
}

TEST_F(WallPrivateSheetTest, MismatchedSheetHashRefusesLoad) {
  WriteKit(std::string(64, '0'));
  auto binding = LoadWallPrivateSheetBinding(kit_, sheet_);
  EXPECT_TRUE(absl::IsFailedPrecondition(binding.status()));
}

TEST(CompiledWallPrivateSheetTest, Room54PairedRecordsStillShareOneStream) {
  Rom rom = MakeMultiBgWsp1Rom();
  auto resolved = ResolveCompiledWallPrivateSheet(rom, 0x54, 4);
  ASSERT_TRUE(resolved.ok()) << resolved.status();
  EXPECT_TRUE(resolved->applied);
  EXPECT_EQ(resolved->sheet_overrides_8bpp.size(), 2u);
  EXPECT_EQ(resolved->sheet_overrides_8bpp.count(0x08), 1u);
  EXPECT_EQ(resolved->sheet_overrides_8bpp.count(0xA9), 1u);
  EXPECT_EQ(resolved->sheet_overrides_8bpp.count(0x6F), 0u);
  EXPECT_EQ(resolved->bg_sheet, 0x08);
  EXPECT_EQ(resolved->sprite_sheet, 0xA9);
  EXPECT_EQ(resolved->sheet_overrides_8bpp.at(0x08),
            resolved->sheet_overrides_8bpp.at(0xA9));
  EXPECT_FALSE(resolved->decoded_sha256.empty());
}

TEST(CompiledWallPrivateSheetTest,
     Room64AppliesIndependentBgSlot3AndSlot6WithoutSprite) {
  Rom rom = MakeMultiBgWsp1Rom();
  auto resolved = ResolveCompiledWallPrivateSheet(rom, 0x64, 5);
  ASSERT_TRUE(resolved.ok()) << resolved.status();
  EXPECT_TRUE(resolved->applied);
  EXPECT_EQ(resolved->sheet_overrides_8bpp.size(), 2u);
  EXPECT_EQ(resolved->sheet_overrides_8bpp.count(0x08), 1u);
  EXPECT_EQ(resolved->sheet_overrides_8bpp.count(0x6F), 1u);
  EXPECT_EQ(resolved->sheet_overrides_8bpp.count(0xA9), 0u);
  EXPECT_EQ(resolved->bg_sheet, 0x08);
  EXPECT_EQ(resolved->sprite_sheet, 0);
  EXPECT_NE(resolved->sheet_overrides_8bpp.at(0x08),
            resolved->sheet_overrides_8bpp.at(0x6F));
}

TEST(CompiledWallPrivateSheetTest, WrongOrInactiveProfileAppliesNeither) {
  Rom rom = MakeMultiBgWsp1Rom();
  auto wrong = ResolveCompiledWallPrivateSheet(rom, 0x64, 4);
  ASSERT_TRUE(wrong.ok()) << wrong.status();
  EXPECT_FALSE(wrong->applied);
  EXPECT_TRUE(wrong->sheet_overrides_8bpp.empty());

  auto inactive = ResolveCompiledWallPrivateSheet(rom, 0x64, 0);
  ASSERT_TRUE(inactive.ok()) << inactive.status();
  EXPECT_FALSE(inactive->applied);
  EXPECT_TRUE(inactive->sheet_overrides_8bpp.empty());
}

TEST(CompiledWallPrivateSheetTest, ConflictingSourcesOnPairedSlotRefuse) {
  std::vector<uint8_t> bytes(0x230000, 0);
  const auto armory =
      CompressSheet(std::vector<uint8_t>(kWallPrivateDecoded3bppBytes, 0x21));
  const auto other =
      CompressSheet(std::vector<uint8_t>(kWallPrivateDecoded3bppBytes, 0x42));
  std::copy(armory.begin(), armory.end(),
            bytes.begin() + SnesToPc(kArmoryStreamSnes));
  std::copy(other.begin(), other.end(),
            bytes.begin() + SnesToPc(kPatrolSlot3Snes));
  const size_t meta = SnesToPc(kWsp1Snes);
  bytes[meta + 0] = 'W';
  bytes[meta + 1] = 'S';
  bytes[meta + 2] = 'P';
  bytes[meta + 3] = '1';
  bytes[meta + 4] = 1;
  bytes[meta + 5] = 12;
  WriteLe16(bytes, meta + 6, 2);
  WriteWsp1Record(bytes, meta + 8, 0x54, 0, 0x08, 3, 1, kArmoryStreamSnes, 4);
  // Sprite points at a different stream → refuse shared-pair invariant.
  WriteWsp1Record(bytes, meta + 20, 0x54, 1, 0x36, 3, 0, kPatrolSlot3Snes, 4);
  Rom rom;
  ASSERT_TRUE(rom.LoadFromData(std::move(bytes)).ok());
  auto resolved = ResolveCompiledWallPrivateSheet(rom, 0x54, 4);
  EXPECT_TRUE(absl::IsFailedPrecondition(resolved.status()));
}

TEST(CompiledWallPrivateSheetTest, MalformedSlotRemainsRefused) {
  std::vector<uint8_t> bytes(0x230000, 0);
  const auto stream =
      CompressSheet(std::vector<uint8_t>(kWallPrivateDecoded3bppBytes, 0x11));
  std::copy(stream.begin(), stream.end(),
            bytes.begin() + SnesToPc(kPatrolSlot6Snes));
  const size_t meta = SnesToPc(kWsp1Snes);
  bytes[meta + 0] = 'W';
  bytes[meta + 1] = 'S';
  bytes[meta + 2] = 'P';
  bytes[meta + 3] = '1';
  bytes[meta + 4] = 1;
  bytes[meta + 5] = 12;
  WriteLe16(bytes, meta + 6, 1);
  WriteWsp1Record(bytes, meta + 8, 0x64, 0, 0x6F, /*slot=*/7, 0,
                  kPatrolSlot6Snes, 5);
  Rom rom;
  ASSERT_TRUE(rom.LoadFromData(std::move(bytes)).ok());
  auto resolved = ResolveCompiledWallPrivateSheet(rom, 0x64, 5);
  EXPECT_TRUE(absl::IsFailedPrecondition(resolved.status()));
}

TEST(CompiledWallPrivateSheetTest, Room54ResolveLeavesNoSlot6Override) {
  Rom rom = MakeMultiBgWsp1Rom();
  auto room54 = ResolveCompiledWallPrivateSheet(rom, 0x54, 4);
  ASSERT_TRUE(room54.ok() && room54->applied);
  EXPECT_EQ(room54->sheet_overrides_8bpp.count(0x6F), 0u);

  auto room64 = ResolveCompiledWallPrivateSheet(rom, 0x64, 5);
  ASSERT_TRUE(room64.ok() && room64->applied);
  EXPECT_EQ(room64->sheet_overrides_8bpp.count(0x6F), 1u);
  // Room54 result is independent of room64's slot6 stream.
  EXPECT_EQ(room54->sheet_overrides_8bpp.count(0x6F), 0u);
}

}  // namespace
}  // namespace yaze::zelda3
