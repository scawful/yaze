#include "zelda3/gfx_sheet_inventory.h"

#include <gtest/gtest.h>

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>

#include "cli/handlers/graphics/gfx_sheet_inventory_commands.h"
#include "nlohmann/json.hpp"
#include "rom/rom.h"
#include "unique_temp_path.h"
#include "unit/zelda3/gfx_sheet_test_rom.h"
#include "zelda3/dungeon/room.h"
#include "zelda3/overworld/overworld.h"

namespace yaze::test {
namespace {

constexpr uint16_t kTestSheet = 0x90;  // sprite value 0x1D

// Fixture ROM whose sheet 0x90 is art except 16x16 blocks 5 and 9, and whose
// spritesets 5 and 0x45 use sprite value 0x1D (sheet 0x90).
Rom InventoryTestRom() {
  auto fixture = BuildGfxSheetTestRom(/*alias_2bpp=*/true);
  fixture.bytes[0x5B57 + 5 * 4 + 2] = 0x1D;
  fixture.bytes[0x5B57 + 0x45 * 4 + 0] = 0x1D;
  Rom rom;
  EXPECT_TRUE(rom.LoadFromData(fixture.bytes).ok());
  std::vector<uint8_t> sheet(zelda3::kGfxSheet3bppBytes, 0x11);
  for (int block : {5, 9}) {
    for (int tile : zelda3::GfxSheetBlockTiles(block)) {
      std::fill_n(sheet.begin() + tile * 24, 24, 0x00);
    }
  }
  zelda3::GfxSheetWritePolicy policy;
  policy.allocation_regions.push_back(
      {GfxSheetTestRom::kFreeStart, GfxSheetTestRom::kFreeEnd});
  EXPECT_TRUE(zelda3::WriteGfxSheet(rom, kTestSheet, sheet, policy).ok());
  return rom;
}

TEST(GfxSheetInventoryTest, BlockTilesFollowTheSnes16x16Layout) {
  using Tiles = std::array<int, 4>;
  EXPECT_EQ(zelda3::GfxSheetBlockTiles(0), (Tiles{0, 1, 16, 17}));
  EXPECT_EQ(zelda3::GfxSheetBlockTiles(7), (Tiles{14, 15, 30, 31}));
  EXPECT_EQ(zelda3::GfxSheetBlockTiles(8), (Tiles{32, 33, 48, 49}));
  EXPECT_EQ(zelda3::GfxSheetBlockTiles(15), (Tiles{46, 47, 62, 63}));
}

TEST(GfxSheetInventoryTest, A16x16BlockIsEmptyOnlyWhenAllFourTilesAreZero) {
  std::vector<uint8_t> data(zelda3::kGfxSheet3bppBytes, 0x00);
  EXPECT_TRUE(zelda3::IsGfxSheetBlockEmpty(data, 0));
  // One byte in the bottom-right tile (17) of block 0.
  data[17 * 24 + 23] = 0x01;
  EXPECT_FALSE(zelda3::IsGfxSheetBlockEmpty(data, 0));
  EXPECT_TRUE(zelda3::IsGfxSheetBlockEmpty(data, 1));
  // Tile 2 is the top-left of block 1 only.
  data[2 * 24] = 0x80;
  EXPECT_FALSE(zelda3::IsGfxSheetBlockEmpty(data, 1));
  // Short data is never empty.
  EXPECT_FALSE(zelda3::IsGfxSheetBlockEmpty(std::vector<uint8_t>(24, 0), 0));
}

TEST(GfxSheetInventoryTest, ReportsStorageEmptyBlocksAndUsage) {
  Rom rom = InventoryTestRom();
  std::map<int, zelda3::OverworldAreaGfxInfo> areas;
  areas[0x10].sprite_graphics = {5, 0, 0};
  areas[0x10].static_graphics[3] = kTestSheet;
  std::map<int, zelda3::RoomGfxInfo> rooms = {{7, {0, 5}}, {8, {0, 6}}};

  auto inventory = zelda3::BuildGfxSheetInventory(rom, areas, rooms);
  ASSERT_TRUE(inventory.ok()) << inventory.status();
  ASSERT_EQ(inventory->sheets.size(), zelda3::kGfxSheetCount);

  const auto& sheet = inventory->sheets[kTestSheet];
  ASSERT_TRUE(sheet.sprite_value.has_value());
  EXPECT_EQ(*sheet.sprite_value, 0x1D);
  EXPECT_TRUE(sheet.has_block_stats);
  EXPECT_EQ(sheet.empty_16x16_blocks, (std::vector<int>{5, 9}));
  EXPECT_EQ(sheet.empty_8x8, 8);
  EXPECT_FALSE(sheet.all_empty);
  EXPECT_EQ(sheet.FreeBlocks(), (std::vector<int>{5, 9}));
  EXPECT_EQ(sheet.usage.spritesets, (std::vector<int>{5, 0x45}));
  EXPECT_EQ(sheet.usage.ow_areas_sprite, (std::vector<int>{0x10}));
  EXPECT_EQ(sheet.usage.ow_areas_static, (std::vector<int>{0x10}));
  EXPECT_EQ(sheet.usage.rooms_sprite, (std::vector<int>{7}));
  EXPECT_FALSE(sheet.unreferenced);

  // Raw and 2bpp sheets are engine-loaded and never free.
  EXPECT_TRUE(inventory->sheets[116].engine_loaded);
  EXPECT_TRUE(inventory->sheets[116].FreeBlocks().empty());
  EXPECT_TRUE(inventory->sheets[113].engine_loaded);
  EXPECT_FALSE(inventory->sheets[113].has_block_stats);
  EXPECT_FALSE(inventory->sheets[113].stored_bytes.has_value());
  // Vanilla-style aliases.
  EXPECT_EQ(inventory->sheets[221].shares_pointer_with,
            (std::vector<uint16_t>{113}));
}

TEST(GfxSheetInventoryTest, ReservedFlaggedAndReservedBlocks) {
  Rom rom = InventoryTestRom();
  zelda3::GfxSheetInventoryOptions options;
  options.reserved_sheets = {0x7B, 0x91};
  options.flagged_sheets = {kTestSheet};
  options.reserved_blocks = {{kTestSheet, {9}}};
  auto inventory = zelda3::BuildGfxSheetInventory(rom, {}, {}, options);
  ASSERT_TRUE(inventory.ok());

  const auto& sheet = inventory->sheets[kTestSheet];
  EXPECT_TRUE(sheet.flagged);
  EXPECT_FALSE(sheet.reserved);
  EXPECT_EQ(sheet.FreeBlocks(), (std::vector<int>{5}));
  EXPECT_TRUE(inventory->sheets[0x7B].reserved);
  EXPECT_TRUE(inventory->sheets[0x91].reserved);
  EXPECT_TRUE(inventory->sheets[0x91].FreeBlocks().empty());

  const auto json = zelda3::GfxSheetInventoryToJson(*inventory);
  EXPECT_TRUE(json["sheets"][0x91].contains("reserved"));
  EXPECT_EQ(json["sheets"][kTestSheet]["free_16x16_blocks"],
            nlohmann::json::array({5}));
  EXPECT_TRUE(json["sheets"][113]["stored_bytes"].is_null());
}

TEST(GfxSheetInventoryTest, ParsesQuotedSpritesetLabelCells) {
  const std::string csv =
      "ID,Usage,Slot 1,Slot 2,Slot 3,Slot 4,,,x\n"
      "0x09,,\"0x1F Stalfos, Miri\",0x49 Soldiers,0x55 Farore,"
      "\"0x42 Squirrel, Bird\",,,0x77 Ignored\r\n"
      "0x3B,,0x00,0x1F Stalfos,bare,\n";
  const auto labels = zelda3::ParseSpritesetLabelCsv(csv);
  EXPECT_EQ(labels.at(0x1F),
            (std::set<std::string>{"Stalfos", "Stalfos, Miri"}));
  EXPECT_EQ(labels.at(0x55), (std::set<std::string>{"Farore"}));
  EXPECT_EQ(labels.at(0x42), (std::set<std::string>{"Squirrel, Bird"}));
  EXPECT_EQ(labels.count(0x77), 0u);  // column 8 is not a slot
  EXPECT_EQ(labels.count(0x00), 0u);  // "0x00" has no name
}

TEST(GfxSheetInventoryCommandTest, WritesReferenceShapedJson) {
  Rom rom = InventoryTestRom();
  rom.mutable_data()[0x7FD9] = 0x01;  // US pointer tables, as the fixture uses
  cli::GfxSheetInventoryCommandHandler handler;
  const auto out_path = UniqueTempPath("gfx_inventory", ".json");
  std::string out;
  ASSERT_TRUE(handler
                  .Run({"--reserved=0x7B,0x7C", "--flagged=0xD4",
                        "--out=" + out_path.string(), "--format=json"},
                       &rom, &out)
                  .ok())
      << out;
  const auto report = nlohmann::json::parse(out.substr(out.find('{')));
  ASSERT_EQ(report["sheets"].size(), zelda3::kGfxSheetCount);
  EXPECT_EQ(report["spritesets"].size(), 144u);
  EXPECT_EQ(report["main_blocksets"].size(), 37u);
  EXPECT_EQ(report["room_blocksets"].size(), 82u);
  EXPECT_EQ(report["overworld_areas"].size(), 160u);
  EXPECT_TRUE(report["rules"]["reserved"].contains("0x7C"));
  EXPECT_EQ(report["sheets"][kTestSheet]["kind"], "3bpp-lz2");
  EXPECT_EQ(report["sheets"][kTestSheet]["sprite_value"], "0x1D");
  EXPECT_TRUE(report["sheets"][0x7B].contains("reserved"));
  EXPECT_TRUE(report["sheets"][0xD4].contains("flagged"));

  std::ifstream file(out_path);
  ASSERT_TRUE(file.is_open());
  const auto written = nlohmann::json::parse(file);
  EXPECT_EQ(written["sheets"], report["sheets"]);
  file.close();
  std::filesystem::remove(out_path);

  EXPECT_FALSE(handler.Run({"--reserved=0x1FF"}, &rom, &out).ok());
}

std::vector<std::string> RomTestPaths() {
  std::vector<std::string> paths;
  for (const char* env : {"YAZE_TEST_ROM_OOS", "YAZE_TEST_ROM_VANILLA"}) {
    if (const char* path = std::getenv(env);
        path != nullptr && std::filesystem::exists(path)) {
      paths.push_back(path);
    }
  }
  return paths;
}

Rom LoadRomFile(const std::string& path) {
  std::ifstream file(path, std::ios::binary);
  std::vector<uint8_t> bytes((std::istreambuf_iterator<char>(file)),
                             std::istreambuf_iterator<char>());
  Rom rom;
  EXPECT_TRUE(rom.LoadFromData(bytes).ok());
  return rom;
}

// The cheap room collector must read what LoadRoomHeaderFromRom reads.
TEST(GfxSheetInventoryRomTest, RoomCollectorMatchesRoomHeaderLoader) {
  const auto paths = RomTestPaths();
  if (paths.empty()) {
    GTEST_SKIP() << "Set YAZE_TEST_ROM_OOS or YAZE_TEST_ROM_VANILLA";
  }
  for (const auto& path : paths) {
    SCOPED_TRACE(path);
    Rom rom = LoadRomFile(path);
    const auto rooms = zelda3::CollectRoomGfx(rom);
    ASSERT_EQ(rooms.size(), static_cast<size_t>(zelda3::kNumDungeonRooms));
    for (const auto& [id, info] : rooms) {
      const auto room = zelda3::LoadRoomHeaderFromRom(&rom, id);
      ASSERT_EQ(info.blockset, room.blockset()) << "room " << id;
      ASSERT_EQ(info.spriteset, room.spriteset()) << "room " << id;
    }
  }
}

// The cheap area collector must report what `overworld-describe-map` reports
// (Overworld::Load + EnsureMapBuilt).
TEST(GfxSheetInventoryRomTest, AreaCollectorMatchesTheBuiltOverworld) {
  const auto paths = RomTestPaths();
  if (paths.empty()) {
    GTEST_SKIP() << "Set YAZE_TEST_ROM_OOS or YAZE_TEST_ROM_VANILLA";
  }
  for (const auto& path : paths) {
    SCOPED_TRACE(path);
    Rom rom = LoadRomFile(path);
    // The app and z3ed expand a 1 MiB vanilla image before loading the
    // overworld; expanded bytes are zero and do not change the tables read.
    if (rom.size() < 0x200000) {
      rom.Expand(0x200000);
    }
    const auto areas = zelda3::CollectOverworldAreaGfx(rom);
    ASSERT_EQ(areas.size(), static_cast<size_t>(zelda3::kNumOverworldAreas));

    zelda3::Overworld overworld(&rom);
    const absl::Status load = overworld.Load(&rom);
    ASSERT_TRUE(load.ok()) << load;
    for (const auto& [id, info] : areas) {
      ASSERT_TRUE(overworld.EnsureMapBuilt(id).ok());
      const auto* map = overworld.overworld_map(id);
      ASSERT_NE(map, nullptr);
      ASSERT_EQ(info.parent, map->parent()) << "area " << id;
      ASSERT_EQ(info.area_graphics, map->area_graphics()) << "area " << id;
      for (int i = 0; i < 3; ++i) {
        ASSERT_EQ(info.sprite_graphics[i], map->sprite_graphics(i))
            << "area " << id;
      }
      for (int i = 0; i < 16; ++i) {
        ASSERT_EQ(info.static_graphics[i], map->static_graphics(i))
            << "area " << id << " slot " << i;
      }
    }
  }
}

}  // namespace
}  // namespace yaze::test
