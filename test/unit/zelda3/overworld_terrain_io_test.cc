#include "zelda3/overworld/overworld_terrain_io.h"

#include <algorithm>
#include <array>
#include <cstdlib>
#include <filesystem>
#include <utility>
#include <vector>

#include "app/gfx/util/compression.h"
#include "core/source_artifact_publisher.h"
#include "gtest/gtest.h"
#include "rom/snes.h"
#include "zelda3/overworld/lyra_terrain_source.h"
#include "zelda3/overworld/overworld.h"
#include "zelda3/overworld/overworld_map.h"

namespace yaze::zelda3 {
namespace {

class OverworldTerrainIoTest : public testing::Test {
 protected:
  void MakeRom(int count = 192, uint32_t end = 0x18000) {
    storage = {0x1000, 0x1300, count, {{0x10000, end, "test streams"}}};
    std::vector<uint8_t> data(0x200000, 0);
    for (uint32_t pc : {0x10000U, 0x10004U}) {
      data[pc] = 0xE4;
      data[pc + 1] = 0xFF;
      data[pc + 2] = 0;
      data[pc + 3] = 0xFF;
    }
    for (int map = 0; map < count; ++map) {
      PutPointer(&data, storage.high_table_pc + map * 3, 0x10000);
      PutPointer(&data, storage.low_table_pc + map * 3, 0x10004);
    }
    ASSERT_TRUE(rom.LoadFromData(data).ok());
    rom.set_dirty(false);
    pages.assign(count, OverworldTerrainPage{});
  }

  static void PutPointer(std::vector<uint8_t>* data, uint32_t slot,
                         uint32_t pc) {
    const uint32_t address = PcToSnes(pc) | 0x800000;
    for (int i = 0; i < 3; ++i)
      (*data)[slot + i] = static_cast<uint8_t>(address >> (i * 8));
  }

  void AddNativePrefix() {
    storage.native_prefix = OverworldTerrainPointerTables{0x2000, 0x2300, 160};
    auto data = rom.vector();
    std::copy_n(data.begin() + storage.high_table_pc, 160 * 3,
                data.begin() + 0x2000);
    std::copy_n(data.begin() + storage.low_table_pc, 160 * 3,
                data.begin() + 0x2300);
    data[0x21E0] = 0x77;
    data[0x24E0] = 0x66;
    ASSERT_TRUE(rom.LoadFromData(data).ok());
    rom.set_dirty(false);
  }

  void MakeInitialImport(int count) {
    MakeRom(count);
    auto data = rom.vector();
    data.resize(0x400000);
    storage.native_prefix =
        OverworldTerrainPointerTables{0x1794D, 0x17B2D, 160};
    std::copy_n(data.begin() + storage.high_table_pc, 160 * 3,
                data.begin() + 0x1794D);
    std::copy_n(data.begin() + storage.low_table_pc, 160 * 3,
                data.begin() + 0x17B2D);
    storage.high_table_pc = 0x260000;
    storage.low_table_pc = 0x260300;
    // No source pointers exist at the destination, including the added maps.
    // Deliberately invalid bytes ensure the planner cannot read fake old rows.
    for (uint32_t table : {storage.high_table_pc, storage.low_table_pc})
      std::fill_n(data.begin() + table, count * 3, 0x7E);
    storage.read_only_stream_regions = {{0x10000, 0x10008, "original streams"}};
    storage.stream_regions = {{0x240000, 0x260000, "import streams"}};
    ASSERT_TRUE(rom.LoadFromData(data).ok());
    rom.set_dirty(false);
  }

  uint32_t StreamPc(uint32_t slot) const {
    const auto& data = rom.vector();
    return SnesToPc(data[slot] | (data[slot + 1] << 8) |
                    (data[slot + 2] << 16));
  }

  // Reopen and use the existing game-format decompressor, independently of
  // the allocation scanner used to validate the production write plan.
  void CheckColdReadback() {
    Rom cold;
    ASSERT_TRUE(cold.LoadFromData(rom.vector()).ok());
    const auto& data = cold.vector();
    for (size_t map = 0; map < pages.size(); ++map) {
      for (bool high : {true, false}) {
        const uint32_t slot =
            (high ? storage.high_table_pc : storage.low_table_pc) + map * 3;
        const uint32_t pc = SnesToPc(data[slot] | (data[slot + 1] << 8) |
                                     (data[slot + 2] << 16));
        int consumed = 0;
        auto decoded = gfx::HyruleMagicDecompress(
            data.data() + pc, &consumed, 1,
            std::min<size_t>(0x8000 - (pc & 0x7FFF), data.size() - pc));
        ASSERT_EQ(decoded.size(), 256u) << map;
        for (size_t i = 0; i < decoded.size(); ++i)
          ASSERT_EQ(decoded[i], static_cast<uint8_t>(high ? pages[map][i] >> 8
                                                          : pages[map][i]))
              << "map=" << map << " tile=" << i << " high=" << high;
      }
    }
  }

  Rom rom;
  OverworldTerrainStorage storage;
  std::vector<OverworldTerrainPage> pages;
};

TEST(OverworldTile32PackingTest,
     PackedDictionariesReconstructEveryOriginalCell) {
  OverworldMapTiles tiles;
  std::array<OverworldBlockset*, 3> grids = {
      &tiles.light_world, &tiles.dark_world, &tiles.special_world};
  for (auto* grid : grids)
    grid->assign(256, std::vector<uint16_t>(256));
  for (int map = 0; map < 192; ++map) {
    auto& grid = *grids[map / 64];
    for (int y = 0; y < 32; ++y)
      for (int x = 0; x < 32; ++x)
        grid[map % 8 * 32 + x][map % 64 / 8 * 32 + y] =
            (map * 7 + x * 3 + y * 5) % 4096;
  }
  tiles.special_world[255][255] = 4095;
  const auto plan = PlanOverworldTile32Packing(tiles, 192, 17728);
  ASSERT_TRUE(plan.ok()) << plan.status();
  ASSERT_EQ(plan->pages.size(), 192u);
  EXPECT_EQ(plan->definitions.size() % 4, 0u);
  for (int map = 0; map < 192; ++map)
    for (int y = 0; y < 32; ++y)
      for (int x = 0; x < 32; ++x) {
        const size_t id = plan->pages[map][y / 2 * 16 + x / 2];
        ASSERT_LT(id, plan->definitions.size());
        const auto& bytes = plan->quadrant_bytes[y % 2 * 2 + x % 2];
        const size_t row = id / 4 * 6, lane = id % 4;
        // Decode the game-format 12-bit word, independently of Tile32 values.
        const uint16_t word =
            bytes[row + lane] |
            (((bytes[row + 4 + lane / 2] >> (lane % 2 == 0 ? 4 : 0)) & 15)
             << 8);
        ASSERT_EQ(word,
                  (*grids[map / 64])[map % 8 * 32 + x][map % 64 / 8 * 32 + y]);
      }
  const auto again = PlanOverworldTile32Packing(tiles, 192, 17728);
  ASSERT_TRUE(again.ok());
  EXPECT_EQ(again->pages, plan->pages);
  EXPECT_EQ(again->quadrant_bytes, plan->quadrant_bytes);
  EXPECT_EQ(PlanOverworldTile32Packing(tiles, 192, 4).status().code(),
            absl::StatusCode::kResourceExhausted);
}

TEST(OverworldTile32PackingTest,
     CapacityGridAndTile16BoundsRefuseBeforePacking) {
  OverworldMapTiles tiles;
  tiles.light_world.assign(256, std::vector<uint16_t>(256));
  tiles.dark_world = tiles.light_world;
  tiles.special_world.assign(256, std::vector<uint16_t>(128));
  const auto original = PlanOverworldTile32Packing(tiles, 160, 17728);
  ASSERT_TRUE(original.ok()) << original.status();
  EXPECT_EQ(original->definitions.size(), 4u);
  EXPECT_EQ(PlanOverworldTile32Packing(tiles, 192, 17728).status().code(),
            absl::StatusCode::kFailedPrecondition);
  EXPECT_FALSE(PlanOverworldTile32Packing(tiles, 159, 17728).ok());
  EXPECT_FALSE(PlanOverworldTile32Packing(tiles, 160, 0).ok());
  tiles.light_world[31][31] = 4096;
  EXPECT_EQ(PlanOverworldTile32Packing(tiles, 160, 17728).status().code(),
            absl::StatusCode::kInvalidArgument);
  EXPECT_EQ(tiles.light_world[31][31], 4096);
}

TEST(SpecialOverworldTerrainImportTest,
     RealLegacyTerrainAndAssignedDocumentSharePackingAndGeometry) {
  const char* path = std::getenv("YAZE_TEST_ROM_OOS");
  if (!path || !std::filesystem::exists(path))
    GTEST_SKIP() << "Set YAZE_TEST_ROM_OOS";
  Rom source;
  ASSERT_TRUE(source.LoadFromFile(path).ok());
  Overworld world(&source);
  ASSERT_TRUE(world.Load(&source).ok());
  const auto before = source.vector();
  const std::array<OverworldBlockset, 3> original = {
      world.GetMapTiles(0), world.GetMapTiles(1), world.GetMapTiles(2)};
  core::LyraTerrainProfile profile;
  profile.tile_count = world.tiles16().size();
  std::string definitions;
  for (const auto& tile : world.tiles16())
    for (const auto& quadrant : tile.tiles_info) {
      const uint16_t word = gfx::TileInfoToWord(quadrant);
      definitions.push_back(static_cast<char>(word));
      definitions.push_back(static_cast<char>(word >> 8));
    }
  profile.tile16_definition_fingerprint =
      core::ComputeSourceArtifactSha256(definitions);
  // This test covers terrain/geometry; full asset authentication is the
  // caller's separate normal native resolver operation.
  profile.graphics_fingerprint = "graphics:terrain-fixture";
  profile.palette_fingerprint = "palette:terrain-fixture";
  profile.animation_fingerprint = "animation:terrain-fixture";
  auto document = core::LyraTerrainDocument::Create(profile, 0);
  ASSERT_TRUE(document.ok()) << document.status();
  ASSERT_TRUE(document->BeginStroke().ok());
  ASSERT_TRUE(document->Paint(0, 0, 1).ok());
  ASSERT_TRUE(document->Paint(255, 127, 2).ok());
  ASSERT_TRUE(document->EndStroke().ok());
  OverworldTerrainStorage storage{
      0x260000,
      0x260300,
      192,
      {{0x240000, 0x260000, "new streams"}},
      {{kOverworldCompressedMapPos, 0x5FE70, "original bank0B"},
       {0x60000, 0x6411F, "original bank0C"},
       {kOverworldMapDataOverflow, kOverworldCompressedOverflowPos,
        "original overflow"}},
      OverworldTerrainPointerTables{0x1794D, 0x17B2D, 160}};
  const auto unassigned =
      PlanSpecialOverworldTerrainImport(source, *document, storage);
  EXPECT_FALSE(unassigned.ok());
  EXPECT_NE(unassigned.status().message().find("Assign added map"),
            absl::string_view::npos)
      << unassigned.status();
  auto authored = document->authoring();
  std::array<bool, 32> covered{};
  for (int parent = 0xA0; parent < 0xC0; ++parent) {
    if (covered[parent - 0xA0])
      continue;
    const auto size = parent == 0xA0 || parent == 0xBE
                          ? core::LyraGameplayAreaSize::kWide
                      : parent == 0xA6 || parent == 0xB0 || parent == 0xB3
                          ? core::LyraGameplayAreaSize::kLarge
                          : core::LyraGameplayAreaSize::kSmall;
    const auto maps = core::LyraGameplayAreaFootprint(parent, size);
    ASSERT_TRUE(maps.ok());
    authored.gameplay_areas.push_back({parent, size, *maps});
    for (int map : *maps)
      covered[map - 0xA0] = true;
  }
  ASSERT_EQ(authored.gameplay_areas.size(), 21u);
  ASSERT_TRUE(document->SetAuthoring(document->authoring(), authored).ok());
  // A clean ROM does not imply a clean editor model. Import derives its
  // preserved prefix from saved bytes even if the editor has pending edits.
  world.GetMapTiles(0)[0][0] ^= 1;
  const auto edited_model = world.GetMapTiles(0);
  ScopedRomReadOnly source_read_only(source);
  const auto plan =
      PlanSpecialOverworldTerrainImport(source, *document, storage);
  ASSERT_TRUE(plan.ok()) << plan.status();
  EXPECT_TRUE(plan->terrain.initial_import);
  EXPECT_EQ(plan->terrain.baseline.size(), 0x400000u);
  ASSERT_EQ(plan->geometry.size(), 192u);
  for (int map = 0; map < 192; ++map) {
    if (map < 160) {
      EXPECT_EQ(plan->geometry[map].parent, world.overworld_map(map)->parent());
      EXPECT_EQ(plan->geometry[map].size,
                world.overworld_map(map)->area_size());
    } else {
      const auto* area = document->GameplayAreaForMap(map);
      ASSERT_NE(area, nullptr);
      EXPECT_EQ(plan->geometry[map].parent, area->parent_map_id);
      EXPECT_EQ(static_cast<int>(plan->geometry[map].size),
                static_cast<int>(area->size));
    }
    for (int y = 0; y < 32; ++y)
      for (int x = 0; x < 32; ++x) {
        const auto& definition =
            plan->tile32
                .definitions[plan->tile32.pages[map][y / 2 * 16 + x / 2]];
        const std::array<uint16_t, 4> quadrants = {
            definition.tile0_, definition.tile1_, definition.tile2_,
            definition.tile3_};
        const uint16_t expected =
            map < 160
                ? original[map / 64][map % 8 * 32 + x][map % 64 / 8 * 32 + y]
                : document->maps()[map - 160][y * 32 + x];
        ASSERT_EQ(quadrants[y % 2 * 2 + x % 2], expected) << map;
      }
  }
  EXPECT_EQ(source.vector(), before);
  EXPECT_FALSE(source.dirty());
  EXPECT_EQ(world.GetMapTiles(0), edited_model);
  for (int kind = 1; kind < 3; ++kind)
    EXPECT_EQ(world.GetMapTiles(kind), original[kind]);
  EXPECT_EQ(document->authoring(), authored);
  auto stale_profile = profile;
  stale_profile.tile16_definition_fingerprint = "changed:definitions";
  const auto stale = core::LyraTerrainDocument::Create(stale_profile, 0);
  ASSERT_TRUE(stale.ok());
  const auto refused =
      PlanSpecialOverworldTerrainImport(source, *stale, storage);
  EXPECT_FALSE(refused.ok());
  EXPECT_NE(refused.status().message().find("binding"),
            absl::string_view::npos);
}

TEST_F(OverworldTerrainIoTest,
       InitialImportHasOnly160OldMapsAndReadsAllOutputs) {
  for (int count : {160, 192}) {
    SCOPED_TRACE(count);
    MakeInitialImport(count);
    const auto before = rom.vector();
    // Repacking a dictionary may renumber every original map's Tile32 IDs.
    for (auto& page : pages)
      page.fill(0x0102);
    pages.back()[255] = 0x0321;
    EXPECT_FALSE(PlanOverworldTerrainSave(rom, storage, pages).ok());
    const auto plan = PlanInitialOverworldTerrainImport(rom, storage, pages);
    ASSERT_TRUE(plan.ok()) << plan.status();
    EXPECT_TRUE(plan->initial_import);
    EXPECT_FALSE(ApplyOverworldTerrainSave(rom, *plan).ok());
    EXPECT_EQ(rom.vector(), before);
    EXPECT_FALSE(rom.dirty());
    auto staged = before;
    for (const auto& write : plan->writes) {
      ASSERT_TRUE(std::equal(write.before.begin(), write.before.end(),
                             before.begin() + write.pc));
      std::copy(write.after.begin(), write.after.end(),
                staged.begin() + write.pc);
    }
    EXPECT_TRUE(std::equal(before.begin() + 0x10000, before.begin() + 0x10008,
                           staged.begin() + 0x10000));
    for (auto [primary, mirror] : {std::pair{storage.high_table_pc, 0x1794DU},
                                   std::pair{storage.low_table_pc, 0x17B2DU}})
      EXPECT_TRUE(std::equal(staged.begin() + primary,
                             staged.begin() + primary + 160 * 3,
                             staged.begin() + mirror));
    // Test-only staging: production must join dictionaries/directory before
    // publishing this plan. Independently decompress every desired page here.
    ASSERT_TRUE(rom.LoadFromData(staged).ok());
    CheckColdReadback();
    const auto no_op = PlanOverworldTerrainSave(rom, storage, pages);
    ASSERT_TRUE(no_op.ok()) << no_op.status();
    EXPECT_TRUE(no_op->writes.empty());
  }
}

TEST_F(OverworldTerrainIoTest,
       InitialImportRefusesBadSourceAndExhaustedOwnership) {
  MakeInitialImport(192);
  const auto before = rom.vector();
  storage.native_prefix->map_count = 159;
  EXPECT_FALSE(PlanInitialOverworldTerrainImport(rom, storage, pages).ok());
  storage.native_prefix->map_count = 160;
  storage.native_prefix->low_table_pc++;
  EXPECT_FALSE(PlanInitialOverworldTerrainImport(rom, storage, pages).ok());
  storage.native_prefix->low_table_pc--;
  storage.stream_regions = {{0x240000, 0x240004, "too small"}};
  for (auto& page : pages)
    page.fill(0x0102);
  EXPECT_EQ(
      PlanInitialOverworldTerrainImport(rom, storage, pages).status().code(),
      absl::StatusCode::kResourceExhausted);
  EXPECT_EQ(rom.vector(), before);
  storage.stream_regions = {{0x240000, 0x260000, "import streams"}};
  auto bytes = before;
  bytes[0x17B2D + 0x9F * 3 + 2] = 0x7E;
  ASSERT_TRUE(rom.LoadFromData(bytes).ok());
  EXPECT_FALSE(PlanInitialOverworldTerrainImport(rom, storage, pages).ok());
  EXPECT_EQ(rom.vector(), bytes);
  ASSERT_TRUE(rom.LoadFromData(before).ok());
  storage.read_only_stream_regions.clear();
  storage.stream_regions.push_back(
      {0x10000, 0x10008, "wrongly writable source"});
  EXPECT_FALSE(PlanInitialOverworldTerrainImport(rom, storage, pages).ok());
  EXPECT_EQ(rom.vector(), before);
}

TEST_F(OverworldTerrainIoTest, NoOpPreservesExactPointersAndDirtyState) {
  MakeRom(160);
  const auto before = rom.vector();
  auto plan = PlanOverworldTerrainSave(rom, storage, pages);
  ASSERT_TRUE(plan.ok()) << plan.status();
  EXPECT_TRUE(plan->writes.empty());
  EXPECT_TRUE(ApplyOverworldTerrainSave(rom, *plan).ok());
  EXPECT_EQ(rom.vector(), before);
  EXPECT_FALSE(rom.dirty());
}

TEST_F(OverworldTerrainIoTest, MixedNativeAndTailPagesColdReopenAll192) {
  MakeRom();
  const auto before = rom.vector();
  pages[0x80][7] = 0x0321;
  pages[0x9F][31] = 0x0444;
  pages[0xA0][127] = 0x05D5;
  pages[0xBF][255] = 0x06E6;
  auto plan = PlanOverworldTerrainSave(rom, storage, pages);
  ASSERT_TRUE(plan.ok()) << plan.status();
  ASSERT_TRUE(ApplyOverworldTerrainSave(rom, *plan).ok());
  CheckColdReadback();
  for (size_t pc = 0; pc < before.size(); ++pc) {
    if (rom.vector()[pc] == before[pc])
      continue;
    EXPECT_TRUE(std::any_of(plan->writes.begin(), plan->writes.end(),
                            [pc](const auto& write) {
                              return pc >= write.pc &&
                                     pc < write.pc + write.after.size();
                            }))
        << pc;
  }
}

TEST_F(OverworldTerrainIoTest, SharedStreamsStayIntactAndNewPlanesDeduplicate) {
  MakeRom();
  const auto original_streams = std::vector<uint8_t>(
      rom.vector().begin() + 0x10000, rom.vector().begin() + 0x10008);
  ASSERT_TRUE(rom.WriteByte(0x10008, 0x42).ok());
  pages[0xBF][0] = 0x0101;
  auto plan = PlanOverworldTerrainSave(rom, storage, pages);
  ASSERT_TRUE(plan.ok()) << plan.status();
  ASSERT_TRUE(ApplyOverworldTerrainSave(rom, *plan).ok());
  EXPECT_EQ(StreamPc(storage.high_table_pc), 0x10000u);
  EXPECT_EQ(StreamPc(storage.low_table_pc), 0x10004u);
  EXPECT_EQ(StreamPc(storage.high_table_pc + 0xBF * 3),
            StreamPc(storage.low_table_pc + 0xBF * 3));
  EXPECT_EQ(std::vector<uint8_t>(rom.vector().begin() + 0x10000,
                                 rom.vector().begin() + 0x10008),
            original_streams);
  EXPECT_EQ(rom.vector()[0x10008], 0x42);
  CheckColdReadback();
}

TEST_F(OverworldTerrainIoTest, ReclaimsStreamsOnlyWhenEveryUserMoves) {
  MakeRom(160, 0x10008);
  for (auto& page : pages)
    page.fill(0x0101);
  auto plan = PlanOverworldTerrainSave(rom, storage, pages);
  ASSERT_TRUE(plan.ok()) << plan.status();
  ASSERT_TRUE(ApplyOverworldTerrainSave(rom, *plan).ok());
  CheckColdReadback();
}

TEST_F(OverworldTerrainIoTest, FastRomLastBanksPreserveAndPlaceChangedPlanes) {
  MakeRom();
  auto bytes = rom.vector();
  bytes.resize(0x400000);
  storage.stream_regions = {{0x3F0000, 0x3F0004, "FE stream"},
                            {0x3F8000, 0x400000, "FF streams"}};
  for (uint32_t pc : {0x3F0000U, 0x3F8000U})
    std::copy_n(bytes.begin() + 0x10000, 4, bytes.begin() + pc);
  for (int map = 0; map < 192; ++map) {
    PutPointer(&bytes, storage.high_table_pc + map * 3, 0x3F0000);
    PutPointer(&bytes, storage.low_table_pc + map * 3, 0x3F8000);
  }
  ASSERT_EQ(bytes[storage.high_table_pc + 2], 0xFE);
  ASSERT_EQ(bytes[storage.low_table_pc + 2], 0xFF);
  ASSERT_TRUE(rom.LoadFromData(bytes).ok());
  auto no_op = PlanOverworldTerrainSave(rom, storage, pages);
  ASSERT_TRUE(no_op.ok()) << no_op.status();
  EXPECT_TRUE(no_op->writes.empty());
  pages[0xBF][0] = 0x0101;
  auto plan = PlanOverworldTerrainSave(rom, storage, pages);
  ASSERT_TRUE(plan.ok()) << plan.status();
  ASSERT_TRUE(ApplyOverworldTerrainSave(rom, *plan).ok());
  EXPECT_GE(StreamPc(storage.low_table_pc + 0xBF * 3), 0x3F8004u);
  CheckColdReadback();
  for (uint32_t raw_bank : {0x7EU, 0x7FU}) {
    ASSERT_TRUE(
        rom.WriteLong(storage.low_table_pc + 0xBF * 3, raw_bank << 16 | 0x8000)
            .ok());
    EXPECT_FALSE(PlanOverworldTerrainSave(rom, storage, pages).ok());
  }
}

TEST_F(OverworldTerrainIoTest, PointerInsideAnotherStreamKeepsBothLiveSpans) {
  MakeRom();
  auto bytes = rom.vector();
  // The outer literal256 contains a valid inner fill256 in its first4 bytes.
  bytes[0x10000] = 0xE0;
  bytes[0x10001] = 0xFF;
  std::fill_n(bytes.begin() + 0x10002, 256, 0);
  bytes[0x10002] = 0xE4;
  bytes[0x10003] = 0xFF;
  bytes[0x10004] = 0;
  bytes[0x10005] = 0xFF;
  bytes[0x10102] = 0xFF;
  for (int map = 0; map < 192; ++map) {
    PutPointer(&bytes, storage.high_table_pc + map * 3, 0x10002);
    PutPointer(&bytes, storage.low_table_pc + map * 3, 0x10000);
    pages[map][0] = 0xE4;
    pages[map][1] = 0xFF;
    pages[map][3] = 0xFF;
  }
  ASSERT_TRUE(rom.LoadFromData(bytes).ok());
  pages[0xBF][0] = 0xE5;
  auto plan = PlanOverworldTerrainSave(rom, storage, pages);
  ASSERT_TRUE(plan.ok()) << plan.status();
  ASSERT_TRUE(ApplyOverworldTerrainSave(rom, *plan).ok());
  EXPECT_TRUE(std::equal(bytes.begin() + 0x10000, bytes.begin() + 0x10103,
                         rom.vector().begin() + 0x10000));
  EXPECT_EQ(StreamPc(storage.high_table_pc), 0x10002u);
  EXPECT_EQ(StreamPc(storage.low_table_pc), 0x10000u);
  CheckColdReadback();
}

TEST_F(OverworldTerrainIoTest, FullPoolFailsWithoutChangingSource) {
  MakeRom(192, 0x10008);
  const auto before = rom.vector();
  pages[0xBF][0] = 1;
  auto plan = PlanOverworldTerrainSave(rom, storage, pages);
  EXPECT_EQ(plan.status().code(), absl::StatusCode::kResourceExhausted);
  EXPECT_EQ(rom.vector(), before);
  EXPECT_FALSE(rom.dirty());
}

TEST_F(OverworldTerrainIoTest, ReclaimsUnreferencedBytesFromEarlierSaves) {
  MakeRom(192, 0x10010);
  storage.stream_regions_exclusive = true;
  auto data = rom.vector();
  std::fill_n(data.begin() + 0x10008, 8, 0xAB);
  ASSERT_TRUE(rom.LoadFromData(data).ok());
  rom.set_dirty(false);
  pages[0xBF][0] = 1;
  auto plan = PlanOverworldTerrainSave(rom, storage, pages);
  ASSERT_TRUE(plan.ok()) << plan.status();
  ASSERT_TRUE(ApplyOverworldTerrainSave(rom, *plan).ok());
  EXPECT_GE(StreamPc(storage.low_table_pc + 0xBF * 3), 0x10008u);
  CheckColdReadback();
}

TEST_F(OverworldTerrainIoTest, UnknownBytesNeverDisplaceLiveStreams) {
  MakeRom(192, 0x10040);
  storage.stream_regions_exclusive = true;
  auto data = rom.vector();
  // Move map 1 to a live stream surrounded by unreferenced non-blank bytes.
  std::fill(data.begin() + 0x10008, data.begin() + 0x10040, 0xAB);
  const std::array<uint8_t, 4> live = {0xE4, 0xFF, 0x22, 0xFF};
  std::copy(live.begin(), live.end(), data.begin() + 0x10020);
  PutPointer(&data, storage.low_table_pc + 3, 0x10020);
  ASSERT_TRUE(rom.LoadFromData(data).ok());
  rom.set_dirty(false);
  pages[1].fill(0x0022);
  pages[0][0] = 1;
  auto plan = PlanOverworldTerrainSave(rom, storage, pages);
  ASSERT_TRUE(plan.ok()) << plan.status();
  ASSERT_TRUE(ApplyOverworldTerrainSave(rom, *plan).ok());
  EXPECT_EQ(StreamPc(storage.low_table_pc + 3), 0x10020u);
  EXPECT_TRUE(
      std::equal(live.begin(), live.end(), rom.vector().begin() + 0x10020));
  CheckColdReadback();
}

TEST_F(OverworldTerrainIoTest, RepeatedSavesDoNotConsumeAssignedRegion) {
  MakeRom(192, 0x10100);
  storage.stream_regions_exclusive = true;
  for (int save = 0; save < 300; ++save) {
    for (int i = 0; i < 8; ++i)
      pages[0][i * 11 % 256] = static_cast<uint16_t>((save * 7 + i) & 0xFF);
    auto plan = PlanOverworldTerrainSave(rom, storage, pages);
    ASSERT_TRUE(plan.ok()) << "save=" << save << " " << plan.status();
    ASSERT_TRUE(ApplyOverworldTerrainSave(rom, *plan).ok()) << "save=" << save;
    if (save % 10 == 0)
      CheckColdReadback();
    if (HasFatalFailure())
      return;
  }
  CheckColdReadback();
}

TEST_F(OverworldTerrainIoTest, UnreferencedBytesCannotHideRegionExhaustion) {
  MakeRom(192, 0x10010);
  storage.stream_regions_exclusive = true;
  auto data = rom.vector();
  std::fill_n(data.begin() + 0x10008, 8, 0xAB);
  ASSERT_TRUE(rom.LoadFromData(data).ok());
  rom.set_dirty(false);
  const auto before = rom.vector();
  for (int i = 0; i < 256; ++i)
    pages[0xBF][i] = static_cast<uint16_t>(i * 37 & 0xFF);
  auto plan = PlanOverworldTerrainSave(rom, storage, pages);
  EXPECT_EQ(plan.status().code(), absl::StatusCode::kResourceExhausted);
  EXPECT_EQ(rom.vector(), before);
  EXPECT_FALSE(rom.dirty());
}

TEST_F(OverworldTerrainIoTest, SharedRegionsKeepUnknownBytes) {
  MakeRom(192, 0x10010);
  auto data = rom.vector();
  std::fill_n(data.begin() + 0x10008, 8, 0xAB);
  ASSERT_TRUE(rom.LoadFromData(data).ok());
  rom.set_dirty(false);
  pages[0xBF][0] = 1;
  EXPECT_EQ(PlanOverworldTerrainSave(rom, storage, pages).status().code(),
            absl::StatusCode::kResourceExhausted);
}

TEST_F(OverworldTerrainIoTest, AllocationDoesNotCrossLoRomBank) {
  MakeRom(192, 0x10008);
  storage.stream_regions.push_back({0x17FFE, 0x18100, "test bank edge"});
  pages[0xBF][0] = 1;
  auto plan = PlanOverworldTerrainSave(rom, storage, pages);
  ASSERT_TRUE(plan.ok()) << plan.status();
  ASSERT_TRUE(ApplyOverworldTerrainSave(rom, *plan).ok());
  EXPECT_GE(StreamPc(storage.low_table_pc + 0xBF * 3), 0x18000u);
  CheckColdReadback();
}

TEST_F(OverworldTerrainIoTest, UnrelatedReadSetDriftRefusesStalePlan) {
  MakeRom();
  pages[0xBF][0] = 1;
  auto plan = PlanOverworldTerrainSave(rom, storage, pages);
  ASSERT_TRUE(plan.ok()) << plan.status();
  ASSERT_TRUE(rom.WriteByte(0x2345, 0x42).ok());
  const auto changed = rom.vector();
  EXPECT_FALSE(ApplyOverworldTerrainSave(rom, *plan).ok());
  EXPECT_EQ(rom.vector(), changed);
  EXPECT_TRUE(rom.dirty());
}

TEST_F(OverworldTerrainIoTest,
       OuterFenceFailureRestoresPointerWritesAndDirtyState) {
  MakeRom();
  pages[0xBF][0] = 1;
  const auto before = rom.vector();
  auto plan = PlanOverworldTerrainSave(rom, storage, pages);
  ASSERT_TRUE(plan.ok()) << plan.status();
  rom::WriteFence outer;
  ASSERT_TRUE(outer
                  .Allow(storage.high_table_pc, storage.high_table_pc + 192 * 3,
                         "high only")
                  .ok());
  ASSERT_TRUE(outer
                  .Allow(storage.low_table_pc, storage.low_table_pc + 192 * 3,
                         "low only")
                  .ok());
  rom::ScopedWriteFence scope(&rom, &outer);
  EXPECT_EQ(ApplyOverworldTerrainSave(rom, *plan).code(),
            absl::StatusCode::kPermissionDenied);
  EXPECT_EQ(rom.vector(), before);
  EXPECT_FALSE(rom.dirty());
}

TEST_F(OverworldTerrainIoTest, MalformedLastPointerFailsBeforeAnyWrite) {
  MakeRom();
  ASSERT_TRUE(rom.WriteLong(storage.low_table_pc + 0xBF * 3, 0).ok());
  const auto before = rom.vector();
  EXPECT_FALSE(PlanOverworldTerrainSave(rom, storage, pages).ok());
  EXPECT_EQ(rom.vector(), before);
}

TEST_F(OverworldTerrainIoTest, CapacityMismatchAndStorageOverlapAreRejected) {
  MakeRom();
  storage.map_count = 160;
  EXPECT_FALSE(PlanOverworldTerrainSave(rom, storage, pages).ok());
  storage.map_count = 192;
  storage.low_table_pc = storage.high_table_pc + 3;
  EXPECT_FALSE(PlanOverworldTerrainSave(rom, storage, pages).ok());
}

TEST_F(OverworldTerrainIoTest, NativePrefixAndTailPublishInOnePlan) {
  MakeRom();
  AddNativePrefix();
  pages[0x80][0] = 0x0101;
  pages[0x9F][12] = 0x0222;
  pages[0xA0][100] = 0x0333;
  pages[0xBF][255] = 0x0444;
  auto plan = PlanOverworldTerrainSave(rom, storage, pages);
  ASSERT_TRUE(plan.ok()) << plan.status();
  ASSERT_TRUE(ApplyOverworldTerrainSave(rom, *plan).ok());
  CheckColdReadback();
  for (auto [primary, mirror] : {std::pair{storage.high_table_pc, 0x2000U},
                                 std::pair{storage.low_table_pc, 0x2300U}}) {
    EXPECT_TRUE(std::equal(rom.vector().begin() + primary,
                           rom.vector().begin() + primary + 160 * 3,
                           rom.vector().begin() + mirror));
  }
  EXPECT_EQ(rom.vector()[0x21E0], 0x77);
  EXPECT_EQ(rom.vector()[0x24E0], 0x66);
  // Decode through the roots read by the retained game engine as well.
  storage.high_table_pc = 0x2000;
  storage.low_table_pc = 0x2300;
  storage.map_count = 160;
  pages.resize(160);
  CheckColdReadback();
}

TEST_F(OverworldTerrainIoTest, DivergentNativePrefixRefusesBeforeMutation) {
  MakeRom();
  AddNativePrefix();
  ASSERT_TRUE(rom.WriteLong(0x2300, PcToSnes(0x10000) | 0x800000).ok());
  const auto before = rom.vector();
  auto plan = PlanOverworldTerrainSave(rom, storage, pages);
  EXPECT_EQ(plan.status().code(), absl::StatusCode::kFailedPrecondition);
  EXPECT_EQ(rom.vector(), before);
}

TEST_F(OverworldTerrainIoTest, NativePrefixFenceFailureRollsBackAllTables) {
  MakeRom();
  AddNativePrefix();
  pages[0x9F][0] = 0x0123;
  auto plan = PlanOverworldTerrainSave(rom, storage, pages);
  ASSERT_TRUE(plan.ok()) << plan.status();
  const auto before = rom.vector();
  rom::WriteFence outer;
  ASSERT_TRUE(outer
                  .Allow(storage.high_table_pc, storage.high_table_pc + 192 * 3,
                         "directory high")
                  .ok());
  ASSERT_TRUE(outer
                  .Allow(storage.low_table_pc, storage.low_table_pc + 192 * 3,
                         "directory low")
                  .ok());
  ASSERT_TRUE(outer.Allow(0x10000, 0x18000, "streams").ok());
  rom::ScopedWriteFence scope(&rom, &outer);
  EXPECT_EQ(ApplyOverworldTerrainSave(rom, *plan).code(),
            absl::StatusCode::kPermissionDenied);
  EXPECT_EQ(rom.vector(), before);
  EXPECT_FALSE(rom.dirty());
}

TEST_F(OverworldTerrainIoTest, ReadOnlyStreamsRemainIntactAfterEveryUserMoves) {
  MakeRom(160, 0x10008);
  storage.read_only_stream_regions = storage.stream_regions;
  storage.stream_regions = {{0x18000, 0x18100, "new writable pool"}};
  const auto before = rom.vector();
  for (auto& page : pages)
    page.fill(0x0101);
  auto plan = PlanOverworldTerrainSave(rom, storage, pages);
  ASSERT_TRUE(plan.ok()) << plan.status();
  ASSERT_TRUE(ApplyOverworldTerrainSave(rom, *plan).ok());
  EXPECT_TRUE(std::equal(before.begin() + 0x10000, before.begin() + 0x10008,
                         rom.vector().begin() + 0x10000));
  EXPECT_GE(StreamPc(storage.high_table_pc), 0x18000u);
  CheckColdReadback();
}

TEST_F(OverworldTerrainIoTest, ReadOnlyPoolCannotHideWritableExhaustion) {
  MakeRom();
  AddNativePrefix();
  storage.read_only_stream_regions = storage.stream_regions;
  storage.stream_regions = {{0x18000, 0x18003, "too small writable pool"}};
  pages[0x9F][0] = 0x0101;
  const auto before = rom.vector();
  auto plan = PlanOverworldTerrainSave(rom, storage, pages);
  EXPECT_EQ(plan.status().code(), absl::StatusCode::kResourceExhausted);
  EXPECT_EQ(rom.vector(), before);
  EXPECT_FALSE(rom.dirty());
}

}  // namespace
}  // namespace yaze::zelda3
