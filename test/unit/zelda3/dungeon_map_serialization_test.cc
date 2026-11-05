#include "zelda3/screen/dungeon_map.h"

#include <gtest/gtest.h>

#include <algorithm>
#include <cstdlib>

#include <vector>
#include "app/gfx/resource/arena.h"
#include "rom/snes.h"
#include "rom/write_fence.h"

namespace yaze::zelda3 {
namespace {
std::vector<uint8_t> MapRomBytes(size_t size = 0x200000) {
  std::vector<uint8_t> bytes(size, 0xA5);
  bytes[kDungeonMapExpCheck] = 0x20;
  return bytes;
}
DungeonMap MakeMap(int basements, int floors, int boss_floor = -1) {
  std::vector<std::array<uint8_t, kNumRooms>> rooms(basements + floors);
  auto graphics = rooms;
  for (auto& floor : rooms)
    floor.fill(0x0F);
  for (auto& floor : graphics)
    floor.fill(0xFF);
  if (boss_floor >= 0) {
    rooms[boss_floor][3] = 0x42;
    graphics[boss_floor][3] = 0x21;
  }
  return DungeonMap(boss_floor < 0 ? 0xFFFF : 0x42, floors, basements, rooms,
                    graphics);
}

std::vector<DungeonMap> FullMaps() {
  std::vector<DungeonMap> maps;
  while (maps.size() < kNumDungeons)
    maps.push_back(MakeMap(0, 1));
  return maps;
}

TEST(DungeonMapSerializationTest, SignedFloorLowByteAndNoBossRoundTrip) {
  Rom rom;
  ASSERT_TRUE(rom.LoadFromData(MapRomBytes()).ok());
  std::vector<DungeonMap> maps;
  maps.push_back(MakeMap(4, 1, 0));  // B4 -> 00FC
  maps.push_back(MakeMap(1, 1, 0));  // B1 -> 00FF, NOT FFFF
  maps.push_back(MakeMap(2, 1, 2));  // 1F -> 0000
  maps.push_back(MakeMap(1, 7, 7));  // 7F -> 0006
  maps.push_back(MakeMap(0, 1));
  maps.push_back(MakeMap(0, 1));
  maps.back().boss_room = 0x000F;
  while (maps.size() < kNumDungeons)
    maps.push_back(MakeMap(0, 1));
  ASSERT_TRUE(SaveDungeonMaps(rom, maps).ok());
  const uint16_t expected[] = {0x00FC, 0x00FF, 0x0000, 0x0006, 0xFFFF, 0xFFFF};
  for (int d = 0; d < 6; ++d) {
    ASSERT_TRUE(rom.ReadWord(kDungeonMapBossFloors + d * 2).ok());
    EXPECT_EQ(*rom.ReadWord(kDungeonMapBossFloors + d * 2), expected[d]) << d;
  }
  DungeonMapLabels labels;
  auto loaded = LoadDungeonMaps(rom, labels);
  ASSERT_TRUE(loaded.ok());
  ASSERT_EQ(loaded->size(), maps.size());
  for (size_t d = 0; d < maps.size(); ++d) {
    EXPECT_EQ((*loaded)[d].boss_room, maps[d].boss_room);
    EXPECT_EQ((*loaded)[d].nbr_of_basement, maps[d].nbr_of_basement);
    EXPECT_EQ((*loaded)[d].nbr_of_floor, maps[d].nbr_of_floor);
    EXPECT_EQ((*loaded)[d].floor_rooms, maps[d].floor_rooms);
    EXPECT_EQ((*loaded)[d].floor_gfx, maps[d].floor_gfx);
  }
  const auto first_save = rom.vector();
  ASSERT_TRUE(SaveDungeonMaps(rom, *loaded).ok());
  EXPECT_EQ(rom.vector(), first_save);
}

TEST(DungeonMapSerializationTest, MissingBossInLaterMapRefusesBeforeAnyWrite) {
  Rom rom;
  ASSERT_TRUE(rom.LoadFromData(MapRomBytes()).ok());
  auto maps = FullMaps();
  maps[0] = MakeMap(1, 1, 0);
  maps.back().boss_room = 0x77;
  rom.set_dirty(false);
  const auto before = rom.vector();
  EXPECT_EQ(SaveDungeonMaps(rom, maps).code(), absl::StatusCode::kNotFound);
  EXPECT_EQ(rom.vector(), before);
  EXPECT_FALSE(rom.dirty());
}

TEST(DungeonMapSerializationTest,
     InvalidDimensionsAndBossRefuseBeforeAnyWrite) {
  for (int invalid = 0; invalid < 6; ++invalid) {
    SCOPED_TRACE(invalid);
    Rom rom;
    ASSERT_TRUE(rom.LoadFromData(MapRomBytes()).ok());
    auto maps = FullMaps();
    maps[0] = MakeMap(1, 1, 0);
    switch (invalid) {
      case 0:
        maps.back().nbr_of_floor = 16;
        break;
      case 1:
        maps.back().nbr_of_basement = 16;
        break;
      case 2:
        maps.back().floor_rooms.clear();
        break;
      case 3:
        maps.back().floor_gfx.clear();
        break;
      case 4:
        maps.back().boss_room = 0x142;
        break;
      case 5:
        while (maps.size() <= kNumDungeons)
          maps.push_back(MakeMap(0, 1));
        break;
    }
    rom.set_dirty(false);
    const auto before = rom.vector();
    EXPECT_EQ(SaveDungeonMaps(rom, maps).code(),
              absl::StatusCode::kInvalidArgument);
    EXPECT_EQ(rom.vector(), before);
    EXPECT_FALSE(rom.dirty());
  }
}

TEST(DungeonMapSerializationTest,
     ShortRomPlanRefusalPreservesBufferAndDirtyState) {
  for (bool dirty : {false, true}) {
    Rom rom;
    // Planning must reject this truncated image before applying any writes.
    ASSERT_TRUE(rom.LoadFromData(MapRomBytes(0x57600)).ok());
    ASSERT_TRUE(rom.WriteByte(kDungeonMapExpCheck, 0xB9).ok());
    rom.set_filename("owned-fixture.sfc");
    rom.set_dirty(dirty);
    const auto before = rom.vector();
    auto maps = FullMaps();
    maps[0] = MakeMap(1, 1, 0);
    EXPECT_EQ(SaveDungeonMaps(rom, maps).code(), absl::StatusCode::kOutOfRange);
    EXPECT_EQ(rom.vector(), before);
    EXPECT_EQ(rom.dirty(), dirty);
    EXPECT_EQ(rom.filename(), "owned-fixture.sfc");
  }
}

TEST(DungeonMapSerializationTest, DenseStreamsPreserveBothPointerBlocks) {
  Rom rom;
  ASSERT_TRUE(rom.LoadFromData(MapRomBytes()).ok());
  auto maps = FullMaps();
  for (int d = 0; d < kNumDungeons; ++d) {
    maps[d] = MakeMap(0, d >= 12 ? 5 : 4);
    for (auto& row : maps[d].floor_rooms)
      row.fill(0x20 + d);
    for (auto& row : maps[d].floor_gfx)
      row.fill(0x40 + d);
  }
  ASSERT_TRUE(SaveDungeonMaps(rom, maps).ok());
  DungeonMapLabels labels;
  auto loaded = LoadDungeonMaps(rom, labels);
  ASSERT_TRUE(loaded.ok());
  for (int d = 0; d < kNumDungeons; ++d) {
    EXPECT_EQ((*loaded)[d].floor_rooms, maps[d].floor_rooms) << d;
    EXPECT_EQ((*loaded)[d].floor_gfx, maps[d].floor_gfx) << d;
  }
}

TEST(DungeonMapSerializationTest,
     PlanIsPureDisjointBankBoundedAndAppliedExactly) {
  Rom rom;
  ASSERT_TRUE(rom.LoadFromData(MapRomBytes()).ok());
  auto maps = FullMaps();
  for (int d = 0; d < kNumDungeons; ++d) {
    maps[d] = MakeMap(0, d >= 12 ? 5 : 4);
    for (auto& row : maps[d].floor_rooms)
      row.fill(0x20 + d);
    for (auto& row : maps[d].floor_gfx)
      row.fill(0x40 + d);
  }
  const auto before = rom.vector();
  rom.set_dirty(false);
  auto plan =
      BuildDungeonMapWritePlan(maps, rom.size(), /*relocated_tile16=*/true);
  ASSERT_TRUE(plan.ok()) << plan.status();
  EXPECT_EQ(rom.vector(), before);
  EXPECT_FALSE(rom.dirty());
  std::vector<bool> written(rom.size());
  auto expected = before;
  bool after_second_table = false;
  for (const auto& write : plan->writes) {
    EXPECT_EQ(PcToSnes(write.pc_address) >> 16, 0x0A);
    EXPECT_EQ(PcToSnes(write.pc_address + write.data.size() - 1) >> 16, 0x0A);
    for (size_t i = 0; i < write.data.size(); ++i) {
      const size_t pc = write.pc_address + i;
      ASSERT_LT(pc, written.size());
      EXPECT_FALSE(written[pc]);
      written[pc] = true;
      expected[pc] = write.data[i];
    }
    if (write.data.size() > 2) {
      const int end = write.pc_address + write.data.size();
      EXPECT_TRUE(end <= kDungeonMapDataReservedStart ||
                  write.pc_address > kDungeonMapDataReservedEnd);
      EXPECT_TRUE(end <= kDungeonMapGfxPtr ||
                  write.pc_address >= kDungeonMapGfxPtr + 2 * kNumDungeons);
      after_second_table |=
          write.pc_address >= kDungeonMapGfxPtr + 2 * kNumDungeons;
    }
  }
  EXPECT_TRUE(after_second_table);
  ASSERT_TRUE(SaveDungeonMaps(rom, maps).ok());
  EXPECT_EQ(rom.vector(), expected);  // Includes every unrelated-byte canary.
}

TEST(DungeonMapSerializationTest, CapacityAndIncompleteSetRefuseAtomically) {
  Rom rom;
  ASSERT_TRUE(rom.LoadFromData(MapRomBytes()).ok());
  auto maps = FullMaps();
  for (auto& map : maps) {
    map = MakeMap(8, 8);
    for (auto& row : map.floor_rooms)
      row.fill(0x22);
  }
  const auto before = rom.vector();
  rom.set_dirty(false);
  EXPECT_EQ(SaveDungeonMaps(rom, maps).code(), absl::StatusCode::kOutOfRange);
  EXPECT_EQ(rom.vector(), before);
  EXPECT_FALSE(rom.dirty());
  maps = FullMaps();
  maps.pop_back();
  EXPECT_EQ(SaveDungeonMaps(rom, maps).code(),
            absl::StatusCode::kInvalidArgument);
  EXPECT_EQ(rom.vector(), before);
  EXPECT_FALSE(rom.dirty());
}

TEST(DungeonMapSerializationTest, ApplyFailureRollsBackEarlierPlannedWrites) {
  Rom rom;
  ASSERT_TRUE(rom.LoadFromData(MapRomBytes()).ok());
  auto maps = FullMaps();
  auto plan =
      BuildDungeonMapWritePlan(maps, rom.size(), /*relocated_tile16=*/true);
  ASSERT_TRUE(plan.ok());
  ASSERT_GT(plan->writes.size(), 1u);
  const auto& first = plan->writes.front();
  rom::WriteFence fence;
  ASSERT_TRUE(fence
                  .Allow(first.pc_address, first.pc_address + first.data.size(),
                         "first planned write only")
                  .ok());
  const auto before = rom.vector();
  rom.set_dirty(false);
  rom::ScopedWriteFence scoped(&rom, &fence);
  EXPECT_EQ(SaveDungeonMaps(rom, maps).code(),
            absl::StatusCode::kPermissionDenied);
  EXPECT_FALSE(
      fence.written_ranges().empty());  // Confirms a write preceded failure.
  EXPECT_EQ(rom.vector(), before);
  EXPECT_FALSE(rom.dirty());
}

TEST(DungeonMapSerializationTest,
     VanillaPreservesTile16AndUnknownFormatRefuses) {
  Rom rom;
  auto bytes = MapRomBytes();
  bytes[kDungeonMapExpCheck] = 0xB9;
  ASSERT_TRUE(rom.LoadFromData(bytes).ok());
  auto maps = FullMaps();
  ASSERT_TRUE(SaveDungeonMaps(rom, maps).ok());
  EXPECT_TRUE(std::equal(bytes.begin() + kDungeonMapTile16,
                         bytes.begin() + kDungeonMapDataReservedStart,
                         rom.vector().begin() + kDungeonMapTile16));
  ASSERT_TRUE(rom.WriteByte(kDungeonMapExpCheck, 0xEA).ok());
  const auto before = rom.vector();
  rom.set_dirty(false);
  EXPECT_EQ(SaveDungeonMaps(rom, maps).code(),
            absl::StatusCode::kFailedPrecondition);
  EXPECT_EQ(rom.vector(), before);
  EXPECT_FALSE(rom.dirty());
}

TEST(DungeonMapSerializationTest, InvalidRelocatedArtworkRefusesWithoutWrites) {
  struct RestoreSettings {
    core::RomAddressOverrides previous =
        core::RomSettings::Get().address_overrides();
    ~RestoreSettings() {
      core::RomSettings::Get().SetAddressOverrides(previous);
    }
  } restore;
  for (uint32_t address : {uint32_t{0x7FFFFFFF}, uint32_t{kDungeonMapDataStart},
                           uint32_t{kDungeonMapBossRooms}}) {
    SCOPED_TRACE(address);
    core::RomAddressOverrides overrides;
    overrides.addresses[core::RomAddressKey::kDungeonMapTile16Expanded] =
        address;
    core::RomSettings::Get().SetAddressOverrides(overrides);
    Rom rom;
    ASSERT_TRUE(rom.LoadFromData(MapRomBytes()).ok());
    auto maps = FullMaps();
    const auto before = rom.vector();
    rom.set_dirty(false);
    EXPECT_EQ(SaveDungeonMaps(rom, maps).code(),
              absl::StatusCode::kFailedPrecondition);
    EXPECT_EQ(rom.vector(), before);
    EXPECT_FALSE(rom.dirty());
  }
}

class DungeonMapTile16Test : public ::testing::Test {
 protected:
  void SetUp() override {
    previous_overrides_ = core::RomSettings::Get().address_overrides();
    core::RomSettings::Get().SetAddressOverrides({});
  }
  void TearDown() override {
    gfx::Arena::Get().RetireBitmap(tiles_.atlas);
    core::RomSettings::Get().SetAddressOverrides(previous_overrides_);
  }
  void FillTiles(int count) {
    const auto blank = gfx::WordToTileInfo(0x0B00);
    tiles_.tile_info.assign(count, {blank, blank, blank, blank});
  }
  static std::vector<uint8_t> GraphicsPixels() {
    std::vector<uint8_t> pixels(4 * 128 * 32);
    for (int tile = 0; tile < 256; ++tile) {
      const int x = (tile % 16) * 8;
      const int y = (tile / 64) * 32 + ((tile % 64) / 16) * 8;
      for (int dy = 0; dy < 8; ++dy) {
        for (int dx = 0; dx < 8; ++dx) {
          pixels[(y + dy) * 128 + x + dx] = tile & 0x0F;
        }
      }
    }
    return pixels;
  }
  static void SeedWords(std::vector<uint8_t>& bytes, int address, int count) {
    for (int i = 0; i < count * 4; ++i) {
      bytes[address + i * 2] = 0;
      bytes[address + i * 2 + 1] = 0x0B;
    }
  }
  gfx::Tilemap tiles_{};
  core::RomAddressOverrides previous_overrides_;
};

TEST_F(DungeonMapTile16Test, ResolvesValidatedCountAndAddressOverride) {
  Rom rom;
  ASSERT_TRUE(rom.LoadFromData(MapRomBytes()).ok());
  auto expanded = ResolveDungeonMapTile16Layout(rom);
  ASSERT_TRUE(expanded.ok());
  EXPECT_EQ(expanded->template_count, 256);
  EXPECT_EQ(expanded->pc_address, kDungeonMapTile16Expanded);
  EXPECT_TRUE(expanded->relocated);
  core::RomAddressOverrides overrides;
  overrides.addresses[core::RomAddressKey::kDungeonMapTile16Expanded] =
      0x180000;
  core::RomSettings::Get().SetAddressOverrides(overrides);
  auto moved = ResolveDungeonMapTile16Layout(rom);
  ASSERT_TRUE(moved.ok());
  EXPECT_EQ(moved->pc_address, 0x180000);
  EXPECT_EQ(moved->template_count, 256);
  ASSERT_TRUE(rom.WriteByte(kDungeonMapExpCheck, 0xB9).ok());
  auto vanilla = ResolveDungeonMapTile16Layout(rom);
  ASSERT_TRUE(vanilla.ok());
  EXPECT_EQ(vanilla->pc_address, kDungeonMapTile16);
  EXPECT_EQ(vanilla->template_count, 186);
  EXPECT_FALSE(vanilla->relocated);
}

TEST_F(DungeonMapTile16Test, LastExpandedSlotRendersAndReloadReplacesModel) {
  auto bytes = MapRomBytes();
  SeedWords(bytes, kDungeonMapTile16Expanded, 256);
  SeedWords(bytes, kDungeonMapTile16, 186);
  const uint16_t words[] = {0x0B01, 0x4B02, 0x8B03, 0xCB04};
  for (int q = 0; q < 4; ++q) {
    bytes[kDungeonMapTile16Expanded + 255 * 8 + q * 2] = words[q] & 0xFF;
    bytes[kDungeonMapTile16Expanded + 255 * 8 + q * 2 + 1] = words[q] >> 8;
    bytes[kDungeonMapTile16 + q * 2] = words[q] & 0xFF;
    bytes[kDungeonMapTile16 + q * 2 + 1] = words[q] >> 8;
  }
  Rom rom;
  ASSERT_TRUE(rom.LoadFromData(bytes).ok());
  for (int reload = 0; reload < 2; ++reload) {
    ASSERT_TRUE(
        LoadDungeonMapTile16(tiles_, rom, nullptr, GraphicsPixels(), true)
            .ok());
    ASSERT_EQ(tiles_.tile_info.size(), 256u);
    EXPECT_EQ(tiles_.atlas.width(), 256);
    EXPECT_EQ(tiles_.atlas.height(), 256);
    for (int q = 0; q < 4; ++q) {
      EXPECT_EQ(gfx::TileInfoToWord(tiles_.tile_info[255][q]), words[q]);
    }
    const auto pixels = gfx::GetTilemapData(tiles_, 255);
    ASSERT_EQ(pixels.size(), 256u);
    EXPECT_EQ(pixels[0], 1);
    EXPECT_EQ(pixels[8], 2);
    EXPECT_EQ(pixels[8 * 16], 3);
    EXPECT_EQ(pixels[8 * 16 + 8], 4);
  }
  ASSERT_TRUE(rom.WriteByte(kDungeonMapExpCheck, 0xB9).ok());
  ASSERT_TRUE(
      LoadDungeonMapTile16(tiles_, rom, nullptr, GraphicsPixels(), true).ok());
  ASSERT_EQ(tiles_.tile_info.size(), 186u);
  EXPECT_EQ(tiles_.atlas.height(), 192);
  EXPECT_EQ(gfx::GetTilemapData(tiles_, 0)[0], 1);
}

TEST_F(DungeonMapTile16Test, ExpandedSavePersistsLastSlotAndPreservesOutside) {
  Rom rom;
  ASSERT_TRUE(rom.LoadFromData(MapRomBytes()).ok());
  FillTiles(256);
  const uint16_t words[] = {0x0B01, 0x4B02, 0x8B03, 0xCB04};
  for (int q = 0; q < 4; ++q) {
    tiles_.tile_info[255][q] = gfx::WordToTileInfo(words[q]);
  }
  const auto before = rom.vector();
  ASSERT_TRUE(SaveDungeonMapTile16(tiles_, rom).ok());
  const int end = kDungeonMapTile16Expanded + 256 * 8;
  EXPECT_TRUE(std::equal(before.begin(),
                         before.begin() + kDungeonMapTile16Expanded,
                         rom.vector().begin()));
  EXPECT_TRUE(std::equal(before.begin() + end, before.end(),
                         rom.vector().begin() + end));
  Rom reopened;
  ASSERT_TRUE(reopened.LoadFromData(rom.vector()).ok());
  for (int q = 0; q < 4; ++q) {
    EXPECT_EQ(*reopened.ReadWord(kDungeonMapTile16Expanded + 255 * 8 + q * 2),
              words[q]);
  }
}

TEST_F(DungeonMapTile16Test, VanillaSaveKeepsLowSlotAndStopsBeforeTables) {
  auto bytes = MapRomBytes();
  bytes[kDungeonMapExpCheck] = 0xB9;
  Rom rom;
  ASSERT_TRUE(rom.LoadFromData(bytes).ok());
  FillTiles(186);
  tiles_.tile_info[0][0] = gfx::WordToTileInfo(0xCB55);
  ASSERT_TRUE(SaveDungeonMapTile16(tiles_, rom).ok());
  EXPECT_EQ(*rom.ReadWord(kDungeonMapTile16), 0xCB55);
  EXPECT_TRUE(std::equal(bytes.begin(), bytes.begin() + kDungeonMapTile16,
                         rom.vector().begin()));
  EXPECT_TRUE(std::equal(bytes.begin() + kDungeonMapDataReservedStart,
                         bytes.end(),
                         rom.vector().begin() + kDungeonMapDataReservedStart));
}

TEST_F(DungeonMapTile16Test, WrongModelCountRefusesBeforeWrite) {
  for (bool expanded : {false, true}) {
    for (int delta : {-1, 1}) {
      Rom rom;
      auto bytes = MapRomBytes();
      bytes[kDungeonMapExpCheck] = expanded ? 0x20 : 0xB9;
      ASSERT_TRUE(rom.LoadFromData(bytes).ok());
      FillTiles((expanded ? 256 : 186) + delta);
      rom.set_dirty(false);
      EXPECT_EQ(SaveDungeonMapTile16(tiles_, rom).code(),
                absl::StatusCode::kInvalidArgument);
      EXPECT_EQ(rom.vector(), bytes);
      EXPECT_FALSE(rom.dirty());
    }
  }
}

TEST_F(DungeonMapTile16Test, UnknownMarkerKeepsModelAndRomUnchanged) {
  Rom rom;
  auto bytes = MapRomBytes();
  bytes[kDungeonMapExpCheck] = 0xEA;
  ASSERT_TRUE(rom.LoadFromData(bytes).ok());
  FillTiles(4);
  const auto before = tiles_.tile_info;
  EXPECT_EQ(
      LoadDungeonMapTile16(tiles_, rom, nullptr, GraphicsPixels(), true).code(),
      absl::StatusCode::kFailedPrecondition);
  EXPECT_EQ(tiles_.tile_info, before);
  EXPECT_EQ(SaveDungeonMapTile16(tiles_, rom).code(),
            absl::StatusCode::kFailedPrecondition);
  EXPECT_EQ(rom.vector(), bytes);
}

TEST_F(DungeonMapTile16Test, ExpandedTailBoundsAndMetadataOverlapRefuse) {
  FillTiles(256);
  for (bool overlap : {false, true}) {
    auto bytes =
        MapRomBytes(overlap ? 0x200000 : kDungeonMapTile16Expanded + 186 * 8);
    core::RomAddressOverrides overrides;
    if (overlap) {
      // The old186-record extent stopped exactly before the boss table;
      // the newly supported tail must not silently overwrite that table.
      overrides.addresses[core::RomAddressKey::kDungeonMapTile16Expanded] =
          kDungeonMapBossRooms - 186 * 8;
    }
    core::RomSettings::Get().SetAddressOverrides(overrides);
    Rom rom;
    ASSERT_TRUE(rom.LoadFromData(bytes).ok());
    rom.set_dirty(false);
    EXPECT_EQ(SaveDungeonMapTile16(tiles_, rom).code(),
              absl::StatusCode::kFailedPrecondition);
    auto maps = FullMaps();
    EXPECT_EQ(SaveDungeonMaps(rom, maps).code(),
              absl::StatusCode::kFailedPrecondition);
    EXPECT_EQ(rom.vector(), bytes);
    EXPECT_FALSE(rom.dirty());
  }
}

TEST_F(DungeonMapTile16Test, ArtworkFenceFailureRollsBackPartialWrite) {
  Rom rom;
  ASSERT_TRUE(rom.LoadFromData(MapRomBytes()).ok());
  FillTiles(256);
  rom::WriteFence fence;
  ASSERT_TRUE(fence
                  .Allow(kDungeonMapTile16Expanded,
                         kDungeonMapTile16Expanded + 2, "first word only")
                  .ok());
  const auto before = rom.vector();
  rom.set_dirty(false);
  rom::ScopedWriteFence scoped(&rom, &fence);
  EXPECT_EQ(SaveDungeonMapTile16(tiles_, rom).code(),
            absl::StatusCode::kPermissionDenied);
  EXPECT_FALSE(fence.written_ranges().empty());
  EXPECT_EQ(rom.vector(), before);
  EXPECT_FALSE(rom.dirty());
}

TEST_F(DungeonMapTile16Test, ExpandedAssignment255RoundTripsAndVanillaRefuses) {
  auto maps = FullMaps();
  maps[0].floor_rooms[0][0] = 0x42;
  maps[0].floor_gfx[0][0] = 0xFF;
  Rom rom;
  ASSERT_TRUE(rom.LoadFromData(MapRomBytes()).ok());
  const auto before = rom.vector();
  ASSERT_TRUE(SaveDungeonMaps(rom, maps).ok());
  DungeonMapLabels labels;
  auto loaded = LoadDungeonMaps(rom, labels);
  ASSERT_TRUE(loaded.ok());
  ASSERT_EQ(loaded->size(), kNumDungeons);
  EXPECT_EQ((*loaded)[0].floor_rooms[0][0], 0x42);
  EXPECT_EQ((*loaded)[0].floor_gfx[0][0], 0xFF);
  EXPECT_TRUE(std::equal(before.begin() + kDungeonMapTile16Expanded,
                         before.begin() + kDungeonMapTile16Expanded + 256 * 8,
                         rom.vector().begin() + kDungeonMapTile16Expanded));
  EXPECT_EQ(BuildDungeonMapWritePlan(maps, 0x200000, false).status().code(),
            absl::StatusCode::kInvalidArgument);
}

TEST(DungeonMapSerializationTest,
     VanillaArtifactRoundTripsWithoutArtworkWrites) {
  const char* path = std::getenv("YAZE_TEST_DUNGEON_MAP_VANILLA_ROM");
  if (!path)
    GTEST_SKIP() << "Set YAZE_TEST_DUNGEON_MAP_VANILLA_ROM";
  Rom rom;
  ASSERT_TRUE(rom.LoadFromFile(path).ok());
  ASSERT_EQ(*rom.ReadByte(kDungeonMapExpCheck), 0xB9);
  DungeonMapLabels labels;
  auto maps = LoadDungeonMaps(rom, labels);
  ASSERT_TRUE(maps.ok());
  const auto before = rom.vector();
  ASSERT_TRUE(SaveDungeonMaps(rom, *maps).ok());
  EXPECT_TRUE(std::equal(before.begin() + kDungeonMapTile16,
                         before.begin() + kDungeonMapDataReservedStart,
                         rom.vector().begin() + kDungeonMapTile16));
  DungeonMapLabels new_labels;
  auto loaded = LoadDungeonMaps(rom, new_labels);
  ASSERT_TRUE(loaded.ok());
  for (int d = 0; d < kNumDungeons; ++d) {
    EXPECT_EQ((*loaded)[d].floor_rooms, (*maps)[d].floor_rooms);
    EXPECT_EQ((*loaded)[d].floor_gfx, (*maps)[d].floor_gfx);
  }
}

TEST(DungeonMapSerializationTest, OracleArtifactRoundTripKeepsEveryMapRecord) {
  const char* path = std::getenv("YAZE_TEST_DUNGEON_MAP_ROM");
  if (!path)
    GTEST_SKIP() << "Set YAZE_TEST_DUNGEON_MAP_ROM to the read-only fixture";
  Rom rom;
  ASSERT_TRUE(rom.LoadFromFile(path).ok());
  DungeonMapLabels labels;
  auto maps = LoadDungeonMaps(rom, labels);
  ASSERT_TRUE(maps.ok());
  const auto before = *maps;
  ASSERT_TRUE(SaveDungeonMaps(rom, *maps).ok());
  DungeonMapLabels fresh_labels;
  auto loaded = LoadDungeonMaps(rom, fresh_labels);
  ASSERT_TRUE(loaded.ok());
  for (int d = 0; d < kNumDungeons; ++d) {
    EXPECT_EQ((*loaded)[d].boss_room, before[d].boss_room);
    EXPECT_EQ((*loaded)[d].nbr_of_floor, before[d].nbr_of_floor);
    EXPECT_EQ((*loaded)[d].nbr_of_basement, before[d].nbr_of_basement);
    EXPECT_EQ((*loaded)[d].floor_rooms, before[d].floor_rooms);
    EXPECT_EQ((*loaded)[d].floor_gfx, before[d].floor_gfx);
  }
  const uint16_t expected[] = {0xFFFF, 0xFFFF, 1,    1, 6, 0xFC, 0,
                               0xFE,   2,      0xFF, 5, 1, 0xFF, 6};
  for (int d = 0; d < kNumDungeons; ++d)
    EXPECT_EQ(*rom.ReadWord(kDungeonMapBossFloors + d * 2), expected[d]) << d;
}
}  // namespace
}  // namespace yaze::zelda3
